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

#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/EBus/ScriptBinder.h>

#include <AzFramework/Entity/EntityContextBus.h>
#include <AzFramework/Entity/GameEntityContextBus.h>

#include <LmbrCentral/Physics/PhysicsComponentBus.h>

#include <IPhysics.h>
#include <MathConversion.h>

#include "ConstraintComponent.h"

namespace LmbrCentral
{
    //=========================================================================
    // ConstraintConfiguration::Reflect
    //=========================================================================
    void ConstraintConfiguration::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<ConstraintConfiguration>()
                ->Version(1)

                ->Field("Type", &ConstraintConfiguration::m_constraintType)

                ->Field("OwnerType", &ConstraintConfiguration::m_ownerType)
                ->Field("TargetType", &ConstraintConfiguration::m_targetType)

                ->Field("Owner", &ConstraintConfiguration::m_owningEntity)
                ->Field("Target", &ConstraintConfiguration::m_targetEntity)

                ->Field("Axis", &ConstraintConfiguration::m_axis)

                ->Field("EnableOnActivate", &ConstraintConfiguration::m_enableOnActivate)

                ->Field("EnablePartIds", &ConstraintConfiguration::m_enableConstrainToPartId)
                ->Field("EnableForceLimits", &ConstraintConfiguration::m_enableForceLimits)
                ->Field("EnableRotationLimits", &ConstraintConfiguration::m_enableRotationLimits)
                ->Field("EnableDamping", &ConstraintConfiguration::m_enableDamping)
                ->Field("EnableSearchRadius", &ConstraintConfiguration::m_enableSearchRadius)

                ->Field("OwnerPartId", &ConstraintConfiguration::m_ownerPartId)
                ->Field("TargetPartId", &ConstraintConfiguration::m_targetPartId)

                ->Field("MaxPullForce", &ConstraintConfiguration::m_maxPullForce)
                ->Field("MaxBendTorque", &ConstraintConfiguration::m_maxBendTorque)
                ->Field("XMin", &ConstraintConfiguration::m_xmin)
                ->Field("XMax", &ConstraintConfiguration::m_xmax)
                ->Field("YZMax", &ConstraintConfiguration::m_yzmax)
                ->Field("Damping", &ConstraintConfiguration::m_damping)
                ->Field("SearchRadius", &ConstraintConfiguration::m_searchRadius)

                ->Field("EnableCollision", &ConstraintConfiguration::m_enableTargetCollisions)
                ->Field("EnableRelativeRotation", &ConstraintConfiguration::m_enableRelativeRotation)
                ->Field("EnforceOnFastMovement", &ConstraintConfiguration::m_enforceOnFastObjects)
                ->Field("Breakable", &ConstraintConfiguration::m_breakable)
            ;
        }
    }

    //=========================================================================
    // ConstraintConfiguration::GetWorldFrame
    //=========================================================================
    AZ::Quaternion ConstraintConfiguration::GetWorldFrame(Axis axis, const AZ::Transform& pivotTransform)
    {
        AZ::Vector3 desiredAxis;
        switch (axis)
        {
            case Axis::Z :
            {
                desiredAxis = AZ::Vector3::CreateAxisZ(1.0f);
                break;
            }
            case Axis::NegativeZ :
            {
                desiredAxis = AZ::Vector3::CreateAxisZ(-1.0f);
                break;
            }
            case Axis::Y :
            {
                desiredAxis = AZ::Vector3::CreateAxisY(1.0f);
                break;
            }
            case Axis::NegativeY :
            {
                desiredAxis = AZ::Vector3::CreateAxisY(-1.0f);
                break;
            }
            case Axis::X :
            {
                desiredAxis = AZ::Vector3::CreateAxisX(1.0f);
                break;
            }
            case Axis::NegativeX :
            {
                desiredAxis = AZ::Vector3::CreateAxisX(-1.0f);
                break;
            }
            default :
            {
                AZ_Assert(false, "Unexpected case");
                desiredAxis = AZ::Vector3::CreateAxisZ(1.0f);
                break;
            }
        }

        // CryPhysics uses the X axis as its default axis for constraints
        const AZ::Vector3 defaultAxis = AZ::Vector3::CreateAxisX(1.0f);
        const AZ::Quaternion fromDefaultToDesiredAxis = AZ::Quaternion::CreateShortestArc(defaultAxis, desiredAxis);

        AZ::Transform pivotWorldNoScale = pivotTransform;
        pivotWorldNoScale.ExtractScale(); // To support correct orientation with non-uniform scale
        const AZ::Quaternion pivotWorldFrame = AZ::Quaternion::CreateFromTransform(pivotWorldNoScale);

        return (pivotWorldFrame * fromDefaultToDesiredAxis);
    }

    AZ_SCRIPTABLE_EBUS(
        ConstraintComponentRequestBus,
        ConstraintComponentRequestBus,
        "{832A1CE9-918E-4C2D-AD1A-1EE8669F843E}",
        "{9C4139DC-F2C0-4286-9009-9CFE7E7A10DB}",
        AZ_SCRIPTABLE_EBUS_EVENT(SetConstraintEntities, const AZ::EntityId&, const AZ::EntityId&)
        AZ_SCRIPTABLE_EBUS_EVENT(SetConstraintEntitiesWithPartIds, const AZ::EntityId&, int, const AZ::EntityId&, int)
        AZ_SCRIPTABLE_EBUS_EVENT(EnableConstraint)
        AZ_SCRIPTABLE_EBUS_EVENT(DisableConstraint)
    )

    AZ_SCRIPTABLE_EBUS(
        ConstraintComponentNotificationBus,
        ConstraintComponentNotificationBus,
        "{FEFE5B85-922A-415C-81C1-5F6D91F151E6}",
        "{16BDFDDB-E70F-4B48-8C43-1E5018CD5722}",
        AZ_SCRIPTABLE_EBUS_EVENT(OnConstraintEntitiesChanged, const AZ::EntityId&, const AZ::EntityId&, const AZ::EntityId&, const AZ::EntityId&)
        AZ_SCRIPTABLE_EBUS_EVENT(OnConstraintEnabled)
        AZ_SCRIPTABLE_EBUS_EVENT(OnConstraintDisabled)
    )

    //=========================================================================
    // ConstraintComponent::Reflect
    //=========================================================================
    void ConstraintComponent::Reflect(AZ::ReflectContext* context)
    {
        ConstraintConfiguration::Reflect(context);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<ConstraintComponent>()
                ->Version(1)
                ->Field("ConstraintConfiguration", &ConstraintComponent::m_config);
            ;
        }

        AZ::ScriptContext* script = azrtti_cast<AZ::ScriptContext*>(context);
        if (script)
        {
            ScriptableEBus_ConstraintComponentRequestBus::Reflect(script);
            ScriptableEBus_ConstraintComponentNotificationBus::Reflect(script);
        }
    }

    //=========================================================================
    // ConstraintComponent::GetProvidedServices
    //=========================================================================
    void ConstraintComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("ConstraintService"));
    }

    //=========================================================================
    // ConstraintComponent::GetRequiredServices
    //=========================================================================
    void ConstraintComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC("TransformService"));
    }

    //=========================================================================
    // ConstraintComponent::GetDependentServices
    //=========================================================================
    void ConstraintComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        dependent.push_back(AZ_CRC("PhysicsService"));
    }

    //=========================================================================
    // ConstraintComponent::Activate
    //=========================================================================
    void ConstraintComponent::Activate()
    {
        m_ownerPhysicsEnabled = false;
        m_targetPhysicsEnabled = false;

        if (m_config.m_enableOnActivate)
        {
            m_shouldBeEnabled = true;
            // CryPhysics is enabled after Activate and OnPhysicsEnabled will actually enable the constraint
        }

        ConstraintComponentRequestBus::Handler::BusConnect(GetEntityId());

        LmbrCentral::PhysicsComponentNotificationBus::MultiHandler::BusConnect(m_config.m_owningEntity);
        LmbrCentral::PhysicsComponentNotificationBus::MultiHandler::BusConnect(m_config.m_targetEntity);
    }

    //=========================================================================
    // ConstraintComponent::Deactivate
    //=========================================================================
    void ConstraintComponent::Deactivate()
    {
        ConstraintComponentRequestBus::Handler::BusDisconnect();

        LmbrCentral::PhysicsComponentNotificationBus::MultiHandler::BusDisconnect(m_config.m_targetEntity);
        LmbrCentral::PhysicsComponentNotificationBus::MultiHandler::BusDisconnect(m_config.m_owningEntity);

        DisableConstraint();
    }

    //=========================================================================
    // ConstraintComponent::OnPhysicsEnabled
    //=========================================================================
    void ConstraintComponent::OnPhysicsEnabled()
    {
        OnPhysicsEnabledChanged(true /* enabled */);
    }

    //=========================================================================
    // ConstraintComponent::OnPhysicsDisabled
    //=========================================================================
    void ConstraintComponent::OnPhysicsDisabled()
    {
        OnPhysicsEnabledChanged(false /* enabled */);
    }

    //=========================================================================
    // ConstraintComponent::OnPhysicsEnabledChanged
    //=========================================================================
    void ConstraintComponent::OnPhysicsEnabledChanged(bool enabled)
    {
        AZ::EntityId entityId = *LmbrCentral::PhysicsComponentNotificationBus::GetCurrentBusId();
        if (entityId == m_config.m_owningEntity)
        {
            m_ownerPhysicsEnabled = enabled;
        }
        else if (entityId == m_config.m_targetEntity)
        {
            m_targetPhysicsEnabled = enabled;
        }
        else
        {
            AZ_Assert(false, "OnPhysicsEnabled/Disabled received from non-owner, non-target entity %llu", entityId);
        }

        if (enabled)
        {
            EnableInternal();
        }
        else
        {
            DisableInternal();
        }
    }

    //=========================================================================
    // ConstraintComponent::SetConstraintEntities
    //=========================================================================
    void ConstraintComponent::SetConstraintEntities(const AZ::EntityId& owningEntity, const AZ::EntityId& targetEntity)
    {
        SetConstraintEntitiesWithPartIds(owningEntity, m_config.m_ownerPartId, m_config.m_targetEntity, m_config.m_targetPartId);
    }

    //=========================================================================
    // ConstraintComponent::SetConstraintEntitiesWithPartIds
    //=========================================================================
    void ConstraintComponent::SetConstraintEntitiesWithPartIds(const AZ::EntityId& owningEntity, int ownerPartId, const AZ::EntityId& targetEntity, int targetPartId)
    {
        if ((owningEntity == m_config.m_owningEntity) && (targetEntity == m_config.m_targetEntity) && 
            (ownerPartId == m_config.m_ownerPartId) && (targetPartId == m_config.m_targetPartId))
        {
            return;
        }

        DisableInternal();

        LmbrCentral::PhysicsComponentNotificationBus::MultiHandler::BusDisconnect(m_config.m_targetEntity);
        LmbrCentral::PhysicsComponentNotificationBus::MultiHandler::BusDisconnect(m_config.m_owningEntity);

        AZ::EntityId oldOwner = m_config.m_owningEntity;
        AZ::EntityId oldTarget = m_config.m_targetEntity;

        m_config.m_owningEntity = owningEntity;
        m_config.m_ownerPartId = ownerPartId;

        m_config.m_targetEntity = targetEntity;
        m_config.m_targetPartId = targetPartId;

        // Note: OnPhysicsEnabled will call EnableInternal
        LmbrCentral::PhysicsComponentNotificationBus::MultiHandler::BusConnect(m_config.m_owningEntity);
        LmbrCentral::PhysicsComponentNotificationBus::MultiHandler::BusConnect(m_config.m_targetEntity);

        EBUS_EVENT_ID(GetEntityId(), ConstraintComponentNotificationBus, OnConstraintEntitiesChanged, oldOwner, oldTarget, owningEntity, targetEntity);
    }

    //=========================================================================
    // ConstraintComponent::EnableConstraint
    //=========================================================================
    void ConstraintComponent::EnableConstraint()
    {
        m_shouldBeEnabled = true;
        EnableInternal();
    }

    //=========================================================================
    // ConstraintComponent::EnableConstraint
    //=========================================================================
    void ConstraintComponent::EnableInternal()
    {
        if (IsEnabled() || !m_ownerPhysicsEnabled || (!m_targetPhysicsEnabled && m_config.m_targetEntity.IsValid()))
        {
            return;
        }

        if (m_config.m_owningEntity == m_config.m_targetEntity)
        {
            AZ_Error("ConstraintComponent", false, "Constraint component on entity %llu incorrectly constrained to itself.", GetEntityId());
            return;
        }

        IPhysicalEntity* physEntity = nullptr;
        EBUS_EVENT_ID_RESULT(physEntity, m_config.m_owningEntity, CryPhysicsComponentRequestBus, GetPhysicalEntity);
        if (!physEntity)
        {
            AZ_Error("ConstraintComponent", false, "Failed to get physical entity for entity %llu", m_config.m_owningEntity);
            return;
        }

        // The owner needs to be awake
        pe_action_awake actionAwake;
        actionAwake.minAwakeTime = 0.1f;
        physEntity->Action(&actionAwake);

        IPhysicalEntity* physBuddy = nullptr;
        if (m_config.m_targetEntity.IsValid())
        {
            EBUS_EVENT_ID_RESULT(physBuddy, m_config.m_targetEntity, CryPhysicsComponentRequestBus, GetPhysicalEntity);
            if (!physBuddy)
            {
                AZ_Error("ConstraintComponent", false, "Failed to get physical entity for target entity %llu on owning entity %llu", m_config.m_targetEntity, m_config.m_owningEntity);
                return;
            }

            // Ensure the buddy is awake as well
            pe_action_awake actionAwake;
            actionAwake.minAwakeTime = 0.1f;
            physBuddy->Action(&actionAwake);
        }
        else
        {
            physBuddy = WORLD_ENTITY;
        }

        pe_action_add_constraint aac;
        aac.pBuddy = physBuddy;

        SetupPivotsAndFrame(aac);

        switch (m_config.m_constraintType)
        {
            case ConstraintConfiguration::ConstraintType::Hinge :
            {
                aac.yzlimits[0] = 0.0f;
                aac.yzlimits[1] = 0.0f;
                if (m_config.m_enableRotationLimits)
                {
                    aac.xlimits[0] = AZ::DegToRad(m_config.m_xmin);
                    aac.xlimits[1] = AZ::DegToRad(m_config.m_xmax);
                }
                break;
            }
            case ConstraintConfiguration::ConstraintType::Ball :
            {
                if (m_config.m_enableRotationLimits)
                {
                    aac.xlimits[0] = 0.0f;
                    aac.xlimits[1] = 0.0f;
                    aac.yzlimits[0] = 0.0f;
                    aac.yzlimits[1] = AZ::DegToRad(m_config.m_yzmax);
                }
                break;
            }
            case ConstraintConfiguration::ConstraintType::Slider :
            {
                aac.yzlimits[0] = 0.0f;
                aac.yzlimits[1] = 0.0f;
                if (m_config.m_enableRotationLimits)
                {
                    aac.xlimits[0] = m_config.m_xmin;
                    aac.xlimits[1] = m_config.m_xmax;
                }
                aac.flags |= constraint_line;
                break;
            }
            case ConstraintConfiguration::ConstraintType::Plane :
            {
                aac.flags |= constraint_plane;
                break;
            }
            case ConstraintConfiguration::ConstraintType::Magnet :
            {
                break;
            }
            case ConstraintConfiguration::ConstraintType::Fixed :
            {
                aac.flags |= constraint_no_rotation;
                break;
            }
            case ConstraintConfiguration::ConstraintType::Free :
            {
                aac.flags |= constraint_free_position;
                aac.flags |= constraint_no_rotation;
                break;
            }
            default:
            {
                AZ_Assert(false, "Unexpected constraint type?");
                break;
            }
        };

        if (m_config.m_enableConstrainToPartId)
        {
            aac.partid[0] = m_config.m_ownerPartId;
            aac.partid[1] = m_config.m_targetPartId;
        }

        if (m_config.m_enableForceLimits)
        {
            aac.maxPullForce = m_config.m_maxPullForce;
        }

        if (m_config.m_enableRotationLimits)
        {
            aac.maxBendTorque = m_config.m_maxBendTorque;
        }

        if (m_config.m_enableDamping)
        {
            aac.damping = m_config.m_damping;
        }

        if (m_config.m_enableSearchRadius)
        {
            aac.sensorRadius = m_config.m_searchRadius;
        }

        if (!m_config.m_enableTargetCollisions)
        {
            aac.flags |= constraint_ignore_buddy;
        }
        if (!m_config.m_enableRelativeRotation)
        {
            aac.flags |= constraint_no_rotation;
        }
        if (!m_config.m_enforceOnFastObjects)
        {
            aac.flags |= constraint_no_enforcement;
        }
        if (!m_config.m_breakable)
        {
            aac.flags |= constraint_no_tears;
        }

        m_constraintId = physEntity->Action(&aac) != 0;
        if (m_constraintId)
        {
            EBUS_EVENT_ID(GetEntityId(), ConstraintComponentNotificationBus, OnConstraintEnabled);
        }
        else
        {
            AZ_Error("ConstraintComponent", false, "Constraint failed to initialize on entity %llu.", GetEntityId());
        }
    }

    //=========================================================================
    // ConstraintComponent::SetupPivotsAndFrame
    //=========================================================================
    void ConstraintComponent::SetupPivotsAndFrame(pe_action_add_constraint &aac) const
    {
        // Note: Coordinate space does not change constraint behavior, it only sets the coordinate space for initial positions and coordinate frames
        aac.flags &= ~local_frames;
        aac.flags |= world_frames;

        const AZ::EntityId selfId = GetEntityId();

        AZ::Transform selfTransform = AZ::Transform::Identity();
        EBUS_EVENT_ID_RESULT(selfTransform, selfId, AZ::TransformBus, GetWorldTM);
        const AZ::Vector3 selfWorldTranslation = selfTransform.GetTranslation();

        // World frame (qframe is ignored on ball socket constraints)
        if (m_config.m_constraintType != ConstraintConfiguration::ConstraintType::Ball)
        {
            const AZ::Quaternion worldFrame = ConstraintConfiguration::GetWorldFrame(m_config.m_axis, selfTransform);
            const quaternionf frame = AZQuaternionToLYQuaternion(worldFrame);
            aac.qframe[0] = frame;
            aac.qframe[1] = frame;
        }

        // Owner pivot
        AZ::Vector3 ownerPivot;
        const bool isMagnet = m_config.m_constraintType == ConstraintConfiguration::ConstraintType::Magnet;
        if (!isMagnet || (m_config.m_owningEntity == selfId))
        {
            // For all types except magnet, the owner pivot is always at self, whether it is owner, or a separate pivot
            ownerPivot = selfWorldTranslation;
        }
        else
        {
            // For magnet the pivot is always directly on the owner
            AZ::Transform ownerTransform = AZ::Transform::Identity();
            EBUS_EVENT_ID_RESULT(ownerTransform, m_config.m_owningEntity, AZ::TransformBus, GetWorldTM);
            ownerPivot = ownerTransform.GetTranslation();
        }
        aac.pt[0] = AZVec3ToLYVec3(ownerPivot);

        // Target pivot
        AZ::Vector3 targetPivot;
        if (!isMagnet || !m_config.m_targetEntity.IsValid())
        {
            // For all types except magnet, the target pivot is always at self, except for entity-to-entity magnet constraints
            targetPivot = selfWorldTranslation;
        }
        else
        {
            AZ::Transform targetTransform = AZ::Transform::Identity();
            EBUS_EVENT_ID_RESULT(targetTransform, m_config.m_targetEntity, AZ::TransformBus, GetWorldTM);
            targetPivot = targetTransform.GetTranslation();
        }
        aac.pt[1] = AZVec3ToLYVec3(targetPivot);
    }

    //=========================================================================
    // ConstraintComponent::DisableConstraint
    //=========================================================================
    void ConstraintComponent::DisableConstraint()
    {
        m_shouldBeEnabled = false;
        DisableInternal();
    }

    //=========================================================================
    // ConstraintComponent::SetConfiguration
    //=========================================================================
    void ConstraintComponent::SetConfiguration(const ConstraintConfiguration& config)
    {
        m_config = config;
    }

    //=========================================================================
    // ConstraintComponent::Disable
    //=========================================================================
    void ConstraintComponent::DisableInternal()
    {
        if (!IsEnabled())
        {
            return;
        }

        IPhysicalEntity* physEntity = nullptr;
        EBUS_EVENT_ID_RESULT(physEntity, m_config.m_owningEntity, CryPhysicsComponentRequestBus, GetPhysicalEntity);
        if (physEntity)
        {
            pe_action_update_constraint auc;
            auc.idConstraint = m_constraintId;
            auc.bRemove = 1;
            physEntity->Action(&auc);
        }

        m_constraintId = 0;
        EBUS_EVENT_ID(GetEntityId(), ConstraintComponentNotificationBus, OnConstraintDisabled);
    }

    //=========================================================================
    // ConstraintComponent::Disable
    //=========================================================================
    bool ConstraintComponent::IsEnabled() const
    {
        return m_constraintId != 0;
    }
} // namespace LmbrCentral
