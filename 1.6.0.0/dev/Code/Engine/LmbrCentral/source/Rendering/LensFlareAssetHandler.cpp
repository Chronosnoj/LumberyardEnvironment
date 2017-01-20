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

#include <AzCore/IO/GenericStreams.h>
#include <AzFramework/Asset/SimpleAsset.h>
#include <AzFramework/Asset/AssetCatalogBus.h>

#include <ISystem.h>
#include <ICryPak.h>

#include <LmbrCentral/Rendering/LensFlareAsset.h>
#include "LensFlareAssetHandler.h"

#define LENS_FLARE_EXT "xml"

namespace LmbrCentral
{
    LensFlareAssetHandler::~LensFlareAssetHandler()
    {
        Unregister();
    }

    AZ::Data::AssetPtr LensFlareAssetHandler::CreateAsset(const AZ::Data::AssetId& id, const AZ::Data::AssetType& type)
    {
        (void)type;
        AZ_Assert(type == AZ::AzTypeInfo<LensFlareAsset>::Uuid(), "Invalid asset type! We handle only 'LensFlareAsset'");

        if (!CanHandleAsset(id))
        {
            return nullptr;
        }

        return aznew LensFlareAsset;
    }

    bool LensFlareAssetHandler::LoadAssetData(const AZ::Data::Asset<AZ::Data::AssetData>& asset, AZ::IO::GenericStream* stream, const AZ::Data::AssetFilterCB& assetLoadFilterCB)
    {
        (void)assetLoadFilterCB;

        // Load from preloaded stream.
        AZ_Assert(asset.GetType() == AZ::AzTypeInfo<LensFlareAsset>::Uuid(), "Invalid asset type! We handle only 'LensFlareAsset'");
        if (stream)
        {
            LensFlareAsset* data = asset.GetAs<LensFlareAsset>();

            const size_t sizeBytes = static_cast<size_t>(stream->GetLength());
            char* buffer = new char[sizeBytes];
            stream->Read(sizeBytes, buffer);

            bool loaded = LoadFromBuffer(data, buffer, sizeBytes);

            delete[] buffer;

            return loaded;
        }

        return false;
    }

    bool LensFlareAssetHandler::LoadAssetData(const AZ::Data::Asset<AZ::Data::AssetData>& asset, const char* assetPath, const AZ::Data::AssetFilterCB& assetLoadFilterCB)
    {
        (void)assetLoadFilterCB;

        // Load from CryPak.
        AZ::IO::HandleType fileHandle = gEnv->pCryPak->FOpen(assetPath, "rb");
        if (fileHandle)
        {
            LensFlareAsset* data = asset.GetAs<LensFlareAsset>();

            const size_t sizeBytes = gEnv->pCryPak->FGetSize(fileHandle);

            char* buffer = new char[sizeBytes];
            gEnv->pCryPak->FReadRawAll(buffer, sizeBytes, fileHandle);
            gEnv->pCryPak->FClose(fileHandle);
            fileHandle = AZ::IO::InvalidHandle;

            bool loaded = LoadFromBuffer(data, buffer, sizeBytes);

            delete[] buffer;

            return loaded;
        }

        return false;
    }

    void LensFlareAssetHandler::DestroyAsset(AZ::Data::AssetPtr ptr)
    {
        delete ptr;
    }

    void LensFlareAssetHandler::GetHandledAssetTypes(AZStd::vector<AZ::Data::AssetType>& assetTypes)
    {
        assetTypes.push_back(AZ::AzTypeInfo<LensFlareAsset>::Uuid());
    }

    void LensFlareAssetHandler::Register()
    {
        AZ_Assert(AZ::Data::AssetDatabase::IsReady(), "Asset database isn't ready!");
        AZ::Data::AssetDatabase::Instance().RegisterHandler(this, AZ::AzTypeInfo<LensFlareAsset>::Uuid());

        AZ::AssetTypeInfoBus::Handler::BusConnect(AZ::AzTypeInfo<LensFlareAsset>::Uuid());
    }

    void LensFlareAssetHandler::Unregister()
    {
        AZ::AssetTypeInfoBus::Handler::BusDisconnect();

        if (AZ::Data::AssetDatabase::IsReady())
        {
            AZ::Data::AssetDatabase::Instance().UnregisterHandler(this);
        }
    }

    const char* LensFlareAssetHandler::GetAssetTypeDisplayName()
    {
        return "Lens Flare Library";
    }

    void LensFlareAssetHandler::GetAssetTypeExtensions(AZStd::vector<AZStd::string>& extensions)
    {
        extensions.push_back(LENS_FLARE_EXT);
    }

    bool LensFlareAssetHandler::CanHandleAsset(const AZ::Data::AssetId& id) const
    {
        // Look up the asset path to ensure it's actually a lens flare library.
        AZStd::string assetPath;
        EBUS_EVENT_RESULT(assetPath, AZ::Data::AssetCatalogRequestBus, GetAssetPathById, id);

        if (!assetPath.empty() && strstr(assetPath.c_str(), "." LENS_FLARE_EXT))
        {
            XmlNodeRef root = GetISystem()->LoadXmlFromFile(assetPath.c_str());
            if (root)
            {
                AZStd::string rootNode = root->getTag();
                if (rootNode == "LensFlareLibrary")
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool LensFlareAssetHandler::LoadFromBuffer(LensFlareAsset* data, char* buffer, size_t bufferSize)
    {
        XmlNodeRef root = GetISystem()->LoadXmlFromBuffer(buffer, bufferSize);
        if (root)
        {
            const char* name;
            root->getAttr("Name", &name);

            for (int i = 0; i < root->getChildCount(); i++)
            {
                XmlNodeRef itemNode = root->getChild(i);

                // Only accept nodes with correct name.
                if (strcmp(itemNode->getTag(), "FlareItem") != 0)
                {
                    continue;
                }

                AZStd::string flarePath = AZStd::string::format("%s.%s", name, itemNode->getAttr("Name"));
                data->AddPath(std::move(flarePath));
            }

            return true;
        }

        return false;
    }
} // namespace LmbrCentral

