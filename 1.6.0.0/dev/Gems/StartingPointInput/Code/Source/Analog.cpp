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
#include "Analog.h"
#include <AzCore/EBus/EBus.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/VectorFloat.h>

namespace Input
{
    static const int s_analogVersion = 2;
    bool ConvertAnalogVersion(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
    {
        int currentUpgradedVersion = classElement.GetVersion();
        while (currentUpgradedVersion != s_analogVersion)
        {
            switch (currentUpgradedVersion)
            {
            case 1:
            {
                float currentDeadZone = AZ::g_fltEps;
                classElement.GetChildData(AZ::Crc32("Dead Zone"), currentDeadZone);
                AZ::Crc32 singleEventToAction("BaseClass1");
                int singleEventToActionPropertyIndex = classElement.FindElement(singleEventToAction);
                if (singleEventToActionPropertyIndex != -1)
                {
                    auto& baseClass = classElement.GetSubElement(singleEventToActionPropertyIndex);
                    if (baseClass.GetVersion() == 2)
                    {
                        baseClass.AddElementWithData(context, "Dead Zone", currentDeadZone);
                    }
                    ++currentUpgradedVersion;
                }
                else
                {
                    AZ_Warning("Input", false, "Unable to convert Analog from version 1 to 2, its data will be lost on save");
                    return false;
                }
            }
            case s_analogVersion:
            {
                return true;
            }
            default:
                AZ_Warning("Input", false, "Unable to convert Analog: unsupported version %i, its data will be lost on save", currentUpgradedVersion);
                return false;
            }
        }
        return true;
    }

    void Analog::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<Analog, SingleEventToAction>()
                ->Version(s_analogVersion, &ConvertAnalogVersion)
                ->Field("Send Continuous Updates", &Analog::m_shouldSendContinuousUpdates);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<Analog>("Analog", "Analog Input Events like mouse and controller thumb sticks")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &Analog::GetEditorText)
                    ->DataElement(0, &Analog::m_shouldSendContinuousUpdates, "Send continuous updates", "When false you will only get a new message when the analog value has changed")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"));
            }
        }
    }

    void Analog::Activate(const AZ::GameplayNotificationId& channelId)
    {
        AZ::Crc32 inputNameCrc(m_inputName.c_str());
        m_eventBusId = channelId;
        m_lastKnownInputValue = 0.f;
        AZ::InputNotificationId inputBusId(channelId.m_channel, inputNameCrc);
        AZ::InputNotificationBus::MultiHandler::BusConnect(inputBusId);
    }

    void Analog::Deactivate(const AZ::GameplayNotificationId& channelId)
    {
        AZ::Crc32 inputNameCrc(m_inputName.c_str());

        AZ::InputNotificationId inputBusId(channelId.m_channel, inputNameCrc);
        AZ::InputNotificationBus::MultiHandler::BusDisconnect(inputBusId);
        
        AZ::TickBus::Handler::BusDisconnect();
    }
    
    void Analog::OnNotifyInputEvent(const SInputEvent& completeInputEvent)
    {
        m_lastKnownInputValue = completeInputEvent.value;

        if (!m_shouldSendContinuousUpdates)
        {
            TrySendSuccessEvent();
        }
        else if (!AZ::TickBus::Handler::BusIsConnected())
        {
            // if we've received an event and we want continuous updates, connect to the tick bus
            AZ::TickBus::Handler::BusConnect();
        }
        TrySendFailEvent();
    }

    void Analog::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        TrySendSuccessEvent();
    }

    void Analog::TrySendSuccessEvent() const
    {
        if (fabs(m_lastKnownInputValue) > m_deadZone)
        {
            EBUS_EVENT_ID(m_eventBusId, AZ::GameplayNotificationBus<float>, OnGameplayEventAction, m_lastKnownInputValue * m_eventValueMultiplier);
        }
    }

    void Analog::TrySendFailEvent() const
    {
        if (!(fabs(m_lastKnownInputValue) > m_deadZone))
        {
            EBUS_EVENT_ID(m_eventBusId, AZ::GameplayNotificationBus<float>, OnGameplayEventFailed);
        }
    }

    AZStd::string Analog::GetEditorText() const
    {
        return AZStd::string::format("Analog (%s) %s", this->SingleEventToAction::GetEditorText().c_str(), m_shouldSendContinuousUpdates ? "continuous" : "");
    }

} // namespace Input
