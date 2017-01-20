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
#include <AzCore/Asset/AssetCommon.h>

#include <LmbrCentral/Physics/ColliderComponentBus.h>
#include <LmbrCentral/Rendering/MeshComponentBus.h>

namespace LmbrCentral
{
    /**
     * Provides collision from the MeshComponent to the PhysicsComponent.
     */
    class MeshColliderComponent
        : public AZ::Component
        , public ColliderComponentRequestBus::Handler
        , public MeshComponentNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(MeshColliderComponent, "{2D559EB0-F6FE-46E0-9FCE-E8F375177724}");

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("ColliderService"));
        }
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("StaticMeshService"));
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

        ////////////////////////////////////////////////////////////////////////
        // MeshComponentEvents interface implementation
        void OnMeshCreated(const AZ::Data::Asset<AZ::Data::AssetData>& asset) override;
        ////////////////////////////////////////////////////////////////////////
    };
} // namespace LmbrCentral
