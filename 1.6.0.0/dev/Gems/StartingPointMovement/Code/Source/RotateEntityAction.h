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
#include "StartingPointMovement/StartingPointMovementConstants.h"

namespace Movement
{
    //////////////////////////////////////////////////////////////////////////
    /// This will move an entity either along its relative axis or along world
    /// space axis
    //////////////////////////////////////////////////////////////////////////
    class RotateEntityAction
        : public IEventActionHandler<float>
    {
    public:
        ~RotateEntityAction() override = default;
        AZ_RTTI(RotateEntityAction, "{7E412C75-AC6F-42EC-A685-4EF5FC28B4FD}", IEventActionHandler<float>);
        static void Reflect(AZ::ReflectContext* reflection);

        //////////////////////////////////////////////////////////////////////////
        // AZ::GameplayNotificationBus::Handler
        void OnGameplayEventAction(const float& value) override;
        void OnGameplayEventFailed() override {}

        //////////////////////////////////////////////////////////////////////////
        // IEventActionHandler
        void Init() override {}
        void Activate(const AZ::EntityId& entityIdToRotate, const AZ::EntityId& channelId) override;
        void Deactivate(const AZ::EntityId& channelId) override;

    private:
        //////////////////////////////////////////////////////////////////////////
        // Reflected Data
        AxisOfRotation m_axisOfRotation = AxisOfRotation::Z_Axis;
        AZStd::string m_eventName = "";
        AZ::EntityId m_entityId;
        float m_rotationSpeedScale = 1.f;
        bool m_shouldInvertAxis = false;
        bool m_isRelative = true;
        bool m_shouldIgnorePhysics = false;
    };
} //namespace Movement