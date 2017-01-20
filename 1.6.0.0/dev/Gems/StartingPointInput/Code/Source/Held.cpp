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
#include "Held.h"
#include <AzCore/EBus/EBus.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Component/TickBus.h>

namespace Input
{
    static const int s_heldVersion = 2;
    bool ConvertHeldVersion(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
    {
        int currentUpgradedVersion = classElement.GetVersion();
        while (currentUpgradedVersion != s_heldVersion)
        {
            switch (currentUpgradedVersion)
            {
            case 1:
            {
                AZ::Crc32 invokeOnceCrc("Invoke Once Per Release");
                int invokeOncePropertyIndex = classElement.FindElement(invokeOnceCrc);
                if (invokeOncePropertyIndex != -1)
                {
                    bool invokeOncePerRelease = true;
                    classElement.GetSubElement(invokeOncePropertyIndex).GetData(invokeOncePerRelease);
                    classElement.ReplaceElement<HeldInvokeType>(context, invokeOncePropertyIndex, "Invoke Type");
                    HeldInvokeType invokeType = HeldInvokeType::OncePerRelease;
                    if (!invokeOncePerRelease)
                    {
                        float holdDuration = 0.1f;
                        classElement.GetChildData(AZ::Crc32("Duration To Hold"), holdDuration);
                        classElement.AddElementWithData(context, "Success Pulse Interval", holdDuration);
                        invokeType = HeldInvokeType::EveryDuration;
                    }
                    classElement.GetSubElement(invokeOncePropertyIndex).SetData(context, invokeType);
                    ++currentUpgradedVersion;
                }
                else
                {
                    AZ_Warning("Input", false, "Unable to convert Held from version 1 to 2, its data will be lost on save");
                    return false;
                }
            }
            case s_heldVersion:
            {
                return true;
            }
            default:
                AZ_Warning("Input", false, "Unable to convert Held: unsupported version %i, its data will be lost on save", currentUpgradedVersion);
                return false;
            }
        }
        return true;
    }

    void Held::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<Held, SingleEventToAction>()
                ->Version(s_heldVersion, &ConvertHeldVersion)
                ->Field("Duration To Hold", &Held::m_durationToHold)
                ->Field("Success Pulse Interval", &Held::m_successPulseInterval)
                ->Field("Invoke Type", &Held::m_invokeType);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<Held>("Held", "Hold an input to generate an event")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &Held::GetEditorText)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &Held::m_durationToHold, "Duration to hold", "The duration in seconds that the input must be held for")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &Held::m_successPulseInterval, "Success pulse interval", "The interval between successful messages after holding for \"duration to hold\" seconds")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.01f)
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &Held::m_invokeType, "Invoke type", "Select when you want the events to be invoked")
                        ->EnumAttribute(HeldInvokeType::EveryDuration, "Every duration")
                        ->EnumAttribute(HeldInvokeType::OncePerRelease, "Once per release")
                        ->EnumAttribute(HeldInvokeType::EveryFrameAfterDuration, "Every frame after duration");
            }
        }
    }

    void Held::OnNotifyInputEvent(const SInputEvent& completeInputEvent)
    {
        bool initiateAnalogHeld = false;
        bool sendAnalogFailEvent = false;
        if (completeInputEvent.state == eIS_Changed)
        {
            bool isBeingHeld = completeInputEvent.value > m_deadZone;
            bool previousAnalogValueHeld = m_lastKnownEventValue > m_deadZone;
            m_lastKnownEventValue = completeInputEvent.value;
            if (isBeingHeld && previousAnalogValueHeld)
            {
                initiateAnalogHeld = true;
                AZ::TickBus::Handler::BusConnect();
            }
            else if (!isBeingHeld && previousAnalogValueHeld)
            {
                AZ::TickBus::Handler::BusDisconnect();
                sendAnalogFailEvent = true;
            }
        }

        if (completeInputEvent.state == eIS_Pressed)
        {
            EBUS_EVENT_RESULT(m_timeOfPress, AZ::TickRequestBus, GetTimeAtCurrentTick);
            m_lastKnownEventValue = completeInputEvent.value;
        }
        else if (completeInputEvent.state == eIS_Down)
        {
            m_lastKnownEventValue = completeInputEvent.value;
            SendEventIfSuccessful();
        }
        else if (completeInputEvent.state == eIS_Released || sendAnalogFailEvent)
        {
            m_isReady = true;
            m_successPulseCount = 0.f;
            EBUS_EVENT_ID(m_eventBusId, AZ::GameplayNotificationBus<float>, OnGameplayEventFailed);
        }
    }

    void Held::SendEventIfSuccessful()
    {
        AZ::ScriptTimePoint now;
        EBUS_EVENT_RESULT(now, AZ::TickRequestBus, GetTimeAtCurrentTick);
        float accruedTime = now.GetSeconds() - m_timeOfPress.GetSeconds();
        bool heldLongEnough = accruedTime >= m_durationToHold + (m_successPulseInterval * m_successPulseCount);
        if (heldLongEnough && m_isReady)
        {
            switch (m_invokeType)
            {
            case HeldInvokeType::EveryDuration:
                break;
            case HeldInvokeType::OncePerRelease:
                m_isReady = false;
                break;
            default:
                break;
            }
            m_successPulseCount = std::floor(1.f + (accruedTime - m_durationToHold) / m_successPulseInterval);
            EBUS_EVENT_ID(m_eventBusId, AZ::GameplayNotificationBus<float>, OnGameplayEventAction, m_lastKnownEventValue * m_eventValueMultiplier);
        }
    }

    void Held::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        SendEventIfSuccessful();
    }

    void Held::Activate(const AZ::GameplayNotificationId& channelId)
    {
        AZ::Crc32 inputNameCrc(m_inputName.c_str());
        m_eventBusId = channelId;

        m_isReady = true;
        m_successPulseCount = 0.f;
        AZ::InputNotificationId inputBusId(channelId.m_channel, inputNameCrc);
        AZ::InputNotificationBus::Handler::BusConnect(inputBusId);
    }

    void Held::Deactivate(const AZ::GameplayNotificationId& channelId)
    {
        AZ::Crc32 inputNameCrc(m_inputName.c_str());
        AZ::InputNotificationId inputBusId(channelId.m_channel, inputNameCrc);
        AZ::InputNotificationBus::Handler::BusDisconnect(inputBusId);
    }

    AZStd::string Held::GetEditorText() const
    {
        return AZStd::string::format("Held (%s) %.2fs ", this->SingleEventToAction::GetEditorText().c_str(), m_durationToHold);
    }
} // namespace Input
