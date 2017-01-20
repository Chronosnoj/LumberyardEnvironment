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

#include <AzCore/Component/ComponentBus.h>

#include <AzFramework/Entity/EntityContextBus.h>

namespace LmbrCentral
{
    /*!
     * SpawnerComponentRequests
     * Messages serviced by the SpawnerComponent.
     */
    class SpawnerComponentRequests : public AZ::ComponentBus
    {
    public:
        virtual ~SpawnerComponentRequests() {}

        //! Spawn the selected slice at the entity's location
        virtual AzFramework::SliceInstantiationTicket Spawn() = 0;

        //! Spawn the selected slice at the entity's location with the provided relative offset
        virtual AzFramework::SliceInstantiationTicket SpawnRelative(const AZ::Transform& relative) = 0;

        //! Spawn the provided slice at the entity's location
        virtual AzFramework::SliceInstantiationTicket SpawnSlice(const AZ::Data::Asset<AZ::Data::AssetData>& slice) { (void)slice; return AzFramework::SliceInstantiationTicket(); };

        //! Spawn the provided slice at the entity's location with the provided relative offset
        virtual AzFramework::SliceInstantiationTicket SpawnSliceRelative(const AZ::Data::Asset<AZ::Data::AssetData>& slice, const AZ::Transform& relative) { (void)slice; (void)relative; return AzFramework::SliceInstantiationTicket(); };
    };

    using SpawnerComponentRequestBus = AZ::EBus<SpawnerComponentRequests>;


    /*!
     * SpawnerComponentNotificationBus
     * Events dispatched by the SpawnerComponent
     */
    class SpawnerComponentNotifications : public AZ::ComponentBus
    {
    public:
        virtual ~SpawnerComponentNotifications() {}

         //! Notify that a Spawn has occurred
        virtual void OnSpawned(const AzFramework::SliceInstantiationTicket& ticket, const AZStd::vector<AZ::EntityId>& spawnedEntities) = 0;
    };

    using SpawnerComponentNotificationBus = AZ::EBus<SpawnerComponentNotifications>;

} // namespace LmbrCentral
