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
#include "PhysicsSystemComponent.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <MathConversion.h>
#include <IPhysics.h>

namespace LmbrCentral
{
    void PhysicsSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<PhysicsSystemComponent, AZ::Component>()
                ->Version(1)
                ->SerializerForEmptyClass()
            ;

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<PhysicsSystemComponent>(
                    "CryPhysics Manager", "Allows Component Entities to work with CryPhysics")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Game")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
                    ;
            }
        }
    }

    // private namespace for helper functions
    namespace PhysicalWorldEventClient
    {
        // return whether object involved in event was an AZ::Entity
        static bool InvolvesAzEntity(const EventPhysMono& event)
        {
            return event.iForeignData == PHYS_FOREIGN_ID_COMPONENT_ENTITY;
        }

        // return whether either object involved in event was an AZ::Entity
        static bool InvolvesAzEntity(const EventPhysStereo& event)
        {
            return event.iForeignData[0] == PHYS_FOREIGN_ID_COMPONENT_ENTITY
                   || event.iForeignData[1] == PHYS_FOREIGN_ID_COMPONENT_ENTITY;
        }

        static AZ::EntityId GetEntityId(const EventPhysMono& event)
        {
            if (event.iForeignData == PHYS_FOREIGN_ID_COMPONENT_ENTITY)
            {
                return static_cast<AZ::EntityId>(event.pForeignData);
            }

            return AZ::EntityId();
        }

        static AZ::EntityId GetEntityId(const EventPhysStereo& event, int entityIndex)
        {
            AZ_Assert(entityIndex >= 0 && entityIndex <= 1, "invalid entityI");

            if (event.iForeignData[entityIndex] == PHYS_FOREIGN_ID_COMPONENT_ENTITY)
            {
                return static_cast<AZ::EntityId>(event.pForeignData[entityIndex]);
            }

            return AZ::EntityId();
        }

        // An IPhysicalWorld event client can prevent the event
        // from propagating any further
        enum EventClientReturnValues
        {
            StopEventProcessing = 0,
            ContinueEventProcessing = 1,
        };

        static int OnCollision(const EventPhys* event)
        {
            const EventPhysCollision& collisionIn = static_cast<const EventPhysCollision&>(*event);
            if (!InvolvesAzEntity(collisionIn))
            {
                return ContinueEventProcessing;
            }

            EntityPhysicsEvents::Collision collisionOut;
            collisionOut.m_location = LYVec3ToAZVec3(collisionIn.pt);
            for (int colliderI : {0, 1})
            {
                collisionOut.m_entity = GetEntityId(collisionIn, colliderI);

                if (collisionOut.m_entity.IsValid())
                {
                    collisionOut.m_otherEntity = GetEntityId(collisionIn, 1 - colliderI);
                    EBUS_EVENT_ID(collisionOut.m_entity, EntityPhysicsEventBus, OnCollision, collisionOut);
                }
            }

            return ContinueEventProcessing;
        }

        static int OnPostStep(const EventPhys* event)
        {
            auto& eventIn = static_cast<const EventPhysPostStep&>(*event);
            if (!InvolvesAzEntity(eventIn))
            {
                return ContinueEventProcessing;
            }

            EntityPhysicsEvents::PostStep eventOut;
            eventOut.m_entity = GetEntityId(eventIn);
            eventOut.m_entityPosition = LYVec3ToAZVec3(eventIn.pos);
            eventOut.m_entityRotation = LYQuaternionToAZQuaternion(eventIn.q);
            eventOut.m_stepTimeDelta = eventIn.dt;
            eventOut.m_stepId = eventIn.idStep;

            EBUS_EVENT_ID(eventOut.m_entity, EntityPhysicsEventBus, OnPostStep, eventOut);

            return ContinueEventProcessing;
        }
    } // namespace PhysicalWorldEventClient

    void PhysicsSystemComponent::Activate()
    {
        PhysicsSystemRequestBus::Handler::BusConnect();
        CrySystemEventBus::Handler::BusConnect();
    }

    void PhysicsSystemComponent::Deactivate()
    {
        PhysicsSystemRequestBus::Handler::BusDisconnect();
        CrySystemEventBus::Handler::BusDisconnect();
        SetEnabled(false);
    }

    void PhysicsSystemComponent::PrePhysicsUpdate()
    {
        EBUS_EVENT(PhysicsSystemEventBus, OnPrePhysicsUpdate);
    }
    
    void PhysicsSystemComponent::PostPhysicsUpdate()
    {
        EBUS_EVENT(PhysicsSystemEventBus, OnPostPhysicsUpdate);
    }

    void PhysicsSystemComponent::OnCrySystemInitialized(ISystem& system, const SSystemInitParams&)
    {
        m_physicalWorld = system.GetGlobalEnvironment()->pPhysicalWorld;
        SetEnabled(true);
    }

    void PhysicsSystemComponent::OnCrySystemShutdown(ISystem& system)
    {
        // Purposely clearing m_physicalWorld before SetEnabled(false).
        // The IPhysicalWorld is going away, it doesn't matter if we carefully unregister from it.
        m_physicalWorld = nullptr;
        SetEnabled(false);
    }

    void PhysicsSystemComponent::SetEnabled(bool enable)
    {
        // can't be enabled if physical world doesn't exist
        if (!m_physicalWorld)
        {
            m_enabled = false;
            return;
        }

        if (enable == m_enabled)
        {
            return;
        }

        // Params for calls to IPhysicalWorld::AddEventClient and RemoveEventClient.
        enum
        {
            PARAM_ID, // id of the IPhysicalWorld event type
            PARAM_HANDLER, // handler function
            PARAM_LOGGED, // 1 for a logged event version, 0 for immediate
            PARAM_PRIORITY, // priority (higher means handled first)
        };
        std::tuple<int, int(*)(const EventPhys*), int, float> physicalWorldEventClientParams[] = {
            std::make_tuple(EventPhysCollision::id, PhysicalWorldEventClient::OnCollision,  1,  1.f),
            std::make_tuple(EventPhysPostStep::id,  PhysicalWorldEventClient::OnPostStep,   1,  1.f),
        };

        // start/stop listening for events
        for (auto& i : physicalWorldEventClientParams)
        {
            if (enable)
            {
                m_physicalWorld->AddEventClient(std::get<PARAM_ID>(i), std::get<PARAM_HANDLER>(i), std::get<PARAM_LOGGED>(i), std::get<PARAM_PRIORITY>(i));
            }
            else
            {
                m_physicalWorld->RemoveEventClient(std::get<PARAM_ID>(i), std::get<PARAM_HANDLER>(i), std::get<PARAM_LOGGED>(i));
            }
        }

        m_enabled = enable;
    }
} // namespace LmbrCentral
