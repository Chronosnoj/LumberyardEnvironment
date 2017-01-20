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
#include <AzCore/std/smart_ptr/weak_ptr.h>

struct IPhysicalEntity;
struct pe_params;
struct pe_action;
struct pe_status;

namespace LmbrCentral
{
    /*!
     * Messages serviced by the in-game physics component.
     */
    class PhysicsComponentRequests
        : public AZ::ComponentBus
    {
    public:

        virtual ~PhysicsComponentRequests() {}

        //! Make the entity a participant in the physics simulation.
        virtual void EnablePhysics() = 0;

        //! Stop the entity from participating in the physics simulation
        virtual void DisablePhysics() = 0;

        //! Is Physics enabled on this entity?
        virtual bool IsPhysicsEnabled() = 0;
    };
    using PhysicsComponentRequestBus = AZ::EBus<PhysicsComponentRequests>;

    /*!
     * Messages (specific to CryPhysics) serviced by the in-game physics component.
     */
    class CryPhysicsComponentRequests
        : public AZ::ComponentBus
    {
    public:

        virtual ~CryPhysicsComponentRequests() {}

        virtual IPhysicalEntity* GetPhysicalEntity() = 0;
        virtual void GetPhysicsParameters(pe_params& outParameters) = 0;
        virtual void SetPhysicsParameters(const pe_params& parameters) = 0;
        virtual void GetPhysicsStatus(pe_status& outStatus) = 0;
        virtual void ApplyPhysicsAction(const pe_action& action, bool threadSafe) = 0;
    };
    using CryPhysicsComponentRequestBus = AZ::EBus<CryPhysicsComponentRequests>;

    /*!
    * Events emitted by a physics component.
    * For events that arise from interactions with the physics system
    * (eg: OnCollision) check out the EntityPhysicsEventBus.
    * 
    * Note: OnPhysicsEnabled will fire immediately upon connecting to the bus if physics is enabled
    */
    class PhysicsComponentNotifications
        : public AZ::ComponentBus
    {
    public:
        template<class Bus>
        struct PhysicsComponentNotificationsConnectionPolicy
            : public AZ::EBusConnectionPolicy<Bus>
        {
            static void Connect(typename Bus::BusPtr& busPtr, typename Bus::Context& context, typename Bus::HandlerNode& handler, const typename Bus::BusIdType& id = 0)
            {
                AZ::EBusConnectionPolicy<Bus>::Connect(busPtr, context, handler, id);

                bool physicsEnabled = false;
                EBUS_EVENT_ID_RESULT(physicsEnabled, id, PhysicsComponentRequestBus, IsPhysicsEnabled);
                if (physicsEnabled)
                {
                    typename Bus::template CallstackEntryIterator<typename Bus::InterfaceType**> callstack(nullptr, busPtr.get()); // Workaround for GetCurrentBusId in callee
                    handler->OnPhysicsEnabled();
                }
            }
        };

        template<class Bus>
        using ConnectionPolicy = PhysicsComponentNotificationsConnectionPolicy<Bus>;

        virtual ~PhysicsComponentNotifications() {}

        /// The entity has begun participating in the physics simulation.
        /// If the entity is active when a handler connects to the bus,
        /// then OnPhysicsEnabled() is immediately dispatched.
        virtual void OnPhysicsEnabled() = 0;

        //! The entity is stopping participation in the physics simulation.
        virtual void OnPhysicsDisabled() = 0;
    };
    using PhysicsComponentNotificationBus = AZ::EBus<PhysicsComponentNotifications>;
} // namespace LmbrCentral
