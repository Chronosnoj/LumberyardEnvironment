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

#include <AzCore/Math/Crc.h>
#include <IProximityTriggerSystem.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

#include <LmbrCentral/Physics/PhysicsComponentBus.h>
#include <LmbrCentral/Physics/PhysicsSystemComponentBus.h>
#include <LmbrCentral/Physics/ColliderComponentBus.h>

namespace AZ
{
    class Transform;
}

namespace LmbrCentral
{
    class CryPhysicsBehavior;

    /*!
     * Configuration data for PhysicsComponent
     */
    struct PhysicsConfiguration
    {
        AZ_CLASS_ALLOCATOR(PhysicsConfiguration, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(PhysicsConfiguration, "{3EE60668-D14C-458F-9E83-FEBC654C898E}")

        virtual ~PhysicsConfiguration() = default;

        static void Reflect(AZ::ReflectContext* context);

        //! The physics behavior allows customization of the physics object,
        //! (eg: Rigid Body, Static)
        AZStd::shared_ptr<CryPhysicsBehavior> m_behavior;

        //! Indicates whether this component can interact with proximity triggers
        bool m_canInteractProximityTriggers = true;

        //! The physics object will use colliders from itself, as well as these other entities.
        AZStd::vector<AZ::EntityId> m_childColliders;
    };

    /*!
     * In-game physics component.
     * An entity with a physics component can interact with the physics simulation.
     */
    class PhysicsComponent
        : public AZ::Component
        , public PhysicsComponentRequestBus::Handler
        , public CryPhysicsComponentRequestBus::Handler
        , public EntityPhysicsEventBus::Handler
        , public ColliderComponentEventBus::Handler
        , public AZ::TransformNotificationBus::Handler
        , public AZ::EntityBus::MultiHandler
    {
    public:
        AZ_COMPONENT(PhysicsComponent, "{A74FA374-8F68-495B-96C1-0BCC8D00EB61}");
        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("PhysicsService"));
        }

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("PhysicsService"));
        }

        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
        {
            dependent.push_back(AZ_CRC("ColliderService"));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("TransformService"));
        }

        ////////////////////////////////////////////////////////////////////////
        // PhysicsComponentRequests
        void EnablePhysics() override;
        void DisablePhysics() override;
        bool IsPhysicsEnabled() override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // CryPhysicsComponentRequests
        IPhysicalEntity* GetPhysicalEntity() override;
        void GetPhysicsParameters(pe_params& outParameters) override;
        void SetPhysicsParameters(const pe_params& parameters) override;
        void GetPhysicsStatus(pe_status& outStatus) override;
        void ApplyPhysicsAction(const pe_action& action, bool threadSafe) override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // EntityPhysicsEvents

        //! When object moves in physics simulation,
        //! update its Transform in component-land.
        void OnPostStep(const EntityPhysicsEvents::PostStep& event) override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // ColliderComponentEvents
        void OnColliderChanged() override;
        ////////////////////////////////////////////////////////////////////////


        bool IsPhysicsEnabled() const;

    private:

        ////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // TransformNotifications
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;
        void OnParentChanged(AZ::EntityId oldParent, AZ::EntityId newParent) override;
        //////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // EntityEvents
        void OnEntityActivated(AZ::Entity* entity) override;
        ////////////////////////////////////////////////////////////////////////

        PhysicsConfiguration m_configuration;
        AZStd::shared_ptr<IPhysicalEntity> m_physicalEntity;
        AZStd::vector<AZ::EntityId> m_childColliders;
        bool m_isApplyingPhysicsToEntityTransform = false;
        SProximityElement* m_proximityTriggerProxy = nullptr;
    };
} // namespace LmbrCentral
