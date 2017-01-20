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

#include <HMDBus.h>
#include <VRControllerBus.h>
#include <CrySystemBus.h>

#include <OVR_CAPI.h>
#include <OVR_Version.h>

namespace Oculus 
{

class OculusTouchController
    : public AZ::VR::ControllerRequestBus::Handler
    , public IInputDevice
    , protected CrySystemEventBus::Handler
{
    public:

        OculusTouchController();
        ~OculusTouchController();

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

        // ControllerBus overrides ////////////////////////////////////////////////
        AZ::VR::TrackingState* GetTrackingState(AZ::VR::ControllerIndex controllerIndex) override;
        bool IsConnected(AZ::VR::ControllerIndex controllerIndex) override;
        ////////////////////////////////////////////////////////////////////////////

        // CrySystemEventBus ///////////////////////////////////////////////////////
        void OnCrySystemInitialized(ISystem& system, const SSystemInitParams&) override;
        ////////////////////////////////////////////////////////////////////////////

        void ConnectToControllerBus();
        void DisconnectFromControllerBus();

        void SetCurrentState(const AZ::VR::TrackingState trackingState[static_cast<uint32_t>(AZ::VR::ControllerIndex::MaxNumControllers)], const ovrInputState& inputState);

    private:

        OculusTouchController(const OculusTouchController& other) = delete;
        OculusTouchController& operator=(const OculusTouchController& other) = delete;

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
                    memcpy(this, &otherState, sizeof(otherState));
                }

                return *this;
            }

            inline bool IsButtonPressed(const uint32 buttonId) const
            {
                return (inputState.Buttons & buttonId) != 0;
            }

            inline bool IsButtonTouched(const uint32 buttonId) const
            {
                return (inputState.Touches & buttonId) != 0;
            }

            AZ::VR::TrackingState trackingState[static_cast<uint32_t>(AZ::VR::ControllerIndex::MaxNumControllers)];
            ovrInputState inputState;
        };

        IInputDevice::TDevSpecIdToSymbolMap m_symbolMapping;
        int m_deviceIndex;
        uint8 m_deviceID;
        bool m_enabled;
        ControllerState m_currentState;
        ControllerState m_previousState;
};

} // namespace Oculus


