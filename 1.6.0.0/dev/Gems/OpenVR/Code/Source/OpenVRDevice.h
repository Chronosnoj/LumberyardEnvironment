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
#include <OpenVR/OpenVRBus.h>
#include "HMDBus.h"
#include "OpenVRController.h"

#include <openvr.h>

namespace OpenVR
{
    
class OpenVRController;

class OpenVRDevice
    : public AZ::Component
    , public AZ::VR::HMDDeviceRequestBus::Handler
    , public AZ::VR::HMDInitRequestBus::Handler
    , private OpenVRRequestBus::Handler
{
    public:

        AZ_COMPONENT(OpenVRDevice, "{D0442255-503E-4867-A548-947A31AD7839}");
        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        OpenVRDevice() = default;
        ~OpenVRDevice() override = default;

        /// AZ::Component //////////////////////////////////////////////////////////
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////////

        /// HMDInitBus overrides /////////////////////////////////////////////////////
        bool AttemptInit() override;
        void Shutdown() override;
        AZ::VR::HMDInitBus::HMDInitPriority GetInitPriority() const override;
        //////////////////////////////////////////////////////////////////////////////

        /// IHMDDevice overrides ///////////////////////////////////////////////////
        void GetPerEyeCameraInfo(const EStereoEye eye, AZ::VR::PerEyeCameraInfo& cameraInfo) override;
        bool CreateRenderTargetsDX11(void* renderDevice, const TextureDesc& desc, size_t eyeCount, AZ::VR::HMDRenderTarget* renderTargets[STEREO_EYE_COUNT]) const override;
        void DestroyRenderTarget(AZ::VR::HMDRenderTarget& renderTarget) const override;
        void Update() override;
        AZ::VR::TrackingState* GetTrackingState() override;
        void SubmitFrame(const EyeTarget& left, const EyeTarget& right) override;
        void RecenterPose() override;
        void SetTrackingLevel(const AZ::VR::HMDTrackingLevel level) override;
        void OutputHMDInfo() override;
        void EnableDebugging(bool enable) override;
        void DrawDebugInfo(const AZ::Transform& transform, IRenderAuxGeom* auxGeom) override;
        AZ::VR::HMDDeviceInfo* GetDeviceInfo() override;
        bool IsInitialized() override;
        ////////////////////////////////////////////////////////////////////////////

    private:
        /// OpenVRRequests overrides ///////////////////////////////////////////////
        void GetPlayspace(Playspace& playspace) const override;
        ////////////////////////////////////////////////////////////////////////////

        vr::IVRSystem* m_system = nullptr;
        vr::IVRCompositor* m_compositor = nullptr;
        vr::TrackedDevicePose_t m_trackedDevicePose[vr::k_unMaxTrackedDeviceCount];
        vr::ETrackingUniverseOrigin m_trackingOrigin;

        OpenVRController* m_controller;
        AZ::VR::TrackingState m_trackingState; ///< Most recent tracking state of the HMD (not including any connected controllers).
        AZ::VR::HMDDeviceInfo m_deviceInfo;
};

} // namespace OpenVR
