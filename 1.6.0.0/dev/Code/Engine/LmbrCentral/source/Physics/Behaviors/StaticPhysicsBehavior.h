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
    /*!
     * Configuration data for StaticPhysicsBehavior.
     */
    struct StaticPhysicsConfiguration
    {
        AZ_CLASS_ALLOCATOR(StaticPhysicsConfiguration, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(StaticPhysicsConfiguration, "{E87BB4E0-D771-4477-83C2-02EFE0016EC7}");
        virtual ~StaticPhysicsConfiguration() {}
        static void Reflect(AZ::ReflectContext* context);

        //! Whether the entity is initially enabled in the physics simulation.
        //! Entities can be enabled later via PhysicsComponentRequests::Enable(true).
        bool m_enabledInitially = true;
    };

    /*!
     *  Physics behavior for a solid object that cannot move.
     */
    class StaticPhysicsBehavior
        : public CryPhysicsBehavior
        , public AZ::EntityBus::Handler
    {
    public:
        AZ_CLASS_ALLOCATOR(StaticPhysicsBehavior, AZ::SystemAllocator, 0);
        AZ_RTTI(StaticPhysicsBehavior, "{BC0600CC-5EF5-4753-A8BE-E28194149CA5}", CryPhysicsBehavior);
        static void Reflect(AZ::ReflectContext* context);

        ~StaticPhysicsBehavior() override = default;

        ////////////////////////////////////////////////////////////////////////
        // PhysicsBehavior
        void InitPhysicsBehavior(AZ::EntityId entityId) override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // CryPhysicsBehavior
        pe_type GetPhysicsType() const override { return PE_STATIC; }
        bool ConfigurePhysicalEntity(IPhysicalEntity& physicalEntity) override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZ::EntityBus
        void OnEntityActivated(AZ::Entity* entity) override;
        ////////////////////////////////////////////////////////////////////////

    protected:

        StaticPhysicsConfiguration m_configuration;

        AZ::EntityId m_entityId;
    };
} // namespace LmbrCentral