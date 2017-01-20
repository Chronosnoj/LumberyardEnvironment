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

#include "Precompiled.h"
#include "AudioSwitchComponent.h"

#include <ISystem.h>

#include <LmbrCentral/Audio/AudioProxyComponentBus.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/EBus/ScriptBinder.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace LmbrCentral
{
    //=========================================================================
    //! Script Interface
    AZ_SCRIPTABLE_EBUS(
        AudioSwitchComponentRequestsBus,
        AudioSwitchComponentRequestsBus,
        "{FF92261E-AA44-4790-8FFF-7B35C27B1695}",
        "{C0200856-3C01-4156-8333-3E62385CD63B}",
        AZ_SCRIPTABLE_EBUS_EVENT(SetState, const char*)
        AZ_SCRIPTABLE_EBUS_EVENT(SetSwitchState, const char*, const char*)
        )

    //=========================================================================
    void AudioSwitchComponent::Reflect(AZ::ReflectContext* context)
    {
        auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<AudioSwitchComponent, AZ::Component>()
                ->Version(1)
                ->Field("Switch name", &AudioSwitchComponent::m_defaultSwitchName)
                ->Field("State name", &AudioSwitchComponent::m_defaultStateName)
                ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<AudioSwitchComponent>("Audio Switch", "Used to set an enumerated state on an Entity.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Audio")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/AudioSwitch")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/AudioSwitch.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AudioSwitchComponent::m_defaultSwitchName, "Default Switch", "The default ATL Switch to use when Activated.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &AudioSwitchComponent::OnDefaultSwitchChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AudioSwitchComponent::m_defaultStateName, "Default State", "The default ATL State to set on the default Switch when Activated.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &AudioSwitchComponent::OnDefaultStateChanged)
                    ;
            }
        }

        auto scriptContext = azrtti_cast<AZ::ScriptContext*>(context);
        if (scriptContext)
        {
            ScriptableEBus_AudioSwitchComponentRequestsBus::Reflect(scriptContext);
        }
    }

    //=========================================================================
    void AudioSwitchComponent::Activate()
    {
        OnDefaultSwitchChanged();
        OnDefaultStateChanged();

        // set the default switch state, if valid IDs were found.
        if (m_defaultSwitchID != INVALID_AUDIO_CONTROL_ID && m_defaultStateID != INVALID_AUDIO_SWITCH_STATE_ID)
        {
            EBUS_EVENT_ID(GetEntityId(), AudioProxyComponentRequestsBus, SetSwitchState, m_defaultSwitchID, m_defaultStateID);
        }

        AudioSwitchComponentRequestsBus::Handler::BusConnect(GetEntityId());
    }

    //=========================================================================
    void AudioSwitchComponent::Deactivate()
    {
        AudioSwitchComponentRequestsBus::Handler::BusDisconnect(GetEntityId());
    }

    //=========================================================================
    void AudioSwitchComponent::SetState(const char* stateName)
    {
        if (m_defaultSwitchID != INVALID_AUDIO_CONTROL_ID)
        {
            // only allowed if there's a default switch that is known.
            if (stateName && stateName[0] != '\0')
            {
                Audio::TAudioSwitchStateID stateID = INVALID_AUDIO_SWITCH_STATE_ID;
                gEnv->pAudioSystem->GetAudioSwitchStateID(m_defaultSwitchID, stateName, stateID);
                if (stateID != INVALID_AUDIO_SWITCH_STATE_ID)
                {
                    EBUS_EVENT_ID(GetEntityId(), AudioProxyComponentRequestsBus, SetSwitchState, m_defaultSwitchID, stateID);
                }
            }
        }
    }

    //=========================================================================
    void AudioSwitchComponent::SetSwitchState(const char* switchName, const char* stateName)
    {
        Audio::TAudioControlID switchID = INVALID_AUDIO_CONTROL_ID;
        Audio::TAudioSwitchStateID stateID = INVALID_AUDIO_SWITCH_STATE_ID;

        // lookup switch...
        if (switchName && switchName[0] != '\0')
        {
            gEnv->pAudioSystem->GetAudioSwitchID(switchName, switchID);
        }

        // using the switchID (if found), lookup the state...
        if (switchID != INVALID_AUDIO_CONTROL_ID && stateName && stateName[0] != '\0')
        {
            gEnv->pAudioSystem->GetAudioSwitchStateID(switchID, stateName, stateID);
        }

        // if both IDs found, make the call...
        if (switchID != INVALID_AUDIO_CONTROL_ID && stateID != INVALID_AUDIO_SWITCH_STATE_ID)
        {
            EBUS_EVENT_ID(GetEntityId(), AudioProxyComponentRequestsBus, SetSwitchState, switchID, stateID);
        }
    }

    //=========================================================================
    void AudioSwitchComponent::OnDefaultSwitchChanged()
    {
        m_defaultSwitchID = INVALID_AUDIO_CONTROL_ID;
        if (!m_defaultSwitchName.empty())
        {
            gEnv->pAudioSystem->GetAudioSwitchID(m_defaultSwitchName.c_str(), m_defaultSwitchID);
        }
    }

    //=========================================================================
    void AudioSwitchComponent::OnDefaultStateChanged()
    {
        m_defaultStateID = INVALID_AUDIO_SWITCH_STATE_ID;
        if (!m_defaultStateName.empty() && m_defaultSwitchID != INVALID_AUDIO_CONTROL_ID)
        {
            gEnv->pAudioSystem->GetAudioSwitchStateID(m_defaultSwitchID, m_defaultStateName.c_str(), m_defaultStateID);
        }
    }

} // namespace LmbrCentral
