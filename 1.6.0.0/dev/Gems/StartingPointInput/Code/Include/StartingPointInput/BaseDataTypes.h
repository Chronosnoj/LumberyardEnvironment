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
#pragma once

#include <InputManagementFramework/InputSubComponent.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/EBus/EBus.h>

// CryCommon includes
#include <InputRequestBus.h>
#include <GameplayEventBus.h>

namespace Input
{
    struct SingleEventToAction
    {
        virtual ~SingleEventToAction() = default;
        AZ_RTTI(SingleEventToAction, "{2C93824D-D011-459C-B12B-9F4A6148730C}");

        //////////////////////////////////////////////////////////////////////////
        // Non Reflected Data
        AZStd::vector<AZStd::string> m_availableInputsByDevice;
        AZ::GameplayNotificationId m_eventBusId;

        //////////////////////////////////////////////////////////////////////////
        // Reflected Data
        float m_eventValueMultiplier = 1.f;
        AZStd::string m_inputName = "";
        AZStd::string m_inputDeviceType = "";
        float m_deadZone = 0.2f;

        static void Reflect(AZ::ReflectContext* reflection)
        {
            if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection))
            {
                serializeContext->Class<SingleEventToAction>()
                    ->Version(2)
                    ->Field("Input Device Type", &SingleEventToAction::m_inputDeviceType)
                    ->Field("Input Name", &SingleEventToAction::m_inputName)
                    ->Field("Event Value Multiplier", &SingleEventToAction::m_eventValueMultiplier)
                    ->Field("Dead Zone", &SingleEventToAction::m_deadZone);

                if (AZ::EditContext* editContext = serializeContext->GetEditContext())
                {
                    auto && classInfo = editContext->Class<SingleEventToAction>("SingleEventToAction", "Maps a single event to a single action")
                            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                                ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &SingleEventToAction::GetEditorText)
                                ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"))
                                ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly"))
                            ->DataElement(AZ::Edit::UIHandlers::ComboBox, &SingleEventToAction::m_inputDeviceType, "Input Device Type", "The type of input device, ex keyboard")
                                ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshEntireTree"))
                                ->Attribute(AZ::Edit::Attributes::StringList, &SingleEventToAction::GetInputDeviceTypes)
                            ->DataElement(AZ::Edit::UIHandlers::ComboBox, &SingleEventToAction::m_inputName, "Input Name", "The name of the input you want to hold ex. space")
                                ->Attribute(AZ::Edit::Attributes::StringList, &SingleEventToAction::GetInputNamesBySelectedDevice)
                                ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"))
                            ->DataElement(0, &SingleEventToAction::m_eventValueMultiplier, "Event value multiplier", "When the event fires, the value will be scaled by this multiplier")
                                ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"))
                            ->DataElement(0, &SingleEventToAction::m_deadZone, "Dead zone", "An event will only be sent out if the value is above this threshold")
                                ->Attribute(AZ::Edit::Attributes::Min, 0.001f);
                }
            }
        }

        AZStd::string GetEditorText() const
        {
            return m_inputName.empty() ? "<Select input>" : m_inputName;
        }
    private:

        const AZStd::vector<AZStd::string> GetInputDeviceTypes()
        {
            const AZStd::vector<AZStd::string>* retval;
            EBUS_EVENT_RESULT(retval, AZ::InputRequestsBus, GetRegisteredDeviceList);
            return *retval;
        }

        const AZStd::vector<AZStd::string> GetInputNamesBySelectedDevice()
        {
            const AZStd::vector<AZStd::string>* retval;
            EBUS_EVENT_RESULT(retval, AZ::InputRequestsBus, GetInputListByDevice, m_inputDeviceType);
            return *retval;
        }
    };
} //namespace Input