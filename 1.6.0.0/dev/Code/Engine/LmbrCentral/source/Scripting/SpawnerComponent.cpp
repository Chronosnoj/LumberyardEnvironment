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

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/EBus/ScriptBinder.h>

#include <AzFramework/Entity/EntityContextBus.h>
#include <AzFramework/Entity/GameEntityContextBus.h>

#include "SpawnerComponent.h"


namespace LmbrCentral
{
    AZ_SCRIPTABLE_EBUS(
        SpawnerComponentRequestBus,
        SpawnerComponentRequestBus,
        "{39F9BD3F-8827-4C60-809D-17F347FF5396}",
        "{690AB154-3265-4E95-BC97-98218B6B1A69}",
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(AzFramework::SliceInstantiationTicket, AzFramework::SliceInstantiationTicket(), Spawn)
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(AzFramework::SliceInstantiationTicket, AzFramework::SliceInstantiationTicket(), SpawnRelative, const AZ::Transform&)
    )

    AZ_SCRIPTABLE_EBUS(
        SpawnerComponentNotificationBus,
        SpawnerComponentNotificationBus,
        "{5F270AE4-FADA-43D4-96C7-637A747AB8C1}",
        "{AC202871-2522-48A6-9B62-5FDAABB302CD}",
        AZ_SCRIPTABLE_EBUS_EVENT(OnSpawned, const AzFramework::SliceInstantiationTicket&, const AZStd::vector<AZ::EntityId>&)
    )

    //=========================================================================
    // SpawnerComponent::Reflect
    //=========================================================================
    void SpawnerComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<SpawnerComponent, AZ::Component>()
                ->Version(1)
                ->Field("Slice", &SpawnerComponent::m_sliceAsset)
                ->Field("SpawnOnActivate", &SpawnerComponent::m_spawnOnActivate);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<SpawnerComponent>("Spawner", "The spawner component provides dynamic slice spawning support.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Game")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/Spawner.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/Spawner.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &SpawnerComponent::m_sliceAsset, "Slice", "The slice to spawn")
                    ->DataElement(0, &SpawnerComponent::m_spawnOnActivate, "Spawn On Activate", "Should the component spawn the selected slice upon activation?");
            }
        }

        AZ::ScriptContext* scriptContext = azrtti_cast<AZ::ScriptContext*>(context);
        if (scriptContext)
        {
            ScriptableEBus_SpawnerComponentRequestBus::Reflect(scriptContext);
            ScriptableEBus_SpawnerComponentNotificationBus::Reflect(scriptContext);
            
            scriptContext->Class<AzFramework::SliceInstantiationTicket, AZ::ScriptContext::SP_VALUE>("SliceInstantiationTicket")
                ->Operator(AZ::ScriptContext::OPERATOR_EQ, &AzFramework::SliceInstantiationTicket::operator==)
                ->Method("IsValid", &AzFramework::SliceInstantiationTicket::operator bool);
        }
    }

    //=========================================================================
    // SpawnerComponent::GetProvidedServices
    //=========================================================================
    void SpawnerComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("SpawnerService"));
    }


    //=========================================================================
    // SpawnerComponent::GetDependentServices
    //=========================================================================
    void SpawnerComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        dependent.push_back(AZ_CRC("TransformService"));
    }

    //=========================================================================
    // SpawnerComponent::SpawnerComponent
    //=========================================================================
    SpawnerComponent::SpawnerComponent()
    {
        // Slice asset should load purely on-demand.
        m_sliceAsset.SetFlags(static_cast<AZ::u8>(AZ::Data::AssetFlags::OBJECTSTREAM_NO_LOAD));
    }

    //=========================================================================
    // SpawnerComponent::Activate
    //=========================================================================
    void SpawnerComponent::Activate()
    {
        SpawnerComponentRequestBus::Handler::BusConnect(GetEntityId());

        if (m_spawnOnActivate)
        {
            SpawnSliceInternal(m_sliceAsset, AZ::Transform::Identity());
        }
    }

    //=========================================================================
    // SpawnerComponent::Deactivate
    //=========================================================================
    void SpawnerComponent::Deactivate()
    {
        SpawnerComponentRequestBus::Handler::BusDisconnect();
    }

    //=========================================================================
    // SpawnerComponent::Spawn
    //=========================================================================
    AzFramework::SliceInstantiationTicket SpawnerComponent::Spawn()
    {
        return SpawnSliceInternal(m_sliceAsset, AZ::Transform::Identity());
    }

    //=========================================================================
    // SpawnerComponent::Spawn
    //=========================================================================
    AzFramework::SliceInstantiationTicket SpawnerComponent::SpawnRelative(const AZ::Transform& relative)
    {
        return SpawnSliceInternal(m_sliceAsset, relative);
    }

    //=========================================================================
    // SpawnerComponent::SpawnSlice
    //=========================================================================
    AzFramework::SliceInstantiationTicket SpawnerComponent::SpawnSlice(const AZ::Data::Asset<AZ::Data::AssetData>& slice)
    {
        return SpawnSliceInternal(slice, AZ::Transform::Identity());
    }

    //=========================================================================
    // SpawnerComponent::SpawnSlice
    //=========================================================================
    AzFramework::SliceInstantiationTicket SpawnerComponent::SpawnSliceRelative(const AZ::Data::Asset<AZ::Data::AssetData>& slice, const AZ::Transform& relative)
    {
        return SpawnSliceInternal(slice, relative);
    }

    //=========================================================================
    // SpawnerComponent::SpawnSliceInternal
    //=========================================================================
    AzFramework::SliceInstantiationTicket SpawnerComponent::SpawnSliceInternal(const AZ::Data::Asset<AZ::Data::AssetData>& slice, const AZ::Transform& relative)
    {
        AZ::Transform transform = AZ::Transform::Identity();
        EBUS_EVENT_ID_RESULT(transform, GetEntityId(), AZ::TransformBus, GetWorldTM);

        transform *= relative;

        AzFramework::SliceInstantiationTicket ticket;
        EBUS_EVENT_RESULT(ticket, AzFramework::GameEntityContextRequestBus, InstantiateDynamicSlice, slice, transform, nullptr);

        AzFramework::SliceInstantiationResultBus::MultiHandler::BusConnect(ticket);

        return ticket;
    }


    //=========================================================================
    // SpawnerComponent::OnSliceInstantiated
    //=========================================================================
    void SpawnerComponent::OnSliceInstantiated(const AZ::Data::AssetId& sliceAssetId, const AZ::PrefabComponent::PrefabInstanceAddress& sliceAddress)
    {
        const AzFramework::SliceInstantiationTicket& ticket = (*AzFramework::SliceInstantiationResultBus::GetCurrentBusId());

        // Stop listening for this ticket (since it's done). We can have have multiple tickets in flight.
        AzFramework::SliceInstantiationResultBus::MultiHandler::BusDisconnect(ticket);

        const AZ::PrefabComponent::EntityList& entities = sliceAddress.second->GetInstantiated()->m_entities;

        AZStd::vector<AZ::EntityId> entityIds;
        entityIds.reserve(entities.size());
        for (AZ::Entity * currEntity : entities)
        {
            entityIds.push_back(currEntity->GetId());
        }

        EBUS_EVENT_ID(GetEntityId(), SpawnerComponentNotificationBus, OnSpawned, ticket, entityIds);
    }

    //=========================================================================
    // SpawnerComponent::OnSliceInstantiationFailed
    //=========================================================================
    void SpawnerComponent::OnSliceInstantiationFailed(const AZ::Data::AssetId& sliceAssetId)
    {
        AzFramework::SliceInstantiationResultBus::MultiHandler::BusDisconnect(*AzFramework::SliceInstantiationResultBus::GetCurrentBusId());

        AZ_Error("SpawnerComponent", false, "Slice '%s' failed to instantiate", sliceAssetId.ToString<AZStd::string>().c_str());
    }
} // namespace LmbrCentral
