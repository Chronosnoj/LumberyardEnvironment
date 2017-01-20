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
#include "HMDDebuggerComponent.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>

// For debug rendering
#include "../CryAction/IViewSystem.h"
#include <IGame.h>
#include <IGameFramework.h>
#include <IRenderAuxGeom.h>
#include <MathConversion.h>

void HMDDebuggerComponent::Reflect(AZ::ReflectContext* context)
{
    if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
    {
        serializeContext->Class<HMDDebuggerComponent, AZ::Component>()
            ->Version(1)
            ->SerializerForEmptyClass()
            ;

        if (AZ::EditContext* editContext = serializeContext->GetEditContext())
        {
            editContext->Class<HMDDebuggerComponent>(
                "HMD Debugger", "")
                ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                ->Attribute(AZ::Edit::Attributes::Category, "VR")
                ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
                ;
        }
    }
}

void HMDDebuggerComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
{
    provided.push_back(AZ_CRC("HMDDebugger"));
}

void HMDDebuggerComponent::Init()
{
}

void HMDDebuggerComponent::Activate()
{
    AZ::VR::HMDDebuggerRequestBus::Handler::BusConnect();
}

void HMDDebuggerComponent::Deactivate()
{
    AZ::VR::HMDDebuggerRequestBus::Handler::BusDisconnect();
}

void HMDDebuggerComponent::Enable(bool enable)
{
    if (enable)
    {
        AZ::TickBus::Handler::BusConnect();
    }
    else
    {
        AZ::TickBus::Handler::BusDisconnect();
    }

    EBUS_EVENT(AZ::VR::HMDDeviceRequestBus, EnableDebugging, enable);
}

void DrawHMDController(AZ::VR::ControllerIndex id, const SViewParams* viewParameters, IRenderAuxGeom* auxGeom)
{
    // The HMD and controllers are tracked in their own local spaces which are converted into a world-space offset
    // internally in the device. This offset is relative to the re-centered pose of the HMD. To actually render something
    // like a controller is world space, we need to get the current entity that is attached to the view system and use its
    // transformations (viewParameters).

    bool connected = false;
    EBUS_EVENT_RESULT(connected, AZ::VR::ControllerRequestBus, IsConnected, id);

    if (connected)
    {
        // Draw a cross with a line pointing backwards along the controller. These positions are in local space.
        float size = 0.05f;
        Vec3 leftPoint(-size, 0.0f, 0.0f);
        Vec3 rightPoint(size, 0.0f, 0.0f);
        Vec3 upPoint(0.0f, 0.0f, size);
        Vec3 downPoint(0.0f, 0.0f, -size);
        Vec3 center(0.0f, 0.0f, 0.0f);
        Vec3 back(0.0f, -size, 0.0f);

        AZ::VR::TrackingState* state = nullptr;
        EBUS_EVENT_RESULT(state, AZ::VR::ControllerRequestBus, GetTrackingState, id);
        Quat orientation = viewParameters->rotation * AZQuaternionToLYQuaternion(state->pose.orientation);

        Matrix34 transform;
        transform.SetRotation33(Matrix33(orientation));
        transform.SetTranslation(viewParameters->position + (viewParameters->rotation * AZVec3ToLYVec3(state->pose.position)));

        leftPoint  = transform * leftPoint;
        rightPoint = transform * rightPoint;
        upPoint    = transform * upPoint;
        downPoint  = transform * downPoint;
        center     = transform * center;
        back       = transform * back;

        ColorB white(255, 255, 255);
        auxGeom->DrawLine(leftPoint, white, rightPoint, white);
        auxGeom->DrawLine(upPoint, white, downPoint, white);
        auxGeom->DrawLine(center, white, back, white);
    }
}

void HMDDebuggerComponent::OnTick(float deltaTime, AZ::ScriptTimePoint time)
{
    IViewSystem* viewSystem = gEnv->pGame->GetIGameFramework()->GetIViewSystem();
    if (viewSystem)
    {
        IView* mainView = viewSystem->GetActiveView();
        if (mainView)
        {
            const SViewParams* viewParams = mainView->GetCurrentParams();
            IRenderAuxGeom* auxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

            {
                DrawHMDController(AZ::VR::ControllerIndex::LeftHand, viewParams, auxGeom);
                DrawHMDController(AZ::VR::ControllerIndex::RightHand, viewParams, auxGeom);
            }

            {
                // Let the device do any device-specific debugging output that it wants.
                AZ::Quaternion rotation = LYQuaternionToAZQuaternion(viewParams->rotation);
                AZ::Vector3 translation = LYVec3ToAZVec3(viewParams->position);
                AZ::Transform transform = AZ::Transform::CreateFromQuaternionAndTranslation(rotation, translation);
                EBUS_EVENT(AZ::VR::HMDDeviceRequestBus, DrawDebugInfo, transform, auxGeom);
            }
        }
    }
}
