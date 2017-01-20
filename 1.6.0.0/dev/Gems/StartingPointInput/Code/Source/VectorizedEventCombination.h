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
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/array.h>
#include "StartingPointInput/BaseDataTypes.h"

namespace AZ
{
    class ReflectContext;
}

namespace Input
{
    //////////////////////////////////////////////////////////////////////////
    /// This class binds 3 incoming action values to an AZ::Vector3 and sends
    /// out a new action event containing that AZ::Vector3
    /// Bind to this action by inheriting AZ::GameplayNotificationBus<AZ::Vector3>::Handler
    /// and connecting to the bus
    //////////////////////////////////////////////////////////////////////////
    class VectorizedEventCombination
        : public AZ::GameplayNotificationBus<float>::MultiHandler
        , public InputSubComponent
    {
    public:
        ~VectorizedEventCombination() override = default;
        AZ_RTTI(VectorizedEventCombination, "{2BAB18ED-7CA6-4147-B857-9523E653B2A5}", InputSubComponent);
        static void Reflect(AZ::ReflectContext* reflection);

        //////////////////////////////////////////////////////////////////////////
        // InputSubComponent
        void Activate(const AZ::GameplayNotificationId& channelId) override;
        void Deactivate(const AZ::GameplayNotificationId& channelId) override;

    private:
        static const int s_numberOfFieldsInAVector3 = 3;

        AZStd::string GetEditorText() const;
        //////////////////////////////////////////////////////////////////////////
        // AZ::GameplayNotificationBus<float>
        void OnGameplayEventAction(const float& value) override;
        void OnGameplayEventFailed() override;

        //////////////////////////////////////////////////////////////////////////
        // Reflected data
        AZStd::array<AZStd::string, s_numberOfFieldsInAVector3> m_incomingEventNames;
        bool m_shouldNormalize = false;
        float m_deadZoneLength = AZ::g_fltEps;

        //////////////////////////////////////////////////////////////////////////
        // internal data
        AZ::Vector3 m_aggregatedEventValue = AZ::Vector3::CreateZero();
        AZStd::unordered_map<AZ::Crc32, int> m_eventCrcToIndex;
        AZ::GameplayNotificationId m_eventBusId;
    };
} //namespace Input