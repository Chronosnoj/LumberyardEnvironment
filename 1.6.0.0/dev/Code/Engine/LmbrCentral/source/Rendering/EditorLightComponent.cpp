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

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Component/ComponentApplicationBus.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <Cry_Math.h>

#include "EditorLightComponent.h"

namespace LmbrCentral
{
    // Private statics
    IEditor*             EditorLightComponent::m_editor = nullptr;
    IMaterialManager*    EditorLightComponent::m_materialManager = nullptr;

    void EditorLightComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        EditorLightConfiguration::Reflect(context);

        if (serializeContext)
        {
            serializeContext->Class<EditorLightComponent, EditorComponentBase>()
                ->Version(1)
                ->Field("EditorLightConfiguration", &EditorLightComponent::m_configuration)
                ->Field("ViewCubemap", &EditorLightComponent::m_viewCubemap)
                ->Field("CubemapRegen", &EditorLightComponent::m_cubemapRegen)
            ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<EditorLightComponent>(
                    "Light", "Attach lighting to an entity.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Rendering")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/Light.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/Light.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &EditorLightComponent::m_configuration, "Settings", "Light configuration")
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly"))
                    ->DataElement(0, &EditorLightComponent::m_viewCubemap, "View cubemap", "Preview the cubemap in scene")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorLightComponent::OnViewCubemapChanged)
                        ->Attribute(AZ::Edit::Attributes::Visibility, &EditorLightComponent::IsProbe)
                        ->Attribute(AZ::Edit::Attributes::ReadOnly, &EditorLightComponent::IsViewCubemapReadonly)
                    ->DataElement("Button", &EditorLightComponent::m_cubemapRegen, "Cubemap", "Create/regenerate the associated cubemap")
                        ->Attribute(AZ::Edit::Attributes::ButtonText, &EditorLightComponent::GetGenerateCubemapButtonText)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorLightComponent::GenerateCubemap)
                        ->Attribute(AZ::Edit::Attributes::Visibility, &EditorLightComponent::IsProbe)
                ;
            }
        }
    }

    void EditorLightConfiguration::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<EditorLightConfiguration, LightConfiguration>()->
                Version(1);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<LightConfiguration>(
                    "Configuration", "Light configuration")->

                    ClassElement(AZ::Edit::ClassElements::EditorData, "")->
                      Attribute(AZ::Edit::Attributes::AutoExpand, true)->
                      Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly"))->

                    DataElement(AZ::Edit::UIHandlers::CheckBox, &LightConfiguration::m_visible, "Visible", "The current visibility status of this flare")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MajorPropertyChanged)->

                    DataElement(AZ::Edit::UIHandlers::ComboBox, &LightConfiguration::m_lightType, "Type", "The type of light.")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::LightTypeChanged)->
                        EnumAttribute(LightType::Point, "Point")->
                        EnumAttribute(LightType::Area, "Area")->
                        EnumAttribute(LightType::Projector, "Projector")->
                        EnumAttribute(LightType::Probe, "Environment probe")->

                    DataElement(0, &LightConfiguration::m_onInitially, "On initially", "The light is initially turned on.")->

                    ClassElement(AZ::Edit::ClassElements::Group, "General Settings")->
                        Attribute(AZ::Edit::Attributes::AutoExpand, true)->

                    DataElement(AZ::Edit::UIHandlers::Color, &LightConfiguration::m_color, "Color", "Light color")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    DataElement(0, &LightConfiguration::m_diffuseMultiplier, "Diffuse multiplier", "Diffuse color multiplier")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Suffix, "x")->

                    DataElement(0, &LightConfiguration::m_specMultiplier, "Specular multiplier", "Specular multiplier")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Suffix, "x")->

                    DataElement(0, &LightConfiguration::m_ambient, "Ambient", "Ambient light")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetAmbientLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    ClassElement(AZ::Edit::ClassElements::Group, "Point Light Settings")->
                        Attribute(AZ::Edit::Attributes::AutoExpand, true)->

                    DataElement(0, &LightConfiguration::m_pointMaxDistance, "Max distance", "Point light radius")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetPointLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Suffix, " m")->
                        Attribute(AZ::Edit::Attributes::Min, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        
                    DataElement(0, &LightConfiguration::m_pointAttenuationBulbSize, "Attenuation bulb size", "Radius of area inside falloff.")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetPointLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Suffix, " m")->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->

                    ClassElement(AZ::Edit::ClassElements::Group, "Area Settings")->
                        Attribute(AZ::Edit::Attributes::AutoExpand, true)->

                    DataElement(0, &LightConfiguration::m_areaWidth, "Area width", "Area light width.")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetAreaSettingVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Suffix, " m")->

                    DataElement(0, &LightConfiguration::m_areaHeight, "Area height", "Area light height.")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetAreaSettingVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Suffix, " m")->

                    DataElement(0, &LightConfiguration::m_areaMaxDistance, "Max distance", "Area light max distance.")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetAreaSettingVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Suffix, " _")->

                    // Projector settings.
                    ClassElement(AZ::Edit::ClassElements::Group, "Projector Light Settings")->
                        Attribute(AZ::Edit::Attributes::AutoExpand, true)->

                    DataElement(0, &LightConfiguration::m_projectorRange, "Max distance", "Projector light range")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProjectorLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Suffix, " m")->

                    DataElement(0, &LightConfiguration::m_projectorAttenuationBulbSize, "Attenuation bulb size", "Radius of area inside falloff.")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProjectorLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Suffix, " m")->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->

                    DataElement(AZ::Edit::UIHandlers::Slider, &LightConfiguration::m_projectorFOV, "FOV", "Projector light FOV")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProjectorLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 1.f)->
                        Attribute(AZ::Edit::Attributes::Max, 360.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Suffix, " degrees")->

                    DataElement(0, &LightConfiguration::m_projectorNearPlane, "Near plane", "Projector light near plane")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProjectorLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Max, 100.f)->
                        Attribute(AZ::Edit::Attributes::Step, 1.f)->
                        Attribute(AZ::Edit::Attributes::Suffix, " m")->

                    DataElement(0, &LightConfiguration::m_projectorTexture, "Texture", "Projector light texture")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProjectorLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MajorPropertyChanged)->

                    ClassElement(AZ::Edit::ClassElements::Group, "Environment Probe Settings")->
                        Attribute(AZ::Edit::Attributes::AutoExpand, true)->

                    DataElement(0, &LightConfiguration::m_probeArea, "Area dimensions", "Probe area")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProbeLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::StyleForX, "font: bold; color: rgb(184,51,51);")->
                        Attribute(AZ::Edit::Attributes::StyleForY, "font: bold; color: rgb(48,208,120);")->
                        Attribute(AZ::Edit::Attributes::StyleForZ, "font: bold; color: rgb(66,133,244);")->

                    DataElement(0, &LightConfiguration::m_probeSortPriority, "Sort priority", "Sort priority")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProbeLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    DataElement(AZ::Edit::UIHandlers::ComboBox, &LightConfiguration::m_probeCubemapResolution, "Resolution", "Cubemap resolution")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProbeLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MajorPropertyChanged)->
                        EnumAttribute(ResolutionSetting::ResDefault, "Default (256)")->
                        EnumAttribute(ResolutionSetting::Res32, "32")->
                        EnumAttribute(ResolutionSetting::Res64, "64")->
                        EnumAttribute(ResolutionSetting::Res128, "128")->
                        EnumAttribute(ResolutionSetting::Res256, "256")->
                        EnumAttribute(ResolutionSetting::Res512, "512")->

                    DataElement(0, &LightConfiguration::m_probeCubemap, "Cubemap asset", "Cubemap file path")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProbeLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ReadOnly, true)->

                    ClassElement(AZ::Edit::ClassElements::Group, "Animation")->

                    DataElement(0, &LightConfiguration::m_animIndex, "Style", "Light animation curve ID (\"style\") as it corresponds to values in Light.cfx")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::OnAnimationSettingChanged)->
                        Attribute(AZ::Edit::Attributes::Max, 255)->

                    DataElement(0, &LightConfiguration::m_animSpeed, "Speed", "Multiple of the base animation rate")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::OnAnimationSettingChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Max, 4.f)->
                        Attribute(AZ::Edit::Attributes::Suffix, "x")->

                    DataElement(0, &LightConfiguration::m_animPhase, "Phase", "Animation start offset from 0 to 1.  0.1 would be 10% into the animation")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::OnAnimationSettingChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Max, 1.f)->

                    ClassElement(AZ::Edit::ClassElements::Group, "Options")->

                    DataElement(0, &LightConfiguration::m_viewDistMultiplier, "View distance multiplier", "Adjusts max view distance. If 1.0 then default is used. 1.1 would be 10% further than default.")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Suffix, "x")->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->

                    DataElement(AZ::Edit::UIHandlers::ComboBox, &LightConfiguration::m_minSpec, "Minimum spec", "Min spec for light to be active.")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        EnumAttribute(EngineSpec::Never, "Never")->
                        EnumAttribute(EngineSpec::VeryHigh, "Very high")->
                        EnumAttribute(EngineSpec::High, "High")->
                        EnumAttribute(EngineSpec::Medium, "Medium")->
                        EnumAttribute(EngineSpec::Low, "Low")->

                    DataElement(AZ::Edit::UIHandlers::ComboBox, &LightConfiguration::m_castShadowsSpec, "Cast shadow spec", "Min spec for shadow casting.")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MajorPropertyChanged)->
                        EnumAttribute(EngineSpec::Never, "Never")->
                        EnumAttribute(EngineSpec::VeryHigh, "Very high")->
                        EnumAttribute(EngineSpec::High, "High")->
                        EnumAttribute(EngineSpec::Medium, "Medium")->
                        EnumAttribute(EngineSpec::Low, "Low")->

                    DataElement(0, &LightConfiguration::m_ignoreVisAreas, "Ignore vis areas", "Ignore vis areas")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    DataElement(0, &LightConfiguration::m_volumetricFog, "Volumetric fog", "Affects volumetric fog")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    DataElement(0, &LightConfiguration::m_volumetricFogOnly, "Volumetric fog only", "Only affects volumetric fog")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    DataElement(0, &LightConfiguration::m_indoorOnly, "Indoor only", "Indoor only")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    DataElement(0, &LightConfiguration::m_affectsThisAreaOnly, "Affects this area only", "Light only affects this area")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    ClassElement(AZ::Edit::ClassElements::Group, "Advanced")->

                    DataElement(0, &LightConfiguration::m_deferred, "Deferred", "Deferred light")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_Hide"))->         // Deprecated on non mobile platforms - hidden until we have a platform to use this

                    ClassElement(AZ::Edit::ClassElements::Group, "Shadow Settings")->
                        Attribute(AZ::Edit::Attributes::AutoExpand, true)->

                    DataElement(0, &LightConfiguration::m_shadowBias, "Shadow bias", "Shadow bias")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetShadowSettingsVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Max, 100.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.5f)->

                    DataElement(0, &LightConfiguration::m_shadowSlopeBias, "Shadow slope bias", "Shadow slope bias")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetShadowSettingsVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Max, 100.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.5f)->

                    DataElement(0, &LightConfiguration::m_shadowResScale, "Shadow resolution scale", "Shadow res scale")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetShadowSettingsVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Max, 10.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->

                    DataElement(0, &LightConfiguration::m_shadowUpdateMinRadius, "Shadow update radius", "Shadow update min radius")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetShadowSettingsVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Max, 100.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.5f)->
                        Attribute(AZ::Edit::Attributes::Suffix, " m")->

                    DataElement(0, &LightConfiguration::m_shadowUpdateRatio, "Shadow update ratio", "Shadow update ratio")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetShadowSettingsVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Max, 10.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)
                ;
            }
        }
    }

    AZ::Crc32 EditorLightConfiguration::GetAmbientLightVisibility() const 
    {
        return m_lightType != LightType::Probe ?
                AZ_CRC("PropertyVisibility_Show") : AZ_CRC("PropertyVisibility_Hide");
    }

    AZ::Crc32 EditorLightConfiguration::GetPointLightVisibility() const
    {
        return m_lightType == LightType::Point ?
               AZ_CRC("PropertyVisibility_Show") : AZ_CRC("PropertyVisibility_Hide");
    }

    AZ::Crc32 EditorLightConfiguration::GetProjectorLightVisibility() const
    {
        return m_lightType == LightType::Projector ?
               AZ_CRC("PropertyVisibility_Show") : AZ_CRC("PropertyVisibility_Hide");
    }

    AZ::Crc32 EditorLightConfiguration::GetProbeLightVisibility() const
    {
        return m_lightType == LightType::Probe ?
               AZ_CRC("PropertyVisibility_Show") : AZ_CRC("PropertyVisibility_Hide");
    }

    AZ::Crc32 EditorLightConfiguration::GetShadowSettingsVisibility() const
    {
        return m_castShadowsSpec != EngineSpec::Never ?
               AZ_CRC("PropertyVisibility_Show") : AZ_CRC("PropertyVisibility_Hide");
    }

    AZ::Crc32 EditorLightConfiguration::GetAreaSettingVisibility() const
    {
        return m_lightType == LightType::Area ?
               AZ_CRC("PropertyVisibility_Show") : AZ_CRC("PropertyVisibility_Hide");
    }

    AZ::Crc32 EditorLightConfiguration::MajorPropertyChanged()
    {
        if (m_editorEntityId.IsValid())
        {
            EBUS_EVENT_ID(m_editorEntityId, LightComponentEditorRequestBus, RefreshLight);
        }

        return AZ::Edit::PropertyRefreshLevels::EntireTree;
    }

    AZ::Crc32 EditorLightConfiguration::LightTypeChanged() 
    {
        // If the light is now a probe lets cache pointers to some resources.
        if (m_lightType == LightConfiguration::LightType::Probe)
        {
            EBUS_EVENT_ID(m_editorEntityId, LightComponentEditorRequestBus, CacheCubemapPreviewResources);
        }

        return MajorPropertyChanged();
    }

    AZ::Crc32 EditorLightConfiguration::MinorPropertyChanged()
    {
        if (m_editorEntityId.IsValid())
        {
            EBUS_EVENT_ID(m_editorEntityId, LightComponentEditorRequestBus, RefreshLight);
        }

        return AZ::Edit::PropertyRefreshLevels::None;
    }

    AZ::Crc32 EditorLightConfiguration::OnAnimationSettingChanged()
    {
        if (m_editorEntityId.IsValid())
        {
            EBUS_EVENT_ID(m_editorEntityId, LightComponentEditorRequestBus, RefreshLight);

            EBUS_EVENT(LightSettingsNotificationsBus, AnimationSettingsChanged);
        }

        return AZ_CRC("RefreshNone");
    }

    EditorLightComponent::EditorLightComponent()
    {
        m_viewCubemap = false;
        m_cubemapPreview = nullptr;
    }

    EditorLightComponent::~EditorLightComponent()
    {
    }

    void EditorLightComponent::Init()
    {
        Base::Init();
    }

    void EditorLightComponent::Activate()
    {
        Base::Activate();

        m_configuration.m_editorEntityId = GetEntityId();

        m_light.SetEntity(GetEntityId());
        m_light.CreateRenderLight(m_configuration);
        m_light.TurnOn();

        if (m_configuration.m_lightType == LightConfiguration::LightType::Probe)
        {
            CacheCubemapPreviewResources();
        }

        LightComponentEditorRequestBus::Handler::BusConnect(GetEntityId());
        RenderNodeRequestBus::Handler::BusConnect(GetEntityId());
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
    }

    void EditorLightComponent::Deactivate()
    {
        LightComponentEditorRequestBus::Handler::BusDisconnect();
        RenderNodeRequestBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();

        m_light.DestroyRenderLight();
        m_light.SetEntity(AZ::EntityId());

        m_configuration.m_editorEntityId.SetInvalid();

        // Release the cubemap preview.
        SAFE_RELEASE(m_cubemapPreview);

        Base::Deactivate();
    }

    void EditorLightComponent::RefreshLight()
    {
        m_light.UpdateRenderLight(m_configuration);
    }

    IRenderNode* EditorLightComponent::GetRenderNode()
    {
        return m_light.GetRenderNode();
    }

    float EditorLightComponent::GetRenderNodeRequestBusOrder() const
    {
        return LightComponent::s_renderNodeRequestBusOrder;
    }

    bool EditorLightComponent::IsProbe() const
    {
        return (m_configuration.m_lightType == LightConfiguration::LightType::Probe);
    }

    void EditorLightComponent::GenerateCubemap()
    {
        if (IsProbe())
        {
            EBUS_EVENT(AzToolsFramework::EditorRequests::Bus,
                GenerateCubemapForEntity,
                GetEntityId(),
                nullptr);

            if (m_viewCubemap)
            {
                OnViewCubemapChanged();
            }
        }
    }

    void EditorLightComponent::OnViewCubemapChanged()
    {
        if (m_viewCubemap)
        {
            if (m_cubemapPreview)
            {
                m_cubemapPreview = m_cubemapPreview->Clone(false, false, false);
                m_cubemapPreview->SetMaterial(CreateCubemapMaterial());
                m_cubemapPreview->AddRef();
            }
        }
    }

    IMaterial* EditorLightComponent::CreateCubemapMaterial()
    {
        CString deferredCubemapPath = m_configuration.m_probeCubemap.c_str();
        CString matName = Path::GetFileName(deferredCubemapPath);

        if (m_cubemapMatSrc)
        {
            IMaterial* cubemapMatDst = m_materialManager->CreateMaterial(static_cast<const char*>(matName), m_cubemapMatSrc->GetFlags() | MTL_FLAG_NON_REMOVABLE);
            if (cubemapMatDst)
            {
                SShaderItem& si = m_cubemapMatSrc->GetShaderItem();
                SInputShaderResources isr = si.m_pShaderResources;
                isr.m_Textures[EFTT_ENV].m_Name = deferredCubemapPath;

                SShaderItem siDst = m_editor->GetRenderer()->EF_LoadShaderItem(si.m_pShader->GetName(), true, 0, &isr, si.m_pShader->GetGenerationMask());
                cubemapMatDst->AssignShaderItem(siDst);
                return cubemapMatDst;
            }
        }
        return nullptr;
    }

    bool EditorLightComponent::IsViewCubemapReadonly()
    {
        // The view cubemap check box should be read only if there is no cubemap available.
        return !m_configuration.m_probeCubemap.empty();
    }

    void EditorLightComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        LightComponent* lightComponent = gameEntity->CreateComponent<LightComponent>();

        if (lightComponent)
        {
            lightComponent->m_configuration = m_configuration;
        }
    }

    void EditorLightComponent::SetCubemap(const char* cubemap)
    {
        if (cubemap != m_configuration.m_probeCubemap)
        {
            AzToolsFramework::ScopedUndoBatch undo("Cubemap Assignment");

            m_configuration.m_probeCubemap = cubemap;

            EBUS_EVENT(AzToolsFramework::ToolsApplicationRequests::Bus, AddDirtyEntity, GetEntityId());

            if (IsSelected())
            {
                EBUS_EVENT(AzToolsFramework::ToolsApplicationEvents::Bus, InvalidatePropertyDisplay, AzToolsFramework::Refresh_AttributesAndValues);
            }

            if (EditorLightConfiguration::LightType::Probe == m_configuration.m_lightType)
            {
                RefreshLight();
            }
        }
    }

    AZ::u32 EditorLightComponent::GetCubemapResolution()
    {
        return static_cast<AZ::u32>(m_configuration.m_probeCubemapResolution);
    }

    const LightConfiguration& EditorLightComponent::GetConfiguration() const
    {
        return m_configuration;
    }

    void EditorLightComponent::CacheCubemapPreviewResources()
    {
        if (!m_editor)
        {
            EBUS_EVENT_RESULT(m_editor, AzToolsFramework::EditorRequests::Bus, GetEditor);
        }

        if (!m_materialManager)
        {
            m_materialManager = m_editor->Get3DEngine()->GetMaterialManager();
        }

        m_cubemapPreview = m_editor->Get3DEngine()->LoadStatObj("Editor/Objects/envcube.cgf", nullptr, nullptr, false);
        m_cubemapPreview->AddRef();

        m_cubemapMatSrc = m_materialManager->LoadMaterial("Editor/Objects/envcube", false, true);

        // If we needed to cache cubemap resources lets also check to see if we need to display the cubemap.
        OnViewCubemapChanged();
    }

    const char* EditorLightComponent::GetGenerateCubemapButtonText() const
    {
        return (m_configuration.m_probeCubemap.empty()) ? "Create" : "Regenerate";
    }

    void EditorLightComponent::DisplayEntity(bool& handled)
    {
        auto* dc = AzFramework::EntityDebugDisplayRequestBus::FindFirstHandler();
        AZ_Assert(dc, "Invalid display context.");

        AZ::Transform transform = AZ::Transform::CreateIdentity();
        EBUS_EVENT_ID_RESULT(transform, GetEntityId(), AZ::TransformBus, GetWorldTM);

        // Don't draw extra visualization unless selected.
        if (!IsSelected())
        {
            handled = true;
            return;
        }

        transform.ExtractScaleExact();
        dc->PushMatrix(transform);
        const AZ::Vector3& color = m_configuration.m_color;
        dc->SetColor(AZ::Vector4(color.GetX(), color.GetY(), color.GetZ(), 1.f));

        switch (m_configuration.m_lightType)
        {
        case EditorLightConfiguration::LightType::Point:
        {
            dc->DrawWireSphere(AZ::Vector3::CreateZero(), m_configuration.m_pointMaxDistance);
            dc->DrawWireSphere(AZ::Vector3::CreateZero(), m_configuration.m_pointAttenuationBulbSize);
            break;
        }
        case EditorLightConfiguration::LightType::Area:
        {
            dc->SetColor(AZ::Vector4(color.GetX(), color.GetY(), color.GetZ(), 0.5f));
            const auto& area = AZ::Vector3(m_configuration.m_areaMaxDistance, m_configuration.m_areaWidth, m_configuration.m_areaHeight);
            dc->DrawWireBox(AZ::Vector3(0, -area.GetY() * 0.5f, -area.GetZ() * 0.5f),
                AZ::Vector3(area.GetX(),  area.GetY() * 0.5f,  area.GetZ() * 0.5f));
            break;
        }
        case EditorLightConfiguration::LightType::Projector:
        {
            dc->SetColor(AZ::Vector4(color.GetX(), color.GetY(), color.GetZ(), 0.5f));

            const float range = m_configuration.m_projectorRange;
            const float attenuation = m_configuration.m_projectorAttenuationBulbSize;
            const float nearPlane = m_configuration.m_projectorNearPlane;

            DrawProjectionGizmo(dc, range);
            DrawProjectionGizmo(dc, attenuation);
            DrawProjectionGizmo(dc, nearPlane);

            break;
        }
        case EditorLightConfiguration::LightType::Probe:
        {
            const auto& area = m_configuration.m_probeArea;
            dc->DrawWireBox(AZ::Vector3(-area.GetX() * 0.5f, -area.GetY() * 0.5f, -area.GetZ() * 0.5f),
                AZ::Vector3(area.GetX() * 0.5f,  area.GetY() * 0.5f,  area.GetZ() * 0.5f));

            if (m_viewCubemap && m_cubemapPreview != nullptr)
            {
                SRendParams rp;
                SRenderingPassInfo passInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(gEnv->p3DEngine->GetRenderingCamera());

                AZ::Vector3 translation = transform.GetTranslation();

                rp.AmbientColor = ColorF(1.0f, 1.0f, 1.0f, 1);
                rp.fAlpha = 1;
                rp.pMatrix = &Matrix34::CreateTranslationMat(Vec3(translation.GetX(), translation.GetY(), translation.GetZ()));
                rp.pMaterial = m_cubemapPreview->GetMaterial();

                m_cubemapPreview->Render(rp, passInfo);
            }
            break;
        }
        }

        dc->PopMatrix();

        handled = true;
    }

    void EditorLightComponent::DrawProjectionGizmo(AzFramework::EntityDebugDisplayRequests* dc, const float radius) const
    {
        const int numPoints = 16;     // per one arc
        const int numArcs = 6;

        AZ::Vector3 points[numPoints * numArcs];
        {
            // Generate 4 arcs on intersection of sphere with pyramid.
            const float fov = DEG2RAD(m_configuration.m_projectorFOV);

            const AZ::Vector3 lightAxis(radius, 0.0f, 0.0f);
            const float tanA = tan_tpl(fov * 0.5f);
            const float fovProj = asin_tpl(1.0f / sqrtf(2.0f + 1.0f / (tanA * tanA))) * 2.0f;

            const float halfFov = 0.5f * fov;
            const float halfFovProj = fovProj * 0.5f;
            const float anglePerSegmentOfFovProj = 1.0f / (numPoints - 1) * fovProj;

            const AZ::Quaternion yRot = AZ::Quaternion::CreateRotationY(halfFov);
            AZ::Vector3* arcPoints = points;
            for (int i = 0; i < numPoints; ++i)
            {
                float angle = i * anglePerSegmentOfFovProj - halfFovProj;
                arcPoints[i] = yRot * AZ::Quaternion::CreateRotationZ(angle) * lightAxis;
            }

            const AZ::Quaternion zRot = AZ::Quaternion::CreateRotationZ(halfFov);
            arcPoints += numPoints;
            for (int i = 0; i < numPoints; ++i)
            {
                float angle = (numPoints - i - 1) * anglePerSegmentOfFovProj - halfFovProj;
                arcPoints[i] = zRot * AZ::Quaternion::CreateRotationY(angle) * lightAxis;
            }

            const AZ::Quaternion nyRot = AZ::Quaternion::CreateRotationY(-halfFov);
            arcPoints += numPoints;
            for (int i = 0; i < numPoints; ++i)
            {
                float angle = (numPoints - i - 1) * anglePerSegmentOfFovProj - halfFovProj;
                arcPoints[i] = nyRot * AZ::Quaternion::CreateRotationZ(angle) * lightAxis;
            }

            const AZ::Quaternion nzRot = AZ::Quaternion::CreateRotationZ(-halfFov);
            arcPoints += numPoints;
            for (int i = 0; i < numPoints; ++i)
            {
                float angle = i * anglePerSegmentOfFovProj - halfFovProj;
                arcPoints[i] = nzRot * AZ::Quaternion::CreateRotationY(angle) * lightAxis;
            }

            arcPoints += numPoints;
            const float anglePerSegmentOfFov = 1.0f / (numPoints - 1) * fov;
            for (int i = 0; i < numPoints; ++i)
            {
                float angle = i * anglePerSegmentOfFov - halfFov;
                arcPoints[i] = AZ::Quaternion::CreateRotationY(angle) * lightAxis;
            }

            arcPoints += numPoints;
            for (int i = 0; i < numPoints; ++i)
            {
                float angle = i * anglePerSegmentOfFov - halfFov;
                arcPoints[i] = AZ::Quaternion::CreateRotationZ(angle) * lightAxis;
            }
        }

        // Draw pyramid and sphere intersection.
        dc->DrawPolyLine(points, numPoints * 4, false);

        // Draw cross.
        dc->DrawPolyLine(points + numPoints * 4, numPoints, false);
        dc->DrawPolyLine(points + numPoints * 5, numPoints, false);
        dc->DrawLine(AZ::Vector3::CreateZero(), points[numPoints * 0]);
        dc->DrawLine(AZ::Vector3::CreateZero(), points[numPoints * 1]);
        dc->DrawLine(AZ::Vector3::CreateZero(), points[numPoints * 2]);
        dc->DrawLine(AZ::Vector3::CreateZero(), points[numPoints * 3]);
    }
} // namespace LmbrCentral
