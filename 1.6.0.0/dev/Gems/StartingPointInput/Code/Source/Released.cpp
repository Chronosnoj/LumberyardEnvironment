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
#include "Released.h"
#include "AzCore/EBus/EBus.h"
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Component/TickBus.h>

namespace Input
{
    void Released::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<Released, SingleEventToAction>()
                ->Version(1)
                ->Field("Maximum Time Before Suppressed", &Released::m_maximumTimeBeforeSuppressed);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<Released>("Released", "Released an input to generate an event")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &Released::GetEditorText)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"))
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &Released::m_maximumTimeBeforeSuppressed, "Maximum time before suppressed", "Holding an input longer than this will suppress the released event");
            }
        }
    }

    void Released::OnNotifyInputEvent(const SInputEvent& completeInputEvent)
    {
        bool isAnalogReleased = false;
        bool belowThreshold = completeInputEvent.value < m_deadZone;
        bool justPressed = !m_previousAnalogValueAboveThreshold && !belowThreshold;
        if (completeInputEvent.state == eIS_Changed)
        {
            if (belowThreshold && m_previousAnalogValueAboveThreshold)
            {
                isAnalogReleased = true;
            }
            m_previousAnalogValueAboveThreshold = !belowThreshold;
        }

        if (completeInputEvent.state == eIS_Released || isAnalogReleased)
        {
            float previousHighestEventValueSeen = m_highestEventValueSeen;
            m_highestEventValueSeen = 0.f;
            AZ::ScriptTimePoint now;
            if ((now.GetSeconds() - m_inputDownTime.GetSeconds()) < m_maximumTimeBeforeSuppressed)
            {
                EBUS_EVENT_ID(m_eventBusId, AZ::GameplayNotificationBus<float>, OnGameplayEventAction, previousHighestEventValueSeen * m_eventValueMultiplier);
            }
        }
        else if (completeInputEvent.state == eIS_Pressed || justPressed)
        {
            EBUS_EVENT_RESULT(m_inputDownTime, AZ::TickRequestBus, GetTimeAtCurrentTick);
        }
        else if (!isAnalogReleased)
        {
            if (fabs(m_highestEventValueSeen) < fabs(completeInputEvent.value))
            {
                m_highestEventValueSeen = completeInputEvent.value;
            }
            EBUS_EVENT_ID(m_eventBusId, AZ::GameplayNotificationBus<float>, OnGameplayEventFailed);
        }
    }

    void Released::Activate(const AZ::GameplayNotificationId& channelId)
    {
        AZ::Crc32 inputNameCrc(m_inputName.c_str());
        m_eventBusId = channelId;

        m_highestEventValueSeen = 0.f;
        AZ::InputNotificationId inputBusId(channelId.m_channel, inputNameCrc);
        AZ::InputNotificationBus::Handler::BusConnect(inputBusId);
    }

    void Released::Deactivate(const AZ::GameplayNotificationId& channelId)
    {
        AZ::Crc32 inputNameCrc(m_inputName.c_str());

        AZ::InputNotificationId inputBusId(channelId.m_channel, inputNameCrc);
        AZ::InputNotificationBus::Handler::BusDisconnect(inputBusId);
    }

    AZStd::string Released::GetEditorText() const
    {
        return AZStd::string::format("Released (%s)", this->SingleEventToAction::GetEditorText().c_str());
    }
} // namespace Input
