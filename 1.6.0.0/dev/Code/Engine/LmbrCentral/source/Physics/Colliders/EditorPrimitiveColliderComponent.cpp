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
#include "EditorPrimitiveColliderComponent.h"

namespace LmbrCentral
{
    const AZ::Vector4 EditorPrimitiveColliderComponent::s_ColliderWireColor = AZ::Vector4(1.00f, 1.00f, 0.78f, 1.00f);
    const AZ::Vector4 EditorPrimitiveColliderComponent::s_ColliderSolidColor = AZ::Vector4(1.00f, 1.00f, 0.78f, 0.40f);

    void EditorPrimitiveColliderComponent::Activate()
    {
        Super::Activate();

        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
    }

    void EditorPrimitiveColliderComponent::Deactivate()
    {
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();

        Super::Deactivate();
    }

    void EditorPrimitiveColliderComponent::DisplayEntity(bool& handled)
    {
        if (!IsSelected())
        {
            return;
        }

        AzFramework::EntityDebugDisplayRequests * displayContext = AzFramework::EntityDebugDisplayRequestBus::FindFirstHandler();
        AZ_Assert(displayContext, "Invalid display context.");

        // Physics only does uniform scaling (use Z-scale)
        AZ::Transform transform = GetWorldTM();
        AZ::Vector3 previousScale = transform.ExtractScale(); // removes scale from transform
        AZ::Vector3 physicsScale = AZ::Vector3(previousScale.GetZ(), previousScale.GetZ(), previousScale.GetZ());
        transform.MultiplyByScale(physicsScale);

        displayContext->PushMatrix(transform);
        {
            DisplayShape(*displayContext);
        }
        displayContext->PopMatrix();

        handled = true;
    }
} // namespace LmbrCentral
