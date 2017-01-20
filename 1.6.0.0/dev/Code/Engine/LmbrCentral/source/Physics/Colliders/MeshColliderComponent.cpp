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
#include "MeshColliderComponent.h"
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <MathConversion.h>
#include <IStatObj.h>

namespace LmbrCentral
{
    void MeshColliderComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<MeshColliderComponent, AZ::Component>()
                ->Version(1)
                ->SerializerForEmptyClass()
            ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<MeshColliderComponent>(
                    "Mesh Collider", "Provides collision from the MeshComponent to the PhysicsComponent.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Physics/Colliders")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/ColliderMesh.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                ;
            }
        }
    }

    void MeshColliderComponent::Activate()
    {
        ColliderComponentRequestBus::Handler::BusConnect(GetEntityId());
        MeshComponentNotificationBus::Handler::BusConnect(GetEntityId());
    }

    void MeshColliderComponent::Deactivate()
    {
        ColliderComponentRequestBus::Handler::BusDisconnect();
        MeshComponentNotificationBus::Handler::BusDisconnect();
    }

    void MeshColliderComponent::OnMeshCreated(const AZ::Data::Asset<AZ::Data::AssetData>&)
    {
        EBUS_EVENT_ID(GetEntityId(), ColliderComponentEventBus, OnColliderChanged);
    }

    bool MeshColliderComponent::AddColliderToPhysicalEntity(IPhysicalEntity& physicalEntity)
    {
        IStatObj* entityStatObj = nullptr;
        EBUS_EVENT_ID_RESULT(entityStatObj, GetEntityId(), StaticMeshComponentRequestBus, GetStatObj);
        if (!entityStatObj)
        {
            return false;
        }

        // configure pe_geomparams
        pe_geomparams geometryParameters;
        geometryParameters.flags = geom_collides | geom_floats | geom_colltype_ray;

        AZ::Transform myAzTransform = AZ::Transform::Identity();
        EBUS_EVENT_ID_RESULT(myAzTransform, GetEntityId(), AZ::TransformBus, GetWorldTM);

        // only uniform scale is supported in physics
        const AZ::Vector3 scale = myAzTransform.RetrieveScale();
        geometryParameters.scale = AZ::GetMax(AZ::GetMax(scale.GetX(), scale.GetY()), scale.GetZ());

        // determine position and rotation relative to physical entity
        QuatT myQuatT = AZTransformToLYQuatT(myAzTransform);

        pe_status_pos physicalEntityPosition;
        physicalEntity.GetStatus(&physicalEntityPosition);

        QuatT physicalEntityQuatT {
            physicalEntityPosition.q, physicalEntityPosition.pos
        };
        QuatT myLocalQuatT = physicalEntityQuatT.GetInverted() * myQuatT;

        geometryParameters.pos = myLocalQuatT.t;
        geometryParameters.q = myLocalQuatT.q;

        // Use the next free index as an Id.
        // Using the default id (0) will overwrite any pre-existing geometry on the entity.
        pe_status_nparts status_nparts;
        const int geometryId = physicalEntity.GetStatus(&status_nparts);

        enum
        {
            PhysicalizeError = -1
        };
        int physicalizeResult = entityStatObj->Physicalize(&physicalEntity, &geometryParameters, geometryId);
        return physicalizeResult != PhysicalizeError;
    }
} // namespace LmbrCentral
