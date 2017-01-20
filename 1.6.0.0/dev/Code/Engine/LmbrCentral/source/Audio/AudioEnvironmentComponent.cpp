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
#include "AudioEnvironmentComponent.h"

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
        AudioEnvironmentComponentRequestsBus,
        AudioEnvironmentComponentRequestsBus,
        "{CDDD0B03-35B7-4F33-99E8-8AD87B2372AB}",
        "{4DDD2ED2-93DC-46CD-8789-99A8F1C7461E}",
        AZ_SCRIPTABLE_EBUS_EVENT(SetAmount, float)
        AZ_SCRIPTABLE_EBUS_EVENT(SetEnvironmentAmount, const char*, float)
        )

    //=========================================================================
    void AudioEnvironmentComponent::Reflect(AZ::ReflectContext* context)
    {
        auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<AudioEnvironmentComponent, AZ::Component>()
                ->Version(1)
                ->Field("Environment name", &AudioEnvironmentComponent::m_defaultEnvironmentName)
                ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<AudioEnvironmentComponent>("Audio Environment", "Used to send signal to auxillary effects busses.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Audio")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/AudioEnvironment")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/AudioEnvironment.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AudioEnvironmentComponent::m_defaultEnvironmentName, "Default Environment", "Name of the default ATL Environment control to use.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &AudioEnvironmentComponent::OnDefaultEnvironmentChanged)
                    ;
            }
        }

        auto scriptContext = azrtti_cast<AZ::ScriptContext*>(context);
        if (scriptContext)
        {
            ScriptableEBus_AudioEnvironmentComponentRequestsBus::Reflect(scriptContext);
        }
    }

    //=========================================================================
    void AudioEnvironmentComponent::Activate()
    {
        OnDefaultEnvironmentChanged();

        AudioEnvironmentComponentRequestsBus::Handler::BusConnect(GetEntityId());
    }

    //=========================================================================
    void AudioEnvironmentComponent::Deactivate()
    {
        AudioEnvironmentComponentRequestsBus::Handler::BusDisconnect(GetEntityId());
    }

    //=========================================================================
    void AudioEnvironmentComponent::SetAmount(float amount)
    {
        // set amount on the default Environment (if set)
        if (m_defaultEnvironmentID != INVALID_AUDIO_ENVIRONMENT_ID)
        {
            EBUS_EVENT_ID(GetEntityId(), AudioProxyComponentRequestsBus, SetEnvironmentAmount, m_defaultEnvironmentID, amount);
        }
    }

    //=========================================================================
    void AudioEnvironmentComponent::SetEnvironmentAmount(const char* environmentName, float amount)
    {
        Audio::TAudioEnvironmentID environmentID = INVALID_AUDIO_ENVIRONMENT_ID;
        if (environmentName && environmentName[0] != '\0')
        {
            gEnv->pAudioSystem->GetAudioEnvironmentID(environmentName, environmentID);
        }

        if (environmentID != INVALID_AUDIO_ENVIRONMENT_ID)
        {
            EBUS_EVENT_ID(GetEntityId(), AudioProxyComponentRequestsBus, SetEnvironmentAmount, environmentID, amount);
        }
    }

    //=========================================================================
    void AudioEnvironmentComponent::OnDefaultEnvironmentChanged()
    {
        m_defaultEnvironmentID = INVALID_AUDIO_ENVIRONMENT_ID;
        if (!m_defaultEnvironmentName.empty())
        {
            gEnv->pAudioSystem->GetAudioEnvironmentID(m_defaultEnvironmentName.c_str(), m_defaultEnvironmentID);
        }
    }

} // namespace LmbrCentral
