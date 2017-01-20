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
#include <Oculus/OculusBus.h>
#include "HMDBus.h"

#include <OVR_CAPI.h>
#include <OVR_Version.h>

namespace Oculus
{

class OculusTouchController;

class OculusDevice
    : public AZ::VR::HMDDeviceRequestBus::Handler
    , public AZ::VR::HMDInitRequestBus::Handler
    , public AZ::Component
    , private OculusRequestBus::Handler
{
    public:

        AZ_COMPONENT(OculusDevice, "{1FEC6CDA-48DA-4CB4-A186-491E26081CAF}");
        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        OculusDevice() = default;
        ~OculusDevice() override = default;

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

        /// HMDDeviceBus overrides ///////////////////////////////////////////////////
        void GetPerEyeCameraInfo(const EStereoEye eye, AZ::VR::PerEyeCameraInfo& cameraInfo) override;
        bool CreateRenderTargetsDX11(void* renderDevice, const TextureDesc& desc, size_t eyeCount, AZ::VR::HMDRenderTarget* renderTargets[]) const override;
        void DestroyRenderTarget(AZ::VR::HMDRenderTarget& renderTarget) const override;
        void Update() override;
        AZ::VR::TrackingState* GetTrackingState() override;
        void SubmitFrame(const EyeTarget& left, const EyeTarget& right) override;
        void RecenterPose() override;
        void SetTrackingLevel(const AZ::VR::HMDTrackingLevel level) override;
        void OutputHMDInfo() override;
        void EnableDebugging(bool enable) override;
        AZ::VR::HMDDeviceInfo* GetDeviceInfo() override;
        bool IsInitialized() override;
        ////////////////////////////////////////////////////////////////////////////

    private:

        ///
        /// Per-frame rendering parameters which store the pose used for the most recently tracked time sample.
        ///
        struct FrameParameters
        {
            ovrPosef eyePoses[ovrEye_Count];
            ovrFovPort eyeFovs[ovrEye_Count];
            ovrViewScaleDesc viewScaleDesc;
            int frameID;
            double sensorTimeSample;
        };

        FrameParameters& GetFrameParameters();

        ovrSession m_session;
        ovrHmdDesc m_hmdDesc;
        FrameParameters m_frameParams[2]; // double buffered frame render parameters to avoid race condition between caching it (main thread) and reading it (render thread)

        OculusTouchController* m_controller;
        AZ::VR::TrackingState m_trackingState; ///< Most recent tracking state of the HMD (not including any connected controllers).
        AZ::VR::HMDDeviceInfo m_deviceInfo;

        float m_verticalFOVs[STEREO_EYE_COUNT];
};

} // namespace Oculus
