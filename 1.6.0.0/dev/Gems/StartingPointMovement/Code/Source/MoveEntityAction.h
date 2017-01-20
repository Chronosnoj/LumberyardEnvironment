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
#include "IEventActionHandler.h"
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Component/EntityId.h>

namespace Movement
{
    //////////////////////////////////////////////////////////////////////////
    /// This will move an entity either along its relative axis or along world
    /// space axis
    //////////////////////////////////////////////////////////////////////////
    class MoveEntityAction
        : public IEventActionHandler<AZ::Vector3>
    {
    public:
        ~MoveEntityAction() override = default;
        AZ_RTTI(MoveEntityAction, "{1BCB0638-7D8F-40E1-8004-199993BA6116}", IEventActionHandler<AZ::Vector3>);
        static void Reflect(AZ::ReflectContext* reflection);

        //////////////////////////////////////////////////////////////////////////
        // AZ::GameplayNotificationBus::Handler
        void OnGameplayEventAction(const AZ::Vector3& value) override;
        void OnGameplayEventFailed() override {}

        //////////////////////////////////////////////////////////////////////////
        // IEventActionHandler
        void Init() override {}
        void Activate(const AZ::EntityId& entityIdToMove, const AZ::EntityId& channelId) override;
        void Deactivate(const AZ::EntityId& channelId) override;

    private:
        //////////////////////////////////////////////////////////////////////////
        // Reflected Data
        float m_xScale = 1.f;
        float m_yScale = 1.f;
        float m_zScale = 1.f;
        AZStd::string m_eventName = "";
        bool m_shouldIgnorePhysics = false;
        bool m_isRelative = true;

        //////////////////////////////////////////////////////////////////////////
        // Private Data
        AZ::Crc32 m_eventNameCrc = AZ::Crc32();
        AZ::EntityId m_entityId;
    };
} //namespace Movement