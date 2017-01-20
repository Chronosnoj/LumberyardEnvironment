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

#include <AzCore/Component/ComponentBus.h>

struct IPhysicalEntity;
struct pe_geomparams;

namespace LmbrCentral
{
    /**
     * Messages serviced by a ColliderComponent.
     * A ColliderComponent describes the shape of an entity to the physics system.
     */
    class ColliderComponentRequests
        : public AZ::ComponentBus
    {
    public:
        //! Collider adds its geometry to the IPhysicalEntity
        //! \param physicalEntity The IPhysicalEntity to which geometry will be added.
        //! \return Whether successful. If unsuccessful no geometry is added.
        virtual bool AddColliderToPhysicalEntity(IPhysicalEntity& physicalEntity) = 0;
    };
    using ColliderComponentRequestBus = AZ::EBus<ColliderComponentRequests>;

    /**
     * Events dispatched by a ColliderComponent.
     * A ColliderComponent describes the shape of an entity to the physics system.
     */
    class ColliderComponentEvents
        : public AZ::ComponentBus
    {
    public:
        virtual void OnColliderChanged() {}
    };
    using ColliderComponentEventBus = AZ::EBus<ColliderComponentEvents>;
} // namespace LmbrCentral
