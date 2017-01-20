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
#include "FollowTargetFromDistance.h"
#include <AzCore/Math/Transform.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/Serialization/EditContext.h>
#include "StartingPointCamera/StartingPointCameraConstants.h"


namespace Camera
{
    void FollowTargetFromDistance::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<FollowTargetFromDistance>()
                ->Version(1)
                ->Field("Follow Distance", &FollowTargetFromDistance::m_followDistance)
                ->Field("Minimum Follow Distance", &FollowTargetFromDistance::m_minFollowDistance)
                ->Field("Maximum Follow Distance", &FollowTargetFromDistance::m_maxFollowDistance)
                ->Field("Zoom In Event Name", &FollowTargetFromDistance::m_zoomInEventName)
                ->Field("Zoom Out Event Name", &FollowTargetFromDistance::m_zoomOutEventName)
                ->Field("Zoom Speed Scale", &FollowTargetFromDistance::m_zoomSpeedScale)
                ->Field("Input Source Entity", &FollowTargetFromDistance::m_channelId);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<FollowTargetFromDistance>("FollowTargetFromDistance", "Follows behind the target by Follow Distance meters")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->DataElement(0, &FollowTargetFromDistance::m_followDistance, "Follow Distance", "The distance to follow behind the target in meters")
                        ->Attribute(AZ::Edit::Attributes::Suffix, "m")
                        ->Attribute(AZ::Edit::Attributes::Min, &FollowTargetFromDistance::GetMinimumFollowDistance)
                        ->Attribute(AZ::Edit::Attributes::Max, &FollowTargetFromDistance::GetMaximumFollowDistance)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"))
                    ->DataElement(0, &FollowTargetFromDistance::m_minFollowDistance, "Minimum Follow Distance", "The MINIMUM distance to follow the behind the target in meters")
                        ->Attribute(AZ::Edit::Attributes::Suffix, "m")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->Attribute(AZ::Edit::Attributes::Max, &FollowTargetFromDistance::GetMaximumFollowDistance)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"))
                    ->DataElement(0, &FollowTargetFromDistance::m_maxFollowDistance, "Maximum Follow Distance", "The MAXIMUM distance to follow the behind the target in meters")
                        ->Attribute(AZ::Edit::Attributes::Suffix, "m")
                        ->Attribute(AZ::Edit::Attributes::Min, &FollowTargetFromDistance::GetMinimumFollowDistance)
                        ->Attribute(AZ::Edit::Attributes::Max, std::numeric_limits<float>::max())
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"))
                    ->DataElement(0, &FollowTargetFromDistance::m_zoomInEventName, "Zoom In Event Name", "The name of the event to trigger a zoom in")
                    ->DataElement(0, &FollowTargetFromDistance::m_zoomOutEventName, "Zoom Out Event Name", "The name of the event to trigger a zoom out")
                    ->DataElement(0, &FollowTargetFromDistance::m_zoomSpeedScale, "Zoom Speed Scale", "The amount to scale the incoming zoom event by")
                    ->DataElement(0, &FollowTargetFromDistance::m_channelId, "Input Source Entity", "The entity whose input channel to use");
            }
        }
    }

    void FollowTargetFromDistance::AdjustCameraTransform(float /*deltaTime*/, const AZ::Transform& /*initialCameraTransform*/, const AZ::Transform& targetTransform, AZ::Transform& inOutCameraTransform)
    {
        inOutCameraTransform.SetPosition(targetTransform.GetPosition() - targetTransform.GetColumn(ForwardBackward) * m_followDistance);
    }

    void FollowTargetFromDistance::Activate(AZ::EntityId channelId)
    {
        if (!m_channelId.IsValid())
        {
            m_channelId = channelId;
        }
        if (!m_zoomInEventName.empty())
        {
            AZ::GameplayNotificationId busId(m_channelId, AZ_CRC(m_zoomInEventName.c_str()));
            AZ::GameplayNotificationBus<float>::MultiHandler::BusConnect(busId);
        }

        if (!m_zoomOutEventName.empty())
        {
            AZ::GameplayNotificationId busId(m_channelId, AZ_CRC(m_zoomOutEventName.c_str()));
            AZ::GameplayNotificationBus<float>::MultiHandler::BusConnect(busId);
        }
    }

    void FollowTargetFromDistance::Deactivate()
    {
        if (!m_zoomInEventName.empty())
        {
            AZ::GameplayNotificationId busId(m_channelId, AZ_CRC(m_zoomInEventName.c_str()));
            AZ::GameplayNotificationBus<float>::MultiHandler::BusDisconnect(busId);
        }

        if (!m_zoomOutEventName.empty())
        {
            AZ::GameplayNotificationId busId(m_channelId, AZ_CRC(m_zoomOutEventName.c_str()));
            AZ::GameplayNotificationBus<float>::MultiHandler::BusDisconnect(busId);
        }
    }

    void FollowTargetFromDistance::OnGameplayEventAction(const float& value)
    {
        m_followDistance = AZ::GetClamp(m_followDistance - (value * m_zoomSpeedScale), m_minFollowDistance, m_maxFollowDistance);
    }
} // namespace Camera

