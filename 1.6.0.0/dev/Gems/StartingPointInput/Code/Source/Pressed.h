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
#include <AzCore/RTTI/RTTI.h>
#include "StartingPointInput/BaseDataTypes.h"

namespace AZ
{
    class ReflectContext;
}

namespace Input
{
    //////////////////////////////////////////////////////////////////////////
    /// Pressed will be Complete when the registered InputId is pressed
    class Pressed
        : public InputSubComponent
        , public AZ::InputNotificationBus::MultiHandler
        , public SingleEventToAction
    {
    public:
        ~Pressed() override = default;
        AZ_RTTI(Pressed, "{8C6868E9-CBFF-4B42-AC72-FD22421F9C6A}", InputSubComponent, SingleEventToAction);

        static void Reflect(AZ::ReflectContext* reflection);
        //////////////////////////////////////////////////////////////////////////
        // IInputSubComponent
        void OnNotifyInputEvent(const SInputEvent& completeInputEvent) override;
        void Activate(const AZ::GameplayNotificationId& channelId) override;
        void Deactivate(const AZ::GameplayNotificationId& channelId) override;

    private:
        AZStd::string GetEditorText() const;
        bool m_previousAnalogValueBelowThreshold = true;
    };
} // namespace LYGame