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
    /// The Ordered Event Combination is a combination input event handler that
    /// listens for a list of events and then treats them all as one.  As long
    /// as the events come in order the Outgoing Event will fire.
    /// Ex. "Down" then "Right" then "Heavy Punch" ->  "Heavy Special Attack"
    //////////////////////////////////////////////////////////////////////////
    class OrderedEventCombination
        : public AZ::GameplayNotificationBus<float>::MultiHandler
        , public InputSubComponent
        , public AZ::TickBus::Handler
    {
    public:
        ~OrderedEventCombination() override = default;
        AZ_RTTI(OrderedEventCombination, "{D2D4CF91-392E-4D0E-BD60-67E09981F63D}", InputSubComponent);
        static void Reflect(AZ::ReflectContext* reflection);

        //////////////////////////////////////////////////////////////////////////
        // IInputSubComponent
        void Activate(const AZ::GameplayNotificationId& channelId) override;
        void Deactivate(const AZ::GameplayNotificationId& channelId) override;

    protected:
        AZStd::string GetEditorText() const;
        bool EventIsActive();
        void FailInternal();

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
        float m_maximumDelayBetweenEvents = 0.5f;

        //////////////////////////////////////////////////////////////////////////
        // internal data
        unsigned m_currentIndexOfExpectedEvent = 0;
        float m_currentTimeSinceLastSuccessfulEvent = 0.f;
        AZStd::unordered_map<AZ::Crc32, unsigned> m_eventNameToIndex;
        AZ::GameplayNotificationId m_eventBusId;
    };
} // namespace Input