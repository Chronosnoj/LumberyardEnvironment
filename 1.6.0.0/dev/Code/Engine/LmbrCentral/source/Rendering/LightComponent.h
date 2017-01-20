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

#include <AzCore/base.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/ComponentBus.h>
#include <AzFramework/Asset/SimpleAsset.h>

#include <LmbrCentral/Rendering/LightComponentBus.h>
#include <LmbrCentral/Rendering/RenderNodeBus.h>
#include <LmbrCentral/Rendering/MaterialAsset.h>

#include "LightInstance.h"

namespace LmbrCentral
{
    /*!
     * Stores configuration settings for engine lights.
     * - Common color and shader settings
     * - Common shadow-casting settings
     * - Settings unique to different light types
     */
    class LightConfiguration
    {
    public:

        AZ_TYPE_INFO(LightConfiguration, "{F4CC7BB4-C541-480C-88FC-C5A8F37CC67F}");

        // LY renderer supported light types.
        enum class LightType : AZ::u32
        {
            Point = 0,  ///> Omni-directional point light
            Area,       ///> Area/box light
            Projector,  ///> Texture projector light
            Probe,      ///> Environment probe
        };

        // LY renderer system spec levels.
        enum class EngineSpec : AZ::u32
        {
            Low = 0,
            Medium,
            High,
            VeryHigh,
            Never,
        };

        // Texture resolution settings.
        enum class ResolutionSetting : AZ::u32
        {
            ResDefault = 256,
            Res32 = 32,
            Res64 = 64,
            Res128 = 128,
            Res256 = 256,
            Res512 = 512,
        };

        LightConfiguration();
        virtual ~LightConfiguration() {};

        static void Reflect(AZ::ReflectContext* context);

        //! The type of render light (point/omni, projector, area, or environment probe).
        LightType m_lightType;

        //! Turned on by default?
        bool m_onInitially;

        //! Currently visible?
        bool m_visible;

        //! Point light settings
        float m_pointMaxDistance;
        float m_pointAttenuationBulbSize;

        //! Settings for area lights
        float m_areaWidth;
        float m_areaHeight;
        float m_areaMaxDistance;

        //! Settings for projector lights
        float m_projectorRange;
        float m_projectorAttenuationBulbSize;
        float m_projectorFOV;
        float m_projectorNearPlane;
        AzFramework::SimpleAssetReference<TextureAsset> m_projectorTexture;

        //! Settings for environment lights (probes)
        AZ::Vector3 m_probeArea;
        AZ::u32 m_probeSortPriority;
        ResolutionSetting m_probeCubemapResolution;
        AZStd::string m_probeCubemap;

        //! Settings common to all LY engine lights
        EngineSpec m_minSpec;
        float m_viewDistMultiplier;
        EngineSpec m_castShadowsSpec;
        
        AZ::Vector3 m_color;
        float m_diffuseMultiplier;
        float m_specMultiplier;
        bool m_affectsThisAreaOnly;
        bool m_ignoreVisAreas;
        bool m_indoorOnly;
        bool m_ambient;
        bool m_deferred;
        AZ::u32 m_animIndex;
        float m_animSpeed;
        float m_animPhase;
        bool m_volumetricFog;
        bool m_volumetricFogOnly;

        //! Shadow settings
        float m_shadowBias;
        float m_shadowSlopeBias;
        float m_shadowResScale;
        float m_shadowUpdateMinRadius;
        float m_shadowUpdateRatio;

        // \todo clip bounds - artists aren't using it. Includes fade dimensions.

        AZ::EntityId m_editorEntityId; // Editor-only, not reflected.

        // Property event-handlers implemented in editor component.
        virtual AZ::Crc32 GetAmbientLightVisibility() const { return 0; }
        virtual AZ::Crc32 GetPointLightVisibility() const { return 0; }
        virtual AZ::Crc32 GetProjectorLightVisibility() const { return 0; }
        virtual AZ::Crc32 GetProbeLightVisibility() const { return 0; }
        virtual AZ::Crc32 GetShadowSettingsVisibility() const { return 0; }
        virtual AZ::Crc32 GetAreaSettingVisibility() const { return 0; }
        virtual AZ::Crc32 MinorPropertyChanged() { return 0; }
        virtual AZ::Crc32 MajorPropertyChanged() { return 0; }
        virtual AZ::Crc32 LightTypeChanged() { return 0; }
        virtual AZ::Crc32 OnAnimationSettingChanged() { return 0; }

    private:

        static bool VersionConverter(AZ::SerializeContext& context,
            AZ::SerializeContext::DataElementNode& classElement);
    };

    /*!
     * In-game light component.
     */
    class LightComponent
        : public AZ::Component
        , public LightComponentRequestBus::Handler
        , public RenderNodeRequestBus::Handler
    {
    public:

        friend class EditorLightComponent;

        AZ_COMPONENT(LightComponent, "{6B9AB512-CA8A-4D2B-B570-DF128EA7CE6A}");

        LightComponent();
        ~LightComponent() override;

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // LightComponentRequests::Bus::Handler interface implementation
        void SetLightState(State state) override;
        void TurnOnLight() override;
        void TurnOffLight() override;
        void ToggleLight() override;
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
            provided.push_back(AZ_CRC("LightService"));
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

        LightConfiguration m_configuration;
        LightInstance m_light;
    };
} // namespace LmbrCentral