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
#include "Pressed.h"
#include "AzCore/EBus/EBus.h"
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>

namespace Input
{
    void Pressed::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<Pressed, SingleEventToAction>()
                ->Version(1)
                ->SerializerForEmptyClass();

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<Pressed>("Pressed", "Press an input to generate an event")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &Pressed::GetEditorText)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"));
            }
        }
    }

    void Pressed::OnNotifyInputEvent(const SInputEvent& completeInputEvent)
    {
        bool isAnalogPressed = false;
        if (completeInputEvent.state == eIS_Changed)
        {
            bool aboveThreshold = completeInputEvent.value > m_deadZone;
            if (aboveThreshold && m_previousAnalogValueBelowThreshold)
            {
                isAnalogPressed = true;
            }
            m_previousAnalogValueBelowThreshold = !aboveThreshold;
        }

        if (completeInputEvent.state == eIS_Pressed || isAnalogPressed)
        {
            EBUS_EVENT_ID(m_eventBusId, AZ::GameplayNotificationBus<float>, OnGameplayEventAction, completeInputEvent.value * m_eventValueMultiplier);
        }
        else
        {
            EBUS_EVENT_ID(m_eventBusId, AZ::GameplayNotificationBus<float>, OnGameplayEventFailed);
        }
    }

    void Pressed::Activate(const AZ::GameplayNotificationId& channelId)
    {
        AZ::Crc32 inputNameCrc(m_inputName.c_str());
        m_eventBusId = channelId;

        AZ::InputNotificationId inputBusId(channelId.m_channel, inputNameCrc);
        AZ::InputNotificationBus::MultiHandler::BusConnect(inputBusId);
    }

    void Pressed::Deactivate(const AZ::GameplayNotificationId& channelId)
    {
        AZ::Crc32 inputNameCrc(m_inputName.c_str());

        AZ::InputNotificationId inputBusId(channelId.m_channel, inputNameCrc);
        AZ::InputNotificationBus::MultiHandler::BusDisconnect(inputBusId);
    }

    AZStd::string Pressed::GetEditorText() const
    {
        return AZStd::string::format("Pressed (%s)", this->SingleEventToAction::GetEditorText().c_str());
    }
} // namespace Input
