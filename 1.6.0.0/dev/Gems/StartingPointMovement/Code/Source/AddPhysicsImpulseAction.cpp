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
#include "AddPhysicsImpulseAction.h"
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
    void AddPhysicsInpulseAction::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<AddPhysicsInpulseAction>()
                ->Version(1)
                ->Field("X Scale", &AddPhysicsInpulseAction::m_xScale)
                ->Field("Y Scale", &AddPhysicsInpulseAction::m_yScale)
                ->Field("Z Scale", &AddPhysicsInpulseAction::m_zScale)
                ->Field("Event Name", &AddPhysicsInpulseAction::m_eventName)
                ->Field("Should Apply Instantly", &AddPhysicsInpulseAction::m_shouldApplyInstantly)
                ->Field("Is Relative", &AddPhysicsInpulseAction::m_isRelative);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<AddPhysicsInpulseAction>("Add Physics Impulse Action"
                    , "This will apply an impulse to an Entity when the EventName fires")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->DataElement(0, &AddPhysicsInpulseAction::m_xScale, "X Scale", "Scale the incoming direction's X component")
                    ->DataElement(0, &AddPhysicsInpulseAction::m_yScale, "Y Scale", "Scale the incoming direction's Y component")
                    ->DataElement(0, &AddPhysicsInpulseAction::m_zScale, "Z Scale", "Scale the incoming direction's Z component")
                    ->DataElement(0, &AddPhysicsInpulseAction::m_eventName, "Event Name", "The Name of the expected Event")
                    ->DataElement(0, &AddPhysicsInpulseAction::m_shouldApplyInstantly, "Should Apply Instantly", "Set to false if you plan to send continuous impulses")
                    ->DataElement(0, &AddPhysicsInpulseAction::m_isRelative, "Is Relative", "When true, the entity's transform will form the basis for the movement");
            }
        }
    }

    void AddPhysicsInpulseAction::OnGameplayEventAction(const AZ::Vector3& value)
    {
        float timeScale = 1.0f;
        if (!m_shouldApplyInstantly)
        {
            EBUS_EVENT_RESULT(timeScale, AZ::TickRequestBus, GetTickDeltaTime);
        }

        AZ::Vector3 displacement = value * timeScale;
        displacement.SetX(displacement.GetX() * m_xScale);
        displacement.SetY(displacement.GetY() * m_yScale);
        displacement.SetZ(displacement.GetZ() * m_zScale);
        AZ::Transform entityTransform;
        EBUS_EVENT_ID_RESULT(entityTransform, m_entityId, AZ::TransformBus, GetWorldTM);

        if (m_isRelative)
        {
            AZ::Quaternion entityRotation = AZ::Quaternion::CreateFromTransform(entityTransform);
            displacement = entityRotation * displacement;
        }
        pe_action_impulse impulseAction;
        impulseAction.impulse = AZVec3ToLYVec3(displacement);
        EBUS_EVENT_ID(m_entityId, LmbrCentral::CryPhysicsComponentRequestBus, ApplyPhysicsAction, impulseAction, false);
    }

    void AddPhysicsInpulseAction::Activate(const AZ::EntityId& entityIdToMove, const AZ::EntityId& channelId)
    {
        m_entityId = entityIdToMove;

        if (m_eventNameCrc = AZ::Crc32(m_eventName.c_str()))
        {
            AZ::GameplayNotificationId actionBusId(channelId, m_eventNameCrc);
            AZ::GameplayNotificationBus<AZ::Vector3>::Handler::BusConnect(actionBusId);
        }
    }

    void AddPhysicsInpulseAction::Deactivate(const AZ::EntityId& channelId)
    {
        if (m_eventNameCrc)
        {
            AZ::GameplayNotificationId actionBusId(channelId, m_eventNameCrc);
            AZ::GameplayNotificationBus<AZ::Vector3>::Handler::BusDisconnect(actionBusId);
        }
    }
} //namespace Movement