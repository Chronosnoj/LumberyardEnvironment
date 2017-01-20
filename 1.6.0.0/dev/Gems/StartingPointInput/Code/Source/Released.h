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
#include <AzCore/Script/ScriptTimePoint.h>

namespace AZ
{
    class ReflectContext;
}

namespace Input
{
    //////////////////////////////////////////////////////////////////////////
    /// Released will be Complete when the registered InputId is released
    class Released
        : public InputSubComponent
        , public AZ::InputNotificationBus::Handler
        , public SingleEventToAction
    {
    public:
        ~Released() override = default;
        AZ_RTTI(Released, "{8280FA20-7171-41BA-ACA1-4F79190DD60F}", InputSubComponent, SingleEventToAction);

        static void Reflect(AZ::ReflectContext* reflection);
        //////////////////////////////////////////////////////////////////////////
        // IInputSubComponent
        void OnNotifyInputEvent(const SInputEvent& completeInputEvent) override;
        void Activate(const AZ::GameplayNotificationId& channelId) override;
        void Deactivate(const AZ::GameplayNotificationId& channelId) override;

    private:
        AZStd::string GetEditorText() const;

        //////////////////////////////////////////////////////////////////////////
        // Reflected Data
        float m_maximumTimeBeforeSuppressed = 1000.f;

        //////////////////////////////////////////////////////////////////////////
        // Private Data
        float m_highestEventValueSeen = 0.f;
        bool m_previousAnalogValueAboveThreshold = false;
        AZ::ScriptTimePoint m_inputDownTime;
    };
} // namespace LYGame