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

#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

#include <AzFramework/Entity/EntityDebugDisplayBus.h>

#include "PrimitiveColliderComponent.h"

namespace LmbrCentral
{
    /**
     * In-editor PrimitiveColliderComponent.
     * Serves as base class for specific primitive shapes.
     */
    class EditorPrimitiveColliderComponent
        : public AzToolsFramework::Components::EditorComponentBase
        , private AzFramework::EntityDebugDisplayEventBus::Handler
    {
    public:
        using Super = AzToolsFramework::Components::EditorComponentBase;
        AZ_RTTI(EditorPrimitiveColliderComponent, "{DD93D121-2048-48CB-8161-830F8CFA3DD8}", Super);

        ~EditorPrimitiveColliderComponent() override = default;

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            LmbrCentral::PrimitiveColliderComponent::GetProvidedServices(provided);
        }
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            LmbrCentral::PrimitiveColliderComponent::GetRequiredServices(required);
        }

        ////////////////////////////////////////////////////////////////////////
        // AzFramework::EntityDebugDisplayEventBus interface implementation
        void DisplayEntity(bool& handled) override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

    protected:

        //! Draw shape to the display context.
        virtual void DisplayShape(AzFramework::EntityDebugDisplayRequests& displayContext) const = 0;

        static const AZ::Vector4 s_ColliderWireColor;
        static const AZ::Vector4 s_ColliderSolidColor;
    };
} // namespace LmbrCentral
