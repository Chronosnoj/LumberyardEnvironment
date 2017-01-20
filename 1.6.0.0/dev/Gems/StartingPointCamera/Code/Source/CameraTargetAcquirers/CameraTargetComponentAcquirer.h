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
#include "StartingPointCamera/CameraTargetBus.h"
#include <AzCore/std/string/string.h>
#include <AzCore/RTTI/RTTI.h>
#include <CameraFramework/ICameraTargetAcquirer.h>
#include <AzCore/Component/Component.h>

namespace AZ
{
    class ReflectContext;
}

namespace Camera
{
    //////////////////////////////////////////////////////////////////////////
    /// This will request Camera targets from the CameraTarget buses.  It will
    /// then return that target's transform when requested by the Camera Rig
    //////////////////////////////////////////////////////////////////////////
    class CameraTargetComponentAcquirer
        : public ICameraTargetAcquirer
        , public CameraTargetNotificationBus::Handler
    {
    public:
        ~CameraTargetComponentAcquirer() override = default;
        AZ_RTTI(CameraTargetComponentAcquirer, "{CF1C04E4-1195-42DD-AF0B-C9F94E80B35D}", ICameraTargetAcquirer)
        static void Reflect(AZ::ReflectContext* reflection);

        //////////////////////////////////////////////////////////////////////////
        // ICameraTargetAcquirer
        bool AcquireTarget(AZ::Transform& outTransformInformation) override;
        void Activate(AZ::EntityId) override;
        void Deactivate() override;

        //////////////////////////////////////////////////////////////////////////
        // CameraTargetNotificationBus
        void OnCameraTargetAdded(AZ::EntityId entityTargetAdded, AZ::Crc32 tagCrc) override;
        void OnCameraTargetRemoved(AZ::EntityId entityTargetAdded, AZ::Crc32 tagCrc) override;

    private:
        //////////////////////////////////////////////////////////////////////////
        // Reflected Data
        AZStd::string m_tagOfSpecificTarget = "";
        bool m_shouldUseTargetRotation = true;
        bool m_shouldUseTargetPosition = true;

        //////////////////////////////////////////////////////////////////////////
        // Internal data
        AZ::EntityId m_currentEntityTarget = AZ::EntityId();
        AZ::Crc32 m_tagCrc = AZ::Crc32();
    };
} //namespace Camera