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
#include <AzCore/Component/TickBus.h>

namespace AZ
{
    class ReflectContext;
}

namespace Input
{
    enum class HeldInvokeType
    {
        EveryDuration,
        OncePerRelease,
        EveryFrameAfterDuration,
    };

    //////////////////////////////////////////////////////////////////////////
    /// Held will be Complete when the registered InputId is held for Duration seconds
    class Held
        : public InputSubComponent
        , public AZ::InputNotificationBus::Handler
        , public SingleEventToAction
        , private AZ::TickBus::Handler
    {
    public:
        ~Held() override = default;
        AZ_RTTI(Held, "{A3F1D51B-3473-49D6-9131-98D1FBA9003A}", InputSubComponent, SingleEventToAction);
        static void Reflect(AZ::ReflectContext* reflection);

        //////////////////////////////////////////////////////////////////////////
        // IInputSubComponent
        void Activate(const AZ::GameplayNotificationId& channelId) override;
        void Deactivate(const AZ::GameplayNotificationId& channelId) override;

    protected:

        AZStd::string GetEditorText() const;
        void SendEventIfSuccessful();
        //////////////////////////////////////////////////////////////////////////
        // AZ::InputNotificationBus::Handler
        void OnNotifyInputEvent(const SInputEvent& completeInputEvent) override;

        //////////////////////////////////////////////////////////////////////////
        // AZ::Tickbus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        //////////////////////////////////////////////////////////////////////////
        // Reflected Data
        float m_durationToHold = 0.01f;
        float m_successPulseInterval = 0.01f;
        HeldInvokeType m_invokeType = HeldInvokeType::OncePerRelease;

        //////////////////////////////////////////////////////////////////////////
        // Private Data
        bool m_isReady = false;
        float m_successPulseCount = 0.f;
        AZ::ScriptTimePoint m_timeOfPress;
        float m_lastKnownEventValue = 0.f;
    };
} // namespace LYGame