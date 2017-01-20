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

#include <AzCore/Math/Transform.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/TransformBus.h>

#include <AzFramework/Math/MathUtils.h>

namespace LmbrCentral
{
    //=========================================================================
    // LookAtComponent
    //=========================================================================
    class LookAtComponent
        : public AZ::Component
        , private AZ::TransformNotificationBus::MultiHandler
        , private AZ::TickBus::Handler
        , private AZ::EntityBus::Handler
    {
    public:
        friend class EditorLookAtComponent;

        AZ_COMPONENT(LookAtComponent, "{11CDC627-25A9-4760-A61F-576CDB189B38}");

        //=====================================================================
        // AZ::Component
        void Activate() override;
        void Deactivate() override;
        //=====================================================================

        //=====================================================================
        // TransformBus
        void OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& /*world*/) override;
        //=====================================================================

        //=====================================================================
        // TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;
        //=====================================================================

        //=====================================================================
        // EntityBus
        void OnEntityActivated(AZ::Entity* entity) override;
        void OnEntityDeactivate(AZ::Entity* entity) override;
        //=====================================================================

    protected:
        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("LookAtService"));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("TransformService"));
        }

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("LookAtService"));
        }

    private:
        void RecalculateTransform();

        // Serialized data
        AZ::EntityId m_targetId;
        AzFramework::Axis m_forwardAxis;
    };

}//namespace LmbrCentral
