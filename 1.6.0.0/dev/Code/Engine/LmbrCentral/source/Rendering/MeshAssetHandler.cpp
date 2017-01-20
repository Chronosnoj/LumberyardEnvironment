/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include "Precompiled.h"

#include <AzCore/std/parallel/conditional_variable.h>
#include <AzCore/IO/GenericStreams.h>
#include <AzFramework/Asset/SimpleAsset.h>
#include <AzFramework/Asset/AssetCatalogBus.h>
#include <AzFramework/StringFunc/StringFunc.h>

#include <LmbrCentral/Rendering/MeshAsset.h>
#include "MeshAssetHandler.h"

#include <ICryAnimation.h>

namespace LmbrCentral
{    
    //////////////////////////////////////////////////////////////////////////
    // Static Mesh Asset Handler
    //////////////////////////////////////////////////////////////////////////

    void AsyncStatObjLoadCallback(const AZ::Data::Asset<StaticMeshAsset>& asset, AZStd::condition_variable* loadVariable, IStatObj* statObj)
    {
        if (statObj)
        {
            asset.Get()->m_statObj = statObj;
        }
        else
        {
#if defined(AZ_DEBUG_BUILD)
            AZStd::string assetDescription = asset.GetId().ToString<AZStd::string>();
            EBUS_EVENT_RESULT(assetDescription, AZ::Data::AssetCatalogRequestBus, GetAssetPathById, asset.GetId());
            AZ_Error("MeshAssetHandler", false, "Failed to load mesh asset \"%s\".", assetDescription.c_str());
#endif // AZ_DEBUG_BUILD
        }

        loadVariable->notify_one();
    }

    StaticMeshAssetHandler::~StaticMeshAssetHandler()
    {
        Unregister();
    }

    AZ::Data::AssetPtr StaticMeshAssetHandler::CreateAsset(const AZ::Data::AssetId& id, const AZ::Data::AssetType& type)
    {
        (void)type;

        AZ_Assert(type == AZ::AzTypeInfo<StaticMeshAsset>::Uuid(), "Invalid asset type! We handle only 'StaticMeshAsset'");

        AZStd::string assetPath;
        EBUS_EVENT_RESULT(assetPath, AZ::Data::AssetCatalogRequestBus, GetAssetPathById, id);

        return aznew StaticMeshAsset();
    }

    bool StaticMeshAssetHandler::LoadAssetData(const AZ::Data::Asset<AZ::Data::AssetData>& /*asset*/, AZ::IO::GenericStream* /*stream*/, const AZ::Data::AssetFilterCB& /*assetLoadFilterCB*/)
    {
        // Load from preloaded stream.
        AZ_Assert(false, "Favor loading through custom stream override of LoadAssetData, in order to load through CryPak.");
        return false;
    }

    bool StaticMeshAssetHandler::LoadAssetData(const AZ::Data::Asset<AZ::Data::AssetData>& asset, const char* assetPath, const AZ::Data::AssetFilterCB& /*assetLoadFilterCB*/)
    {
        AZ_Assert(asset.GetType() == AZ::AzTypeInfo<StaticMeshAsset>::Uuid(), "Invalid asset type! We only load 'StaticMeshAsset'");
        if (StaticMeshAsset* meshAsset = asset.GetAs<StaticMeshAsset>())
        {
            AZ_Assert(!meshAsset->m_statObj.get(), "Attempting to create static mesh without cleaning up the old one.");

            // Strip the alias. StatObj instances are stored in a dictionary by their path,
            // so to share instances with legacy cry entities, we need to use the same un-aliased format.
            static const char assetAliasToken[] = "@assets@/";
            static const size_t assetAliasTokenLen = AZ_ARRAY_SIZE(assetAliasToken) - 1;
            if (0 == strncmp(assetPath, assetAliasToken, assetAliasTokenLen))
            {
                assetPath += assetAliasTokenLen;
            }

            AZStd::mutex loadMutex;
            AZStd::condition_variable loadVariable;

            auto callback = std::bind(&AsyncStatObjLoadCallback, asset, &loadVariable, std::placeholders::_1);

            AZStd::unique_lock<AZStd::mutex> loadLock(loadMutex);
            gEnv->p3DEngine->LoadStatObjAsync(callback, assetPath);

            // Block the job thread on a signal variable until notified of completion (by the main thread).
            loadVariable.wait(loadLock);

            return true;
        }
        return false;
    }

    void StaticMeshAssetHandler::DestroyAsset(AZ::Data::AssetPtr ptr)
    {
        delete ptr;
    }

    void StaticMeshAssetHandler::GetHandledAssetTypes(AZStd::vector<AZ::Data::AssetType>& assetTypes)
    {
        assetTypes.push_back(AZ::AzTypeInfo<StaticMeshAsset>::Uuid());
    }

    void StaticMeshAssetHandler::Register()
    {
        AZ_Assert(AZ::Data::AssetDatabase::IsReady(), "Asset database isn't ready!");
        AZ::Data::AssetDatabase::Instance().RegisterHandler(this, AZ::AzTypeInfo<StaticMeshAsset>::Uuid());

        AZ::AssetTypeInfoBus::Handler::BusConnect(AZ::AzTypeInfo<StaticMeshAsset>::Uuid());
    }

    void StaticMeshAssetHandler::Unregister()
    {
        AZ::AssetTypeInfoBus::Handler::BusDisconnect(AZ::AzTypeInfo<StaticMeshAsset>::Uuid());

        if (AZ::Data::AssetDatabase::IsReady())
        {
            AZ::Data::AssetDatabase::Instance().UnregisterHandler(this);
        }
    }

    const char* StaticMeshAssetHandler::GetAssetTypeDisplayName()
    {
        return "Static Mesh Library";
    }

    void StaticMeshAssetHandler::GetAssetTypeExtensions(AZStd::vector<AZStd::string>& extensions)
    {
        extensions.push_back(CRY_GEOMETRY_FILE_EXT);
    }

    //////////////////////////////////////////////////////////////////////////
    // Skinned Mesh Asset Handler
    //////////////////////////////////////////////////////////////////////////
    
    void AsyncCharacterInstanceLoadCallback(const AZ::Data::Asset<SkinnedMeshAsset>& asset, AZStd::condition_variable* loadVariable, ICharacterInstance* instance)
    {
        if (instance)
        {
            asset.Get()->m_characterInstance = instance;
        }
        else
        {
#if defined(AZ_DEBUG_BUILD)
            AZStd::string assetDescription = asset.GetId().ToString<AZStd::string>();
            EBUS_EVENT_RESULT(assetDescription, AZ::Data::AssetCatalogRequestBus, GetAssetPathById, asset.GetId());
            AZ_Error("MeshAssetHandler", false, "Failed to load character instance asset \"%s\".", asset.GetId().ToString<AZStd::string>().c_str());
#endif // AZ_DEBUG_BUILD
        }

        loadVariable->notify_one();
    }

    SkinnedMeshAssetHandler::~SkinnedMeshAssetHandler()
    {
        Unregister();
    }

    AZ::Data::AssetPtr SkinnedMeshAssetHandler::CreateAsset(const AZ::Data::AssetId& id, const AZ::Data::AssetType& type)
    {
        (void)type;

        AZ_Assert(type == AZ::AzTypeInfo<SkinnedMeshAsset>::Uuid(), "Invalid asset type! We handle only 'SkinnedMeshAsset'");

        AZStd::string assetPath;
        EBUS_EVENT_RESULT(assetPath, AZ::Data::AssetCatalogRequestBus, GetAssetPathById, id);
        
        return aznew SkinnedMeshAsset();
    }

    bool SkinnedMeshAssetHandler::LoadAssetData(const AZ::Data::Asset<AZ::Data::AssetData>& /*asset*/, AZ::IO::GenericStream* /*stream*/, const AZ::Data::AssetFilterCB& /*assetLoadFilterCB*/)
    {
        // Load from preloaded stream.
        AZ_Assert(false, "Favor loading through custom stream override of LoadAssetData, in order to load through CryPak.");
        return false;
    }

    bool SkinnedMeshAssetHandler::LoadAssetData(const AZ::Data::Asset<AZ::Data::AssetData>& asset, const char* assetPath, const AZ::Data::AssetFilterCB& /*assetLoadFilterCB*/)
    {
        AZ_Assert(asset.GetType() == AZ::AzTypeInfo<SkinnedMeshAsset>::Uuid(), "Invalid asset type! We only load 'SkinnedMeshAsset'");
        if (SkinnedMeshAsset* meshAsset = asset.GetAs<SkinnedMeshAsset>())
        {
            AZ_Assert(!meshAsset->m_characterInstance.get(), "Attempting to create character instance without cleaning up the old one.");

            // Strip the alias. Character instances are stored in a dictionary by their path,
            // so to share instances with legacy cry entities, we need to use the same un-aliased format.
            static const char assetAliasToken[] = "@assets@/";
            static const size_t assetAliasTokenLen = AZ_ARRAY_SIZE(assetAliasToken) - 1;
            if (0 == strncmp(assetPath, assetAliasToken, assetAliasTokenLen))
            {
                assetPath += assetAliasTokenLen;
            }

            AZStd::mutex loadMutex;
            AZStd::condition_variable loadVariable;

            auto callback = std::bind(&AsyncCharacterInstanceLoadCallback, asset, &loadVariable, std::placeholders::_1);

            AZStd::unique_lock<AZStd::mutex> loadLock(loadMutex);
            gEnv->pCharacterManager->CreateInstanceAsync(callback, assetPath);

            // Block the job thread on a signal variable until notified of completion (by the main thread).
            loadVariable.wait(loadLock);

            return true;
        }

        return false;
    }

    void SkinnedMeshAssetHandler::DestroyAsset(AZ::Data::AssetPtr ptr)
    {
        delete ptr;
    }

    void SkinnedMeshAssetHandler::GetHandledAssetTypes(AZStd::vector<AZ::Data::AssetType>& assetTypes)
    {
        assetTypes.push_back(AZ::AzTypeInfo<SkinnedMeshAsset>::Uuid());
    }

    void SkinnedMeshAssetHandler::Register()
    {
        AZ_Assert(AZ::Data::AssetDatabase::IsReady(), "Asset database isn't ready!");
        AZ::Data::AssetDatabase::Instance().RegisterHandler(this, AZ::AzTypeInfo<SkinnedMeshAsset>::Uuid());

        AZ::AssetTypeInfoBus::Handler::BusConnect(AZ::AzTypeInfo<SkinnedMeshAsset>::Uuid());
    }

    void SkinnedMeshAssetHandler::Unregister()
    {
        AZ::AssetTypeInfoBus::Handler::BusDisconnect(AZ::AzTypeInfo<SkinnedMeshAsset>::Uuid());

        if (AZ::Data::AssetDatabase::IsReady())
        {
            AZ::Data::AssetDatabase::Instance().UnregisterHandler(this);
        }
    }

    const char* SkinnedMeshAssetHandler::GetAssetTypeDisplayName()
    {
        return "Skinned Mesh Library";
    }

    void SkinnedMeshAssetHandler::GetAssetTypeExtensions(AZStd::vector<AZStd::string>& extensions)
    {
        extensions.push_back(CRY_SKEL_FILE_EXT);
        extensions.push_back(CRY_CHARACTER_DEFINITION_FILE_EXT);
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace LmbrCentral
