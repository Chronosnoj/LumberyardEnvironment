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
#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/EntityId.h>

namespace Camera
{
    //////////////////////////////////////////////////////////////////////////
    /// This bus allows you to register for camera targets being added and
    /// removed in the future
    //////////////////////////////////////////////////////////////////////////
    class CameraTargetNotifications
        : public AZ::EBusTraits
    {
    public:
        /// Handlers will be notified via this message when a new camera target has been added
        virtual void OnCameraTargetAdded(AZ::EntityId entityTargetAdded, AZ::Crc32 tagCrC) = 0;

        /// Handlers will be notified via this message when character has been removed
        virtual void OnCameraTargetRemoved(AZ::EntityId entityTargetAdded, AZ::Crc32 tagCrC) = 0;
    };
    using CameraTargetNotificationBus = AZ::EBus<CameraTargetNotifications>;

    //////////////////////////////////////////////////////////////////////////
    /// This defines an entry for a camera target
    //////////////////////////////////////////////////////////////////////////
    struct CameraTargetEntry
    {
        CameraTargetEntry(AZ::EntityId entityId, AZ::Crc32 tagCrc)
            : m_entityId(entityId)
            , m_tagCrc(tagCrc)
        {}
        AZ::EntityId m_entityId;
        AZ::Crc32 m_tagCrc;
    };

    using CameraTargetList = AZStd::vector<CameraTargetEntry>;
    //////////////////////////////////////////////////////////////////////////
    /// This bus will provide a list of all CameraTargets with their tag CRCs
    /// that are active
    /// Requests serviced simultaneously by *all* CameraTargetRequest handlers.
    //////////////////////////////////////////////////////////////////////////
    class CameraTargetRequests
        : public AZ::EBusTraits
    {
    public:
        /// Fills targetList with all camera targets. A camera target is any entity with a CameraTargetComponent.
        virtual void RequestAllCameraTargets(CameraTargetList& targetList) = 0;
    };
    using CameraTargetRequestBus = AZ::EBus<CameraTargetRequests>;
} //namespace Camera