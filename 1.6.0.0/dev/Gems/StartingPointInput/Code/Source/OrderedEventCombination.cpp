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
#include "OrderedEventCombination.h"
#include "AzCore/EBus/EBus.h"
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Crc.h>

namespace Input
{
    void OrderedEventCombination::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<OrderedEventCombination>()
                ->Version(1)
                ->Field("Incoming Event Names", &OrderedEventCombination::m_incomingEventNames)
                ->Field("Max Delay Between Events", &OrderedEventCombination::m_maximumDelayBetweenEvents);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<OrderedEventCombination>("OrderedEventCombination", "Fire outgoing event after all Incoming Events fire in order")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &OrderedEventCombination::GetEditorText)
                    ->DataElement(0, &OrderedEventCombination::m_incomingEventNames, "Incoming event names", "The names of the ingoing events, ex 'Jump', 'Run'")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"))
                    ->DataElement(0, &OrderedEventCombination::m_maximumDelayBetweenEvents, "Max delay between events", "Exceeding this limit between successful events causes a failure")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"));
            }
        }
    }

    bool OrderedEventCombination::EventIsActive()
    {
        return m_currentIndexOfExpectedEvent > 0;
    }

    void OrderedEventCombination::FailInternal()
    {
        m_currentIndexOfExpectedEvent = 0;
        m_currentTimeSinceLastSuccessfulEvent = 0.0f;
        EBUS_EVENT_ID(m_eventBusId, AZ::GameplayNotificationBus<float>, OnGameplayEventFailed);
    }

    void OrderedEventCombination::Activate(const AZ::GameplayNotificationId& channelId)
    {
        m_eventBusId = channelId;
        m_eventNameToIndex.clear();
        unsigned eventIndex = 0;
        for (const AZStd::string& eventName : m_incomingEventNames)
        {
            AZ::Crc32 eventCrc = AZ_CRC(eventName.c_str());
            m_eventNameToIndex[eventCrc] = eventIndex;
            ++eventIndex;

            m_currentIndexOfExpectedEvent = 0;
            m_currentTimeSinceLastSuccessfulEvent = 0.f;
            AZ::GameplayNotificationId actionBusId(channelId.m_channel, eventCrc);
            AZ::GameplayNotificationBus<float>::MultiHandler::BusConnect(actionBusId);
        }
        AZ::TickBus::Handler::BusConnect();
    }

    void OrderedEventCombination::Deactivate(const AZ::GameplayNotificationId& channelId)
    {
        AZ::TickBus::Handler::BusDisconnect();
        for (AZStd::string& eventName : m_incomingEventNames)
        {
            AZ::Crc32 eventCrc = AZ_CRC(eventName.c_str());

            AZ::GameplayNotificationId actionBusId(channelId.m_channel, eventCrc);
            AZ::GameplayNotificationBus<float>::MultiHandler::BusDisconnect(actionBusId);
        }
    }

    void OrderedEventCombination::OnGameplayEventAction(const float& value)
    {
        // We've received an event, now we need to make sure it's the correct one.
        const AZ::GameplayNotificationId actionBusId = *AZ::GameplayNotificationBus<float>::GetCurrentBusId();
        unsigned receivedEventIndex = m_eventNameToIndex[actionBusId.m_actionNameCrc];

        if (receivedEventIndex == m_currentIndexOfExpectedEvent)
        {
            ++m_currentIndexOfExpectedEvent;
            m_currentTimeSinceLastSuccessfulEvent = 0.f;
            if (m_currentIndexOfExpectedEvent == m_incomingEventNames.size())
            {
                m_currentIndexOfExpectedEvent = 0;
                EBUS_EVENT_ID(m_eventBusId, AZ::GameplayNotificationBus<float>, OnGameplayEventAction, value);
            }
        }
        else if (EventIsActive())
        {
            FailInternal();
        }
    }

    void OrderedEventCombination::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        if (EventIsActive())
        {
            float& currentElapsedTimeRef = m_currentTimeSinceLastSuccessfulEvent;
            currentElapsedTimeRef += deltaTime;
            if (currentElapsedTimeRef > m_maximumDelayBetweenEvents)
            {
                // we failed to finish the combo in time
                FailInternal();
            }
        }
    }

    AZStd::string OrderedEventCombination::GetEditorText() const
    {
        return AZStd::string::format("%s...", m_incomingEventNames.size() > 0 ? m_incomingEventNames[0].c_str() : "");
    }
} //namespace Input