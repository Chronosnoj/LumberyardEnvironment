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
#include "PrimitiveColliderComponent.h"
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/Component/Entity.h>
#include <MathConversion.h>
#include <physinterface.h>
#include <ISystem.h>
#include <LmbrCentral/Shape/ShapeComponentBus.h>
#include <LmbrCentral/Shape/BoxShapeComponentBus.h>
#include <LmbrCentral/Shape/SphereShapeComponentBus.h>
#include <LmbrCentral/Shape/CylinderShapeComponentBus.h>
#include <LmbrCentral/Shape/CapsuleShapeComponentBus.h>
#include "primitives.h"

namespace LmbrCentral
{

    void PrimitiveColliderComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<PrimitiveColliderComponent, AZ::Component>()
                ->Version(1)
                ->SerializerForEmptyClass();

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<PrimitiveColliderComponent>(
                    "Primitive Collider", "Collider component that can use a shape to define its collision")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Physics/Colliders")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/ColliderMesh.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/MeshCollider.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true);
            }
        }
    }

    //! Create IGeometry. Returns IGeometry wrapped in a _smart_ptr.
    //! If unsuccessful _smart_ptr is empty.
    static _smart_ptr<IGeometry> CreatePrimitiveGeometry(int primitiveType, const primitives::primitive& primitive)
    {
        IGeomManager& geometryManager = *gEnv->pPhysicalWorld->GetGeomManager();
        _smart_ptr<IGeometry> geometry = geometryManager.CreatePrimitive(primitiveType, &primitive);

        // IGeomManager::CreatePrimitive() returns a raw pointer with a refcount of 1.
        // Putting the IGeometry* in a _smart_ptr increments the refcount to 2.
        // Therefore, we decrement the refcount back to 1 so the thing we return
        // will behave like people expect.
        if (geometry)
        {
            geometry->Release();
        }

        return geometry;
    }

    bool PrimitiveColliderComponent::AddColliderToPhysicalEntity(IPhysicalEntity& physicalEntity)
    {
        AZ::Crc32 shapeType;
        EBUS_EVENT_ID_RESULT(shapeType, GetEntityId(), ShapeComponentRequestsBus, GetShapeType);

        if (shapeType == AZ::Crc32("Sphere"))
        {
            SphereShapeConfiguration config;
            EBUS_EVENT_ID_RESULT(config, GetEntityId(), SphereShapeComponentRequestsBus, GetSphereConfiguration);
            primitives::sphere sphere;
            sphere.center.Set(0.f, 0.f, 0.f);
            sphere.r = config.GetRadius();
            return AddPrimitiveToPhysicalEntity(physicalEntity, primitives::sphere::type, sphere);
        }
        else if (shapeType == AZ::Crc32("Box"))
        {
            BoxShapeConfiguration config;
            EBUS_EVENT_ID_RESULT(config, GetEntityId(), BoxShapeComponentRequestsBus, GetBoxConfiguration);
            primitives::box box;
            box.Basis.SetIdentity();
            box.bOriented = 0;
            box.center.Set(0.f, 0.f, 0.f);

            // box.m_size[i] is "half length" of side
            // config.m_dimensions[i] is "total length" of side
            box.size = AZVec3ToLYVec3(config.GetDimensions() * 0.5f);
            return AddPrimitiveToPhysicalEntity(physicalEntity, primitives::box::type, box);
        }
        else if (shapeType == AZ::Crc32("Cylinder"))
        {
            CylinderShapeConfiguration config;
            EBUS_EVENT_ID_RESULT(config, GetEntityId(), CylinderShapeComponentRequestsBus, GetCylinderConfiguration);
            primitives::cylinder cylinder;
            cylinder.center.Set(0.f, 0.f, 0.f);
            cylinder.axis.Set(0.f, 0.f, 1.f);
            cylinder.r = config.GetRadius();
            // cylinder.hh is "half height"
            // config.m_height is "total height"
            cylinder.hh = 0.5f * config.GetHeight();
            return AddPrimitiveToPhysicalEntity(physicalEntity, primitives::cylinder::type, cylinder);
        }
        else if (shapeType == AZ::Crc32("Capsule"))
        {
            CapsuleShapeConfiguration config;
            EBUS_EVENT_ID_RESULT(config, GetEntityId(), CapsuleShapeComponentRequestsBus, GetCapsuleConfiguration);
            primitives::capsule capsule;
            capsule.center.Set(0.f, 0.f, 0.f);
            capsule.axis.Set(0.f, 0.f, 1.f);
            capsule.r = config.GetRadius();
            // config.m_height specifies "total height" of capsule.
            // capsule.hh is the "half height of the straight section of a capsule".
            // config.hh == (2 * capsule.hh) + (2 * capsule.r)
            capsule.hh = AZStd::max(0.f, (0.5f * config.GetHeight()) - config.GetRadius());
            return AddPrimitiveToPhysicalEntity(physicalEntity, primitives::capsule::type, capsule);
        }

        return false;
    }

    bool PrimitiveColliderComponent::AddPrimitiveToPhysicalEntity(
        IPhysicalEntity& physicalEntity,
        int primitiveType,
        const primitives::primitive& primitive)
    {
        _smart_ptr<IGeometry> geometry = CreatePrimitiveGeometry(primitiveType, primitive);
        if (!geometry)
        {
            return false;
        }

        phys_geometry* physGeometry = gEnv->pPhysicalWorld->GetGeomManager()->RegisterGeometry(geometry);
        if (!physGeometry)
        {
            return false;
        }

        // configure pe_geomparams
        pe_geomparams geometryParameters;
        geometryParameters.flags = geom_collides | geom_floats | geom_colltype_ray;

        AZ::Transform myAzTransform = AZ::Transform::Identity();
        EBUS_EVENT_ID_RESULT(myAzTransform, GetEntityId(), AZ::TransformBus, GetWorldTM);

        AZ::Crc32 shapeType;
        EBUS_EVENT_ID_RESULT(shapeType, GetEntityId(), ShapeComponentRequestsBus, GetShapeType);

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
        geometryParameters.q = myLocalQuatT.q.GetNormalizedFast();

        // add geometry
        enum
        {
            AddGeometryError = -1
        };
        int addGeometryResult = physicalEntity.AddGeometry(physGeometry, &geometryParameters);

        // release our phys_geometry reference (IPhysicalEntity should have
        // a reference now)
        gEnv->pPhysicalWorld->GetGeomManager()->UnregisterGeometry(physGeometry);

        return addGeometryResult != AddGeometryError;
    }

    void PrimitiveColliderComponent::Activate()
    {
        ShapeComponentNotificationsBus::Handler::BusConnect(GetEntityId());
        ColliderComponentRequestBus::Handler::BusConnect(GetEntityId());
    }

    void PrimitiveColliderComponent::Deactivate()
    {
        ColliderComponentRequestBus::Handler::BusDisconnect();
        ShapeComponentNotificationsBus::Handler::BusDisconnect();
    }

    void PrimitiveColliderComponent::OnShapeChanged()
    {
        AZ_Warning("[Primitive Collider Component]", false, "Primitive collider component does not currently support dynamic changes to collider shape...Entity %s [%llu]..."
                   , m_entity->GetName().c_str(), m_entity->GetId());
    }
} // namespace LmbrCentral
