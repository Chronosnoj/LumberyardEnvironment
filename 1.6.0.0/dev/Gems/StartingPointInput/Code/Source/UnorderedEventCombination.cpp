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
#include "UnorderedEventCombination.h"
#include "AzCore/EBus/EBus.h"
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Crc.h>

namespace Input
{
    void UnorderedEventCombination::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<UnorderedEventCombination>()
                ->Version(1)
                ->Field("Incoming Event Names", &UnorderedEventCombination::m_incomingEventNames)
                ->Field("Max Delay For All Events", &UnorderedEventCombination::m_maximumTimeAllowedForAllEvents);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<UnorderedEventCombination>("UnorderedEventCombination", "All events must pass within the time, order is not important")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &UnorderedEventCombination::GetEditorText)
                    ->DataElement(0, &UnorderedEventCombination::m_incomingEventNames, "Incoming event names", "The names of the ingoing events, ex 'Jump', 'Run'")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"))
                    ->DataElement(0, &UnorderedEventCombination::m_maximumTimeAllowedForAllEvents, "Max delay for all events", "exceeding this limit without a successful incoming event causes a fail")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"));
            }
        }
    }

    void UnorderedEventCombination::FailInternal()
    {
        ResetActiveState();
        EBUS_EVENT_ID(m_eventBusId, AZ::GameplayNotificationBus<float>, OnGameplayEventFailed);
    }

    void UnorderedEventCombination::SucceedInternal(float value)
    {
        // All incoming events are now active, send out a success event
        ResetActiveState();
        EBUS_EVENT_ID(m_eventBusId, AZ::GameplayNotificationBus<float>, OnGameplayEventAction, value);
    }

    void UnorderedEventCombination::ResetActiveState()
    {
        m_isAttemptingCombination = false;
        for (auto& eventAndActivity : m_eventIsActive)
        {
            eventAndActivity.second = false;
        }
    }

    void UnorderedEventCombination::Activate(const AZ::GameplayNotificationId& channelId)
    {
        m_eventBusId = channelId;
        for (const AZStd::string& eventName : m_incomingEventNames)
        {
            AZ::Crc32 eventCrc = AZ_CRC(eventName.c_str());

            m_eventIsActive[eventCrc] = false;
            m_isAttemptingCombination = false;
            AZ::GameplayNotificationId actionBusId(channelId.m_channel, eventCrc);
            AZ::GameplayNotificationBus<float>::MultiHandler::BusConnect(actionBusId);
        }
        AZ::TickBus::Handler::BusConnect();
    }

    void UnorderedEventCombination::Deactivate(const AZ::GameplayNotificationId& channelId)
    {
        AZ::TickBus::Handler::BusDisconnect();
        for (const AZStd::string& eventName : m_incomingEventNames)
        {
            AZ::Crc32 eventCrc = AZ_CRC(eventName.c_str());
            m_eventIsActive.clear();
            AZ::GameplayNotificationId actionBusId(channelId.m_channel, eventCrc);
            AZ::GameplayNotificationBus<float>::MultiHandler::BusDisconnect(actionBusId);
        }
    }

    void UnorderedEventCombination::OnGameplayEventAction(const float& value)
    {
        const AZ::GameplayNotificationId actionBusId = *AZ::GameplayNotificationBus<float>::GetCurrentBusId();
        m_isAttemptingCombination = true;
        const AZ::Crc32 eventCrc = actionBusId.m_actionNameCrc;

        // Update State
        m_eventNameToLastActivatedTime[eventCrc] = m_currentTimeInSeconds;
        m_eventIsActive[eventCrc] = true;

        // check to see if all events have been completed
        for (const auto& eventAndActivity : m_eventIsActive)
        {
            if (!eventAndActivity.second)
            {
                return;
            }
        }

        float timeCutoffForSuccess = m_currentTimeInSeconds - m_maximumTimeAllowedForAllEvents;
        for (const auto& eventTime : m_eventNameToLastActivatedTime)
        {
            if (eventTime.second < timeCutoffForSuccess)
            {
                // all of the event times will have to be above the cutoff for a success
                return;
            }
        }
        SucceedInternal(value);
    }

    void UnorderedEventCombination::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        m_currentTimeInSeconds = time.GetSeconds();
        float timeCutoffForSuccess = m_currentTimeInSeconds - m_maximumTimeAllowedForAllEvents;

        if (m_isAttemptingCombination)
        {
            bool allEventsAreFailing = true;
            for (const auto& eventTime : m_eventNameToLastActivatedTime)
            {
                if (eventTime.second > timeCutoffForSuccess)
                {
                    // all of the event times will have to be below the cutoff for a fail
                    allEventsAreFailing = false;
                    break;
                }
            }
            // we failed to finish the combo in time
            if (allEventsAreFailing)
            {
                FailInternal();
            }
        }
    }

    AZStd::string UnorderedEventCombination::GetEditorText() const
    {
        return AZStd::string::format("Unordered combo in %.2f", m_maximumTimeAllowedForAllEvents);
    }
} //namespace Input