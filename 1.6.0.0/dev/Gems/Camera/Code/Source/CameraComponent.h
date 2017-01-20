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
#include <AzCore/Component/TransformBus.h>

#include <AzFramework/Components/EditorEntityEvents.h>

#include "Camera/CameraComponentBus.h"
#include <IViewSystem.h>
#include <Cry_Camera.h>

namespace Camera
{
    //////////////////////////////////////////////////////////////////////////
    /// The CameraComponent holds all of the data necessary for a camera.
    /// Get and set data through the CameraRequestBus or TransformBus
    //////////////////////////////////////////////////////////////////////////
    class CameraComponent
        : public AZ::Component
        , public CameraRequestBus::Handler
        , public AZ::TransformNotificationBus::Handler
        , private AzFramework::EditorEntityEvents
    {
    public:
        AZ_COMPONENT(CameraComponent, "{A0C21E18-F759-4E72-AF26-7A36FC59E477}", AzFramework::EditorEntityEvents);
        virtual ~CameraComponent() = default;

        static void Reflect(AZ::ReflectContext* reflection);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // CameraRequestBus::Handler
        float GetFov() const override;
        float GetNearClipDistance() const override;
        float GetFarClipDistance() const override;
        void SetFov(float fov) override;
        void SetNearClipDistance(float nearClipDistance) override;
        void SetFarClipDistance(float farClipDistance) override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AZ::TransformNotificationBus::Handler
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;
        //////////////////////////////////////////////////////////////////////////

        void UpdateCamera();

        //////////////////////////////////////////////////////////////////////////
        // EditorEntityEvents
        void EditorDisplay(AzFramework::EntityDebugDisplayRequests& displayInterface, const AZ::Transform& world, bool& handled) override;
        //////////////////////////////////////////////////////////////////////////

    protected:
        const float s_defaultFoV = 75.0f;
        const float s_minFoV = std::numeric_limits<float>::epsilon();
        const float s_maxFoV = AZ::RadToDeg(AZ::Constants::Pi);
        const float s_defaultNearPlaneDistance = 0.2f;
        const float s_defaultFarClipPlaneDistance = 1024.0f;

        float m_fov = s_defaultFoV;
        float m_nearClipPlaneDistance = s_defaultNearPlaneDistance;
        float m_farClipPlaneDistance = s_defaultFarClipPlaneDistance;

        IView* m_view = nullptr;
        IViewSystem* m_viewSystem = nullptr;
        ISystem* m_system = nullptr;
    };
} // Camera