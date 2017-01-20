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

#include <AzCore/std/string/string.h>
#include <AzCore/Component/Component.h>
#include <LmbrCentral/Audio/AudioRtpcComponentBus.h>

#include <IAudioInterfacesCommonData.h>

namespace LmbrCentral
{
    /*!
    * AudioRtpcComponent
    *  Allows settings values on ATL Rtpcs (Real-Time Parameter Controls).
    *  An Rtpc name can be serialized with the component, or it can be manually specified
    *  at runtime for use in scripting.
    */
    class AudioRtpcComponent
        : public AZ::Component
        , public AudioRtpcComponentRequestsBus::Handler
    {
    public:
        /*!
         * AZ::Component
         */
        AZ_COMPONENT(AudioRtpcComponent, "{4441F9C5-D4AD-40CF-B1DE-D5A296C03798}");
        void Activate() override;
        void Deactivate() override;

        /*!
         * AudioRtpcComponentRequestsBus::Handler Required Interface
         */
        void SetValue(float value) override;
        void SetRtpcValue(const char* rtpcName, float value) override;

    protected:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("AudioRtpcService"));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("AudioProxyService"));
        }

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("AudioRtpcService"));
        }

        static void Reflect(AZ::ReflectContext* context);

    private:
        //! Editor callback
        void OnRtpcNameChanged();

        //! Transient data
        Audio::TAudioControlID m_defaultRtpcID = INVALID_AUDIO_CONTROL_ID;

        //! Serialized data
        AZStd::string m_defaultRtpcName;
    };

} // namespace LmbrCentral
