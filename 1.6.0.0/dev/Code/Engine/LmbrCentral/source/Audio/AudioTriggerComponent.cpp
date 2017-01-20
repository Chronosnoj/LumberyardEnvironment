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
#include "AudioTriggerComponent.h"

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
        AudioTriggerComponentRequestsBus,
        AudioTriggerComponentRequestsBus,
        "{BB0B6CCC-6B61-4F90-A564-30DF92A1750B}",
        "{2844AAA5-5CE9-4250-8813-D2752E96C07F}",
        AZ_SCRIPTABLE_EBUS_EVENT(Play)
        AZ_SCRIPTABLE_EBUS_EVENT(Stop)
        AZ_SCRIPTABLE_EBUS_EVENT(ExecuteTrigger, const char*)
        AZ_SCRIPTABLE_EBUS_EVENT(KillTrigger, const char*)
        AZ_SCRIPTABLE_EBUS_EVENT(KillAllTriggers)
        AZ_SCRIPTABLE_EBUS_EVENT(SetMovesWithEntity, bool)
        )

    AZ_SCRIPTABLE_EBUS(
        AudioTriggerComponentNotificationsBus,
        AudioTriggerComponentNotificationsBus,
        "{99E8FA01-183F-4567-986E-6FCE8DDC17CC}",
        "{ACCB0C42-3752-496B-9B1F-19276925EBB0}",
        AZ_SCRIPTABLE_EBUS_EVENT(OnTriggerFinished, const Audio::TAudioControlID)
        )

    //=========================================================================
    namespace ClassConverters
    {
        static void ExtractOldFields(AZ::SerializeContext::DataElementNode& classElement, AZStd::string& playTrigger, AZStd::string& stopTrigger)
        {
            int playTriggerIndex = classElement.FindElement(AZ_CRC("Play Trigger"));
            int stopTriggerIndex = classElement.FindElement(AZ_CRC("Stop Trigger"));

            if (playTriggerIndex != -1)
            {
                classElement.GetSubElement(playTriggerIndex).GetData<AZStd::string>(playTrigger);
            }

            if (stopTriggerIndex != -1)
            {
                classElement.GetSubElement(stopTriggerIndex).GetData<AZStd::string>(stopTrigger);
            }
        }

        static void RestoreOldFields(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement, AZStd::string& playTrigger, AZStd::string& stopTrigger)
        {
            const int playTriggerIndex = classElement.AddElement<AZStd::string>(context, "Play Trigger");
            AZ_Assert(playTriggerIndex >= 0, "Failed to add 'Play Trigger' element.");
            classElement.GetSubElement(playTriggerIndex).SetData<AZStd::string>(context, playTrigger);
            const int stopTriggerIndex = classElement.AddElement<AZStd::string>(context, "Stop Trigger");
            AZ_Assert(playTriggerIndex >= 0, "Failed to add 'Stop Trigger' element.");
            classElement.GetSubElement(stopTriggerIndex).SetData<AZStd::string>(context, stopTrigger);
        }

        static bool ConvertOldEditorAudioComponent(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
        {
            AZStd::string playTrigger, stopTrigger;
            ExtractOldFields(classElement, playTrigger, stopTrigger);

            // Convert to generic component wrapper. This is necessary since the old audio component
            // had an editor counterpart. The new one does not, so it needs to be wrapped.
            const bool result = classElement.Convert(context, "{68D358CA-89B9-4730-8BA6-E181DEA28FDE}");
            if (result)
            {
                int triggerComponentIndex = classElement.AddElement<AudioTriggerComponent>(context, "m_template");

                if (triggerComponentIndex >= 0)
                {
                    auto& triggerComponentElement = classElement.GetSubElement(triggerComponentIndex);
                    RestoreOldFields(context, triggerComponentElement, playTrigger, stopTrigger);

                    return true;
                }
            }

            return false;
        }

        static bool ConvertOldAudioComponent(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
        {
            AZStd::string playTrigger, stopTrigger;
            ExtractOldFields(classElement, playTrigger, stopTrigger);

            // Convert from old audio component to audio trigger.
            const bool result = classElement.Convert<AudioTriggerComponent>(context);

            if (result)
            {
                RestoreOldFields(context, classElement, playTrigger, stopTrigger);
            }

            return result;
        }

    } // namespace ClassConverters

    //=========================================================================
    void AudioTriggerComponent::Reflect(AZ::ReflectContext* context)
    {
        auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            // Any old audio editor components need to be converted to GenericComponentWrapper-wrapped Audio Trigger components.
            serializeContext->ClassDeprecate("EditorAudioComponent", "{B238217F-1D76-4DEE-AA2B-09B7DFC6BF29}", &ClassConverters::ConvertOldEditorAudioComponent);
            // Any old audio runtime components need to be converted to Audio Trigger components.
            serializeContext->ClassDeprecate("AudioComponent", "{53033C2C-EE40-4D19-A7F4-861D6AA820EB}", &ClassConverters::ConvertOldAudioComponent);

            serializeContext->Class<AudioTriggerComponent, AZ::Component>()
                ->Version(1)
                ->Field("Play Trigger", &AudioTriggerComponent::m_defaultPlayTriggerName)
                ->Field("Stop Trigger", &AudioTriggerComponent::m_defaultStopTriggerName)
                ->Field("Plays Immediately", &AudioTriggerComponent::m_playsImmediately)
                ;

            auto editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<AudioTriggerComponent>("Audio Trigger", "Used to execute events on the Audio System.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Audio")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/AudioTrigger")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/AudioTrigger.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AudioTriggerComponent::m_defaultPlayTriggerName, "Default 'play' Trigger", "The default ATL Trigger control used by 'Play'.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &AudioTriggerComponent::OnPlayTriggerChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AudioTriggerComponent::m_defaultStopTriggerName, "Default 'stop' Trigger", "The default ATL Trigger control used by 'Stop'.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &AudioTriggerComponent::OnStopTriggerChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AudioTriggerComponent::m_playsImmediately, "Plays immediately", "Play when this component is Activated.")
                    ;
            }
        }

        auto scriptContext = azrtti_cast<AZ::ScriptContext*>(context);
        if (scriptContext)
        {
            ScriptableEBus_AudioTriggerComponentRequestsBus::Reflect(scriptContext);
            ScriptableEBus_AudioTriggerComponentNotificationsBus::Reflect(scriptContext);
        }
    }

    //=========================================================================
    void AudioTriggerComponent::Activate()
    {
        m_callbackInfo = new Audio::SAudioCallBackInfos(
            this,
            static_cast<AZ::u64>(GetEntityId()),
            nullptr,
            (Audio::eARF_PRIORITY_NORMAL | Audio::eARF_SYNC_FINISHED_CALLBACK)
        );

        OnPlayTriggerChanged();
        OnStopTriggerChanged();

        gEnv->pAudioSystem->AddRequestListener(
            &AudioTriggerComponent::OnAudioEvent,
            this,
            Audio::eART_AUDIO_CALLBACK_MANAGER_REQUEST,
            Audio::eACMRT_REPORT_FINISHED_TRIGGER_INSTANCE);

        AudioTriggerComponentRequestsBus::Handler::BusConnect(GetEntityId());

        if (m_playsImmediately)
        {
            // if requested, play the set trigger at Activation time...
            Play();
        }
    }

    //=========================================================================
    void AudioTriggerComponent::Deactivate()
    {
        AudioTriggerComponentRequestsBus::Handler::BusDisconnect(GetEntityId());

        gEnv->pAudioSystem->RemoveRequestListener(&AudioTriggerComponent::OnAudioEvent, this);

        KillAllTriggers();

        m_callbackInfo.reset();
    }

    //=========================================================================
    void AudioTriggerComponent::Play()
    {
        if (m_defaultPlayTriggerID != INVALID_AUDIO_CONTROL_ID)
        {
            EBUS_EVENT_ID(GetEntityId(), AudioProxyComponentRequestsBus, ExecuteTrigger, m_defaultPlayTriggerID, *m_callbackInfo);
        }
    }

    //=========================================================================
    void AudioTriggerComponent::Stop()
    {
        if (m_defaultStopTriggerID == INVALID_AUDIO_CONTROL_ID)
        {
            EBUS_EVENT_ID(GetEntityId(), AudioProxyComponentRequestsBus, KillTrigger, m_defaultPlayTriggerID);
        }
        else
        {
            EBUS_EVENT_ID(GetEntityId(), AudioProxyComponentRequestsBus, ExecuteTrigger, m_defaultStopTriggerID, *m_callbackInfo);
        }
    }

    //=========================================================================
    void AudioTriggerComponent::ExecuteTrigger(const char* triggerName)
    {
        if (triggerName && triggerName[0] != '\0')
        {
            Audio::TAudioControlID triggerID = INVALID_AUDIO_CONTROL_ID;
            gEnv->pAudioSystem->GetAudioTriggerID(triggerName, triggerID);
            if (triggerID != INVALID_AUDIO_CONTROL_ID)
            {
                EBUS_EVENT_ID(GetEntityId(), AudioProxyComponentRequestsBus, ExecuteTrigger, triggerID, *m_callbackInfo);
            }
        }
    }

    //=========================================================================
    void AudioTriggerComponent::KillTrigger(const char* triggerName)
    {
        if (triggerName && triggerName[0] != '\0')
        {
            Audio::TAudioControlID triggerID = INVALID_AUDIO_CONTROL_ID;
            gEnv->pAudioSystem->GetAudioTriggerID(triggerName, triggerID);
            if (triggerID != INVALID_AUDIO_CONTROL_ID)
            {
                EBUS_EVENT_ID(GetEntityId(), AudioProxyComponentRequestsBus, KillTrigger, triggerID);
            }
        }
    }

    //=========================================================================
    void AudioTriggerComponent::KillAllTriggers()
    {
        EBUS_EVENT_ID(GetEntityId(), AudioProxyComponentRequestsBus, KillAllTriggers);
    }

    //=========================================================================
    void AudioTriggerComponent::SetMovesWithEntity(bool shouldTrackEntity)
    {
        EBUS_EVENT_ID(GetEntityId(), AudioProxyComponentRequestsBus, SetMovesWithEntity, shouldTrackEntity);
    }

    //=========================================================================
    // static
    void AudioTriggerComponent::OnAudioEvent(const Audio::SAudioRequestInfo* const requestInfo)
    {
        // look for the 'finished trigger instance' event
        if (requestInfo->eAudioRequestType == Audio::eART_AUDIO_CALLBACK_MANAGER_REQUEST)
        {
            const auto notificationType = static_cast<Audio::EAudioCallbackManagerRequestType>(requestInfo->nSpecificAudioRequest);
            if (notificationType == Audio::eACMRT_REPORT_FINISHED_TRIGGER_INSTANCE)
            {
                if (requestInfo->eResult == Audio::eARR_SUCCESS)
                {
                    AZ::EntityId entityId(reinterpret_cast<AZ::u64>(requestInfo->pUserData));
                    EBUS_EVENT_ID(entityId, AudioTriggerComponentNotificationsBus, OnTriggerFinished, requestInfo->nAudioControlID);
                }
            }
        }
    }

    //=========================================================================
    void AudioTriggerComponent::OnPlayTriggerChanged()
    {
        m_defaultPlayTriggerID = INVALID_AUDIO_CONTROL_ID;
        if (!m_defaultPlayTriggerName.empty())
        {
            gEnv->pAudioSystem->GetAudioTriggerID(m_defaultPlayTriggerName.c_str(), m_defaultPlayTriggerID);
        }

        // "ChangeNotify" sends callbacks on every key press for a text field!
        // This results in a lot of failed lookups.
    }

    //=========================================================================
    void AudioTriggerComponent::OnStopTriggerChanged()
    {
        m_defaultStopTriggerID = INVALID_AUDIO_CONTROL_ID;
        if (!m_defaultStopTriggerName.empty())
        {
            gEnv->pAudioSystem->GetAudioTriggerID(m_defaultStopTriggerName.c_str(), m_defaultStopTriggerID);
        }
    }

} // namespace LmbrCentral
