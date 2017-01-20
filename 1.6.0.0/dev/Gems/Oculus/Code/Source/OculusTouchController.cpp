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
#include "OculusTouchController.h"

namespace Oculus
{

enum class OculusInputSymbol
{
    kButton,
    kIndexTrigger,
    kHandTrigger,
    kAxis
};

#define BUTTON_DEFINES \
    X(A,                        ovrButton_A,      OculusInputSymbol::kButton,       AZ::VR::ControllerIndex::RightHand)  \
    X(B,                        ovrButton_B,      OculusInputSymbol::kButton,       AZ::VR::ControllerIndex::RightHand)  \
    X(X,                        ovrButton_X,      OculusInputSymbol::kButton,       AZ::VR::ControllerIndex::LeftHand)   \
    X(Y,                        ovrButton_Y,      OculusInputSymbol::kButton,       AZ::VR::ControllerIndex::LeftHand)   \
    X(LeftThumbstickButton,     ovrButton_LThumb, OculusInputSymbol::kButton,       AZ::VR::ControllerIndex::LeftHand)   \
    X(RightThumbstickButton,    ovrButton_RThumb, OculusInputSymbol::kButton,       AZ::VR::ControllerIndex::RightHand)  \
    X(LeftTrigger,              -1,               OculusInputSymbol::kIndexTrigger, AZ::VR::ControllerIndex::LeftHand)   \
    X(RightTrigger,             -1,               OculusInputSymbol::kIndexTrigger, AZ::VR::ControllerIndex::RightHand)  \
    X(LeftHandTrigger,          -1,               OculusInputSymbol::kHandTrigger,  AZ::VR::ControllerIndex::LeftHand)   \
    X(RightHandTrigger,         -1,               OculusInputSymbol::kHandTrigger,  AZ::VR::ControllerIndex::RightHand)  \
    X(LeftThumbstickX,          -1,               OculusInputSymbol::kAxis,         AZ::VR::ControllerIndex::LeftHand)   \
    X(LeftThumbstickY,          -1,               OculusInputSymbol::kAxis,         AZ::VR::ControllerIndex::LeftHand)   \
    X(RightThumbstickX,         -1,               OculusInputSymbol::kAxis,         AZ::VR::ControllerIndex::RightHand)  \
    X(RightThumbstickY,         -1,               OculusInputSymbol::kAxis,         AZ::VR::ControllerIndex::RightHand) 

#define X(buttonId, deviceButtonId, buttonType, hand) k##buttonId,
enum ButtonIds
{
    BUTTON_DEFINES
    BUTTON_COUNT
};
#undef X

struct OculusTouchButton
{
    OculusTouchButton(const char* buttonName, const uint32 buttonDeviceID, const OculusInputSymbol buttonType, AZ::VR::ControllerIndex buttonHand) :
        name(buttonName),
        deviceID(buttonDeviceID),
        type(buttonType),
        hand(buttonHand)
    {
    }

    const char* name;
    const uint32 deviceID;
    const OculusInputSymbol type;
    const AZ::VR::ControllerIndex hand;
};

#define X(buttonId, deviceButtonId, buttonType, hand) OculusTouchButton("OculusTouch_" #buttonId, deviceButtonId, buttonType, hand),
const OculusTouchButton g_deviceButtons[] =
{
    BUTTON_DEFINES
    OculusTouchButton("InvalidID", -1, OculusInputSymbol::kButton, AZ::VR::ControllerIndex::RightHand)
};
#undef X

OculusTouchController::OculusTouchController() :
    m_deviceIndex(0),
    m_deviceID(0),
    m_enabled(false)
{
    memset(&m_previousState, 0, sizeof(m_previousState));
    memset(&m_currentState, 0, sizeof(m_currentState));

    CreateInputMapping();
    CrySystemEventBus::Handler::BusConnect();
}
OculusTouchController::~OculusTouchController()
{
    CrySystemEventBus::Handler::BusDisconnect();
    TDevSpecIdToSymbolMap::iterator iter = m_symbolMapping.begin();
    for (; iter != m_symbolMapping.end(); ++iter)
    {
        delete iter->second;
    }
}

const char* OculusTouchController::GetDeviceName() const
{
    return "Oculus Touch Controller";
}

EInputDeviceType OculusTouchController::GetDeviceType() const
{
    return eIDT_MotionController;
}

TInputDeviceId OculusTouchController::GetDeviceId() const
{
    return m_deviceID;
}

int OculusTouchController::GetDeviceIndex() const
{
    return m_deviceIndex;
}

bool OculusTouchController::Init()
{
    return true;
}

void OculusTouchController::PostInit()
{
}

void OculusTouchController::Update(bool focus)
{
    for (uint32 buttonIndex = 0; buttonIndex < BUTTON_COUNT; ++buttonIndex)
    {
        SInputEvent event;
        SInputSymbol* symbol = m_symbolMapping[buttonIndex];

        const OculusTouchButton* button = &g_deviceButtons[buttonIndex];
        switch (button->type)
        {
            case OculusInputSymbol::kButton:
                {
                    bool isPressed = m_currentState.IsButtonPressed(button->deviceID);
                    bool wasPressed = m_previousState.IsButtonPressed(button->deviceID);
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

            case OculusInputSymbol::kIndexTrigger:
                {
                    float currentValue = m_currentState.inputState.IndexTrigger[static_cast<uint32_t>(button->hand)];
                    float previousValue = m_previousState.inputState.IndexTrigger[static_cast<uint32_t>(button->hand)];
                    if (currentValue != previousValue)
                    {
                        symbol->ChangeEvent(currentValue);
                        symbol->AssignTo(event);
                        event.deviceIndex = m_deviceIndex;
                        event.deviceType = eIDT_MotionController;
                        gEnv->pInput->PostInputEvent(event);
                    }
                }
                break;

            case OculusInputSymbol::kHandTrigger:
            {
                float currentValue = m_currentState.inputState.HandTrigger[static_cast<uint32_t>(button->hand)];
                float previousValue = m_previousState.inputState.HandTrigger[static_cast<uint32_t>(button->hand)];
                if (currentValue != previousValue)
                {
                    symbol->ChangeEvent(currentValue);
                    symbol->AssignTo(event);
                    event.deviceIndex = m_deviceIndex;
                    event.deviceType = eIDT_MotionController;
                    gEnv->pInput->PostInputEvent(event);
                }
            }
            break;

            case OculusInputSymbol::kAxis:
            {
                ovrVector2f currentState  = m_currentState.inputState.Thumbstick[static_cast<uint32_t>(button->hand)];
                ovrVector2f previousState = m_previousState.inputState.Thumbstick[static_cast<uint32_t>(button->hand)];

                // Process X and Y as separate events.
                {
                    if (currentState.x != previousState.x)
                    {
                        symbol->ChangeEvent(currentState.x);
                        symbol->AssignTo(event);
                        event.deviceIndex = m_deviceIndex;
                        event.deviceType = eIDT_MotionController;
                        gEnv->pInput->PostInputEvent(event);
                    }
                }

                {
                    if (currentState.y != previousState.y)
                    {
                        symbol->ChangeEvent(currentState.y);
                        symbol->AssignTo(event);
                        event.deviceIndex = m_deviceIndex;
                        event.deviceType = eIDT_MotionController;
                        gEnv->pInput->PostInputEvent(event);
                    }
                }
            }
            break;

            default:
                AZ_Assert(0, "Unknown OculusInputSymbol type");
                break;
        }
    }
}

bool OculusTouchController::SetForceFeedback(IFFParams params)
{
    return true;
}

bool OculusTouchController::InputState(const TKeyName& key, EInputState state)
{
    return true;
}

bool OculusTouchController::SetExclusiveMode(bool value)
{
    return true;
}

void OculusTouchController::ClearKeyState()
{
}

void OculusTouchController::ClearAnalogKeyState(TInputSymbols& clearedSymbols)
{
}

void OculusTouchController::SetUniqueId(uint8 const uniqueId)
{
    m_deviceID = uniqueId;
}

const char* OculusTouchController::GetKeyName(const SInputEvent& event) const
{
    return g_deviceButtons[event.keyId].name;
}

const char* OculusTouchController::GetKeyName(const EKeyId keyId) const
{
    return g_deviceButtons[keyId].name;
}

char OculusTouchController::GetInputCharAscii(const SInputEvent& event)
{
    return 'e';
}

const char* OculusTouchController::GetOSKeyName(const SInputEvent& event)
{
    return g_deviceButtons[event.keyId].name;
}

SInputSymbol* OculusTouchController::LookupSymbol(EKeyId id) const
{
    TDevSpecIdToSymbolMap::const_iterator iter = m_symbolMapping.find(id);
    if (iter != m_symbolMapping.end())
    {
        return iter->second;
    }

    return nullptr;
}

const SInputSymbol* OculusTouchController::GetSymbolByName(const char* name) const
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

bool OculusTouchController::IsOfDeviceType(EInputDeviceType type) const
{
    return (type == eIDT_MotionController);
}

void OculusTouchController::Enable(bool enable)
{
    m_enabled = enable;
}

bool OculusTouchController::IsEnabled() const
{
    return m_enabled;
}

void OculusTouchController::OnLanguageChange()
{

}

void OculusTouchController::SetDeadZone(float threshold)
{

}

void OculusTouchController::RestoreDefaultDeadZone()
{

}

const IInputDevice::TDevSpecIdToSymbolMap& OculusTouchController::GetInputToSymbolMappings() const
{
    return m_symbolMapping;
}

AZ::VR::TrackingState* OculusTouchController::GetTrackingState(AZ::VR::ControllerIndex controllerIndex)
{
    return &m_currentState.trackingState[static_cast<uint32_t>(controllerIndex)];
}

bool OculusTouchController::IsConnected(AZ::VR::ControllerIndex controllerIndex)
{
    ovrControllerType type = (controllerIndex == AZ::VR::ControllerIndex::LeftHand) ? ovrControllerType_LTouch : ovrControllerType_RTouch;
    return (m_currentState.inputState.ControllerType & type) != 0;
}

void OculusTouchController::OnCrySystemInitialized(ISystem& system, const SSystemInitParams&)
{
    if (gEnv->pInput)
    {
        system.GetIInput()->AddInputDevice(this);
    }
}

void OculusTouchController::ConnectToControllerBus() 
{
    AZ::VR::ControllerRequestBus::Handler::BusConnect();
}

void OculusTouchController::DisconnectFromControllerBus()
{
    AZ::VR::ControllerRequestBus::Handler::BusDisconnect();
}

void OculusTouchController::SetCurrentState(const AZ::VR::TrackingState trackingState[static_cast<uint32_t>(AZ::VR::ControllerIndex::MaxNumControllers)], const ovrInputState& inputState)
{
    m_previousState = m_currentState;

    for (uint32 index = 0; index < static_cast<uint32>(AZ::VR::ControllerIndex::MaxNumControllers); ++index)
    {
        m_currentState.trackingState[index] = trackingState[index];
    }

    memcpy(&m_currentState.inputState, &inputState, sizeof(inputState));
}

void MapSymbol(const uint32 buttonIndex, const uint32 deviceID, const TKeyName& name, const OculusInputSymbol type, IInputDevice::TDevSpecIdToSymbolMap& mapping)
{
    SInputSymbol::EType symbolType;
    switch (type)
    {
        case OculusInputSymbol::kButton:
            symbolType = SInputSymbol::Button;
            break;

        case OculusInputSymbol::kHandTrigger:
        case OculusInputSymbol::kIndexTrigger:
            symbolType = SInputSymbol::Trigger;
            break;

        case OculusInputSymbol::kAxis:
            symbolType = SInputSymbol::Axis;
            break;

        default:
            AZ_Assert(0, "Unknown OculusInputSymbol type");
            break;
    }

    SInputSymbol* symbol = new SInputSymbol(deviceID, static_cast<EKeyId>(KI_MOTION_BASE), name, symbolType);
    symbol->deviceType = eIDT_MotionController;
    symbol->state = eIS_Unknown;
    symbol->value = 0.0f;

    mapping[buttonIndex] = symbol;
}

void OculusTouchController::CreateInputMapping()
{
    for (uint32 index = 0; index < BUTTON_COUNT; ++index)
    {
        const OculusTouchButton* button = &g_deviceButtons[index];
        MapSymbol(index, button->deviceID, button->name, button->type, m_symbolMapping);
    }
}

} // namespace Oculus


