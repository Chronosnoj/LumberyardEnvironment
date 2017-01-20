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

#include <LmbrCentral/Audio/AudioEnvironmentComponentBus.h>

#include <IAudioInterfacesCommonData.h>

namespace LmbrCentral
{
    /*!
     * AudioEnvironmentComponent
     *  Allows "sending" an amount of sound signal through effects.
     *  Typically this is done via auxillary effects bus sends.
     *  The signal goes through the bus and comes out 'wet' and is
     *  mixed into the original 'dry' sound.
     *  Only 1 AudioEnvironmentComponent is allowed on an Entity,
     *  but the api supports multiple Environment sends.
     */
    class AudioEnvironmentComponent
        : public AZ::Component
        , public AudioEnvironmentComponentRequestsBus::Handler
    {
    public:
        /*!
         * AZ::Component
         */
        AZ_COMPONENT(AudioEnvironmentComponent, "{DDDCA462-A8E2-4D09-952C-22BAF89B7ED1}");
        void Activate() override;
        void Deactivate() override;

        /*!
         * AudioEnvironmentComponentRequestsBus::Handler Interface
         */
        void SetAmount(float amount) override;
        void SetEnvironmentAmount(const char* environmentName, float amount) override;

    protected:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("AudioEnvironmentService"));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("AudioProxyService"));
        }

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("AudioEnvironmentService"));
        }

        static void Reflect(AZ::ReflectContext* context);

    private:
        //! Editor callbacks
        void OnDefaultEnvironmentChanged();

        //! Transient data
        Audio::TAudioEnvironmentID m_defaultEnvironmentID;

        //! Serialized data
        AZStd::string m_defaultEnvironmentName;
    };

} // namespace LmbrCentral
