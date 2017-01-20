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

#include <AzCore/Component/ComponentBus.h>

namespace LmbrCentral
{
    class LightConfiguration;

    /*!
     * LightComponentRequestBus
     * Messages serviced by the light component.
     */
    class LightComponentRequests
        : public AZ::ComponentBus
    {
    public:

        enum class State
        {
            On = 0,
            Off,
        };

        virtual ~LightComponentRequests() {}

        //! Control light state.
        virtual void SetLightState(State state) {}

        //! Turns light on
        virtual void TurnOnLight() {}

        //! Turns light off
        virtual void TurnOffLight() {}

        //! Toggles light state.
        virtual void ToggleLight() {}
    };

    using LightComponentRequestBus = AZ::EBus<LightComponentRequests>;

    /*!
     * LightComponentEditorRequestBus
     * Editor/UI messages serviced by the light component.
     */
    class LightComponentEditorRequests
        : public AZ::ComponentBus
    {
    public:
        virtual ~LightComponentEditorRequests() {}

        //! Recreates the render light.
        virtual void RefreshLight() {}

        //! Sets the active cubemap resource.
        virtual void SetCubemap(const char* cubemap) {}

        //! Retrieves configured cubemap resolution for generation.
        virtual AZ::u32 GetCubemapResolution() { return 0; }

        //! Retrieves Configuration
        virtual const LightConfiguration& GetConfiguration() const = 0;

        //! Caches resources for displaying the cubemap preview
        virtual void CacheCubemapPreviewResources() = 0;
    };

    using LightComponentEditorRequestBus = AZ::EBus < LightComponentEditorRequests >;

    /*!
     * LightComponentNotificationBus
     * Events dispatched by the light component.
     */
    class LightComponentNotifications
        : public AZ::ComponentBus
    {
    public:

        virtual ~LightComponentNotifications() {}

        virtual void LightTurnedOn() {}
        virtual void LightTurnedOff() {}
    };

    using LightComponentNotificationBus = AZ::EBus < LightComponentNotifications >;

    /*!
    * LightSettingsNotifications
    * Events dispatched by the light component or light component editor when settings have changed.
    */
    class LightSettingsNotifications
        : public AZ::ComponentBus
    {
    public:

        virtual ~LightSettingsNotifications() {}

        virtual void AnimationSettingsChanged() = 0;
    };

    using LightSettingsNotificationsBus = AZ::EBus < LightSettingsNotifications >;
    
} // namespace LmbrCentral
