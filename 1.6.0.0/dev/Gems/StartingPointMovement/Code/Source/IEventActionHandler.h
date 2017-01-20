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
#include <GameplayEventBus.h>
#include <AzCore/Component/EntityId.h>

namespace Movement
{
    //////////////////////////////////////////////////////////////////////////
    /// Classes that inherit from this one will share the life-cycle API's
    /// with components.  Components that contain the subclasses are expected
    /// to call these methods in their Init/Activate/Deactivate methods
    //////////////////////////////////////////////////////////////////////////
    class IEventActionSubComponent
    {
    public:
        AZ_RTTI(IEventActionSubComponent, "{C0FFD493-E325-4A22-99F0-C49C265E5D32}");
        virtual ~IEventActionSubComponent() = default;

        virtual void Init() = 0;
        virtual void Activate(const AZ::EntityId& entityIdToEffect, const AZ::EntityId& channelId) = 0;
        virtual void Deactivate(const AZ::EntityId& channelId) = 0;
    };

    //////////////////////////////////////////////////////////////////////////
    /// Inherit from this class if you intend to invoke an action when a
    /// particular event is fired and also need the life cycle events of a component
    //////////////////////////////////////////////////////////////////////////
    template <class EventDataType>
    class IEventActionHandler
        : public AZ::GameplayNotificationBus<EventDataType>::Handler
        , public IEventActionSubComponent
    {
    public:
        AZ_RTTI(IEventActionHandler, "{23AAC5D7-4FFF-40C2-989D-1ECF7FAA592B}", IEventActionSubComponent);
        virtual ~IEventActionHandler() = default;

        //////////////////////////////////////////////////////////////////////////
        // AZ::GameplayNotificationBus::Handler
        virtual void OnGameplayEventAction(const EventDataType& value) = 0;
        virtual void OnGameplayEventFailed() = 0;
    };
} // namespace Input