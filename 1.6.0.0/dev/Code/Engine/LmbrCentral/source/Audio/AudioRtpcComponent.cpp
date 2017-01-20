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
#include "AudioRtpcComponent.h"

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
    AZ_SCRIPTABLE_EBUS(
        AudioRtpcComponentRequestsBus,
        AudioRtpcComponentRequestsBus,
        "{2FE00F75-1AF7-42F5-90F6-56FCB7FEEE64}",
        "{12175AE1-733F-46DB-9C97-30FEEE7BBD09}",
        AZ_SCRIPTABLE_EBUS_EVENT(SetValue, float)
        AZ_SCRIPTABLE_EBUS_EVENT(SetRtpcValue, const char*, float)
        )

    //=========================================================================
    void AudioRtpcComponent::Reflect(AZ::ReflectContext* context)
    {
        auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<AudioRtpcComponent, AZ::Component>()
                ->Version(1)
                ->Field("Rtpc Name", &AudioRtpcComponent::m_defaultRtpcName)
                ;

            auto editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<AudioRtpcComponent>("Audio Rtpc", "Used by the game to tweak parameters in the audio engine.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Audio")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/AudioRtpc")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/AudioRtpc.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AudioRtpcComponent::m_defaultRtpcName, "Default Rtpc", "The default ATL Rtpc control to use.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &AudioRtpcComponent::OnRtpcNameChanged)
                    ;
            }
        }

        auto scriptContext = azrtti_cast<AZ::ScriptContext*>(context);
        if (scriptContext)
        {
            ScriptableEBus_AudioRtpcComponentRequestsBus::Reflect(scriptContext);
        }
    }

    //=========================================================================
    void AudioRtpcComponent::Activate()
    {
        OnRtpcNameChanged();

        AudioRtpcComponentRequestsBus::Handler::BusConnect(GetEntityId());
    }

    //=========================================================================
    void AudioRtpcComponent::Deactivate()
    {
        AudioRtpcComponentRequestsBus::Handler::BusDisconnect(GetEntityId());
    }

    //=========================================================================
    void AudioRtpcComponent::SetValue(float value)
    {
        if (m_defaultRtpcID != INVALID_AUDIO_CONTROL_ID)
        {
            EBUS_EVENT_ID(GetEntityId(), AudioProxyComponentRequestsBus, SetRtpcValue, m_defaultRtpcID, value);
        }
    }

    //=========================================================================
    void AudioRtpcComponent::SetRtpcValue(const char* rtpcName, float value)
    {
        if (rtpcName && rtpcName[0] != '\0')
        {
            Audio::TAudioControlID rtpcID = INVALID_AUDIO_CONTROL_ID;
            gEnv->pAudioSystem->GetAudioRtpcID(rtpcName, rtpcID);
            if (rtpcID != INVALID_AUDIO_CONTROL_ID)
            {
                EBUS_EVENT_ID(GetEntityId(), AudioProxyComponentRequestsBus, SetRtpcValue, rtpcID, value);
            }
        }
    }

    //=========================================================================
    void AudioRtpcComponent::OnRtpcNameChanged()
    {
        m_defaultRtpcID = INVALID_AUDIO_CONTROL_ID;
        if (!m_defaultRtpcName.empty())
        {
            gEnv->pAudioSystem->GetAudioRtpcID(m_defaultRtpcName.c_str(), m_defaultRtpcID);
        }
    }

} // namespace LmbrCentral
