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

#include <HMDBus.h>
#include <VRControllerBus.h>
#include <AzCore/Script/bind_intrusive_ptr.h>
#include <AzCore/EBus/ScriptBinder.h>
#include <AzCore/Serialization/SerializeContext.h>

#include "HMDLuaComponent.h"

namespace AZ 
{
    namespace VR
    {
        AZ_SCRIPTABLE_EBUS(
            HMDDeviceRequestBus,
            HMDDeviceRequestBus,
            "{BA584790-B805-4EF8-AD4F-721E3AC4C8A1}",
            "{9A8B9597-FB51-4317-AD43-0F25D897B42E}",
            AZ_SCRIPTABLE_EBUS_EVENT_RESULT(TrackingState*, nullptr, GetTrackingState)
            AZ_SCRIPTABLE_EBUS_EVENT(RecenterPose)
            AZ_SCRIPTABLE_EBUS_EVENT(SetTrackingLevel, const AZ::VR::HMDTrackingLevel)
            AZ_SCRIPTABLE_EBUS_EVENT(OutputHMDInfo)
            AZ_SCRIPTABLE_EBUS_EVENT_RESULT(HMDDeviceInfo*, nullptr, GetDeviceInfo)
            AZ_SCRIPTABLE_EBUS_EVENT_RESULT(bool, false, IsInitialized)
        )

        AZ_SCRIPTABLE_EBUS(
            ControllerRequestBus,
            ControllerRequestBus,
            "{C914A061-B8FC-4A41-A31D-EB671E5E74E9}",
            "{1394D5B5-A2A4-4ADF-8CEC-C24496DE23E2}",
            AZ_SCRIPTABLE_EBUS_EVENT_RESULT(TrackingState*, nullptr, GetTrackingState, AZ::VR::ControllerIndex)
            AZ_SCRIPTABLE_EBUS_EVENT_RESULT(bool, false, IsConnected, AZ::VR::ControllerIndex)
        )

        void HMDLuaComponent::Reflect(AZ::ReflectContext* context)
        {
            TrackingState::Reflect(context);
            PoseState::Reflect(context);
            DynamicsState::Reflect(context);
            HMDDeviceInfo::Reflect(context);

            SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context);
            if (serializeContext)
            {
                serializeContext->Class<HMDLuaComponent>()
                    ->Version(1);
            }

            ScriptContext* script = azrtti_cast<ScriptContext*>(context);
            if (script)
            {
                ScriptableEBus_HMDDeviceRequestBus::Reflect(script);
                ScriptableEBus_ControllerRequestBus::Reflect(script);
            }
        }

        void TrackingState::Reflect(AZ::ReflectContext* context)
        {
            SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context);
            if (serializeContext)
            {
                serializeContext->Class<TrackingState>()
                    ->Version(1)
                    ->Field("pose", &TrackingState::pose)
                    ->Field("dynamics", &TrackingState::dynamics)
                    ->Field("statusFlags", &TrackingState::statusFlags)
                    ;
            }

            ScriptContext* script = azrtti_cast<ScriptContext*>(context);
            if (script)
            {
                script->Class<TrackingState, AZ::ScriptContext::ObjectStorePolicy::SP_VALUE>("TrackingState")
                    ->Field("pose", &TrackingState::pose)
                    ->Field("dynamics", &TrackingState::dynamics)
                    ->Field("statusFlags", &TrackingState::statusFlags)
                    ;
            }
        }

        void PoseState::Reflect(AZ::ReflectContext* context)
        {
            SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context);
            if (serializeContext)
            {
                serializeContext->Class<PoseState>()
                    ->Version(1)
                    ->Field("orientation", &PoseState::orientation)
                    ->Field("position", &PoseState::position)
                    ;
            }

            ScriptContext* script = azrtti_cast<ScriptContext*>(context);
            if (script)
            {
                script->Class<PoseState, AZ::ScriptContext::ObjectStorePolicy::SP_VALUE>("PoseState")
                    ->Field("orientation", &PoseState::orientation)
                    ->Field("position", &PoseState::position)
                    ;
            }
        }

        void DynamicsState::Reflect(AZ::ReflectContext* context)
        {
            SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context);
            if (serializeContext)
            {
                serializeContext->Class<DynamicsState>()
                    ->Version(1)
                    ->Field("angularVelocity", &DynamicsState::angularVelocity)
                    ->Field("angularAcceleration", &DynamicsState::angularAcceleration)
                    ->Field("linearVelocity", &DynamicsState::linearVelocity)
                    ->Field("linearAcceleration", &DynamicsState::linearAcceleration)
                    ;
            }

            ScriptContext* script = azrtti_cast<ScriptContext*>(context);
            if (script)
            {
                script->Class<DynamicsState, AZ::ScriptContext::ObjectStorePolicy::SP_VALUE>("DynamicsState")
                    ->Field("angularVelocity", &DynamicsState::angularVelocity)
                    ->Field("angularAcceleration", &DynamicsState::angularAcceleration)
                    ->Field("linearVelocity", &DynamicsState::linearVelocity)
                    ->Field("linearAcceleration", &DynamicsState::linearAcceleration)
                    ;
            }
        }

        void HMDDeviceInfo::Reflect(AZ::ReflectContext* context)
        {
            SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context);
            if (serializeContext)
            {
                serializeContext->Class<HMDDeviceInfo>()
                    ->Version(1)
                    ->Field("productName", &HMDDeviceInfo::productName)
                    ->Field("manufacturer", &HMDDeviceInfo::manufacturer)
                    ->Field("renderWidth", &HMDDeviceInfo::renderWidth)
                    ->Field("renderHeight", &HMDDeviceInfo::renderHeight)
                    ->Field("fovH", &HMDDeviceInfo::fovH)
                    ->Field("fovV", &HMDDeviceInfo::fovV)
                    ;
            }

            ScriptContext* script = azrtti_cast<ScriptContext*>(context);
            if (script)
            {
                script->Class<HMDDeviceInfo, AZ::ScriptContext::ObjectStorePolicy::SP_VALUE>("HMDDeviceInfo")
                    ->Field("productName", &HMDDeviceInfo::productName)
                    ->Field("manufacturer", &HMDDeviceInfo::manufacturer)
                    ->Field("renderWidth", &HMDDeviceInfo::renderWidth)
                    ->Field("renderHeight", &HMDDeviceInfo::renderHeight)
                    ->Field("fovH", &HMDDeviceInfo::fovH)
                    ->Field("fovV", &HMDDeviceInfo::fovV)
                    ;
            }
        }
    }
}
