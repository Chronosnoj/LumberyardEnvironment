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
#include "OculusDevice.h"
#include "OculusTouchController.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Matrix4x4.h>
#include <AzCore/Math/MathUtils.h>
#include <MathConversion.h>

#define OVR_D3D_VERSION 11
#include <d3d11.h>
#include <OVR_CAPI_D3D.h>

#define LogMessage(...) CryLogAlways("[HMD][Oculus] - " __VA_ARGS__);

namespace Oculus
{

// Compute the symmetrical FOV that will be used for frustum culling.
ovrFovPort ComputeSymmetricalFOV(ovrHmdDesc& hmdDesc)
{
    const ovrFovPort* fovLeftEye  = &(hmdDesc.DefaultEyeFov[ovrEye_Left]);
    const ovrFovPort* fovRightEye = &(hmdDesc.DefaultEyeFov[ovrEye_Right]);

    // Determine the maximum extents of the FOV for both eyes.
    ovrFovPort fovMax;
    fovMax.UpTan    = max(fovLeftEye->UpTan,    fovRightEye->UpTan);
    fovMax.DownTan  = max(fovLeftEye->DownTan,  fovRightEye->DownTan);
    fovMax.LeftTan  = max(fovLeftEye->LeftTan,  fovRightEye->LeftTan);
    fovMax.RightTan = max(fovLeftEye->RightTan, fovRightEye->RightTan);

    return fovMax;
}

AZ::Quaternion OculusQuatToLYQuat(const ovrQuatf& ovrQuat)
{
    AZ::Quaternion quat(ovrQuat.x, ovrQuat.y, ovrQuat.z, ovrQuat.w);
    AZ::Matrix3x3 m33 = AZ::Matrix3x3::CreateFromQuaternion(quat);

    AZ::Vector3 column1 = -m33.GetColumn(2);
    m33.SetColumn(2, m33.GetColumn(1));
    m33.SetColumn(1, column1);
    AZ::Quaternion rotation = AZ::Quaternion::CreateFromMatrix3x3(m33);

    AZ::Quaternion result = AZ::Quaternion::CreateRotationX(AZ::Constants::HalfPi) * rotation;
    return result;
}

AZ::Vector3 OculusVec3ToLYVec3(const ovrVector3f& vec)
{
    AZ::Vector3 result;
    result.Set(vec.x, -vec.z, vec.y);
    return result;
}

void CopyPose(const ovrPoseStatef& src, AZ::VR::PoseState& poseDest, AZ::VR::DynamicsState& dynamicsDest)
{
    const ovrPosef* pose = &(src.ThePose);

    poseDest.orientation = OculusQuatToLYQuat(pose->Orientation);
    poseDest.position = OculusVec3ToLYVec3(pose->Position);

    dynamicsDest.angularVelocity = OculusVec3ToLYVec3(src.AngularVelocity);
    dynamicsDest.angularAcceleration = OculusVec3ToLYVec3(src.AngularAcceleration);

    dynamicsDest.linearVelocity = OculusVec3ToLYVec3(src.LinearVelocity);
    dynamicsDest.linearAcceleration = OculusVec3ToLYVec3(src.LinearAcceleration);
}

void OculusDevice::Reflect(AZ::ReflectContext* context)
{
    if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
    {
        serializeContext->Class<OculusDevice, AZ::Component>()
            ->Version(1)
            ->SerializerForEmptyClass()
            ;

        if (AZ::EditContext* editContext = serializeContext->GetEditContext())
        {
            editContext->Class<OculusDevice>(
                "Oculus Device Manager", "")
                ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "VR")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
                ;
        }
    }
}

void OculusDevice::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
{
    provided.push_back(AZ_CRC("HMDDevice"));
    provided.push_back(AZ_CRC("OculusDevice"));
}

void OculusDevice::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
{
    incompatible.push_back(AZ_CRC("OculusDevice"));
}

void OculusDevice::Init()
{
    m_controller = new OculusTouchController; // Note that this will be deleted by the input system and should not be deleted here.
}

void OculusDevice::Activate()
{
    AZ::VR::HMDInitRequestBus::Handler::BusConnect();
}

void OculusDevice::Deactivate()
{
    AZ::VR::HMDInitRequestBus::Handler::BusDisconnect();
}

bool OculusDevice::AttemptInit()
{
    LogMessage("Attempting to initialize Oculus SDK " OVR_VERSION_STRING);

    bool success = false;

    // Try and connect to the Oculus SDK.
    if (OVR_SUCCESS(ovr_Initialize(nullptr)))
    {
        // Connection succeeded, create the device properly.

        ovrGraphicsLuid luid;
        ovrResult result = ovr_Create(&m_session, &luid);
        if (m_session && OVR_SUCCESS(result))
        {
            m_hmdDesc = ovr_GetHmdDesc(m_session);
            ovrFovPort symmetricFOV = ComputeSymmetricalFOV(m_hmdDesc);

            m_deviceInfo.productName  = m_hmdDesc.ProductName;
            m_deviceInfo.manufacturer = m_hmdDesc.Manufacturer;
            m_deviceInfo.fovH         = atanf(symmetricFOV.LeftTan) + atanf(symmetricFOV.RightTan);
            m_deviceInfo.fovV         = atanf(symmetricFOV.UpTan) + atanf(symmetricFOV.DownTan);

            {
                ovrSizei leftEyeSize  = ovr_GetFovTextureSize(m_session, ovrEye_Left,  m_hmdDesc.DefaultEyeFov[0], 1.0f);
                ovrSizei rightEyeSize = ovr_GetFovTextureSize(m_session, ovrEye_Right, m_hmdDesc.DefaultEyeFov[1], 1.0f);

                m_deviceInfo.renderWidth  = std::max(leftEyeSize.w, rightEyeSize.w);
                m_deviceInfo.renderHeight = std::max(leftEyeSize.h, rightEyeSize.h);

                m_verticalFOVs[STEREO_EYE_LEFT] = atanf(m_hmdDesc.DefaultEyeFov[ovrEye_Left].DownTan) + atanf(m_hmdDesc.DefaultEyeFov[ovrEye_Left].UpTan);
                m_verticalFOVs[STEREO_EYE_RIGHT] = atanf(m_hmdDesc.DefaultEyeFov[ovrEye_Right].DownTan) + atanf(m_hmdDesc.DefaultEyeFov[ovrEye_Right].UpTan);
            }

            // Enable any touch controllers that may be connected.
            m_controller->Enable(true);

            // Default the tracking origin to the head.
            ovr_SetTrackingOriginType(m_session, ovrTrackingOrigin_EyeLevel);

            // Ensure that queue-ahead is enabled.
            ovr_SetBool(m_session, "QueueAheadEnabled", true);

            // Connect to the HMDDeviceBus in order to get HMD messages from the rest of the VR system.
            AZ::VR::HMDDeviceRequestBus::Handler::BusConnect();
            m_controller->ConnectToControllerBus();
            OculusRequestBus::Handler::BusConnect();

            success = true;
        }
    }

    if (!success)
    {
        Shutdown();

        ovrErrorInfo error;
        ovr_GetLastErrorInfo(&error);
        LogMessage("Failed to initialize Oculus SDK [%s]", *error.ErrorString ? error.ErrorString : "unspecified error");
    }

    return success;
}

AZ::VR::HMDInitBus::HMDInitPriority OculusDevice::GetInitPriority() const
{
    return HMDInitPriority::kHighest;
}

void OculusDevice::Shutdown()
{
    m_controller->DisconnectFromControllerBus();

    AZ::VR::HMDDeviceRequestBus::Handler::BusDisconnect();
    OculusRequestBus::Handler::BusDisconnect();
    if (m_session)
    {
        ovr_Destroy(m_session);
        m_session = nullptr;
    }

    ovr_Shutdown();
}

void OculusDevice::GetPerEyeCameraInfo(const EStereoEye eye, AZ::VR::PerEyeCameraInfo& cameraInfo)
{
    // Grab the frustum planes from the per-eye projection matrix.
    {
        const float zNear = 0.25f; // garbage values, not important for the planes we need.
        const float zFar = 8000.0f;
        ovrMatrix4f projMatrix = ovrMatrix4f_Projection(m_hmdDesc.DefaultEyeFov[eye], zNear, zFar, ovrProjection_None);
        
        cameraInfo.fov = m_verticalFOVs[eye];
        cameraInfo.aspectRatio = projMatrix.M[1][1] / projMatrix.M[0][0];

        const float denom = 1.0f / projMatrix.M[1][1];
        cameraInfo.frustumPlane.horizontalDistance = projMatrix.M[0][2] * denom * cameraInfo.aspectRatio;
        cameraInfo.frustumPlane.verticalDistance   = projMatrix.M[1][2] * denom;
    }

    // Get the world-space offset from the head position for this eye.
    {
        FrameParameters& frameParams = GetFrameParameters();
        ovrVector3f eyeOffset = frameParams.viewScaleDesc.HmdToEyeOffset[eye];
        cameraInfo.eyeOffset = OculusVec3ToLYVec3(eyeOffset);
    }
}

bool OculusDevice::CreateRenderTargetsDX11(void* renderDevice, const TextureDesc& desc, size_t eyeCount, AZ::VR::HMDRenderTarget* renderTargets[]) const
{
    bool success = false;

    for (size_t i = 0; i < eyeCount; i++)
    {
        ID3D11Device* d3dDevice = static_cast<ID3D11Device*>(renderDevice);

        ovrTextureSwapChainDesc textureDesc = {};
        textureDesc.Type                    = ovrTexture_2D;
        textureDesc.ArraySize               = 1;
        textureDesc.Format                  = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
        textureDesc.Width                   = desc.width;
        textureDesc.Height                  = desc.height;
        textureDesc.MipLevels               = 1;
        textureDesc.SampleCount             = 1;
        textureDesc.MiscFlags               = ovrTextureMisc_DX_Typeless;
        textureDesc.BindFlags               = ovrTextureBind_DX_RenderTarget | ovrTextureBind_DX_UnorderedAccess;
        textureDesc.StaticImage             = ovrFalse;

        ovrTextureSwapChain textureChain = nullptr;
        ovrResult result = ovr_CreateTextureSwapChainDX(m_session, d3dDevice, &textureDesc, &textureChain);

        success = OVR_SUCCESS(result) && (textureChain != nullptr);
        if (success)
        {
            int numTextures = 0;
            ovr_GetTextureSwapChainLength(m_session, textureChain, &(numTextures));
            renderTargets[i]->numTextures = numTextures;

            renderTargets[i]->deviceSwapTextureSet = textureChain;
            renderTargets[i]->textures = new void*[numTextures];
            for (int t = 0; t < numTextures; ++t)
            {
                ID3D11Texture2D* texture = nullptr;
                result = ovr_GetTextureSwapChainBufferDX(m_session, textureChain, t, IID_PPV_ARGS(&texture));
                renderTargets[i]->textures[t] = texture;

                if (!OVR_SUCCESS(result))
                {
                    success = false;
                    break;
                }
            }
        }

        if (!success)
        {
            ovrErrorInfo errorInfo;
            ovr_GetLastErrorInfo(&errorInfo);
            LogMessage("%s [%s]", "Unable to create D3D11 texture swap chain!", *errorInfo.ErrorString ? errorInfo.ErrorString : "unspecified error");
        }
    }

    return success;
}

void OculusDevice::DestroyRenderTarget(AZ::VR::HMDRenderTarget& renderTarget) const
{
    ovr_DestroyTextureSwapChain(m_session, static_cast<ovrTextureSwapChain>(renderTarget.deviceSwapTextureSet));
    SAFE_DELETE_ARRAY(renderTarget.textures);
    renderTarget.textures = nullptr;
}

void OculusDevice::Update()
{
    const int frameId = gEnv->pRenderer->GetFrameID(false);

    // Get the eye poses, with IPD (interpupillary distance) already included.
    ovrEyeRenderDesc eyeRenderDesc[2];
    eyeRenderDesc[0] = ovr_GetRenderDesc(m_session, ovrEye_Left, m_hmdDesc.DefaultEyeFov[0]);
    eyeRenderDesc[1] = ovr_GetRenderDesc(m_session, ovrEye_Right, m_hmdDesc.DefaultEyeFov[1]);

    ovrPosef eyePose[2];
    ovrVector3f hmdToEyeOffset[2] =
    {
        eyeRenderDesc[0].HmdToEyeOffset,
        eyeRenderDesc[1].HmdToEyeOffset
    };

    // Calculate the current tracking state.
    double predictedTime = ovr_GetPredictedDisplayTime(m_session, frameId);
    ovrTrackingState currentTrackingState = ovr_GetTrackingState(m_session, predictedTime, ovrTrue);
    ovr_CalcEyePoses(currentTrackingState.HeadPose.ThePose, hmdToEyeOffset, eyePose);

    CopyPose(currentTrackingState.HeadPose, m_trackingState.pose, m_trackingState.dynamics);

    // Specify what is currently being tracked.
    int statusFlags = 0;
    {
        ovrSessionStatus sessionStatus;
        ovr_GetSessionStatus(m_session, &sessionStatus);

        statusFlags = AZ::VR::HMDStatus_CameraPoseTracked |
            ((!sessionStatus.DisplayLost) ? AZ::VR::HMDStatus_HmdConnected | AZ::VR::HMDStatus_PositionConnected : 0)      |
            ((currentTrackingState.StatusFlags & ovrStatus_OrientationTracked) ? AZ::VR::HMDStatus_OrientationTracked : 0) |
            ((currentTrackingState.StatusFlags & ovrStatus_PositionTracked) ? AZ::VR::HMDStatus_PositionTracked : 0);

        m_trackingState.statusFlags = statusFlags;
    }

    // Cache the current tracking state for later submission after this frame has finished rendering.
    {
        FrameParameters& frameParams = GetFrameParameters();
        for (uint32 eye = 0; eye < 2; ++eye)
        {
            frameParams.eyeFovs[eye]                        = m_hmdDesc.DefaultEyeFov[eye];
            frameParams.eyePoses[eye]                       = eyePose[eye];
            frameParams.viewScaleDesc.HmdToEyeOffset[eye]   = hmdToEyeOffset[eye];
        }

        frameParams.frameID          = frameId;
        frameParams.sensorTimeSample = ovr_GetTimeInSeconds();
    }

    // Update the current controller state.
    {
        AZ::VR::TrackingState controllerTrackingStates[AZ::VR::ControllerIndex::MaxNumControllers];
        CopyPose(currentTrackingState.HandPoses[ovrHand_Left], controllerTrackingStates[static_cast<uint32_t>(AZ::VR::ControllerIndex::LeftHand)].pose, controllerTrackingStates[static_cast<uint32_t>(AZ::VR::ControllerIndex::LeftHand)].dynamics);
        CopyPose(currentTrackingState.HandPoses[ovrHand_Right], controllerTrackingStates[static_cast<uint32_t>(AZ::VR::ControllerIndex::RightHand)].pose, controllerTrackingStates[static_cast<uint32_t>(AZ::VR::ControllerIndex::RightHand)].dynamics);

        for (size_t controllerIndex = 0; controllerIndex < static_cast<uint32_t>(AZ::VR::ControllerIndex::MaxNumControllers); ++controllerIndex)
        {
            controllerTrackingStates[controllerIndex].statusFlags = statusFlags;
        }

        ovrInputState inputState;
        ovr_GetInputState(m_session, ovrControllerType_Touch, &inputState);
        m_controller->SetCurrentState(controllerTrackingStates, inputState);
    }
}

AZ::VR::TrackingState* OculusDevice::GetTrackingState()
{
    return &m_trackingState;
}

void OculusDevice::SubmitFrame(const EyeTarget& left, const EyeTarget& right)
{
    FRAME_PROFILER("OculusDevice::SubmitFrame", gEnv->pSystem, PROFILE_SYSTEM);

    const EyeTarget* eyes[] = { &left, &right };
    FrameParameters& frameParams = GetFrameParameters();

    ovrLayerEyeFov layer;
    layer.Header.Type = ovrLayerType_EyeFov;
    layer.Header.Flags = 0;
    for (uint32 eye = 0; eye < 2; ++eye)
    {
        layer.ColorTexture[eye]     = static_cast<ovrTextureSwapChain>(eyes[eye]->renderTarget);
        layer.Viewport[eye].Pos.x   = eyes[eye]->viewportPosition.x;
        layer.Viewport[eye].Pos.y   = eyes[eye]->viewportPosition.y;
        layer.Viewport[eye].Size.w  = eyes[eye]->viewportSize.x;
        layer.Viewport[eye].Size.h  = eyes[eye]->viewportSize.y;
        layer.Fov[eye]              = frameParams.eyeFovs[eye];
        layer.RenderPose[eye]       = frameParams.eyePoses[eye];
        layer.SensorSampleTime      = frameParams.sensorTimeSample;

        ovr_CommitTextureSwapChain(m_session, layer.ColorTexture[eye]);
    }

    ovrLayerHeader* layers = &layer.Header;
    ovrResult result = ovr_SubmitFrame(m_session, frameParams.frameID, &frameParams.viewScaleDesc, &layers, 1);
    if (!OVR_SUCCESS(result))
    {
        ovrErrorInfo errorInfo;
        ovr_GetLastErrorInfo(&errorInfo);
        LogMessage("%s [%s]", "Unable to submit frame to the Oculus device!", *errorInfo.ErrorString ? errorInfo.ErrorString : "unspecified error");
    }
}

void OculusDevice::RecenterPose()
{
    if (m_session)
    {
        ovr_RecenterTrackingOrigin(m_session);
    }
}

void OculusDevice::SetTrackingLevel(const AZ::VR::HMDTrackingLevel level)
{
    switch (level)
    {
        case AZ::VR::HMDTrackingLevel::kHead:
            ovr_SetTrackingOriginType(m_session, ovrTrackingOrigin_EyeLevel);
            break;

        case AZ::VR::HMDTrackingLevel::kFloor:
            ovr_SetTrackingOriginType(m_session, ovrTrackingOrigin_FloorLevel);
            break;

        default:
            AZ_Assert(false, "Unknown tracking level %d requested for the Oculus", static_cast<int>(level));
            break;
    }
}

OculusDevice::FrameParameters& OculusDevice::GetFrameParameters()
{
    static IRenderer* renderer = gEnv->pRenderer;
    const int frameID = renderer->GetFrameID(false);

    return m_frameParams[frameID & 1]; // these parameters are double-buffered.
}

void OculusDevice::OutputHMDInfo()
{
    LogMessage("Device: %s", m_hmdDesc.ProductName ? m_hmdDesc.ProductName : "unknown");
    LogMessage("- Manufacturer: %s", m_hmdDesc.Manufacturer ? m_hmdDesc.Manufacturer : "unknown");
    LogMessage("- VendorId: %d", m_hmdDesc.VendorId);
    LogMessage("- ProductId: %d", m_hmdDesc.ProductId);
    LogMessage("- Firmware: %d.%d", m_hmdDesc.FirmwareMajor, m_hmdDesc.FirmwareMinor);
    LogMessage("- SerialNumber: %s", m_hmdDesc.SerialNumber);
    LogMessage("- Render Resolution: %dx%d", m_deviceInfo.renderWidth * 2, m_deviceInfo.renderHeight);
    LogMessage("- Horizontal FOV: %.2f degrees", RAD2DEG(m_deviceInfo.fovH));
    LogMessage("- Vertical FOV: %.2f degrees", RAD2DEG(m_deviceInfo.fovV));

    LogMessage("- Sensor orientation tracking: %s", (m_hmdDesc.AvailableTrackingCaps & ovrTrackingCap_Orientation) ? "supported" : "unsupported");
    LogMessage("- Sensor mag yaw correction: %s", (m_hmdDesc.AvailableTrackingCaps & ovrTrackingCap_MagYawCorrection) ? "supported" : "unsupported");
    LogMessage("- Sensor position tracking: %s", (m_hmdDesc.AvailableTrackingCaps & ovrTrackingCap_Position) ? "supported" : "unsupported");
}

void OculusDevice::EnableDebugging(bool enable)
{
    if (enable)
    {
        ovr_SetInt(m_session, OVR_PERF_HUD_MODE, ovrPerfHud_PerfSummary);
    }
    else
    {
        ovr_SetInt(m_session, OVR_PERF_HUD_MODE, ovrPerfHud_Off);
        ovr_SetInt(m_session, OVR_LAYER_HUD_MODE, ovrLayerHud_Off);
    }
}

AZ::VR::HMDDeviceInfo* OculusDevice::GetDeviceInfo() 
{
    return &m_deviceInfo;
}

bool OculusDevice::IsInitialized()
{
    return m_session != nullptr;
}

} // namespace Oculus
