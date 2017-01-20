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
#include <LmbrCentral/Shape/ShapeComponentBus.h>
#include <LmbrCentral/Physics/ColliderComponentBus.h>

namespace primitives
{
    struct primitive;
}

namespace LmbrCentral
{
    /**
     * Base class for components that provide collision
     * in the form of primitive shapes.
     */
    class PrimitiveColliderComponent
        : public AZ::Component
        , public ColliderComponentRequestBus::Handler
        , private ShapeComponentNotificationsBus::Handler
    {
    public:
        AZ_COMPONENT(PrimitiveColliderComponent, "{9CB3707A-73B3-4EE5-84EA-3CF86E0E3722}");

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("ColliderService"));
        }
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("TransformService"));
            required.push_back(AZ_CRC("ShapeService"));
        }

        ////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // ColliderComponentRequests interface implementation
        bool AddColliderToPhysicalEntity(IPhysicalEntity& physicalEntity) override;
        ////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // ShapeComponentNotificationsBus::Handler
        void OnShapeChanged() override;
        //////////////////////////////////////////////////////////////////////////

        static void Reflect(AZ::ReflectContext* context);
    protected:
        //! Adds geometry for primitive shape to the IPhysicalEntity.
        //! Returns whether we were successful.
        bool AddPrimitiveToPhysicalEntity(IPhysicalEntity& physicalEntity,
            int primitiveType,
            const primitives::primitive& primitive);
    };
} // namespace LmbrCentral
