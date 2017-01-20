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
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Quaternion.h>

namespace LmbrCentral
{
    /*!
     * Events (concerning AZ::Entities) broadcast by the physics system.
     * Events are broadcast on the ID of the entity involved in the event.
     */
    class EntityPhysicsEvents
        : public AZ::EBusTraits
    {
    public:
        ////////////////////////////////////////////////////////////////////////
        // EBusTraits
        // multiple handlers
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        // addressed by entity id
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;
        ////////////////////////////////////////////////////////////////////////

        //! Data for OnCollision event
        struct Collision
        {
            AZ::EntityId m_entity; //!< ID of Entity involved in event
            AZ::EntityId m_otherEntity; //!< ID of other Entity involved in collision
            AZ::Vector3 m_location; //!< Contact point in world coordinates
        };

        //! Data for OnPostStep event
        struct PostStep
        {
            AZ::EntityId m_entity; //!< ID of Entity involved in event
            AZ::Vector3 m_entityPosition; //!< Position of Entity
            AZ::Quaternion m_entityRotation; //!< Rotation of Entity
            float m_stepTimeDelta; //!< Time interval used in this physics step
            int m_stepId; //!< The world's internal step count
        };

        virtual ~EntityPhysicsEvents() {}

        //! Two AZ::Entities have collided.
        //! An event is sent to the address of each EntityId involved in the collision.
        virtual void OnCollision(const EntityPhysicsEvents::Collision& event) {}

        //! The physics system has moved this Entity.
        virtual void OnPostStep(const EntityPhysicsEvents::PostStep& event) {}
    };

    using EntityPhysicsEventBus = AZ::EBus<EntityPhysicsEvents>;

    /*!
     * Requests for the physics system
     */
    class PhysicsSystemRequests
        : public AZ::EBusTraits
    {
    public:
        ////////////////////////////////////////////////////////////////////////
        // EBusTraits
        // singleton pattern
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        ////////////////////////////////////////////////////////////////////////

        virtual ~PhysicsSystemRequests() {}

        virtual void PrePhysicsUpdate() = 0;
        virtual void PostPhysicsUpdate() = 0;
    };

    using PhysicsSystemRequestBus = AZ::EBus<PhysicsSystemRequests>;

    /*!
     * Broadcast physics system events.
     */
    class PhysicsSystemEvents
        : public AZ::EBusTraits
    {
    public:
        virtual ~PhysicsSystemEvents() {}

        virtual void OnPrePhysicsUpdate() {};
        virtual void OnPostPhysicsUpdate() {};
    };

    using PhysicsSystemEventBus = AZ::EBus<PhysicsSystemEvents>;

} // namespace LmbrCentral
