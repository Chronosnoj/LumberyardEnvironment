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
#include "Precompiled.h"
#include "RagdollComponent.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <Components/IComponentPhysics.h>
#include <AzCore/Component/TransformBus.h>
#include <LmbrCentral/Physics/PhysicsComponentBus.h>
#include <LmbrCentral/Rendering/MeshComponentBus.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/EBus/ScriptBinder.h>

// CryCommon includes
#include "physinterface.h"
#include "MathConversion.h"
#include "IPhysics.h"
#include "IAttachment.h"
#include "IProximityTriggerSystem.h"

namespace LmbrCentral
{
    AZ_SCRIPTABLE_EBUS(
        RagdollPhysicsRequestBus,
        RagdollPhysicsRequestBus,
        "{4ADFBB2C-7D1B-42B7-966C-71DFF8C77B8B}",
        "{3F7AC739-087A-4506-A94B-0B4F9AC80B07}",
        AZ_SCRIPTABLE_EBUS_EVENT(EnterRagdoll)
        AZ_SCRIPTABLE_EBUS_EVENT(ExitRagdoll)
        )


    void RagdollComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<RagdollComponent, AZ::Component>()
                ->Version(1)
                ->Field("Is active", &RagdollComponent::m_isActiveInitially)
                ->Field("Use physics component mass", &RagdollComponent::m_tryToUsePhysicsComponentMass)
                ->Field("Mass", &RagdollComponent::m_mass)
                ->Field("Collides with characters", &RagdollComponent::m_collidesWithCharacters)
                ->Field("Damping", &RagdollComponent::m_damping)
                ->Field("Damping during free fall", &RagdollComponent::m_dampingDuringFreefall)
                ->Field("Time until at rest", &RagdollComponent::m_timeUntilAtRest)
                ->Field("Grounded time until at rest", &RagdollComponent::m_groundedTimeUntilAtRest)
                ->Field("Grounded required points of contact", &RagdollComponent::m_groundedRequiredPointsOfContact)
                ->Field("Grounded damping", &RagdollComponent::m_groundedDamping)
                ->Field("Max time step", &RagdollComponent::m_maxPhysicsTimeStep)
                ->Field("Stiffness scale", &RagdollComponent::m_stiffnessScale)
                ->Field("Skeletal level of detail", &RagdollComponent::m_skeletalLevelOfDetail)
                ->Field("Retain joint velocity", &RagdollComponent::m_retainJointVelocity)
                ->Field("Fluid density", &RagdollComponent::m_fluidDensity)
                ->Field("Fluid damping", &RagdollComponent::m_fluidDamping)
                ->Field("Fluid resistance", &RagdollComponent::m_fluidResistance);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<RagdollComponent>("Ragdoll", "Drive a character through physics instead of animation.  Popular for simulating unconscious characters. Currently authored in external 3d modeling packages.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Physics")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/Ragdoll.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/Ragdoll.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, false)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RagdollComponent::m_isActiveInitially, "Enabled initially", "When true the model will start off as a ragdoll")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RagdollComponent::m_tryToUsePhysicsComponentMass, "Use physics component mass", "When true tries to use mass set by a physics component first.  Defaults to 'Mass' if unchecked or no component is found")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RagdollComponent::m_mass, "Mass", "Simulated mass for the ragdoll. Used as defined by 'Use physics component mass'")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.001f)
                        ->Attribute(AZ::Edit::Attributes::Suffix, " kg")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RagdollComponent::m_collidesWithCharacters, "Collides with characters", "When set to true the ragdoll will continue to collide with characters")
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Damping")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, false)
                    ->DataElement(AZ::Edit::UIHandlers::Slider, &RagdollComponent::m_damping, "Damping", "Physical force applied against the energy in the system to drive the entity to rest")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.01f)
                        ->Attribute(AZ::Edit::Attributes::Max, 5.f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.01f)
                        ->Attribute(AZ::Edit::Attributes::Suffix, " kg/s")
                        ->DataElement(AZ::Edit::UIHandlers::Slider, &RagdollComponent::m_dampingDuringFreefall, "Damping during free fall", "Damping applied while in the air")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.01f)
                        ->Attribute(AZ::Edit::Attributes::Max, 5.f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.01f)
                        ->Attribute(AZ::Edit::Attributes::Suffix, " kg/s")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RagdollComponent::m_timeUntilAtRest, "Time until at rest", "When this amount of time passes without forces applied, physics will sleep for this character")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->Attribute(AZ::Edit::Attributes::Suffix, " s")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RagdollComponent::m_groundedTimeUntilAtRest, "Grounded time until at rest", "The time on the ground that must pass before physics will sleep this character")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RagdollComponent::m_groundedRequiredPointsOfContact, "Grounded required points of contact", "The required number of contacts before a character will be considered grounded")
                    ->DataElement(AZ::Edit::UIHandlers::Slider, &RagdollComponent::m_groundedDamping, "Grounded damping", "Damping applied while grounded")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.01f)
                        ->Attribute(AZ::Edit::Attributes::Max, 5.f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.01f)
                        ->Attribute(AZ::Edit::Attributes::Suffix, " kg/s")
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Buoyancy")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, false)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RagdollComponent::m_fluidDensity, "Fluid density", "The density of the ragdoll for fluid displacement")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->DataElement(AZ::Edit::UIHandlers::Slider, &RagdollComponent::m_fluidDamping, "Fluid damping", "The amount of damping applied while in fluid")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.01f)
                        ->Attribute(AZ::Edit::Attributes::Max, 5.f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.01f)
                        ->Attribute(AZ::Edit::Attributes::Suffix, " kg/s")
                    ->DataElement(AZ::Edit::UIHandlers::Slider, &RagdollComponent::m_fluidResistance, "Fluid resistance", "The amount of resistance applied while in fluid")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.01f)
                        ->Attribute(AZ::Edit::Attributes::Max, 1.f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.01f)
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Advanced")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, false)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RagdollComponent::m_maxPhysicsTimeStep, "Max time step", "Max size for the physics simulation time step for this character")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->Attribute(AZ::Edit::Attributes::Suffix, " s")
                    ->DataElement(AZ::Edit::UIHandlers::Slider, &RagdollComponent::m_stiffnessScale, "Stiffness scale", "The amount of stiffness to scale the joints by")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.01f)
                        ->Attribute(AZ::Edit::Attributes::Max, 1.f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.01f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RagdollComponent::m_skeletalLevelOfDetail, "Skeletal level of detail", "Which level of detail is appropriate for the character's ragdoll")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RagdollComponent::m_retainJointVelocity, "Retain joint velocity", "If true the joint velocities will be conserved at the instant of ragdolling");
            }
        }
        if (AZ::ScriptContext* script = azrtti_cast<AZ::ScriptContext*>(context))
        {
            ScriptableEBus_RagdollPhysicsRequestBus::Reflect(script);
        }
    }

    void RagdollComponent::Activate()
    {
        if (m_isActiveInitially)
        {
            EnterRagdoll();
        }
        RagdollPhysicsRequestBus::Handler::BusConnect(GetEntityId());
    }

    void RagdollComponent::Deactivate()
    {
        RagdollPhysicsRequestBus::Handler::BusDisconnect();
        ExitRagdoll();
    }

    IPhysicalEntity* RagdollComponent::GetPhysicalEntity()
    {
        return m_physicalEntity;
    }

    void RagdollComponent::GetPhysicsParameters(pe_params& outParameters)
    {
        if (m_physicalEntity)
        {
            m_physicalEntity->GetParams(&outParameters);
        }
    }
    void RagdollComponent::SetPhysicsParameters(const pe_params& parameters)
    {
        if (m_physicalEntity)
        {
            m_physicalEntity->SetParams(&parameters);
        }
    }
    void RagdollComponent::GetPhysicsStatus(pe_status& outStatus)
    {
        if (m_physicalEntity)
        {
            m_physicalEntity->GetStatus(&outStatus);
        }
    }

    void RagdollComponent::ApplyPhysicsAction(const pe_action& action, bool threadSafe)
    {
        if (m_physicalEntity)
        {
            m_physicalEntity->Action(&action, threadSafe);
        }
    }

    void RagdollComponent::OnPostStep(const EntityPhysicsEvents::PostStep& event)
    {
        // Inform the TransformComponent that we've been moved by the physics system
        AZ::Transform transform = AZ::Transform::CreateFromQuaternionAndTranslation(event.m_entityRotation, event.m_entityPosition);
        if (GetEntity()->GetTransform())
        {
            // Maintain scale (this must be precise).
            AZ::Transform entityTransform = GetEntity()->GetTransform()->GetWorldTM();
            transform.MultiplyByScale(entityTransform.ExtractScaleExact());
        }
        EBUS_EVENT_ID(event.m_entity, AZ::TransformBus, SetWorldTM, transform);
    }

    void RagdollComponent::EnterRagdoll()
    {
        if (!m_physicalEntity)
        {
            CreateRagdollEntity();
        }
        
        //////////////////////////////////////////////////////////////////////////
        // Set Physics Parameters
        pe_simulation_params  simulationParams;
        simulationParams.damping = m_damping;
        simulationParams.dampingFreefall = m_dampingDuringFreefall;
        simulationParams.density = m_fluidDensity;
        simulationParams.maxTimeStep = m_maxPhysicsTimeStep;
        simulationParams.minEnergy = m_timeUntilAtRest;
        simulationParams.maxLoggedCollisions = 1;
        m_physicalEntity->SetParams(&simulationParams);

        pe_params_buoyancy buoyancyParams;
        buoyancyParams.waterDensity = m_fluidDensity;
        buoyancyParams.waterDamping = m_fluidDamping;
        buoyancyParams.waterResistance = m_fluidResistance;
        m_physicalEntity->SetParams(&buoyancyParams);

        pe_params_articulated_body articulatedParams;
        articulatedParams.bGrounded = 0;
        articulatedParams.scaleBounceResponse = 1;
        articulatedParams.bCheckCollisions = 1;
        articulatedParams.bCollisionResp = 1;
        articulatedParams.nCollLyingMode = m_groundedRequiredPointsOfContact;
        articulatedParams.dampingLyingMode = m_groundedDamping;
        articulatedParams.minEnergyLyingMode = m_groundedTimeUntilAtRest;
        m_physicalEntity->SetParams(&articulatedParams);

        pe_params_part partParams;
        partParams.flagsAND = ~geom_collides;
        partParams.flagsOR = geom_collides;
        m_physicalEntity->SetParams(&partParams);

        pe_params_flags flagsParams;
        if (m_collidesWithCharacters)
        {
            flagsParams.flagsAND = ~pef_pushable_by_players;
            flagsParams.flags = pef_pushable_by_players;
        }
        flagsParams.flagsOR = pef_log_poststep | pef_log_collisions | pef_log_state_changes;
        m_physicalEntity->SetParams(&flagsParams);

        pe_action_awake awakeAction;
        awakeAction.bAwake = 1;
        m_physicalEntity->Action(&awakeAction);        

        CryPhysicsComponentRequestBus::Handler::BusConnect(GetEntityId());
        EntityPhysicsEventBus::Handler::BusConnect(GetEntityId());
    }

    void RagdollComponent::ExitRagdoll()
    {
        CryPhysicsComponentRequestBus::Handler::BusDisconnect();
        EntityPhysicsEventBus::Handler::BusDisconnect();

        if (m_physicalEntity)
        {
            gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_physicalEntity);
            m_physicalEntity = nullptr;
            EBUS_EVENT_ID(GetEntityId(), PhysicsComponentRequestBus, EnablePhysics);
        }

        // Remove Proximity trigger proxy
        if (m_proximityTriggerProxy)
        {
            EBUS_EVENT(ProximityTriggerSystemRequestBus, RemoveEntity, m_proximityTriggerProxy, false);
            m_proximityTriggerProxy = nullptr;
        }
    }

    void RagdollComponent::CreateRagdollEntity()
    {
        // Get the current mass if any
        float massToUse = m_mass;
        if (m_tryToUsePhysicsComponentMass)
        {
            pe_params_part currentPartParameters;
            currentPartParameters.ipart = 0;
            EBUS_EVENT_ID(GetEntityId(), CryPhysicsComponentRequestBus, GetPhysicsParameters, currentPartParameters);
            if (!is_unused(currentPartParameters.mass))
            {
                massToUse = currentPartParameters.mass;
            }
        }

        // Disable the current physics behavior on the entity if any
        EBUS_EVENT_ID(GetEntityId(), PhysicsComponentRequestBus, DisablePhysics);

        // Create Proximity trigger proxy
        m_proximityTriggerProxy = nullptr;
        EBUS_EVENT_RESULT(m_proximityTriggerProxy, ProximityTriggerSystemRequestBus, CreateEntity, GetEntityId());

        // Setup position params
        AZ::Transform transform = AZ::Transform::CreateIdentity();
        EBUS_EVENT_ID_RESULT(transform, GetEntityId(), AZ::TransformBus, GetWorldTM);
        pe_params_pos positionParameters;
        Matrix34 cryTransform(AZTransformToLYTransform(transform));
        positionParameters.pMtx3x4 = &cryTransform;
        positionParameters.iSimClass = 2;
        positionParameters.q = AZQuaternionToLYQuaternion(AZ::Quaternion::CreateFromTransform(transform));

        // create the ragdoll physics entity
        m_physicalEntity = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_ARTICULATED, &positionParameters, static_cast<uint64>(GetEntityId()), PHYS_FOREIGN_ID_COMPONENT_ENTITY);
        AZ_Assert(m_physicalEntity, "new failed to create a physical entity for the ragdoll component");
        m_physicalEntity->AddRef();

        // prep the skeleton for ragdoll
        ICharacterInstance* character = nullptr;
        EBUS_EVENT_ID_RESULT(character, GetEntityId(), SkinnedMeshComponentRequestBus, GetCharacterInstance);
        if (character)
        {
            character->GetISkeletonPose()->BuildPhysicalEntity(m_physicalEntity, massToUse, -1, m_stiffnessScale, m_skeletalLevelOfDetail, 0);
            character->GetISkeletonPose()->CreateAuxilaryPhysics(m_physicalEntity, cryTransform, m_skeletalLevelOfDetail);
            character->GetISkeletonPose()->SetCharacterPhysics(m_physicalEntity);

            // the position has already been set, but we need this to set the rotation
            m_physicalEntity->SetParams(&positionParameters);

            // set the component entity up as foreign data for the skeleton
            pe_params_foreign_data foreignDataParams;
            foreignDataParams.pForeignData = static_cast<uint64>(GetEntityId());
            foreignDataParams.iForeignData = PHYS_FOREIGN_ID_COMPONENT_ENTITY;
            foreignDataParams.iForeignFlagsOR = PFF_UNIMPORTANT;
            m_physicalEntity->SetParams(&foreignDataParams);

            for (int iAux = 0; IPhysicalEntity* auxilliaryPhysicalEntity = character->GetISkeletonPose()->GetCharacterPhysics(iAux); ++iAux)
            {
                auxilliaryPhysicalEntity->SetParams(&foreignDataParams);
            }

            //////////////////////////////////////////////////////////////////////////
            // Assign physical materials mapping table for this material.
            //////////////////////////////////////////////////////////////////////////
            if (IMaterial* pMtl = character->GetIMaterial())
            {
                // Assign custom material to physics.
                int surfaceTypesId[MAX_SUB_MATERIALS];
                memset(surfaceTypesId, 0, sizeof(surfaceTypesId));
                int numIds = pMtl->FillSurfaceTypeIds(surfaceTypesId);

                // Set the material mappings for the parts
                pe_params_part ppart;
                ppart.nMats = numIds;
                ppart.pMatMapping = surfaceTypesId;
                m_physicalEntity->SetParams(&ppart);
            }
        }
    }
} // namespace LmbrCentral
