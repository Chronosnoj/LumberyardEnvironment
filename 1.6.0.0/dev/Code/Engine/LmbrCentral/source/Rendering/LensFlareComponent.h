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

#include <AzCore/Component/Component.h>
#include <AzCore/Component/ComponentBus.h>
#include <AzFramework/Asset/SimpleAsset.h>

#include "LightInstance.h"
#include <LmbrCentral/Rendering/LensFlareComponentBus.h>
#include <LmbrCentral/Rendering/RenderNodeBus.h>
#include <LmbrCentral/Rendering/MaterialAsset.h>


namespace LmbrCentral
{
    /*!
     * Stores configuration settings for engine lens flares.
     * - Common color, animation, and lens flare
     */
    class LensFlareConfiguration
    {
    public:
        AZ_TYPE_INFO(LensFlareConfiguration, "{1E28DADD-0BD4-4AD5-A94B-2665813BF346}");

        // LY renderer system spec levels.
        enum class EngineSpec : AZ::u32
        {
            Low = 0,
            Medium,
            High,
            VeryHigh,
            Never,
        };

        LensFlareConfiguration();
        virtual ~LensFlareConfiguration() {};

        static void Reflect(AZ::ReflectContext* context);

        //! Settings common to all LY engine lights
        EngineSpec m_minSpec;

        //! Turned on by default?
        bool m_onInitially;

        //! Currently visible?
        bool m_visible;

        //! Lens flare size
        float m_size;

        AZ::Vector3 m_tint;
        AZ::u32 m_tintAlpha;
        float m_brightness;

        float m_viewDistMultiplier;
        bool m_affectsThisAreaOnly;
        bool m_ignoreVisAreas;
        bool m_indoorOnly;
        bool m_attachToSun;

        bool m_syncAnimWithLight;
        AZ::EntityId m_lightEntity;
        float m_animSpeed;
        float m_animPhase;
        AZ::u32 m_animIndex;

        AZStd::string m_lensFlare;
        float m_lensFlareFrustumAngle;

        // Animation settings from the light
        float m_syncAnimSpeed;
        float m_syncAnimPhase;
        AZ::u32 m_syncAnimIndex;

        AZ_INLINE bool ShouldShowAnimationSettings() {
            return !m_syncAnimWithLight;
        }

        // Property event-handlers implemented in editor component.
        virtual AZ::u32 PropertyChanged() { return 0; }
        virtual AZ::u32 SyncAnimationChanged() { return AZ_CRC("RefreshNone"); }
    };


    /*!
     * In-game light component.
     */
    class LensFlareComponent
        : public AZ::Component
        , public LensFlareComponentRequestBus::Handler
        , public RenderNodeRequestBus::Handler
    {
    public:
        friend class EditorLensFlareComponent;

        AZ_COMPONENT(LensFlareComponent, "{07593109-4A57-473F-B868-C2DCF9270186}");

        LensFlareComponent() {}
        ~LensFlareComponent() override {}

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override {}
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // LensFlareComponentRequestBus::Handler interface implementation
        void TurnOnLensFlare() override;
        void TurnOffLensFlare() override;
        void SetLensFlareState(State state) override;
        void ToggleLensFlare() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // RenderNodeRequestBus::Handler interface implementation
        IRenderNode* GetRenderNode() override;
        float GetRenderNodeRequestBusOrder() const override;
        static const float s_renderNodeRequestBusOrder;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("LensFlareService"));
        }

        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
        {
            dependent.push_back(AZ_CRC("TransformService"));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("TransformService"));
        }

        static void Reflect(AZ::ReflectContext* context);
        //////////////////////////////////////////////////////////////////////////

    protected:

        LensFlareConfiguration m_configuration;
        LightInstance m_light;
    };
} // namespace LmbrCentral
