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
#include "OpenVRController.h"
#include <sstream>

namespace OpenVR 
{

#define BUTTON_DEFINES \
    X(System,           vr::k_EButton_System,           SInputSymbol::Button)  \
    X(Application,      vr::k_EButton_ApplicationMenu,  SInputSymbol::Button)  \
    X(Grip,             vr::k_EButton_Grip,             SInputSymbol::Button)  \
    X(DPadLeft,         vr::k_EButton_DPad_Left,        SInputSymbol::Button)  \
    X(DPadRight,        vr::k_EButton_DPad_Right,       SInputSymbol::Button)  \
    X(DPadUp,           vr::k_EButton_DPad_Up,          SInputSymbol::Button)  \
    X(DPadDown,         vr::k_EButton_DPad_Down,        SInputSymbol::Button)  \
    X(A,                vr::k_EButton_A,                SInputSymbol::Button)  \
    X(TouchpadX,        vr::k_EButton_SteamVR_Touchpad, SInputSymbol::Axis)    \
    X(TouchpadY,        vr::k_EButton_SteamVR_Touchpad, SInputSymbol::Axis)    \
    X(TouchpadButton,   vr::k_EButton_SteamVR_Touchpad, SInputSymbol::Button)  \
    X(Trigger,          vr::k_EButton_SteamVR_Trigger,  SInputSymbol::Trigger) \
    X(TriggerButton,    vr::k_EButton_SteamVR_Trigger,  SInputSymbol::Button)

#define X(buttonId, deviceButtonId, buttonType) k##buttonId,
enum ButtonIds
{
    BUTTON_DEFINES
    BUTTON_COUNT
};
#undef X

struct OpenVRButton
{
    OpenVRButton(const char* buttonName, const vr::EVRButtonId buttonDeviceID, const SInputSymbol::EType buttonType) :
        name(buttonName),
        deviceID(buttonDeviceID),
        type(buttonType)
    {
    }

    const char* name;
    const vr::EVRButtonId deviceID;
    const SInputSymbol::EType type;
};


const OpenVRButton g_deviceButtons[static_cast<uint32_t>(AZ::VR::ControllerIndex::MaxNumControllers)][BUTTON_COUNT] =
{
#define X(buttonId, deviceButtonId, buttonType) OpenVRButton("OpenVR_" #buttonId "_0", deviceButtonId, buttonType),
    {
        BUTTON_DEFINES
    },
#undef X
#define X(buttonId, deviceButtonId, buttonType) OpenVRButton("OpenVR_" #buttonId "_1", deviceButtonId, buttonType),
    {
        BUTTON_DEFINES
    }
#undef X
};

OpenVRController::OpenVRController() :
    m_deviceIndex(0),
    m_deviceID(0),
    m_enabled(false)
{
    memset(&m_previousState, 0, sizeof(m_previousState));
    memset(&m_currentState, 0, sizeof(m_currentState));

    for (uint32 index = 0; index < static_cast<uint32_t>(AZ::VR::ControllerIndex::MaxNumControllers); ++index)
    {
        m_contollerMapping[index] = vr::k_unTrackedDeviceIndexInvalid;
    }

    CreateInputMapping();
    CrySystemEventBus::Handler::BusConnect();
}
OpenVRController::~OpenVRController()
{
    CrySystemEventBus::Handler::BusDisconnect();
    TDevSpecIdToSymbolMap::iterator iter = m_symbolMapping.begin();
    for (; iter != m_symbolMapping.end(); ++iter)
    {
        delete iter->second;
    }
}

const char* OpenVRController::GetDeviceName() const
{
    return "OpenVR Controller";
}

EInputDeviceType OpenVRController::GetDeviceType() const
{
    return eIDT_MotionController;
}

TInputDeviceId OpenVRController::GetDeviceId() const
{
    return m_deviceID;
}

int OpenVRController::GetDeviceIndex() const
{
    return m_deviceIndex;
}

bool OpenVRController::Init()
{
    return true;
}

void OpenVRController::PostInit()
{
}

void OpenVRController::Update(bool focus)
{
    // Note that the axis range from k_EButton_Axis0 to k_EButton_Axis4. Since these are don't matchup with the
    // rAxis array from OpenVR we have to subtract any axis index by k_EButton_Axis0 in order to get the proper
    // rAxis array index.

    for (uint32 controllerIndex = 0; controllerIndex < static_cast<uint32_t>(AZ::VR::ControllerIndex::MaxNumControllers); ++controllerIndex)
    {
        // Make sure this controller is currently mapped (enabled).
        if (m_contollerMapping[controllerIndex] != vr::k_unTrackedDeviceIndexInvalid)
        {
            ControllerState* currentState  = &m_currentState[controllerIndex];
            ControllerState* previousState = &m_previousState[controllerIndex];

            for (uint32 buttonIndex = 0; buttonIndex < BUTTON_COUNT; ++buttonIndex)
            {
                SInputEvent event;
                const OpenVRButton* button = &g_deviceButtons[controllerIndex][buttonIndex];
                SInputSymbol* symbol       = m_symbolMapping[buttonIndex + (controllerIndex * BUTTON_COUNT)];

                switch (button->type)
                {
                    case SInputSymbol::Button:
                    {
                        bool isPressed  = currentState->IsButtonPressed(button->deviceID);
                        bool wasPressed = previousState->IsButtonPressed(button->deviceID);
                        if (isPressed != wasPressed)
                        {
                            symbol->PressEvent(isPressed);
                            symbol->AssignTo(event);
                            event.deviceIndex = m_deviceIndex;
                            event.deviceType = eIDT_MotionController;
                            gEnv->pInput->PostInputEvent(event);
                        }
                    }
                    break;

                    case SInputSymbol::Trigger:
                    {
                        float trigger         = currentState->buttonState.rAxis[button->deviceID - vr::k_EButton_Axis0].x;
                        float previousTrigger = previousState->buttonState.rAxis[button->deviceID - vr::k_EButton_Axis0].x;

                        if (trigger != previousTrigger)
                        {
                            symbol->ChangeEvent(trigger);
                            symbol->AssignTo(event);
                            event.deviceIndex = m_deviceIndex;
                            event.deviceType = eIDT_MotionController;
                            gEnv->pInput->PostInputEvent(event);
                        }
                    }
                    break;

                case SInputSymbol::Axis:
                {
                    // Process X and Y separately.
                    {
                        float x         = currentState->buttonState.rAxis[button->deviceID - vr::k_EButton_Axis0].x;
                        float previousX = previousState->buttonState.rAxis[button->deviceID - vr::k_EButton_Axis0].x;

                        if (x != previousX)
                        {
                            symbol->ChangeEvent(x);
                            symbol->AssignTo(event);
                            event.deviceIndex = m_deviceIndex;
                            event.deviceType = eIDT_MotionController;
                            gEnv->pInput->PostInputEvent(event);
                        }
                    }

                    {
                        float y         = currentState->buttonState.rAxis[button->deviceID - vr::k_EButton_Axis0].y;
                        float previousY = previousState->buttonState.rAxis[button->deviceID - vr::k_EButton_Axis0].y;

                        if (y != previousY)
                        {
                            symbol->ChangeEvent(y);
                            symbol->AssignTo(event);
                            event.deviceIndex = m_deviceIndex;
                            event.deviceType = eIDT_MotionController;
                            gEnv->pInput->PostInputEvent(event);
                        }
                    }
                }
                break;

                default:
                    AZ_Assert(0, "Unknown button type");
                    break;
                }
            }
        }
    }
}

bool OpenVRController::SetForceFeedback(IFFParams params)
{
    return true;
}

bool OpenVRController::InputState(const TKeyName& key, EInputState state)
{
    return true;
}

bool OpenVRController::SetExclusiveMode(bool value)
{
    return true;
}

void OpenVRController::ClearKeyState()
{
}

void OpenVRController::ClearAnalogKeyState(TInputSymbols& clearedSymbols)
{

}

void OpenVRController::SetUniqueId(uint8 const uniqueId)
{
    m_deviceID = uniqueId;
}

const char* OpenVRController::GetKeyName(const SInputEvent& event) const
{
    return g_deviceButtons[0][event.keyId].name;
}

const char* OpenVRController::GetKeyName(const EKeyId keyId) const
{
    return g_deviceButtons[0][keyId].name;
}

char OpenVRController::GetInputCharAscii(const SInputEvent& event)
{
    return 'e';
}

const char* OpenVRController::GetOSKeyName(const SInputEvent& event)
{
    return g_deviceButtons[0][event.keyId].name;
}

SInputSymbol* OpenVRController::LookupSymbol(EKeyId id) const
{
    TDevSpecIdToSymbolMap::const_iterator iter = m_symbolMapping.find(id);
    if (iter != m_symbolMapping.end())
    {
        return iter->second;
    }

    return nullptr;
}

const SInputSymbol* OpenVRController::GetSymbolByName(const char* name) const
{
    TDevSpecIdToSymbolMap::const_iterator iter = m_symbolMapping.begin();
    for (; iter != m_symbolMapping.end(); ++iter)
    {
        if (iter->second->name == TKeyName(name))
        {
            return iter->second;
        }
    }

    return nullptr;
}

bool OpenVRController::IsOfDeviceType(EInputDeviceType type) const
{
    return (type == eIDT_MotionController);
}

void OpenVRController::Enable(bool enable)
{
    m_enabled = enable;
}

bool OpenVRController::IsEnabled() const
{
    return m_enabled;
}

void OpenVRController::OnLanguageChange()
{

}

void OpenVRController::SetDeadZone(float threshold)
{

}

void OpenVRController::RestoreDefaultDeadZone()
{

}

const IInputDevice::TDevSpecIdToSymbolMap& OpenVRController::GetInputToSymbolMappings() const
{
    return m_symbolMapping;
}

AZ::VR::TrackingState* OpenVRController::GetTrackingState(AZ::VR::ControllerIndex controllerIndex)
{
    return &m_currentState[static_cast<uint32_t>(controllerIndex)].trackingState;
}

bool OpenVRController::IsConnected(AZ::VR::ControllerIndex controllerIndex)
{
    return (m_contollerMapping[static_cast<uint32_t>(controllerIndex)] != vr::k_unTrackedDeviceIndexInvalid);
}

void OpenVRController::OnCrySystemInitialized(ISystem& system, const SSystemInitParams&)
{
    if (gEnv->pInput)
    {
        system.GetIInput()->AddInputDevice(this);
    }
}

void OpenVRController::ConnectToControllerBus()
{
    AZ::VR::ControllerRequestBus::Handler::BusConnect();
}

void OpenVRController::DisconnectFromControllerBus()
{
    AZ::VR::ControllerRequestBus::Handler::BusDisconnect();
}

void OpenVRController::SetCurrentState(const vr::TrackedDeviceIndex_t deviceIndex, const AZ::VR::TrackingState& trackingState, const vr::VRControllerState_t& buttonState)
{
    uint32 index = 0;
    for (; index < static_cast<uint32_t>(AZ::VR::ControllerIndex::MaxNumControllers); ++index)
    {
        if (m_contollerMapping[index] == deviceIndex)
        {
            break;
        }
    }

    AZ_Assert(index != static_cast<uint32_t>(AZ::VR::ControllerIndex::MaxNumControllers), "Attempting to set state on a controller that has not been mapped");

    m_previousState[index] = m_currentState[index];
    m_currentState[index].Set(trackingState, buttonState);
}

void MapSymbol(uint32 buttonIndex, uint32 deviceID, const TKeyName& name, SInputSymbol::EType type, IInputDevice::TDevSpecIdToSymbolMap& mapping)
{
    SInputSymbol* symbol = new SInputSymbol(deviceID, static_cast<EKeyId>(KI_MOTION_BASE), name, type);
    symbol->deviceType = eIDT_MotionController;
    symbol->state = eIS_Unknown;
    symbol->value = 0.0f;

    mapping[buttonIndex] = symbol;
}

void OpenVRController::CreateInputMapping()
{
    for (uint32 controllerIndex = 0; controllerIndex < static_cast<uint32_t>(AZ::VR::ControllerIndex::MaxNumControllers); ++controllerIndex)
    {
        for (uint32 index = 0; index < BUTTON_COUNT; ++index)
        {
            const OpenVRButton* button = &g_deviceButtons[controllerIndex][index];
            uint32 buttonIndex = index + (controllerIndex * BUTTON_COUNT);
            MapSymbol(buttonIndex, button->deviceID, button->name, button->type, m_symbolMapping);
        }
    }
}

void OpenVRController::ConnectController(const vr::TrackedDeviceIndex_t deviceIndex)
{
    for (uint32 index = 0; index < static_cast<uint32_t>(AZ::VR::ControllerIndex::MaxNumControllers); ++index)
    {
        if (m_contollerMapping[index] == vr::k_unTrackedDeviceIndexInvalid)
        {
            // This is an unused controller slot.
            m_contollerMapping[index] = deviceIndex;
            return;
        }
    }

    AZ_Assert(0, "Attempting to map more than %d OpenVR controllers", static_cast<uint32_t>(AZ::VR::ControllerIndex::MaxNumControllers));
}

void OpenVRController::DisconnectController(const vr::TrackedDeviceIndex_t deviceIndex)
{
    for (uint32 index = 0; index < static_cast<uint32_t>(AZ::VR::ControllerIndex::MaxNumControllers); ++index)
    {
        if (m_contollerMapping[index] == deviceIndex)
        {
            m_contollerMapping[index] = vr::k_unTrackedDeviceIndexInvalid;
            return;
        }
    }

    AZ_Assert(0, "Attempting to disconnect an OpenVR controller that doesn't exist");
}

} // namespace OpenVR
