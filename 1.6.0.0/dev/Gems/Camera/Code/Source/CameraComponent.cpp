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
#include "StdAfx.h"
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Component/TransformBus.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>

#include "CameraComponent.h"

#include <MathConversion.h>
#include <IGameFramework.h>
#include <IRenderer.h>

namespace Camera
{
    void CameraComponent::Init()
    {
        m_system = gEnv->pGame->GetIGameFramework()->GetISystem();
        // Initialize local view.
        m_viewSystem = gEnv->pGame->GetIGameFramework()->GetIViewSystem();
        if (!m_viewSystem)
        {
            AZ_Error("CameraComponent", m_viewSystem != nullptr, "The CameraComponent shouldn't be used without a local view system");
        }
    }

    void CameraComponent::Activate()
    {
        if (m_viewSystem)
        {
            if (m_view == nullptr)
            {
                m_view = m_viewSystem->CreateView();

                m_view->LinkTo(GetEntity());
                UpdateCamera();
            }

            m_viewSystem->SetActiveView(m_view);
            UpdateCamera();
        }
        CameraRequestBus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
    }

    void CameraComponent::Deactivate()
    {
        AZ::TransformNotificationBus::Handler::BusDisconnect(GetEntityId());
        CameraRequestBus::Handler::BusDisconnect(GetEntityId());
        if (m_viewSystem)
        {
            if (m_viewSystem->GetActiveView() == m_view)
            {
                m_viewSystem->SetActiveView(nullptr);
            }
            m_viewSystem->RemoveView(m_view);
            m_view = nullptr;
        }
    }

    void CameraComponent::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<CameraComponent, AZ::Component>()
                ->Version(1)
                ->Field("Field of View", &CameraComponent::m_fov)
                ->Field("Near Clip Plane Distance", &CameraComponent::m_nearClipPlaneDistance)
                ->Field("Far Clip Plane Distance", &CameraComponent::m_farClipPlaneDistance);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<CameraComponent>("Camera", "Responsible for registering as the view."
                    "Camera Rigs are a common way to drive a camera's behavior")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Game")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/Camera.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/Camera.png")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                    ->DataElement(0, &CameraComponent::m_fov, "Field of view", "Vertical field of view in degrees")
                        ->Attribute(AZ::Edit::Attributes::Min, FLT_EPSILON)
                        ->Attribute(AZ::Edit::Attributes::Suffix, " degrees")
                        ->Attribute(AZ::Edit::Attributes::Step, 1.f)
                        ->Attribute(AZ::Edit::Attributes::Max, AZ::RadToDeg(AZ::Constants::Pi))
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshValues"))
                    ->DataElement(0, &CameraComponent::m_nearClipPlaneDistance, "Near clip distance",
                        "Distance to the near clip plane of the view Frustum")
                        ->Attribute(AZ::Edit::Attributes::Min, CAMERA_MIN_NEAR)
                        ->Attribute(AZ::Edit::Attributes::Suffix, " m")
                        ->Attribute(AZ::Edit::Attributes::Step, 0.1f)
                        ->Attribute(AZ::Edit::Attributes::Max, &CameraComponent::GetFarClipDistance)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"))
                    ->DataElement(0, &CameraComponent::m_farClipPlaneDistance, "Far clip distance",
                        "Distance to the far clip plane of the view Frustum")
                        ->Attribute(AZ::Edit::Attributes::Min, &CameraComponent::GetNearClipDistance)
                        ->Attribute(AZ::Edit::Attributes::Suffix, " m")
                        ->Attribute(AZ::Edit::Attributes::Step, 10.f)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"))
                    ;
            }
        }
    }

    void CameraComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC("TransformService"));
    }

    void CameraComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("CameraService"));
    }

    void CameraComponent::UpdateCamera()
    {
        auto viewParams = *m_view->GetCurrentParams();
        viewParams.fov = AZ::DegToRad(m_fov);
        viewParams.nearplane = m_nearClipPlaneDistance;
        viewParams.farplane = m_farClipPlaneDistance;
        m_view->SetCurrentParams(viewParams);
    }

    float CameraComponent::GetFov() const
    {
        return m_fov;
    }

    float CameraComponent::GetNearClipDistance() const
    {
        return m_nearClipPlaneDistance;
    }

    float CameraComponent::GetFarClipDistance() const
    {
        return m_farClipPlaneDistance;
    }

    void CameraComponent::SetFov(float fov)
    {
        m_fov = AZ::GetClamp(fov, s_minFoV, s_maxFoV); UpdateCamera();
    }

    void CameraComponent::SetNearClipDistance(float nearClipDistance)
    {
        m_nearClipPlaneDistance = AZ::GetMin(nearClipDistance, m_farClipPlaneDistance); UpdateCamera();
    }

    void CameraComponent::SetFarClipDistance(float farClipDistance)
    {
        m_farClipPlaneDistance = AZ::GetMax(farClipDistance, m_nearClipPlaneDistance); UpdateCamera();
    }

    void CameraComponent::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        CCamera& camera = m_view->GetCamera();
        camera.SetMatrix(AZTransformToLYTransform(world.GetOrthogonalized()));
    }

    void CameraComponent::EditorDisplay(AzFramework::EntityDebugDisplayRequests& displayInterface, const AZ::Transform& world, bool& handled)
    {
        const float distance = 4.f;

        float tangent = static_cast<float>(tan(0.5f * AZ::DegToRad(m_fov)));
        float height = distance * tangent;
        float width = height * displayInterface.GetAspectRatio();

        AZ::Vector3 points[4];
        points[0] = AZ::Vector3( width, distance,  height);
        points[1] = AZ::Vector3(-width, distance,  height);
        points[2] = AZ::Vector3(-width, distance, -height);
        points[3] = AZ::Vector3( width, distance, -height);

        AZ::Vector3 start(0, 0, 0);

        displayInterface.PushMatrix(world);
        displayInterface.DrawLine(start, points[0]);
        displayInterface.DrawLine(start, points[1]);
        displayInterface.DrawLine(start, points[2]);
        displayInterface.DrawLine(start, points[3]);
        displayInterface.DrawPolyLine(points, AZ_ARRAY_SIZE(points));
        displayInterface.PopMatrix();
    }

} //namespace Camera

