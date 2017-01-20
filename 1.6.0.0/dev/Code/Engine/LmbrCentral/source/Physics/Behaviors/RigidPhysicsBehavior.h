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

#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Component/EntityBus.h>
#include "PhysicsBehavior.h"

namespace LmbrCentral
{
    struct RigidPhysicsConfiguration
    {
        AZ_CLASS_ALLOCATOR(RigidPhysicsConfiguration, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(RigidPhysicsConfiguration, "{4D4211C2-4539-444F-A8AC-B0C8417AA579}");
        virtual ~RigidPhysicsConfiguration() {}
        static void Reflect(AZ::ReflectContext* context);

        //! Whether total mass is specified, or calculated at spawn time based on density and volume
        enum class MassOrDensity : uint32_t
        {
            Mass,
            Density,
        };

        bool UseMass() const
        {
            return m_specifyMassOrDensity == MassOrDensity::Mass;
        }

        bool UseDensity() const
        {
            return m_specifyMassOrDensity == MassOrDensity::Density;
        }

        //! Whether the entity is initially enabled in the physics simulation.
        //! Entities can be enabled later via PhysicsComponentRequests::Enable(true).
        bool m_enabledInitially = true;

        //! Whether total mass is specified, or calculated at spawn time based on density and volume
        MassOrDensity m_specifyMassOrDensity = MassOrDensity::Mass;

        //! Total mass of entity, in kg
        float m_mass = 10.f; // 10kg would be a cube of water with 10cm sides.

        //! Density of the entity, in kg per cubic meter.
        //! The total mass of entity will be calculated at spawn,
        //! based on density and the volume of its geometry.
        //! Mass = Density * Volume
        float m_density = 500.f; // 1000 kg/m^3 is density of water. Oak is 600 kg/m^3.

        //! Whether the entity is initially considered at-rest in the physics simulation.
        //! True: Entity remains still until agitated.
        //! False: Entity will be fall if nothing is below it.
        bool m_atRestInitially = true;

        //! Damping value applied while in water.
        float m_buoyancyDamping = 0.f;

        //! Water density strength.
        float m_buoyancyDensity = 1.f;

        //! Water resistance strength.
        float m_buoyancyResistance = 1.f;

        //! Movement damping.
        float m_simulationDamping = 0.f;

        //! Minimum energy under which object will sleep.
        float m_simulationMinEnergy = 0.002f;
    };

    /*!
     * Physics behavior for a rigid movable object.
     */
    class RigidPhysicsBehavior
        : public CryPhysicsBehavior
        , public AZ::EntityBus::Handler
    {
    public:
        AZ_CLASS_ALLOCATOR(RigidPhysicsBehavior, AZ::SystemAllocator, 0);
        AZ_RTTI(RigidPhysicsBehavior, "{48366EA0-71FA-49CA-B566-426EE897468E}", CryPhysicsBehavior);
        static void Reflect(AZ::ReflectContext* context);

        ~RigidPhysicsBehavior() override = default;

        ////////////////////////////////////////////////////////////////////////
        // PhysicsBehavior
        void InitPhysicsBehavior(AZ::EntityId entityId) override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // CryPhysicsBehavior
        pe_type GetPhysicsType() const override { return PE_RIGID; }
        bool ConfigurePhysicalEntity(IPhysicalEntity& physicalEntity) override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZ::EntityBus
        void OnEntityActivated(AZ::Entity* entity) override;
        ////////////////////////////////////////////////////////////////////////

    protected:

        RigidPhysicsConfiguration m_configuration;

        AZ::EntityId m_entityId;
    };
} // namespace LmbrCentral
