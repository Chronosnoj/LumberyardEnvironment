/*
 * All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
 * its licensors.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this
 * distribution (the "License"). All use of this software is governed by the License,
 * or, if provided, by the license below or the license accompanying this file. Do not
 * remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "Precompiled.h"
#include "LmbrCentral.h"

#if !defined(AZ_MONOLITHIC_BUILD)
#include <platform_impl.h> // must be included once per DLL so things from CryCommon will function
#endif

#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/list.h>
#include <AzFramework/API/ApplicationAPI.h>
#include <AzFramework/Asset/AssetCatalogBus.h>

// Component descriptors
#include "Animation/AttachmentComponent.h"
#include "Audio/AudioEnvironmentComponent.h"
#include "Audio/AudioProxyComponent.h"
#include "Audio/AudioRtpcComponent.h"
#include "Audio/AudioSwitchComponent.h"
#include "Audio/AudioTriggerComponent.h"
#include "Rendering/DecalComponent.h"
#include "Scripting/FlowGraphComponent.h"
#include "Rendering/LensFlareComponent.h"
#include "Rendering/LightComponent.h"
#include "Rendering/StaticMeshComponent.h"
#include "Rendering/SkinnedMeshComponent.h"
#include "Ai/BehaviorTreeComponent.h"
#include "Ai/NavigationComponent.h"
#include "Rendering/ParticleComponent.h"
#include "Physics/ConstraintComponent.h"
#include "Physics/PhysicsComponent.h"
#include "Physics/PhysicsSystemComponent.h"
#include "Physics/CharacterPhysicsComponent.h"
#include "Physics/RagdollComponent.h"
#include "Animation/SimpleAnimationComponent.h"
#include "Scripting/TagComponent.h"
#include "Scripting/TriggerAreaComponent.h"
#include "Scripting/SimpleStateComponent.h"
#include "Scripting/SpawnerComponent.h"
#include "Scripting/LookAtComponent.h"
#include "Animation/MannequinScopeComponent.h"
#include "Animation/MannequinComponent.h"
#include "Animation/CharacterAnimationManagerComponent.h"

#include "Physics/Colliders/MeshColliderComponent.h"
#include "Physics/Colliders/PrimitiveColliderComponent.h"

// Asset types
#include <AzCore/Prefab/PrefabAsset.h>
#include <AzCore/Script/ScriptAsset.h>
#include <LmbrCentral/Ai/BehaviorTreeAsset.h>
#include <LmbrCentral/Rendering/MaterialAsset.h>
#include <LmbrCentral/Rendering/LensFlareAsset.h>
#include <LmbrCentral/Rendering/MeshAsset.h>
#include <LmbrCentral/Rendering/ParticleAsset.h>
#include <LmbrCentral/Animation/MannequinAsset.h>

// Asset handlers
#include "Rendering/LensFlareAssetHandler.h"
#include "Rendering/MeshAssetHandler.h"
#include "Rendering/ParticleAssetHandler.h"

// Scriptable Ebus Registration
#include "Events/ReflectScriptableEvents.h"

// Shape components
#include "Shape/SphereShapeComponent.h"
#include "Shape/BoxShapeComponent.h"
#include "Shape/CylinderShapeComponent.h"
#include "Shape/CapsuleShapeComponent.h"

// Cry interfaces.
#include <ICryAnimation.h>
#include <I3DEngine.h>

namespace LmbrCentral
{
    static const char* s_assetCatalogFilename = "assetcatalog.xml";

    ////////////////////////////////////////////////////////////////////////////
    // LmbrCentral::LmbrCentralModule
    ////////////////////////////////////////////////////////////////////////////

    //! Create ComponentDescriptors and add them to the list.
    //! The descriptors will be registered at the appropriate time.
    //! The descriptors will be destroyed (and thus unregistered) at the appropriate time.
    LmbrCentralModule::LmbrCentralModule()
        : AZ::Module()
    {
        m_descriptors.insert(m_descriptors.end(), {
            AttachmentComponent::CreateDescriptor(),
            AudioEnvironmentComponent::CreateDescriptor(),
            AudioProxyComponent::CreateDescriptor(),
            AudioRtpcComponent::CreateDescriptor(),
            AudioSwitchComponent::CreateDescriptor(),
            AudioTriggerComponent::CreateDescriptor(),
            BehaviorTreeComponent::CreateDescriptor(),
            ConstraintComponent::CreateDescriptor(),
            DecalComponent::CreateDescriptor(),
            FlowGraphComponent::CreateDescriptor(),
            LensFlareComponent::CreateDescriptor(),
            LightComponent::CreateDescriptor(),
            LmbrCentralSystemComponent::CreateDescriptor(),
            StaticMeshComponent::CreateDescriptor(),
            SkinnedMeshComponent::CreateDescriptor(),
            NavigationComponent::CreateDescriptor(),
            ParticleComponent::CreateDescriptor(),
            PhysicsComponent::CreateDescriptor(),
            PhysicsSystemComponent::CreateDescriptor(),
            CharacterPhysicsComponent::CreateDescriptor(),
            RagdollComponent::CreateDescriptor(),
            SimpleAnimationComponent::CreateDescriptor(),
            SimpleStateComponent::CreateDescriptor(),
            SpawnerComponent::CreateDescriptor(),
            LookAtComponent::CreateDescriptor(),
            TriggerAreaComponent::CreateDescriptor(),
            TagComponent::CreateDescriptor(),
            MeshColliderComponent::CreateDescriptor(),
            MannequinScopeComponent::CreateDescriptor(),
            MannequinComponent::CreateDescriptor(),
            CharacterAnimationManagerComponent::CreateDescriptor(),
            SphereShapeComponent::CreateDescriptor(),
            BoxShapeComponent::CreateDescriptor(),
            CylinderShapeComponent::CreateDescriptor(),
            CapsuleShapeComponent::CreateDescriptor(),
            PrimitiveColliderComponent::CreateDescriptor(),
        });
    }

    //! Request system components on the system entity.
    //! These components' memory is owned by the system entity.
    AZ::ComponentTypeList LmbrCentralModule::GetRequiredSystemComponents() const
    {
        return {
            azrtti_typeid<LmbrCentralSystemComponent>(),
            azrtti_typeid<PhysicsSystemComponent>(),
            azrtti_typeid<CharacterAnimationManagerComponent>(),
        };
    }

    ////////////////////////////////////////////////////////////////////////////
    // LmbrCentralSystemComponent

    void LmbrCentralSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            AzFramework::SimpleAssetReference<BehaviorTreeAsset>::Register(*serializeContext);
            AzFramework::SimpleAssetReference<MaterialAsset>::Register(*serializeContext);
            AzFramework::SimpleAssetReference<TextureAsset>::Register(*serializeContext);
            AzFramework::SimpleAssetReference<MannequinControllerDefinitionAsset>::Register(*serializeContext);
            AzFramework::SimpleAssetReference<MannequinAnimationDatabaseAsset>::Register(*serializeContext);

            serializeContext->Class<LmbrCentralSystemComponent, AZ::Component>()
                ->Version(1)
                ->SerializerForEmptyClass()
            ;

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<LmbrCentralSystemComponent>(
                    "LmbrCentral", "Coordinates initialization of systems within LmbrCentral")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Game")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
                    ;
            }
        }

        if (AZ::ScriptContext* scriptContext = azrtti_cast<AZ::ScriptContext*>(context))
        {
            ReflectScriptableEvents::Reflect(scriptContext);
        }
    }

    void LmbrCentralSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("LmbrCentralService"));
    }

    void LmbrCentralSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("LmbrCentralService"));
    }

    void LmbrCentralSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC("AssetDatabaseService"));
    }

    void LmbrCentralSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        dependent.push_back(AZ_CRC("AssetCatalogService"));
    }

    void LmbrCentralSystemComponent::Activate()
    {
        // Register asset handlers. Requires "AssetDatabaseService"
        AZ_Assert(AZ::Data::AssetDatabase::IsReady(), "Asset database isn't ready!");

        auto lensFlareAssetHandler = aznew LensFlareAssetHandler;
        lensFlareAssetHandler->Register(); // registers self with AssetDatabase
        m_assetHandlers.emplace_back(lensFlareAssetHandler);

        auto staticMeshAssetHandler = aznew StaticMeshAssetHandler();
        staticMeshAssetHandler->Register(); // registers self with AssetDatabase
        m_assetHandlers.emplace_back(staticMeshAssetHandler);

        auto skinnedMeshAssetHandler = aznew SkinnedMeshAssetHandler();
        skinnedMeshAssetHandler->Register(); // registers self with AssetDatabase
        m_assetHandlers.emplace_back(skinnedMeshAssetHandler);

        auto particleAssetHandler = aznew ParticleAssetHandler;
        particleAssetHandler->Register(); // registers self with AssetDatabase
        m_assetHandlers.emplace_back(particleAssetHandler);

        // Add asset types and extensions to AssetCatalog. Uses "AssetCatalogService".
        auto assetCatalog = AZ::Data::AssetCatalogRequestBus::FindFirstHandler();
        if (assetCatalog)
        {
            assetCatalog->EnableCatalogForAsset(AZ::AzTypeInfo<AZ::PrefabAsset>::Uuid());
            assetCatalog->EnableCatalogForAsset(AZ::AzTypeInfo<AZ::DynamicPrefabAsset>::Uuid());
            assetCatalog->EnableCatalogForAsset(AZ::AzTypeInfo<AZ::ScriptAsset>::Uuid());
            assetCatalog->EnableCatalogForAsset(AZ::AzTypeInfo<LensFlareAsset>::Uuid());
            assetCatalog->EnableCatalogForAsset(AZ::AzTypeInfo<MaterialAsset>::Uuid());
            assetCatalog->EnableCatalogForAsset(AZ::AzTypeInfo<StaticMeshAsset>::Uuid());
            assetCatalog->EnableCatalogForAsset(AZ::AzTypeInfo<SkinnedMeshAsset>::Uuid());
            assetCatalog->EnableCatalogForAsset(AZ::AzTypeInfo<ParticleAsset>::Uuid());

            assetCatalog->AddExtension("cgf");
            assetCatalog->AddExtension("chr");
            assetCatalog->AddExtension("cdf");
            assetCatalog->AddExtension("dds");
            assetCatalog->AddExtension("caf");
            assetCatalog->AddExtension("xml");
            assetCatalog->AddExtension("mtl");
            assetCatalog->AddExtension("lua");
            assetCatalog->AddExtension("slice");
            assetCatalog->AddExtension("dynamicslice");
            assetCatalog->AddExtension("sprite");
        }

        CrySystemEventBus::Handler::BusConnect();
        AZ::Data::AssetDatabaseNotificationBus::Handler::BusConnect();
    }

    void LmbrCentralSystemComponent::Deactivate()
    {
        AZ::Data::AssetDatabaseNotificationBus::Handler::BusDisconnect();
        CrySystemEventBus::Handler::BusDisconnect();

        // AssetHandler's destructor calls Unregister()
        m_assetHandlers.clear();
    }

    void LmbrCentralSystemComponent::OnAssetEventsDispatched()
    {
        // Pump deferred engine loading events.
        if (gEnv && gEnv->mMainThreadId == CryGetCurrentThreadId())
        {
            if (gEnv->pCharacterManager)
            {
                gEnv->pCharacterManager->ProcessAsyncLoadRequests();
            }

            if (gEnv->p3DEngine)
            {
                gEnv->p3DEngine->ProcessAsyncStaticObjectLoadRequests();
            }
        }
    }

    void LmbrCentralSystemComponent::OnCrySystemPreInitialize(ISystem& system, const SSystemInitParams& systemInitParams)
    {
        EBUS_EVENT(AZ::Data::AssetCatalogRequestBus, StartMonitoringAssets);
    }

    void LmbrCentralSystemComponent::OnCrySystemInitialized(ISystem& system, const SSystemInitParams& systemInitParams)
    {
#if !defined(AZ_MONOLITHIC_BUILD)
        // When module is linked dynamically, we must set our gEnv pointer.
        // When module is linked statically, we'll share the application's gEnv pointer.
        gEnv = system.GetGlobalEnvironment();
#endif

        // Update the application's asset root.
        // Requires @assets@ alias which is set during CrySystem initialization.
        AZStd::string assetRoot;
        EBUS_EVENT_RESULT(assetRoot, AzFramework::ApplicationRequests::Bus, GetAssetRoot);

        AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
        if (fileIO)
        {
            const char* aliasPath = fileIO->GetAlias("@assets@");
            if (aliasPath && aliasPath[0] != '\0')
            {
                assetRoot = aliasPath;
            }
        }

        if (!assetRoot.empty())
        {
            EBUS_EVENT(AzFramework::ApplicationRequests::Bus, SetAssetRoot, assetRoot.c_str());
        }

        // Enable catalog now that application's asset root is set.
        if (system.GetGlobalEnvironment()->IsEditor())
        {
            // In the editor, we build the catalog by scanning the disk.
            if (systemInitParams.pUserCallback)
            {
                systemInitParams.pUserCallback->OnInitProgress("Refreshing asset catalog...");
            }
        }

        // load the catalog from disk (supported over VFS).
        EBUS_EVENT(AZ::Data::AssetCatalogRequestBus, LoadCatalog, AZStd::string::format("@assets@/%s", s_assetCatalogFilename).c_str());
    }

    void LmbrCentralSystemComponent::OnCrySystemShutdown(ISystem& system)
    {
        EBUS_EVENT(AZ::Data::AssetCatalogRequestBus, StopMonitoringAssets);

#if !defined(AZ_MONOLITHIC_BUILD)
        gEnv = nullptr;
#endif
    }

    void LmbrCentralSystemComponent::OnCrySystemPrePhysicsUpdate()
    {
        EBUS_EVENT(PhysicsSystemRequestBus, PrePhysicsUpdate);
    }
    
    void LmbrCentralSystemComponent::OnCrySystemPostPhysicsUpdate()
    {
        EBUS_EVENT(PhysicsSystemRequestBus, PostPhysicsUpdate);
    }

} // namespace LmbrCentral

#if !defined(LMBR_CENTRAL_EDITOR)
AZ_DECLARE_MODULE_CLASS(LmbrCentral, LmbrCentral::LmbrCentralModule)
#endif
