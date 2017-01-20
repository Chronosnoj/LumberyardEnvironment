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
#include "RigidPhysicsBehavior.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Component/TransformBus.h>
#include <LmbrCentral/Physics/PhysicsComponentBus.h>
#include <LmbrCentral/Physics/ColliderComponentBus.h>

namespace LmbrCentral
{
    void RigidPhysicsConfiguration::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<RigidPhysicsConfiguration>()
                ->Version(1)
                ->Field("EnabledInitially", &RigidPhysicsConfiguration::m_enabledInitially)
                ->Field("SpecifyMassOrDensity", &RigidPhysicsConfiguration::m_specifyMassOrDensity)
                ->Field("Mass", &RigidPhysicsConfiguration::m_mass)
                ->Field("Density", &RigidPhysicsConfiguration::m_density)
                ->Field("AtRestInitially", &RigidPhysicsConfiguration::m_atRestInitially)
                ->Field("SimulationDamping", &RigidPhysicsConfiguration::m_simulationDamping)
                ->Field("SimulationMinEnergy", &RigidPhysicsConfiguration::m_simulationMinEnergy)
                ->Field("BuoyancyDamping", &RigidPhysicsConfiguration::m_buoyancyDamping)
                ->Field("BuoyancyDensity", &RigidPhysicsConfiguration::m_buoyancyDensity)
                ->Field("BuoyancyResistance", &RigidPhysicsConfiguration::m_buoyancyResistance)
            ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<RigidPhysicsConfiguration>(
                    "Rigid Body Physics Configuration", "")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Physics")
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly"))

                    ->DataElement(0, &RigidPhysicsConfiguration::m_enabledInitially,
                    "Enabled Initially", "Whether the entity is initially enabled in the physics simulation.")

                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &RigidPhysicsConfiguration::m_specifyMassOrDensity,
                    "Specify Mass or Density", "Whether total mass is specified, or calculated at spawn time based on density and volume")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshEntireTree"))
                        ->EnumAttribute(RigidPhysicsConfiguration::MassOrDensity::Mass, "Mass")
                        ->EnumAttribute(RigidPhysicsConfiguration::MassOrDensity::Density, "Density")

                    ->DataElement(0, &RigidPhysicsConfiguration::m_mass,
                    "Total Mass (kg)", "Total mass of entity, in kg")
                        ->Attribute(AZ::Edit::Attributes::Visibility, &RigidPhysicsConfiguration::UseMass)
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)

                    ->DataElement(0, &RigidPhysicsConfiguration::m_density,
                    "Density (kg / cubic meter)", "Mass (kg) per cubic meter of the mesh's volume. Total mass of entity is calculated at spawn. (Water's density is 1000)")
                        ->Attribute(AZ::Edit::Attributes::Visibility, &RigidPhysicsConfiguration::UseDensity)
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)

                    ->DataElement(0, &RigidPhysicsConfiguration::m_atRestInitially,
                    "At Rest Initially", "True: Entity remains at rest until agitated.\nFalse: Entity falls after spawn.")

                    ->ClassElement(AZ::Edit::ClassElements::Group, "Simulation")

                    ->DataElement(0, &RigidPhysicsConfiguration::m_simulationDamping, "Damping", "Uniform damping value applied to object's movement.")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.1f)

                    ->DataElement(0, &RigidPhysicsConfiguration::m_simulationMinEnergy, "Minimum energy", "The energy threshold under which the object will go to sleep.")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.01f)

                    ->ClassElement(AZ::Edit::ClassElements::Group, "Buoyancy")

                    ->DataElement(0, &RigidPhysicsConfiguration::m_buoyancyDamping, "Water damping", "Uniform damping value applied while in water.")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.1f)

                    ->DataElement(0, &RigidPhysicsConfiguration::m_buoyancyDensity, "Water density", "Water density strength.")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.1f)

                    ->DataElement(0, &RigidPhysicsConfiguration::m_buoyancyResistance, "Water resistance", "Water resistance strength.")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.1f)
                ;
            }
        }
    }

    void RigidPhysicsBehavior::Reflect(AZ::ReflectContext* context)
    {
        RigidPhysicsConfiguration::Reflect(context);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<RigidPhysicsBehavior>()
                ->Version(1)
                ->Field("Configuration", &RigidPhysicsBehavior::m_configuration)
            ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<RigidPhysicsBehavior>(
                    "Rigid Body", "The entity behaves as a rigid movable object in the physics simulation.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &RigidPhysicsBehavior::m_configuration,
                    "Configuration", "Rigid Body Physics Configuration")
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly"))
                ;
            }
        }
    }

    void RigidPhysicsBehavior::InitPhysicsBehavior(AZ::EntityId entityId)
    {
        m_entityId = entityId;
        AZ::EntityBus::Handler::BusConnect(m_entityId);
    }

    void RigidPhysicsBehavior::OnEntityActivated(AZ::Entity*)
    {
        if (m_configuration.m_enabledInitially)
        {
            EBUS_EVENT_ID(m_entityId, PhysicsComponentRequestBus, EnablePhysics);
        }
    }

    void AssignWeightToParts(IPhysicalEntity& physicalEntity, const RigidPhysicsConfiguration& configuration)
    {
        pe_status_nparts status_nparts;
        const int numParts = physicalEntity.GetStatus(&status_nparts);

        if (configuration.UseMass())
        {
            // Attempt to distribute mass amongst parts based on their volume.
            // If any part missing volume, just divide mass equally between parts.
            if (numParts > 0)
            {
                AZStd::vector<float> volumes(numParts, 0.f);
                float totalVolume = 0.f;
                bool allHaveVolume = true;
                for (int partId = 0; partId < numParts; ++partId)
                {
                    pe_params_part partParams;
                    partParams.partid = partId;
                    physicalEntity.GetParams(&partParams);
                    if (!is_unused(partParams.pPhysGeom)
                        && partParams.pPhysGeom
                        && partParams.pPhysGeom->V > 0.f)
                    {
                        volumes[partId] = partParams.pPhysGeom->V;
                    }
                    else if (!is_unused(partParams.pPhysGeomProxy)
                             && partParams.pPhysGeomProxy
                             && partParams.pPhysGeomProxy->V > 0.f)
                    {
                        volumes[partId] = partParams.pPhysGeomProxy->V;
                    }

                    if (volumes[partId] > 0.f)
                    {
                        totalVolume += volumes[partId];
                    }
                    else
                    {
                        allHaveVolume = false;
                    }
                }

                for (int partId = 0; partId < numParts; ++partId)
                {
                    pe_params_part partParams;
                    partParams.partid = partId;
                    if (allHaveVolume)
                    {
                        partParams.mass = (volumes[partId] / totalVolume) * configuration.m_mass;
                    }
                    else
                    {
                        partParams.mass = configuration.m_mass / numParts;
                    }
                    physicalEntity.SetParams(&partParams);
                }
            }
        }

        if (configuration.UseDensity())
        {
            for (int partId = 0; partId < numParts; ++partId)
            {
                pe_params_part partParams;
                partParams.partid = partId;
                partParams.density = configuration.m_density;
                physicalEntity.SetParams(&partParams);
            }
        }
    }

    bool RigidPhysicsBehavior::ConfigurePhysicalEntity(IPhysicalEntity& physicalEntity)
    {
        AZ::Transform transform = AZ::Transform::CreateIdentity();
        EBUS_EVENT_ID_RESULT(transform, m_entityId, AZ::TransformBus, GetWorldTM);

        // Setup simulation params.
        pe_simulation_params simParams;
        pe_params_buoyancy buoyancyParams;
        simParams.damping = m_configuration.m_simulationDamping;
        simParams.minEnergy = m_configuration.m_simulationMinEnergy;
        buoyancyParams.waterDamping = m_configuration.m_buoyancyDamping;
        buoyancyParams.kwaterDensity = m_configuration.m_buoyancyDensity;
        buoyancyParams.kwaterResistance = m_configuration.m_buoyancyResistance;
        buoyancyParams.waterEmin = m_configuration.m_simulationMinEnergy;

        // Individual parts need to be told of their mass or density
        AssignWeightToParts(physicalEntity, m_configuration);

        physicalEntity.SetParams(&simParams);
        physicalEntity.SetParams(&buoyancyParams);

        // If not at rest, wake entity
        pe_action_awake awakeParameters;
        awakeParameters.bAwake = m_configuration.m_atRestInitially ? 0 : 1;
        if (!physicalEntity.Action(&awakeParameters))
        {
            return false;
        }

        // Ensure that certain events bubble up from the physics system
        pe_params_flags flagParameters;
        flagParameters.flagsOR  = pef_log_poststep // enable event when entity is moved by physics system
            | pef_log_collisions;                     // enable collision event
        if (!physicalEntity.SetParams(&flagParameters))
        {
            return false;
        }

        return true;
    }
} // namespace LmbrCentral
