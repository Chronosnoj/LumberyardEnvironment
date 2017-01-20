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
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/containers/vector.h>
#include <InputManagementFramework/InputSubComponent.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/containers/unordered_map.h>
#include "StartingPointInput/BaseDataTypes.h"

namespace AZ
{
    class ReflectContext;
}

namespace Input
{
    //////////////////////////////////////////////////////////////////////////
    /// Use UnorderedEventCombination when you need all of the events to pass but
    /// but the order is not important.
    //////////////////////////////////////////////////////////////////////////
    class UnorderedEventCombination
        : public AZ::GameplayNotificationBus<float>::MultiHandler
        , public InputSubComponent
        , public AZ::TickBus::Handler
    {
    public:
        ~UnorderedEventCombination() override = default;
        AZ_RTTI(UnorderedEventCombination, "{A1A67F18-C395-45E0-9648-BE01321EB41A}", InputSubComponent);
        static void Reflect(AZ::ReflectContext* reflection);

        //////////////////////////////////////////////////////////////////////////
        // IInputSubComponent
        void Activate(const AZ::GameplayNotificationId& channelId) override;
        void Deactivate(const AZ::GameplayNotificationId& channelId) override;

    protected:
        AZStd::string GetEditorText() const;
        void FailInternal();
        void SucceedInternal(float value);
        void ResetActiveState();

        //////////////////////////////////////////////////////////////////////////
        // AZ::GameplayNotificationBus<float>
        void OnGameplayEventAction(const float& value) override;
        void OnGameplayEventFailed() override {};

        //////////////////////////////////////////////////////////////////////////
        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        //////////////////////////////////////////////////////////////////////////
        // Reflected data
        AZStd::vector<AZStd::string> m_incomingEventNames;
        float m_maximumTimeAllowedForAllEvents = 0.5f;

        //////////////////////////////////////////////////////////////////////////
        // internal data
        AZStd::unordered_map<AZ::Crc32, float> m_eventNameToLastActivatedTime;
        AZStd::unordered_map<AZ::Crc32, bool> m_eventIsActive;
        bool m_isAttemptingCombination = false;
        float m_currentTimeInSeconds = 0.f;
        AZ::GameplayNotificationId m_eventBusId;
    };
} // namespace Input