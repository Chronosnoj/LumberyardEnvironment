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
#include "StdAfx.h"
#include "MoveEntityAction.h"
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/TransformBus.h>
#include <LmbrCentral/Physics/PhysicsComponentBus.h>
#include <MathConversion.h>
#include <physinterface.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>

namespace Movement
{
    void MoveEntityAction::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<MoveEntityAction>()
                ->Version(1)
                ->Field("X Scale", &MoveEntityAction::m_xScale)
                ->Field("Y Scale", &MoveEntityAction::m_yScale)
                ->Field("Z Scale", &MoveEntityAction::m_zScale)
                ->Field("Event Name", &MoveEntityAction::m_eventName)
                ->Field("Should Ignore Physics", &MoveEntityAction::m_shouldIgnorePhysics)
                ->Field("Is Relative", &MoveEntityAction::m_isRelative);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<MoveEntityAction>("Move Entity Action"
                    , "This will move an Entity in Direction when the EventName fires")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->DataElement(0, &MoveEntityAction::m_xScale, "X Scale", "Scale the incoming direction's X component")
                    ->DataElement(0, &MoveEntityAction::m_yScale, "Y Scale", "Scale the incoming direction's Y component")
                    ->DataElement(0, &MoveEntityAction::m_zScale, "Z Scale", "Scale the incoming direction's Z component")
                    ->DataElement(0, &MoveEntityAction::m_eventName, "Event Name", "The Name of the expected Event")
                    ->DataElement(0, &MoveEntityAction::m_shouldIgnorePhysics, "Should Ignore Physics", "Set to true to just set the entity's transform")
                    ->DataElement(0, &MoveEntityAction::m_isRelative, "Is Relative", "When true, the entity's transform will form the basis for the movement");
            }
        }
    }

    void MoveEntityAction::OnGameplayEventAction(const AZ::Vector3& value)
    {
        float deltaTime = 0.f;
        EBUS_EVENT_RESULT(deltaTime, AZ::TickRequestBus, GetTickDeltaTime);

        AZ::Vector3 displacement = value * deltaTime;
        displacement.SetX(displacement.GetX() * m_xScale);
        displacement.SetY(displacement.GetY() * m_yScale);
        displacement.SetZ(displacement.GetZ() * m_zScale);
        if (m_shouldIgnorePhysics)
        {   
            AZ::Transform entityTransform;
            EBUS_EVENT_ID_RESULT(entityTransform, m_entityId, AZ::TransformBus, GetWorldTM);

            if (m_isRelative)
            {
                AZ::Quaternion entityRotation = AZ::Quaternion::CreateFromTransform(entityTransform);
                displacement = entityRotation * displacement;
            }

            entityTransform.SetPosition(entityTransform.GetPosition() + displacement);
            EBUS_EVENT_ID(m_entityId, AZ::TransformBus, SetWorldTM, entityTransform);
        }
        else
        {
            pe_status_pos transform;
            EBUS_EVENT_ID(m_entityId, LmbrCentral::CryPhysicsComponentRequestBus, GetPhysicsStatus, transform);

            pe_action_move moveAction;
            moveAction.iJump = 1;
            if (m_isRelative)
            {
                moveAction.dir = transform.q * AZVec3ToLYVec3(displacement);
            }
            else
            {
                moveAction.dir = AZVec3ToLYVec3(displacement);
            }
            EBUS_EVENT_ID(m_entityId, LmbrCentral::CryPhysicsComponentRequestBus, ApplyPhysicsAction, moveAction, false);
        }
    }

    void MoveEntityAction::Activate(const AZ::EntityId& entityIdToMove, const AZ::EntityId& channelId)
    {
        m_entityId = entityIdToMove;

        if (m_eventNameCrc = AZ::Crc32(m_eventName.c_str()))
        {
            AZ::GameplayNotificationId actionBusId(channelId, m_eventNameCrc);
            AZ::GameplayNotificationBus<AZ::Vector3>::Handler::BusConnect(actionBusId);
        }
    }

    void MoveEntityAction::Deactivate(const AZ::EntityId& channelId)
    {
        if (m_eventNameCrc)
        {
            AZ::GameplayNotificationId actionBusId(channelId, m_eventNameCrc);
            AZ::GameplayNotificationBus<AZ::Vector3>::Handler::BusDisconnect(actionBusId);
        }
    }
} //namespace Movement