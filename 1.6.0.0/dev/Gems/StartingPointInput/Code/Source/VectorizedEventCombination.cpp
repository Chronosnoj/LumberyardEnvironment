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
#include "VectorizedEventCombination.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>

namespace Input
{
    void VectorizedEventCombination::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<VectorizedEventCombination>()
                ->Version(1)
                ->Field("Incoming Event Names", &VectorizedEventCombination::m_incomingEventNames)
                ->Field("Should Normalize", &VectorizedEventCombination::m_shouldNormalize)
                ->Field("Dead Zone Length", &VectorizedEventCombination::m_deadZoneLength);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<VectorizedEventCombination>("VectorizedEventCombination", "Incoming events will be aggregated into a vector and sent as a single event")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->DataElement(0, &VectorizedEventCombination::m_incomingEventNames, "Incoming event names", "The names of the ingoing events, ex 'Jump', 'Run'")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"))
                    ->DataElement(0, &VectorizedEventCombination::m_shouldNormalize, "Should normalize", "True if you want the output event value to be normalized")
                    ->DataElement(0, &VectorizedEventCombination::m_deadZoneLength, "Dead zone length", "A vector length below this threshold will not send an event out");
            }
        }
    }

    void VectorizedEventCombination::OnGameplayEventAction(const float& value)
    {
        const AZ::GameplayNotificationId actionBusId = *AZ::GameplayNotificationBus<float>::GetCurrentBusId();
        const AZ::EntityId channel = actionBusId.m_channel;
        const AZ::Crc32 eventCrc = actionBusId.m_actionNameCrc;

        // Update the vector value
        m_aggregatedEventValue.SetElement(m_eventCrcToIndex[eventCrc], value);
        // Check to see if our new length is greater than the dead zone threshold
        if (m_aggregatedEventValue.GetLengthSq() < (m_deadZoneLength * m_deadZoneLength))
        {
            m_aggregatedEventValue = AZ::Vector3::CreateZero();
        }
        AZ::Vector3 finalEventValue = m_shouldNormalize ? m_aggregatedEventValue.GetNormalized() : m_aggregatedEventValue;
        EBUS_EVENT_ID(m_eventBusId, AZ::GameplayNotificationBus<AZ::Vector3>, OnGameplayEventAction, finalEventValue);
    }

    void VectorizedEventCombination::OnGameplayEventFailed()
    {
        const AZ::GameplayNotificationId actionBusId = *AZ::GameplayNotificationBus<float>::GetCurrentBusId();
        const AZ::EntityId channel = actionBusId.m_channel;
        const AZ::Crc32 eventCrc = actionBusId.m_actionNameCrc;

        // Update the vector value to 0 for a failed event
        m_aggregatedEventValue.SetElement(m_eventCrcToIndex[eventCrc], 0.f);
        // Check to see if our new length is greater than the dead zone threshold
        if (m_aggregatedEventValue.GetLengthSq() < (m_deadZoneLength * m_deadZoneLength))
        {
            m_aggregatedEventValue = AZ::Vector3::CreateZero();
        }
        AZ::Vector3 finalEventValue = m_shouldNormalize ? m_aggregatedEventValue.GetNormalized() : m_aggregatedEventValue;
        EBUS_EVENT_ID(m_eventBusId, AZ::GameplayNotificationBus<AZ::Vector3>, OnGameplayEventAction, finalEventValue);
    }

    void VectorizedEventCombination::Activate(const AZ::GameplayNotificationId& channelId)
    {
        m_eventBusId = channelId;
        m_aggregatedEventValue = AZ::Vector3::CreateZero();
        int currentEventIndex = 0;
        for (const AZStd::string& eventName : m_incomingEventNames)
        {
            if (AZ::Crc32 eventCrc = AZ_CRC(eventName.c_str()))
            {
                m_eventCrcToIndex[eventCrc] = currentEventIndex;

                AZ::GameplayNotificationId actionBusId(channelId.m_channel, eventCrc);
                AZ::GameplayNotificationBus<float>::MultiHandler::BusConnect(actionBusId);
            }
            ++currentEventIndex;
        }
    }

    void VectorizedEventCombination::Deactivate(const AZ::GameplayNotificationId& channelId)
    {
        for (AZStd::string& eventName : m_incomingEventNames)
        {
            if (AZ::Crc32 eventCrc = AZ_CRC(eventName.c_str()))
            {
                AZ::GameplayNotificationId actionBusId(channelId.m_channel, eventCrc);
                AZ::GameplayNotificationBus<float>::MultiHandler::BusDisconnect(actionBusId);
            }
        }
    }
} // namespace Input