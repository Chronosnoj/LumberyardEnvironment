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
#include "RotateEntityAction.h"
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/TransformBus.h>
#include <MathConversion.h>
#include <physinterface.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/MathUtils.h>
#include <LmbrCentral/Physics/PhysicsComponentBus.h>
#include "StartingPointMovement/StartingPointMovementUtilities.h"

namespace Movement
{
    void RotateEntityAction::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<RotateEntityAction>()
                ->Version(1)
                ->Field("Axis Of Rotation", &RotateEntityAction::m_axisOfRotation)
                ->Field("Event Name", &RotateEntityAction::m_eventName)
                ->Field("Invert Axis", &RotateEntityAction::m_shouldInvertAxis)
                ->Field("Rotation Speed Scale", &RotateEntityAction::m_rotationSpeedScale)
                ->Field("Should Ignore Physics", &RotateEntityAction::m_shouldIgnorePhysics)
                ->Field("Is Relative", &RotateEntityAction::m_isRelative);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<RotateEntityAction>("Rotate Entity Action"
                    , "This will rotate an Entity about Axis when the EventName fires")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &RotateEntityAction::m_axisOfRotation, "Axis Of Rotation",
                    "This is the direction vector that will be applied to the entity's movement scaled for time")
                        ->EnumAttribute(AxisOfRotation::X_Axis, "Entity's X Axis")
                        ->EnumAttribute(AxisOfRotation::Y_Axis, "Entity's Y Axis")
                        ->EnumAttribute(AxisOfRotation::Z_Axis, "Entity's Z Axis")
                    ->DataElement(0, &RotateEntityAction::m_eventName, "Event Name", "The Name of the expected Event")
                    ->DataElement(0, &RotateEntityAction::m_shouldInvertAxis, "Invert Axis", "True if you want to rotate along a negative axis")
                    ->DataElement(0, &RotateEntityAction::m_rotationSpeedScale, "Rotation Speed Scale", "Scale greater than 1 to speed up, between 0 and 1 to slow down")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.001f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.1f)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"))
                    ->DataElement(0, &RotateEntityAction::m_shouldIgnorePhysics, "Should Ignore Physics", "Set to true to just set the entity's transform")
                    ->DataElement(0, &RotateEntityAction::m_isRelative, "Is Relative", "true if the rotation should be relative to the entity");
            }
        }
    }

    void RotateEntityAction::OnGameplayEventAction(const float& value)
    {
        float deltaTime = 0.f;
        EBUS_EVENT_RESULT(deltaTime, AZ::TickRequestBus, GetTickDeltaTime);
        float axisPolarity = m_shouldInvertAxis ? -1.0f : 1.0f;
        float rotationAmount = axisPolarity * value * deltaTime * m_rotationSpeedScale;

        if (m_shouldIgnorePhysics)
        {
            AZ::Transform entityTransform;
            EBUS_EVENT_ID_RESULT(entityTransform, m_entityId, AZ::TransformBus, GetWorldTM);

            // remove translation and scale
            AZ::Vector3 translation = entityTransform.GetTranslation();
            entityTransform.SetTranslation(AZ::Vector3::CreateZero());
            AZ::Vector3 transformScale = entityTransform.ExtractScaleExact();

            // perform our rotation
            AZ::Transform desiredRotationTransform;
            if (m_isRelative)
            {
                desiredRotationTransform = AZ::Transform::CreateFromQuaternion(AZ::Quaternion::CreateFromAxisAngle(entityTransform.GetColumn(m_axisOfRotation), rotationAmount));
            }
            else
            {
                desiredRotationTransform = CreateRotationFromAxis(m_axisOfRotation, rotationAmount);
            }
            entityTransform = desiredRotationTransform * entityTransform;

            // return scale and translate
            entityTransform.ExtractScaleExact();
            entityTransform.MultiplyByScale(transformScale);
            entityTransform.SetTranslation(translation);
            // submit changes
            EBUS_EVENT_ID(m_entityId, AZ::TransformBus, SetWorldTM, entityTransform);
        }
        else
        {
            pe_status_pos physicsPosition;
            pe_params_pos positionAndRotation;
            EBUS_EVENT_ID(m_entityId, LmbrCentral::CryPhysicsComponentRequestBus, GetPhysicsStatus, physicsPosition);
            Quat q = physicsPosition.q;
            positionAndRotation.pos = positionAndRotation.pos;
            if (m_isRelative)
            {
                positionAndRotation.q = q * AZTransformToLYQuatT(CreateRotationFromAxis(m_axisOfRotation, rotationAmount)).q;
            }
            else
            {
                positionAndRotation.q = q * AZQuaternionToLYQuaternion(AZ::Quaternion::CreateFromTransform(CreateRotationFromAxis(m_axisOfRotation, rotationAmount)));
            }
            EBUS_EVENT_ID(m_entityId, LmbrCentral::CryPhysicsComponentRequestBus, SetPhysicsParameters, positionAndRotation);
        }
    }

    void RotateEntityAction::Activate(const AZ::EntityId& entityIdToRotate, const AZ::EntityId& channelId)
    {
        m_entityId = entityIdToRotate;
        if (AZ::Crc32 eventNameCrc = AZ::Crc32(m_eventName.c_str()))
        {
            AZ::GameplayNotificationId actionBusId(channelId, eventNameCrc);
            AZ::GameplayNotificationBus<float>::Handler::BusConnect(actionBusId);
        }
    }

    void RotateEntityAction::Deactivate(const AZ::EntityId& channelId)
    {
        if (AZ::Crc32 eventNameCrc = AZ::Crc32(m_eventName.c_str()))
        {
            AZ::GameplayNotificationId actionBusId(channelId, eventNameCrc);
            AZ::GameplayNotificationBus<float>::Handler::BusDisconnect(actionBusId);
        }
    }
} //namespace Movement