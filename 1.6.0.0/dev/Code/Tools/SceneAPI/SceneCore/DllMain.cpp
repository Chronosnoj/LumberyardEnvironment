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

#if !defined(AZ_MONOLITHIC_BUILD)

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Module/Environment.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <SceneAPI/SceneCore/DataTypes/IManifestObject.h>
#include <SceneAPI/SceneCore/DataTypes/Groups/IGroup.h>
#include <SceneAPI/SceneCore/DataTypes/Groups/IMeshGroup.h>
#include <SceneAPI/SceneCore/DataTypes/Groups/ISkeletonGroup.h>
#include <SceneAPI/SceneCore/DataTypes/Groups/ISkinGroup.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IRule.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/ICommentRule.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IMaterialRule.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IMeshAdvancedRule.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IOriginRule.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IPhysicsRule.h>

#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ManifestBool.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ManifestInt.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ManifestDouble.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ManifestString.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ISceneNodeSelectionList.h>

#include <SceneAPI/SceneCore/Import/ManifestImportRequestHandler.h>

static AZ::SceneAPI::Import::ManifestImportRequestHandler* manifestImporter = nullptr;

extern "C" AZ_DLL_EXPORT void InitializeDynamicModule(void* env)
{
    AZ::Environment::Attach(static_cast<AZ::EnvironmentInstance>(env));
    
    manifestImporter = aznew AZ::SceneAPI::Import::ManifestImportRequestHandler();
}

extern "C" AZ_DLL_EXPORT void Reflect(AZ::SerializeContext* context)
{
    if (!context)
    {
        EBUS_EVENT_RESULT(context, AZ::ComponentApplicationBus, GetSerializeContext);
    }
    if (context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (!serializeContext)
        {
            return;
        }
        serializeContext->Class<AZ::SceneAPI::DataTypes::IManifestObject>();
        // Register group interfaces
        serializeContext->Class<AZ::SceneAPI::DataTypes::IGroup, AZ::SceneAPI::DataTypes::IManifestObject>()->Version(1);
        serializeContext->Class<AZ::SceneAPI::DataTypes::IMeshGroup, AZ::SceneAPI::DataTypes::IGroup>()->Version(1);
        serializeContext->Class<AZ::SceneAPI::DataTypes::ISkeletonGroup, AZ::SceneAPI::DataTypes::IGroup>()->Version(1);
        serializeContext->Class<AZ::SceneAPI::DataTypes::ISkinGroup, AZ::SceneAPI::DataTypes::IGroup>()->Version(1);
        // Register rule interfaces
        serializeContext->Class<AZ::SceneAPI::DataTypes::IRule, AZ::SceneAPI::DataTypes::IManifestObject>()->Version(1);
        serializeContext->Class<AZ::SceneAPI::DataTypes::ICommentRule, AZ::SceneAPI::DataTypes::IRule>()->Version(1);
        serializeContext->Class<AZ::SceneAPI::DataTypes::IMaterialRule, AZ::SceneAPI::DataTypes::IRule>()->Version(1);
        serializeContext->Class<AZ::SceneAPI::DataTypes::IMeshAdvancedRule, AZ::SceneAPI::DataTypes::IRule>()->Version(1);
        serializeContext->Class<AZ::SceneAPI::DataTypes::IOriginRule, AZ::SceneAPI::DataTypes::IRule>()->Version(1);
        serializeContext->Class<AZ::SceneAPI::DataTypes::IPhysicsRule, AZ::SceneAPI::DataTypes::IRule>()->Version(1);

        // Register base manifest types
        serializeContext->Class<AZ::SceneAPI::DataTypes::ISceneNodeSelectionList>()->Version(1);
        AZ::SceneAPI::DataTypes::ManifestBool::Reflect(context);
        AZ::SceneAPI::DataTypes::ManifestInt::Reflect(context);
        AZ::SceneAPI::DataTypes::ManifestDouble::Reflect(context);
        AZ::SceneAPI::DataTypes::ManifestString::Reflect(context);
    }
}

extern "C" AZ_DLL_EXPORT void UninitializeDynamicModule()
{
    delete manifestImporter;
    manifestImporter = nullptr;

    AZ::Environment::Detach();
}

#endif // !defined(AZ_MONOLITHIC_BUILD)
