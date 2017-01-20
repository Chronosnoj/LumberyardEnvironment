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
#include <AzCore/Component/TickBus.h>
#include "StartingPointInput/BaseDataTypes.h"

namespace AZ
{
    class ReflectContext;
}

namespace Input
{
    //////////////////////////////////////////////////////////////////////////
    /// Use this for analog input events like mouse and controller thumb sticks
    class Analog
        : public InputSubComponent
        , public AZ::InputNotificationBus::MultiHandler
        , public AZ::TickBus::Handler
        , public SingleEventToAction
    {
    public:
        ~Analog() override = default;
        AZ_RTTI(Analog, "{806F21D9-11EA-47FC-8B89-FDB67AADE4FF}", InputSubComponent, SingleEventToAction);
        static void Reflect(AZ::ReflectContext* reflection);
        //////////////////////////////////////////////////////////////////////////
        // IInputSubComponent
        void Activate(const AZ::GameplayNotificationId& channelId) override;
        void Deactivate(const AZ::GameplayNotificationId& channelId) override;

    protected:

        AZStd::string GetEditorText() const;

        void TrySendSuccessEvent() const;
        void TrySendFailEvent() const;

        //////////////////////////////////////////////////////////////////////////
        // AZ::InputNotificationBus::MultiHandler
        void OnNotifyInputEvent(const SInputEvent& completeInputEvent) override;

        //////////////////////////////////////////////////////////////////////////
        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

    private:
        //////////////////////////////////////////////////////////////////////////
        // Reflected Data
        bool m_shouldSendContinuousUpdates = true;

        //////////////////////////////////////////////////////////////////////////
        // Private Data
        float m_lastKnownInputValue = 0.f;
    };
} // namespace LYGame