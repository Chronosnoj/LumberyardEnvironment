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
#include "PhysicsComponent.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/EBus/ScriptBinder.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/Entity.h>
#include <MathConversion.h>
#include <ISystem.h>
#include <IPhysics.h>
#include "Physics/Behaviors/RigidPhysicsBehavior.h"
#include "Physics/Behaviors/StaticPhysicsBehavior.h"

namespace LmbrCentral
{
    AZ_SCRIPTABLE_EBUS(
        PhysicsComponentRequestBus,
        PhysicsComponentRequestBus,
        "{4DAABD09-8D66-486D-9F7F-2FBFB6A6D888}",
        "{990FFDE2-14B2-49FB-8422-FD38BC076F40}",
        AZ_SCRIPTABLE_EBUS_EVENT(EnablePhysics)
        AZ_SCRIPTABLE_EBUS_EVENT(DisablePhysics)
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(bool, false, IsPhysicsEnabled)
        )

    AZ_SCRIPTABLE_EBUS(
        PhysicsComponentNotificationBus,
        PhysicsComponentNotificationBus,
        "{80E887F1-D347-4455-9A33-1D651F567DC8}",
        "{5C504F10-7779-4BD6-BE14-F94AC9FFF2BE}",
        AZ_SCRIPTABLE_EBUS_EVENT(OnPhysicsEnabled)
        AZ_SCRIPTABLE_EBUS_EVENT(OnPhysicsDisabled)
        )

    void PhysicsComponent::Reflect(AZ::ReflectContext* context)
    {
        PhysicsConfiguration::Reflect(context);

        PhysicsBehavior::Reflect(context);
        CryPhysicsBehavior::Reflect(context);
        StaticPhysicsBehavior::Reflect(context);
        RigidPhysicsBehavior::Reflect(context);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<PhysicsComponent, AZ::Component>()
                ->Version(1)
                ->Field("Configuration", &PhysicsComponent::m_configuration)
            ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<PhysicsComponent>(
                    "Physics", "Let the entity interact with physics")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Physics")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/Physics.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/Physics.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &PhysicsComponent::m_configuration,
                    "Configuration", "Physics Configuration")
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly"))
                ;
            }
        }

        AZ::ScriptContext* script = azrtti_cast<AZ::ScriptContext*>(context);
        if (script)
        {
            ScriptableEBus_PhysicsComponentRequestBus::Reflect(script);
            ScriptableEBus_PhysicsComponentNotificationBus::Reflect(script);
        }
    }

    void PhysicsConfiguration::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<PhysicsConfiguration>()
                ->Version(1)
                ->Field("Proximity Triggerable", &PhysicsConfiguration::m_canInteractProximityTriggers)
                ->Field("Behavior", &PhysicsConfiguration::m_behavior)
                ->Field("Child Colliders", &PhysicsConfiguration::m_childColliders)
            ;


            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<PhysicsConfiguration>(
                    "Physics Configuration", "Physics configuration for this entity")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly"))
                    ->DataElement(0, &PhysicsConfiguration::m_canInteractProximityTriggers, "Proximity Triggerable",
                    "Indicates whether or not this entity can interact with proximity triggers.")
                    ->DataElement(0, &PhysicsConfiguration::m_behavior, "Behavior", "Physics behavior exhibited by this entity")
                    ->DataElement(0, &PhysicsConfiguration::m_childColliders, "Child Colliders", "The entity will use colliders from itself, as well as these other entities.")
                ;
            }
        }
    }

    void PhysicsComponent::Init()
    {
    }

    void PhysicsComponent::Activate()
    {
        AZ_Error(m_entity->GetName().c_str(), m_configuration.m_behavior, "No behavior specified for PhysicsComponent");
        if (m_configuration.m_behavior)
        {
            m_configuration.m_behavior->InitPhysicsBehavior(GetEntityId());
        }

        PhysicsComponentRequestBus::Handler::BusConnect(GetEntityId());

        // Copy and sanitize the configuration's list of child colliders.
        m_childColliders.clear();
        m_childColliders.reserve(m_configuration.m_childColliders.size());
        for (auto& entityId : m_configuration.m_childColliders)
        {
            if (!entityId.IsValid() || entityId == GetEntityId())
            {
                continue;
            }

            if (AZStd::find(m_childColliders.begin(), m_childColliders.end(), entityId) == m_childColliders.end())
            {
                m_childColliders.push_back(entityId);
            }
        }

        // Listen for activation of m_childColliders.
        // When these entities activate we ask them to add their colliders.
        for (auto& entityId : m_childColliders)
        {
            // Note that OnEntityActivate will fire immediately if the entity is already active.
            AZ::EntityBus::MultiHandler::BusConnect(entityId);
        }
    }

    void PhysicsComponent::Deactivate()
    {
        AZ::EntityBus::MultiHandler::BusDisconnect();

        PhysicsComponentRequestBus::Handler::BusDisconnect();
        ColliderComponentEventBus::Handler::BusDisconnect();
        DisablePhysics();
    }

    bool PhysicsComponent::IsPhysicsEnabled()
    {
        return m_physicalEntity;
    }

    // IPhysicalEntity's lifetime is complex. It utilizes an internal
    // refcount and is meant to be used with _smart_ptr.
    // We don't want to pass a _smart_ptr to external parties because
    // this forces anyone who holds the _smart_ptr to keep the object alive.
    // We want to use a "weak ptr", which lets external parties say
    // "I'm interested in this object, but don't keep it alive on my behalf."
    // There's no "weak ptr" that corresponds to _smart_ptr.
    //
    // This function does the proper incantations to let AZStd::shared_ptr
    // and AZStd::weak_ptr work with IPhysicalEntity.
    static AZStd::shared_ptr<IPhysicalEntity> CreateIPhysicalEntitySharedPtr(IPhysicalEntity* rawPtr)
    {
        if (!rawPtr)
        {
            return AZStd::shared_ptr<IPhysicalEntity>(); // null
        }

        // Increment IPhysicalEntity's internal refcount once.
        // We will decrement it once when the shared_ptr's refcount reaches zero.
        rawPtr->AddRef();

        // When shared_ptr's refcount reaches zero, do the proper
        // incantations to release IPhysicalEntity.
        auto physicalEntityDeleter = [](IPhysicalEntity* p)
            {
                if (p)
                {
                    // The IPhysicalEntity will not be destroyed until:
                    // - IPhysicalEntity's internal refcount has dropped to zero.
                    // - IPhysicalWorld::DestroyPhysicalEntity() has been called on it.
                    // All these things must happen. They can happen in any order.
                    p->Release();
                    gEnv->pPhysicalWorld->DestroyPhysicalEntity(p);
                }
            };

        return AZStd::shared_ptr<IPhysicalEntity>(rawPtr, physicalEntityDeleter);
    }

    void PhysicsComponent::EnablePhysics()
    {
        if (IsPhysicsEnabled())
        {
            return;
        }

        if (!m_configuration.m_behavior)
        {
            return;
        }

        AZ_Warning("PhysicsComponent", !m_entity->GetTransform()->GetParentId().IsValid(),
            "Entity '%s' is using physics while parented to another entity. This may lead to unexpected behavior.",
            m_entity->GetName().c_str());

        // Setup position params
        AZ::Transform transform = AZ::Transform::CreateIdentity();
        EBUS_EVENT_ID_RESULT(transform, GetEntityId(), AZ::TransformBus, GetWorldTM);
        pe_params_pos positionParameters;
        Matrix34 cryTransform(AZTransformToLYTransform(transform));
        positionParameters.pMtx3x4 = &cryTransform;

        // Create physical entity
        IPhysicalEntity* rawPhysicalEntityPtr = gEnv->pPhysicalWorld->CreatePhysicalEntity(
                m_configuration.m_behavior->GetPhysicsType(), // type
                &positionParameters, // params
                static_cast<uint64>(GetEntityId()), // pForeignData
                PHYS_FOREIGN_ID_COMPONENT_ENTITY, // iForeignData
                -1, // id
                nullptr); // IGeneralMemoryHeap

        m_physicalEntity = CreateIPhysicalEntitySharedPtr(rawPhysicalEntityPtr);
        if (!m_physicalEntity)
        {
            return;
        }

        // Add colliders from self and children.
        // If child is inactive then we'll add its colliders later via OnEntityActivate().
        EBUS_EVENT_ID(GetEntityId(), ColliderComponentRequestBus, AddColliderToPhysicalEntity, *m_physicalEntity);
        for (auto& childCollider : m_childColliders)
        {
            EBUS_EVENT_ID(childCollider, ColliderComponentRequestBus, AddColliderToPhysicalEntity, *m_physicalEntity);
        }

        // Let behavior configure the physical entity.
        // If anything goes wrong, destroy m_physicalEntity
        if (!m_configuration.m_behavior->ConfigurePhysicalEntity(*m_physicalEntity))
        {
            m_physicalEntity.reset();
            return;
        }

        // Connect to buses concerning the live physics object
        CryPhysicsComponentRequestBus::Handler::BusConnect(GetEntityId());
        EntityPhysicsEventBus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());

        if (m_configuration.m_canInteractProximityTriggers)
        {
            // Create Proximity trigger proxy
            EBUS_EVENT_RESULT(m_proximityTriggerProxy, ProximityTriggerSystemRequestBus, CreateEntity, GetEntityId());
        }

        // Send notification
        EBUS_EVENT_ID(GetEntityId(), PhysicsComponentNotificationBus, OnPhysicsEnabled);
    }

    void PhysicsComponent::DisablePhysics()
    {
        if (!IsPhysicsEnabled())
        {
            return;
        }

        // Send notification
        EBUS_EVENT_ID(GetEntityId(), PhysicsComponentNotificationBus, OnPhysicsDisabled);

        // Remove Proximity trigger proxy
        if (m_proximityTriggerProxy)
        {
            EBUS_EVENT(ProximityTriggerSystemRequestBus, RemoveEntity, m_proximityTriggerProxy, false);
            m_proximityTriggerProxy = nullptr;
        }

        // Disconnect from buses concerning the live physics object
        CryPhysicsComponentRequestBus::Handler::BusDisconnect();
        EntityPhysicsEventBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();

        m_physicalEntity.reset();
    }

    IPhysicalEntity* PhysicsComponent::GetPhysicalEntity()
    {
        return m_physicalEntity.get();
    }

    void PhysicsComponent::GetPhysicsParameters(pe_params& outParameters)
    {
        if (m_physicalEntity)
        {
            m_physicalEntity->GetParams(&outParameters);
        }
    }

    void PhysicsComponent::SetPhysicsParameters(const pe_params& parameters)
    {
        if (m_physicalEntity)
        {
            m_physicalEntity->SetParams(&parameters);
        }
    }

    void PhysicsComponent::GetPhysicsStatus(pe_status& outStatus)
    {
        if (m_physicalEntity)
        {
            m_physicalEntity->GetStatus(&outStatus);
        }
    }

    void PhysicsComponent::ApplyPhysicsAction(const pe_action& action, bool threadSafe)
    {
        if (m_physicalEntity)
        {
            m_physicalEntity->Action(&action, threadSafe ? 1 : 0);
        }
    }

    void PhysicsComponent::OnEntityActivated(AZ::Entity* entity)
    {
        if (IsPhysicsEnabled())
        {
            // Now that entity is active, ask colliders to add themselves to physics object
            EBUS_EVENT_ID(entity->GetId(), ColliderComponentRequestBus, AddColliderToPhysicalEntity, *m_physicalEntity);
            m_configuration.m_behavior->ConfigurePhysicalEntity(*m_physicalEntity);
        }

        AZ::EntityBus::MultiHandler::BusDisconnect(entity->GetId());
    }

    void PhysicsComponent::OnColliderChanged()
    {
        if (IsPhysicsEnabled())
        {
            DisablePhysics();
            EnablePhysics();
        }
    }

    void PhysicsComponent::OnPostStep(const EntityPhysicsEvents::PostStep& event)
    {
        // Inform the TransformComponent that we've been moved by the physics system
        AZ::Transform transform = AZ::Transform::CreateFromQuaternionAndTranslation(event.m_entityRotation, event.m_entityPosition);
        if (GetEntity()->GetTransform())
        {
            // Maintain scale (this must be precise).
            AZ::Transform entityTransform = GetEntity()->GetTransform()->GetWorldTM();
            transform.MultiplyByScale(entityTransform.ExtractScaleExact());
        }

        AZ_Assert(m_isApplyingPhysicsToEntityTransform == false, "two post steps received before a transform event");
        m_isApplyingPhysicsToEntityTransform = true;
        EBUS_EVENT_ID(event.m_entity, AZ::TransformBus, SetWorldTM, transform);
        m_isApplyingPhysicsToEntityTransform = false;
    }

    void PhysicsComponent::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        // Because physics can create a change in transform through OnPostStep, we need to make sure we
        // are not creating a cycle
        if (!m_isApplyingPhysicsToEntityTransform)
        {
            if (!m_configuration.m_behavior || !m_configuration.m_behavior->HandleTransformChange(world))
            {
                pe_params_pos positionParameters;
                positionParameters.pos = AZVec3ToLYVec3(world.GetPosition());
                AZ::Quaternion worldOrientation = AZ::Quaternion::CreateFromTransform(world);
                positionParameters.q = AZQuaternionToLYQuaternion(worldOrientation);
                SetPhysicsParameters(positionParameters);
            }
        }

        // If the proximity trigger entity proxy exists then move it
        if (m_proximityTriggerProxy)
        {
            EBUS_EVENT(ProximityTriggerSystemRequestBus, MoveEntity, m_proximityTriggerProxy, AZVec3ToLYVec3(world.GetPosition()));
        }
    }

    void PhysicsComponent::OnParentChanged(AZ::EntityId oldParent, AZ::EntityId newParent)
    {
        AZ_Warning("PhysicsComponent", !(newParent.IsValid() && IsPhysicsEnabled()),
            "Entity '%s' is using physics while parented to another entity. This may lead to unexpected behavior.",
            m_entity->GetName().c_str());
    }
} // namespace LmbrCentral
