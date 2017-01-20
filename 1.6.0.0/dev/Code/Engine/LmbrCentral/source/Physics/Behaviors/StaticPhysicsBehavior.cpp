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
#include "StaticPhysicsBehavior.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Component/TransformBus.h>
#include <LmbrCentral/Physics/PhysicsComponentBus.h>
#include <LmbrCentral/Physics/ColliderComponentBus.h>

namespace LmbrCentral
{
    void StaticPhysicsConfiguration::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<StaticPhysicsConfiguration>()
                ->Version(1)
                ->Field("EnabledInitially", &StaticPhysicsConfiguration::m_enabledInitially)
            ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<StaticPhysicsConfiguration>(
                    "Static Physics Configuration", "Configuration for Static physics object")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly"))

                    ->DataElement(0, &StaticPhysicsConfiguration::m_enabledInitially,
                    "Enabled Initially", "Whether the entity is initially enabled in the physics simulation.")
                ;
            }
        }
    }

    void StaticPhysicsBehavior::Reflect(AZ::ReflectContext* context)
    {
        StaticPhysicsConfiguration::Reflect(context);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<StaticPhysicsBehavior>()
                ->Version(1)
                ->Field("Configuration", &StaticPhysicsBehavior::m_configuration)
            ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<StaticPhysicsBehavior>(
                    "Static Body", "The entity behaves as a solid unmovable object in the physics simulation.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Physics")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &StaticPhysicsBehavior::m_configuration,
                    "Configuration", "Static Physics Configuration")
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly"))
                ;
            }
        }
    }

    void StaticPhysicsBehavior::InitPhysicsBehavior(AZ::EntityId entityId)
    {
        m_entityId = entityId;
        AZ::EntityBus::Handler::BusConnect(m_entityId);
    }

    void StaticPhysicsBehavior::OnEntityActivated(AZ::Entity*)
    {
        if (m_configuration.m_enabledInitially)
        {
            EBUS_EVENT_ID(m_entityId, PhysicsComponentRequestBus, EnablePhysics);
        }
    }

    bool StaticPhysicsBehavior::ConfigurePhysicalEntity(IPhysicalEntity& physicalEntity)
    {
        return true;
    }
} // namespace LmbrCentral
