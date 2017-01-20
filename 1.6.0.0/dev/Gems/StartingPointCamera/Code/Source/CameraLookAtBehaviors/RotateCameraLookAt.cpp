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
#include "RotateCameraLookAt.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Component/TickBus.h>

namespace Camera
{
    void RotateCameraLookAt::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<RotateCameraLookAt>()
                ->Version(1)
                ->Field("Axis Of Rotation", &RotateCameraLookAt::m_axisOfRotation)
                ->Field("Event Name", &RotateCameraLookAt::m_eventName)
                ->Field("Player Index", &RotateCameraLookAt::m_playerIndex)
                ->Field("Invert Axis", &RotateCameraLookAt::m_shouldInvertAxis)
                ->Field("Rotation Speed Scale", &RotateCameraLookAt::m_rotationSpeedScale);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<RotateCameraLookAt>("Rotate Camera Target"
                    , "This will rotate a Camera Target about Axis when the EventName fires")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &RotateCameraLookAt::m_axisOfRotation, "Axis Of Rotation",
                    "This is the direction vector that will be applied to the target's movement scaled for time")
                        ->EnumAttribute(AxisOfRotation::X_Axis, "Camera Target's X Axis")
                        ->EnumAttribute(AxisOfRotation::Y_Axis, "Camera Target's Y Axis")
                        ->EnumAttribute(AxisOfRotation::Z_Axis, "Camera Target's Z Axis")
                    ->DataElement(0, &RotateCameraLookAt::m_eventName, "Event Name", "The Name of the expected Event")
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &RotateCameraLookAt::m_playerIndex, "Player Index", "The index of the player attached to this event")
                        ->Attribute(AZ::Edit::Attributes::Min, 0)
                        ->Attribute(AZ::Edit::Attributes::Step, 1)
                        ->Attribute(AZ::Edit::Attributes::Max, std::numeric_limits<AZ::u8>::digits)
                    ->DataElement(0, &RotateCameraLookAt::m_shouldInvertAxis, "Invert Axis", "True if you want to rotate along a negative axis")
                    ->DataElement(0, &RotateCameraLookAt::m_rotationSpeedScale, "Rotation Speed Scale", "Scale greater than 1 to speed up, between 0 and 1 to slow down")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.001f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.1f)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"));
            }
        }
    }

    void RotateCameraLookAt::AdjustLookAtTarget(float deltaTime, const AZ::Transform& targetTransform, AZ::Transform& outLookAtTargetTransform)
    {
        float axisPolarity = m_shouldInvertAxis ? -1.0f : 1.0f;
        float rotationAmount = axisPolarity * m_rotationAmount;

        // remove translation and scale
        AZ::Vector3 translation = outLookAtTargetTransform.GetTranslation();
        outLookAtTargetTransform.SetTranslation(AZ::Vector3::CreateZero());
        AZ::Vector3 transformScale = outLookAtTargetTransform.ExtractScaleExact();

        // perform our rotation
        AZ::Transform desiredRotationTransform = AZ::Transform::CreateFromQuaternion(AZ::Quaternion::CreateFromAxisAngle(outLookAtTargetTransform.GetColumn(m_axisOfRotation), rotationAmount));

        outLookAtTargetTransform = desiredRotationTransform * outLookAtTargetTransform;

        // return scale and translate
        outLookAtTargetTransform.ExtractScaleExact();
        outLookAtTargetTransform.MultiplyByScale(transformScale);
        outLookAtTargetTransform.SetTranslation(translation);
    }

    void RotateCameraLookAt::Activate(AZ::EntityId)
    {
        AZ::Crc32 eventNameCrc = AZ::Crc32(m_eventName.c_str());
        AZ::GameplayNotificationId actionBusId(m_playerIndex, eventNameCrc);
        AZ::GameplayNotificationBus<float>::Handler::BusConnect(actionBusId);
    }

    void RotateCameraLookAt::Deactivate()
    {
        AZ::Crc32 eventNameCrc = AZ::Crc32(m_eventName.c_str());
        AZ::GameplayNotificationId actionBusId(m_playerIndex, eventNameCrc);
        AZ::GameplayNotificationBus<float>::Handler::BusDisconnect(actionBusId);
    }

    void RotateCameraLookAt::OnGameplayEventAction(const float& value)
    {
        float frameTime = 0.0f;
        EBUS_EVENT_RESULT(frameTime, AZ::TickRequestBus, GetTickDeltaTime);
        m_rotationAmount += value * frameTime * m_rotationSpeedScale;
    }

    void RotateCameraLookAt::OnGameplayEventFailed()
    {
        m_rotationAmount = 0.f;
    }
} // namespace Camera