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

#include <IAudioSystem.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/string/string.h>
#include <AzCore/Component/Component.h>
#include <LmbrCentral/Audio/AudioTriggerComponentBus.h>

namespace LmbrCentral
{
    /*!
     * AudioTriggerComponent
     *  Allows controlling ATL Triggers, executing and stopping them.
     *  Trigger name can be serialized with the component, or manually specified
     *  at runtime for use in scripting.
     *  There is only 1 AudioTriggerComponent allowed on an Entity, but the interface
     *  supports firing multiple ATL Triggers.
     */
    class AudioTriggerComponent
        : public AZ::Component
        , public AudioTriggerComponentRequestsBus::Handler
    {
    public:
        /*!
         * AZ::Component
         */
        AZ_COMPONENT(AudioTriggerComponent, "{80089838-4444-4D67-9A89-66B0276BB916}");
        void Activate() override;
        void Deactivate() override;

        /*!
         * AudioTriggerComponentRequestsBus::Handler Interface
         */
        void Play() override;
        void Stop() override;

        void ExecuteTrigger(const char* triggerName) override;
        void KillTrigger(const char* triggerName) override;

        void KillAllTriggers() override;

        void SetMovesWithEntity(bool shouldTrackEntity) override;

        //! Used as a callback when a trigger instance is finished.
        static void OnAudioEvent(const Audio::SAudioRequestInfo* const);

    protected:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("AudioTriggerService"));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("AudioProxyService"));
        }

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("AudioTriggerService"));
        }

        static void Reflect(AZ::ReflectContext* context);

    private:
        //! Editor callbacks
        void OnPlayTriggerChanged();
        void OnStopTriggerChanged();

        //! Transient data
        Audio::TAudioControlID m_defaultPlayTriggerID = INVALID_AUDIO_CONTROL_ID;
        Audio::TAudioControlID m_defaultStopTriggerID = INVALID_AUDIO_CONTROL_ID;
        AZStd::unique_ptr<Audio::SAudioCallBackInfos> m_callbackInfo;

        //! Serialized data
        AZStd::string m_defaultPlayTriggerName;
        AZStd::string m_defaultStopTriggerName;
        bool m_playsImmediately = false;
    };

} // namespace LmbrCentral
