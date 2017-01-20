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

#include <IInput.h>
#include "HMDBus.h"
#include "VRControllerBus.h"
#include <CrySystemBus.h>

#include <openvr.h>

namespace OpenVR
{

class OpenVRController
    : public AZ::VR::ControllerRequestBus::Handler
    , public IInputDevice
    , protected CrySystemEventBus::Handler
{
    public:

        OpenVRController();
        ~OpenVRController();

        // IInputDevice overrides //////////////////////////////////////////////////
        const char* GetDeviceName() const override;
        EInputDeviceType GetDeviceType() const override;
        TInputDeviceId GetDeviceId() const override;
        int GetDeviceIndex() const override;
        bool Init() override;
        void PostInit() override;
        void Update(bool focus) override;
        bool SetForceFeedback(IFFParams params) override;
        bool InputState(const TKeyName& key, EInputState state) override;
        bool SetExclusiveMode(bool value) override;
        void ClearKeyState() override;
        void ClearAnalogKeyState(TInputSymbols& clearedSymbols) override;
        void SetUniqueId(uint8 const uniqueId) override;
        const char* GetKeyName(const SInputEvent& event) const override;
        const char* GetKeyName(const EKeyId keyId) const override;
        char GetInputCharAscii(const SInputEvent& event) override;
        const char* GetOSKeyName(const SInputEvent& event) override;
        SInputSymbol* LookupSymbol(EKeyId id) const override;
        const SInputSymbol* GetSymbolByName(const char* name) const override;
        bool IsOfDeviceType(EInputDeviceType type) const override;
        void Enable(bool enable) override;
        bool IsEnabled() const override;
        void OnLanguageChange() override;
        void SetDeadZone(float threshold) override;
        void RestoreDefaultDeadZone() override;
        const IInputDevice::TDevSpecIdToSymbolMap& GetInputToSymbolMappings() const override;
        ////////////////////////////////////////////////////////////////////////////
       
        // IHMDController overrides ////////////////////////////////////////////////
        AZ::VR::TrackingState* GetTrackingState(AZ::VR::ControllerIndex controllerIndex) override;
        bool IsConnected(AZ::VR::ControllerIndex controllerIndex) override;
        ////////////////////////////////////////////////////////////////////////////

        // CrySystemEventBus ///////////////////////////////////////////////////////
        void OnCrySystemInitialized(ISystem& system, const SSystemInitParams&) override;
        ////////////////////////////////////////////////////////////////////////////

        void ConnectToControllerBus();
        void DisconnectFromControllerBus();

        void SetCurrentState(const vr::TrackedDeviceIndex_t deviceIndex, const AZ::VR::TrackingState& trackingState, const vr::VRControllerState_t& buttonState);
        void ConnectController(const vr::TrackedDeviceIndex_t deviceIndex);
        void DisconnectController(const vr::TrackedDeviceIndex_t deviceIndex);

    private:

        OpenVRController(const OpenVRController& other) = delete;
        OpenVRController& operator=(const OpenVRController& other) = delete;

        void CreateInputMapping();
        void PostInputEvent(const uint32 buttonIndex);

        struct ControllerState
        {
            ControllerState()
            {
            }

            ControllerState& operator=(const ControllerState& otherState)
            {
                if (this != &otherState)
                {
                    memcpy(&buttonState, &otherState.buttonState, sizeof(buttonState));
                }

                return *this;
            }

            void Set(const AZ::VR::TrackingState& newTrackingState, const vr::VRControllerState_t& newButtonState)
            {
                trackingState = newTrackingState;
                memcpy(&buttonState, &newButtonState, sizeof(newButtonState));
            }

            inline bool IsButtonPressed(const vr::EVRButtonId buttonId) const
            {
                return (buttonState.ulButtonPressed & vr::ButtonMaskFromId(buttonId)) != 0;
            }

            inline bool IsButtonTouched(const vr::EVRButtonId buttonId) const
            {
                return (buttonState.ulButtonTouched & vr::ButtonMaskFromId(buttonId)) != 0;
            }

            AZ::VR::TrackingState trackingState;
            vr::VRControllerState_t buttonState;
        };

        IInputDevice::TDevSpecIdToSymbolMap m_symbolMapping;
        int m_deviceIndex;
        uint8 m_deviceID;
        bool m_enabled;
        ControllerState m_currentState[static_cast<uint32_t>(AZ::VR::ControllerIndex::MaxNumControllers)];
        ControllerState m_previousState[static_cast<uint32_t>(AZ::VR::ControllerIndex::MaxNumControllers)];
        vr::TrackedDeviceIndex_t m_contollerMapping[static_cast<uint32_t>(AZ::VR::ControllerIndex::MaxNumControllers)];
};
        
} // namespace OpenVR

