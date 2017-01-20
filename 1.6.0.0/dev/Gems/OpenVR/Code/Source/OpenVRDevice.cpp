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
#include "OpenVRDevice.h"
#include "OpenVRController.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <IRenderAuxGeom.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Matrix4x4.h>
#include <AzCore/Math/MathUtils.h>
#include <MathConversion.h>
#include <d3d11.h>

#define LogMessage(...) CryLogAlways("[HMD][OpenVR] - " __VA_ARGS__);

namespace OpenVR
{

std::string GetTrackedDeviceString(vr::IVRSystem* system, vr::TrackedDeviceIndex_t device, vr::ETrackedDeviceProperty property, vr::ETrackedPropertyError* error)
{
    uint32 bufferLength = system->GetStringTrackedDeviceProperty(device, property, nullptr, 0, error);
    if (bufferLength == 0)
    {
        return "";
    }

    char* buffer = new char[bufferLength];
    bufferLength = system->GetStringTrackedDeviceProperty(device, property, buffer, bufferLength, error);
    std::string result(buffer);
    delete [] buffer;
    return result;
}

const char* GetTrackedDeviceCharPointer(vr::IVRSystem* system, vr::TrackedDeviceIndex_t device, vr::ETrackedDeviceProperty property, vr::ETrackedPropertyError* error)
{
    uint32_t bufferLength = system->GetStringTrackedDeviceProperty(device, property, nullptr, 0, error);
    if (bufferLength == 0)
    {
        return nullptr;
    }

    char* buffer = new char[bufferLength];
    system->GetStringTrackedDeviceProperty(device, property, buffer, bufferLength, error);
    return const_cast<char*>(buffer);
}

AZ::Quaternion OpenVRQuatToLYQuat(const AZ::Quaternion& quat)
{
    AZ::Matrix3x3 m33 = AZ::Matrix3x3::CreateFromQuaternion(quat);

    AZ::Vector3 column1 = -m33.GetColumn(2);
    m33.SetColumn(2, m33.GetColumn(1));
    m33.SetColumn(1, column1);
    AZ::Quaternion rotation = AZ::Quaternion::CreateFromMatrix3x3(m33);

    AZ::Quaternion result = AZ::Quaternion::CreateRotationX(AZ::Constants::HalfPi) * rotation;
    return result;
}

AZ::Vector3 OpenVRVec3ToLYVec3(const vr::HmdVector3_t& vec)
{
    AZ::Vector3 result;
    result.Set(vec.v[0], -vec.v[2], vec.v[1]);
    return result;
}

void BuildMatrix(const vr::HmdMatrix34_t& in, AZ::Matrix4x4& resultMatrix)
{
    resultMatrix = AZ::Matrix4x4::CreateIdentity();
    resultMatrix.SetRow(0, AZ::Vector4(in.m[0][0], in.m[0][1], in.m[0][2], in.m[0][3]));
    resultMatrix.SetRow(1, AZ::Vector4(in.m[1][0], in.m[1][1], in.m[1][2], in.m[1][3]));
    resultMatrix.SetRow(2, AZ::Vector4(in.m[2][0], in.m[2][1], in.m[2][2], in.m[2][3]));
}

void CopyPose(const vr::TrackedDevicePose_t& src, AZ::VR::PoseState& poseDest, AZ::VR::DynamicsState& dynamicDest)
{
    AZ::Matrix4x4 matrix;
    BuildMatrix(src.mDeviceToAbsoluteTracking, matrix);

    {
        AZ::Vector3 translation = matrix.GetTranslation();
        vr::HmdVector3_t openVRPosition;
        openVRPosition.v[0] = translation.GetX();
        openVRPosition.v[1] = translation.GetY();
        openVRPosition.v[2] = translation.GetZ();

        poseDest.position = OpenVRVec3ToLYVec3(openVRPosition);
    }

    AZ::Quaternion openvrQuaternion = AZ::Quaternion::CreateFromMatrix4x4(matrix);
    poseDest.orientation = OpenVRQuatToLYQuat(openvrQuaternion);

    dynamicDest.angularVelocity = OpenVRVec3ToLYVec3(src.vAngularVelocity);
    dynamicDest.angularAcceleration = AZ::Vector3(0.0f, 0.0f, 0.0f); // OpenVR does not support accelerations.

    dynamicDest.linearVelocity = OpenVRVec3ToLYVec3(src.vVelocity);
    dynamicDest.angularAcceleration = AZ::Vector3(0.0f, 0.0f, 0.0f); // OpenVR does not support accelerations.
}

vr::EVREye MapOpenVREyeToLY(const EStereoEye eye)
{
    switch (eye)
    {
    case STEREO_EYE_LEFT:
        return vr::EVREye::Eye_Left;
        break;

    case STEREO_EYE_RIGHT:
        return vr::EVREye::Eye_Right;
        break;

    default:
        AZ_Assert(false, "Attempting to map an invalid eye type");
        break;
    }

    return vr::EVREye::Eye_Left; // default to left.
}

void OpenVRDevice::Reflect(AZ::ReflectContext* context)
{
    if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
    {
        serializeContext->Class<OpenVRDevice, AZ::Component>()
            ->Version(1)
            ->SerializerForEmptyClass()
            ;

        if (AZ::EditContext* editContext = serializeContext->GetEditContext())
        {
            editContext->Class<OpenVRDevice>(
                "OpenVR Device Manager", "")
                ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "VR")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
                ;
        }
    }
}

void OpenVRDevice::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
{
    provided.push_back(AZ_CRC("HMDDevice"));
    provided.push_back(AZ_CRC("OpenVRDevice"));
}

void OpenVRDevice::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
{
    incompatible.push_back(AZ_CRC("OpenVRDevice"));
}

void OpenVRDevice::Init()
{
    memset(m_trackedDevicePose, 0, sizeof(m_trackedDevicePose));
    m_controller = new OpenVRController; // Note that this will be deleted by the input system and should not be deleted here.
}

void OpenVRDevice::Activate()
{
    // Connect to the EBus so that we can receive messages.
    AZ::VR::HMDInitRequestBus::Handler::BusConnect();
}

void OpenVRDevice::Deactivate()
{
    AZ::VR::HMDInitRequestBus::Handler::BusDisconnect();
}

bool OpenVRDevice::AttemptInit()
{
    LogMessage("Attempting to initialize OpenVR SDK");

    bool success = false;

    vr::EVRInitError error = vr::EVRInitError::VRInitError_None;
    m_system = vr::VR_Init(&error, vr::EVRApplicationType::VRApplication_Scene);

    if (error == vr::EVRInitError::VRInitError_None)
    {
        if (!vr::VR_IsHmdPresent())
        {
            LogMessage("No HMD connected");
        }
        else
        {
            m_compositor = static_cast<vr::IVRCompositor*>(vr::VR_GetGenericInterface(vr::IVRCompositor_Version, &error));
            if (error == vr::EVRInitError::VRInitError_None)
            {
                m_system->GetRecommendedRenderTargetSize(&m_deviceInfo.renderWidth, &m_deviceInfo.renderHeight);

                m_deviceInfo.manufacturer = GetTrackedDeviceCharPointer(m_system, vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_ManufacturerName_String, nullptr);
                m_deviceInfo.productName = GetTrackedDeviceCharPointer(m_system, vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_ModelNumber_String, nullptr);

                // Determine the device's symmetric field of view.
                {
                    vr::HmdMatrix44_t projectionMatrix = m_system->GetProjectionMatrix(vr::EVREye::Eye_Left, 0.01, 1000.0f, vr::EGraphicsAPIConvention::API_DirectX);

                    float denom = 1.0f / projectionMatrix.m[1][1];
                    float fovv = 2.0f * atanf(denom);
                    float aspectRatio = projectionMatrix.m[1][1] / projectionMatrix.m[0][0];
                    float fovh = 2.0f * atanf(denom * aspectRatio);

                    m_deviceInfo.fovH = fovh;
                    m_deviceInfo.fovV = fovv;
                }

                // Check for any controllers that may be connected.
                {
                    m_controller->Enable(true);
                    for (int ii = 0; ii < vr::k_unMaxTrackedDeviceCount; ++ii)
                    {
                        if (m_system->GetTrackedDeviceClass(ii) == vr::TrackedDeviceClass_Controller)
                        {
                            if (m_system->IsTrackedDeviceConnected(ii))
                            {
                                m_controller->ConnectController(ii);
                            }
                        }
                    }
                }

                SetTrackingLevel(AZ::VR::HMDTrackingLevel::kFloor); // Default to a seated experience.

                // Connect to the HMDDeviceBus in order to get HMD messages from the rest of the VR system.
                AZ::VR::HMDDeviceRequestBus::Handler::BusConnect();
                m_controller->ConnectToControllerBus();
                OpenVRRequestBus::Handler::BusConnect();

                success = true;
            }
        }
    }

    if (!success)
    {
        // Something went wrong during initialization.
        if (error != vr::EVRInitError::VRInitError_None)
        {
            LogMessage("Unable to initialize VR runtime: %s", vr::VR_GetVRInitErrorAsEnglishDescription(error));
        }

        Shutdown();
    }

    return success;
}

AZ::VR::HMDInitBus::HMDInitPriority OpenVRDevice::GetInitPriority() const
{
    return HMDInitPriority::kMiddle;
}

void OpenVRDevice::Shutdown()
{
    m_controller->DisconnectFromControllerBus();

    AZ::VR::HMDDeviceRequestBus::Handler::BusDisconnect();
    OpenVRRequestBus::Handler::BusDisconnect();
    vr::VR_Shutdown();
}

void OpenVRDevice::GetPerEyeCameraInfo(const EStereoEye eye, AZ::VR::PerEyeCameraInfo& cameraInfo)
{
    float left, right, top, bottom;
    m_system->GetProjectionRaw(MapOpenVREyeToLY(eye), &left, &right, &top, &bottom);

    cameraInfo.fov         = 2.0f * atanf((bottom - top) * 0.5f);
    cameraInfo.aspectRatio = (right - left) / (bottom - top);

    cameraInfo.frustumPlane.horizontalDistance = (right + left) * 0.5f;
    cameraInfo.frustumPlane.verticalDistance   = (bottom + top) * 0.5f;

    // Read the inter-pupilar distance in order to determine the eye offset.
    {
        float eyeDistance = m_system->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_UserIpdMeters_Float, nullptr);
        float xPosition = 0.0f;
        if (eye == STEREO_EYE_LEFT)
        {
            xPosition = -eyeDistance * 0.5f;
        }
        else
        {
            xPosition = eyeDistance * 0.5f;
        }

        cameraInfo.eyeOffset = AZ::Vector3(xPosition, 0.0f, 0.0f);
    }
}

bool OpenVRDevice::CreateRenderTargetsDX11(void* renderDevice, const TextureDesc& desc, size_t eyeCount, AZ::VR::HMDRenderTarget* renderTargets[]) const
{
    for (size_t i = 0; i < eyeCount; ++i)
    {
        ID3D11Device* d3dDevice = static_cast<ID3D11Device*>(renderDevice);

        D3D11_TEXTURE2D_DESC textureDesc;
        textureDesc.Width               = desc.width;
        textureDesc.Height              = desc.height;
        textureDesc.MipLevels           = 1;
        textureDesc.ArraySize           = 1;
        textureDesc.Format              = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.SampleDesc.Count    = 1;
        textureDesc.SampleDesc.Quality  = 0;
        textureDesc.Usage               = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags           = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        textureDesc.CPUAccessFlags      = 0;
        textureDesc.MiscFlags           = 0;

        ID3D11Texture2D* texture;
        d3dDevice->CreateTexture2D(&textureDesc, nullptr, &texture);

        // Create a OpenVR texture that is associated with this new D3D texture.
        vr::Texture_t* deviceTexture = new vr::Texture_t();
        deviceTexture->eColorSpace  = vr::EColorSpace::ColorSpace_Auto;
        deviceTexture->eType        = vr::EGraphicsAPIConvention::API_DirectX;
        deviceTexture->handle       = texture;

        // We only create one texture for OpenVR (no swapchain).
        renderTargets[i]->deviceSwapTextureSet = deviceTexture;
        renderTargets[i]->numTextures = 1;
        renderTargets[i]->textures = new void*[1];
        renderTargets[i]->textures[0] = texture;
    }

    return true;
}

void OpenVRDevice::DestroyRenderTarget(AZ::VR::HMDRenderTarget& renderTarget) const
{
    vr::TextureID_t* deviceTexture = static_cast<vr::TextureID_t*>(renderTarget.deviceSwapTextureSet);
    SAFE_DELETE(deviceTexture);
}

void OpenVRDevice::Update()
{
    // Process internal OpenVR events.
    {
        vr::VREvent_t event;
        while (m_system->PollNextEvent(&event, sizeof(vr::VREvent_t)))
        {
            switch (event.eventType)
            {
            case vr::VREvent_TrackedDeviceActivated:
            {
                vr::ETrackedDeviceClass deviceClass = m_system->GetTrackedDeviceClass(event.trackedDeviceIndex);
                if (deviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_Controller)
                {
                    // A new controller has been connected to the system, process it.
                    LogMessage("Controller connected:", event.trackedDeviceIndex);
                    LogMessage("- Tracked Device Index: %i", event.trackedDeviceIndex);
                    LogMessage("- Attached Device Id: %s", GetTrackedDeviceCharPointer(m_system, event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_AttachedDeviceId_String, nullptr));
                    LogMessage("- Tracking System Name: %s", GetTrackedDeviceCharPointer(m_system, event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_TrackingSystemName_String, nullptr));
                    LogMessage("- Model Number: %s", GetTrackedDeviceCharPointer(m_system, event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_ModelNumber_String, nullptr));
                    LogMessage("- Serial Number: %s", GetTrackedDeviceCharPointer(m_system, event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_SerialNumber_String, nullptr));
                    LogMessage("- Manufacturer: %s", GetTrackedDeviceCharPointer(m_system, event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_ManufacturerName_String, nullptr));
                    LogMessage("- Tracking Firmware Version: %s", GetTrackedDeviceCharPointer(m_system, event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_TrackingFirmwareVersion_String, nullptr));
                    LogMessage("- Hardware Revision: %s", GetTrackedDeviceCharPointer(m_system, event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_HardwareRevision_String, nullptr));
                    LogMessage("- Is Wireless: %s", (m_system->GetBoolTrackedDeviceProperty(event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_DeviceIsWireless_Bool) ? "True" : "False"));
                    LogMessage("- Is Charging: %s", (m_system->GetBoolTrackedDeviceProperty(event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_DeviceIsCharging_Bool) ? "True" : "False"));

                    m_controller->ConnectController(event.trackedDeviceIndex);
                }
                break;
            }

            case vr::VREvent_TrackedDeviceDeactivated:
            {
                vr::ETrackedDeviceClass deviceClass = m_system->GetTrackedDeviceClass(event.trackedDeviceIndex);
                if (deviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_Controller)
                {
                    // Controller has been disconnected.
                    LogMessage("Controller disconnected: %i", event.trackedDeviceIndex);

                    m_controller->DisconnectController(event.trackedDeviceIndex);
                }

                break;
            }

            default:
                break;
            }
        }
    }

    // Update tracking.
    {
        vr::Compositor_FrameTiming frameTiming;
        frameTiming.m_nSize = sizeof(vr::Compositor_FrameTiming);
        m_compositor->GetFrameTiming(&frameTiming, 0);

        float secondsSinceLastVsync;
        m_system->GetTimeSinceLastVsync(&secondsSinceLastVsync, nullptr);
        float frequency = m_system->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_DisplayFrequency_Float);
        float frameDuration = 1.0f / frequency;
        float secondsUntilPhotons = frameDuration - secondsSinceLastVsync + m_system->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_SecondsFromVsyncToPhotons_Float);
        m_system->GetDeviceToAbsoluteTrackingPose(m_trackingOrigin, secondsUntilPhotons, m_trackedDevicePose, vr::k_unMaxTrackedDeviceCount);

        for (int deviceIndex = 0; deviceIndex < vr::k_unMaxTrackedDeviceCount; ++deviceIndex)
        {
            AZ::VR::TrackingState::StatusFlags flags;
            flags = ((m_trackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid) ? AZ::VR::HMDStatus_OrientationTracked | AZ::VR::HMDStatus_PositionConnected | AZ::VR::HMDStatus_CameraPoseTracked : 0) |
                ((m_system->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd)) ? AZ::VR::HMDStatus_PositionConnected | AZ::VR::HMDStatus_HmdConnected : 0);

            if (m_trackedDevicePose[deviceIndex].bPoseIsValid && m_trackedDevicePose[deviceIndex].bDeviceIsConnected)
            {
                vr::ETrackedDeviceClass deviceClass = m_system->GetTrackedDeviceClass(deviceIndex);
                switch (deviceClass)
                {
                    case vr::ETrackedDeviceClass::TrackedDeviceClass_HMD:
                    {
                        m_trackingState.statusFlags = flags;
                        CopyPose(m_trackedDevicePose[deviceIndex], m_trackingState.pose, m_trackingState.dynamics);
                    }
                    break;

                    case vr::ETrackedDeviceClass::TrackedDeviceClass_Controller:
                    {
                        vr::VRControllerState_t buttonState;
                        if (m_system->GetControllerState(deviceIndex, &buttonState))
                        {
                            AZ::VR::TrackingState controllerTrackingState;
                            CopyPose(m_trackedDevicePose[deviceIndex], controllerTrackingState.pose, controllerTrackingState.dynamics);
                            controllerTrackingState.statusFlags = flags;

                            m_controller->SetCurrentState(deviceIndex, controllerTrackingState, buttonState);
                        }
                    }
                    break;

                    default:
                        break;
                }
            }
        }
    }
}

AZ::VR::TrackingState* OpenVRDevice::GetTrackingState()
{
    return &m_trackingState;
}

void OpenVRDevice::SubmitFrame(const EyeTarget& left, const EyeTarget& right)
{
    if (m_compositor)
    {
        m_compositor->Submit(vr::Eye_Left, static_cast<vr::Texture_t*>(left.renderTarget));
        m_compositor->Submit(vr::Eye_Right, static_cast<vr::Texture_t*>(right.renderTarget));

        m_compositor->WaitGetPoses(m_trackedDevicePose, vr::k_unMaxTrackedDeviceCount, nullptr, 0);
    }
}

void OpenVRDevice::RecenterPose()
{
    if (m_system)
    {
        m_system->ResetSeatedZeroPose();
    }
}

void OpenVRDevice::SetTrackingLevel(const AZ::VR::HMDTrackingLevel level)
{
    switch (level)
    {
        case AZ::VR::HMDTrackingLevel::kHead:
            m_trackingOrigin = vr::ETrackingUniverseOrigin::TrackingUniverseSeated;
            break;

        case AZ::VR::HMDTrackingLevel::kFloor:
            m_trackingOrigin = vr::ETrackingUniverseOrigin::TrackingUniverseStanding;
            break;

        default:
            AZ_Assert(false, "Unknown tracking level %d requested for the Oculus", static_cast<int>(level));
            break;
    }
}

void OpenVRDevice::OutputHMDInfo()
{
    LogMessage("Device: %s", m_deviceInfo.productName);
    LogMessage("- Manufacturer: %s", m_deviceInfo.manufacturer);
    LogMessage("- SerialNumber: %s", GetTrackedDeviceString(m_system, vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_SerialNumber_String, nullptr).c_str());
    LogMessage("- Firmware: %s", GetTrackedDeviceString(m_system, vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_TrackingFirmwareVersion_String, nullptr).c_str());
    LogMessage("- Hardware Revision: %s", GetTrackedDeviceString(m_system, vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_HardwareRevision_String, nullptr).c_str());
    LogMessage("- Display Firmware: 0x%x", m_system->GetUint64TrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_HardwareRevision_Uint64));
    LogMessage("- Render Resolution: %dx%d", m_deviceInfo.renderWidth * 2, m_deviceInfo.renderHeight);
    LogMessage("- Refresh Rate: %.2f", m_system->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_DisplayFrequency_Float));
    LogMessage("- FOVH: %.2f", RAD2DEG(m_deviceInfo.fovH));
    LogMessage("- FOVV: %.2f", RAD2DEG(m_deviceInfo.fovV));
}

void OpenVRDevice::EnableDebugging(bool enable)
{
    // Note that these windows are not persistent. OpenVR will only display performance/debug info if something is going wrong (not hitting
    // target FPS, dropping frames, etc.).
    vr::IVRSettings* vrSettings = vr::VRSettings();
    vrSettings->SetBool(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_DisplayDebug_Bool, enable);
    vrSettings->SetBool(vr::k_pch_Perf_Section, vr::k_pch_Perf_HeuristicActive_Bool, enable);
    vrSettings->SetBool(vr::k_pch_Perf_Section, vr::k_pch_Perf_NotifyInHMD_Bool, enable);
    vrSettings->SetBool(vr::k_pch_Perf_Section, vr::k_pch_Perf_NotifyOnlyOnce_Bool, false);
    vrSettings->SetBool(vr::k_pch_Perf_Section, vr::k_pch_Perf_AllowTimingStore_Bool, enable);
}
void OpenVRDevice::DrawDebugInfo(const AZ::Transform& transform, IRenderAuxGeom* auxGeom)
{
    // Draw the playspace.
    {
        Playspace space;
        GetPlayspace(space);

        ColorB white[] =
        {
            ColorB(255, 255, 255, 127),
            ColorB(255, 255, 255, 127),
            ColorB(255, 255, 255, 127),
            ColorB(255, 255, 255, 127),
            ColorB(255, 255, 255, 127),
            ColorB(255, 255, 255, 127),
        };

        // Offset to apply to avoid z-fighting.
        AZ::Vector3 offset(0.0f, 0.0f, 0.01f);
        const Vec3 planeTriangles[] =
        {
            // Triangle 1
            AZVec3ToLYVec3(transform * (space.corners[0] + offset)),
            AZVec3ToLYVec3(transform * (space.corners[1] + offset)),
            AZVec3ToLYVec3(transform * (space.corners[2] + offset)),

            // Triangle 2
            AZVec3ToLYVec3(transform * (space.corners[3] + offset)),
            AZVec3ToLYVec3(transform * (space.corners[0] + offset)),
            AZVec3ToLYVec3(transform * (space.corners[2] + offset))
        };

        SAuxGeomRenderFlags flags = auxGeom->GetRenderFlags();
        flags.SetAlphaBlendMode(e_AlphaBlended);
        auxGeom->SetRenderFlags(flags);
        auxGeom->DrawTriangles(&planeTriangles[0], 6, white);
    }
}

AZ::VR::HMDDeviceInfo* OpenVRDevice::GetDeviceInfo()
{
    return &m_deviceInfo;
}

bool OpenVRDevice::IsInitialized()
{
    return m_system != nullptr;
}

void OpenVRDevice::GetPlayspace(Playspace& playspace) const
{
    vr::HmdQuad_t openVRSpace;
    bool valid = vr::VRChaperone()->GetPlayAreaRect(&openVRSpace);

    playspace.corners[0] = OpenVRVec3ToLYVec3(openVRSpace.vCorners[0]);
    playspace.corners[1] = OpenVRVec3ToLYVec3(openVRSpace.vCorners[1]);
    playspace.corners[2] = OpenVRVec3ToLYVec3(openVRSpace.vCorners[2]);
    playspace.corners[3] = OpenVRVec3ToLYVec3(openVRSpace.vCorners[3]);
    playspace.isValid    = valid;
}

} // namespace OpenVR