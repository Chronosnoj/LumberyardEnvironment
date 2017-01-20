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

#include <AzCore/Component/Component.h>
#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Transform.h>

#include <LmbrCentral/Physics/RagdollPhysicsBus.h>
#include <LmbrCentral/Physics/PhysicsComponentBus.h>
#include <LmbrCentral/Physics/PhysicsSystemComponentBus.h>

#include <physinterface.h>

struct SProximityElement;
struct IPhysicalEntity;

namespace LmbrCentral
{
    /*!
     * Entity component which listens for Ragdoll requests,
     * and re-physicalizes with the ragdoll data
     */
    class RagdollComponent
        : public AZ::Component
        , private RagdollPhysicsRequestBus::Handler
        , private CryPhysicsComponentRequestBus::Handler
        , private EntityPhysicsEventBus::Handler
    {
    public:
        AZ_COMPONENT(RagdollComponent, "{80DF7994-5A4B-4B10-87B3-BE7BC541CFBB}");
        ~RagdollComponent() override = default;

        //////////////////////////////////////////////////////////////////////////
        // Component descriptor
        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("RagdollService"));
        }

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("RagdollService"));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("SkinnedMeshService"));
        }

        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
        {
            dependent.push_back(AZ_CRC("PhysicsService"));
        }


    private:
        ////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // CryCharacterPhysicsRagdollRequestBus::Handler
        void EnterRagdoll() override;
        void ExitRagdoll() override;
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
        void OnPostStep(const EntityPhysicsEvents::PostStep& event) override;
        ////////////////////////////////////////////////////////////////////////

        void CreateRagdollEntity();

        //////////////////////////////////////////////////////////////////////////
        // Reflected Data
        bool m_isActiveInitially = false;
        bool m_tryToUsePhysicsComponentMass = true;

        // Advanced
        float m_maxPhysicsTimeStep = 0.025f;
        float m_mass = 80.f;
        float m_stiffnessScale = 0.f;
        unsigned m_skeletalLevelOfDetail = 1;
        bool m_retainJointVelocity = false;
        bool m_collidesWithCharacters = true;

        // Damping
        float m_timeUntilAtRest = 0.025f;
        float m_damping = 0.3f;
        float m_dampingDuringFreefall = 0.1;
        int   m_groundedRequiredPointsOfContact = 4;
        float m_groundedTimeUntilAtRest = 0.065f;
        float m_groundedDamping = 1.5f;

        // Buoyancy
        float m_fluidDensity = 1000.f;
        float m_fluidDamping = 0;
        float m_fluidResistance = 1000.f;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // Non Reflected Data
        IPhysicalEntity* m_physicalEntity = nullptr;
        SProximityElement* m_proximityTriggerProxy = nullptr;
        //////////////////////////////////////////////////////////////////////////
    };
} // namespace LmbrCentral
