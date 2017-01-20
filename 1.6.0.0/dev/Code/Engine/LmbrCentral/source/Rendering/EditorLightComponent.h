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
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <AzFramework/Entity/EntityDebugDisplayBus.h>

#include "LightComponent.h"

namespace LmbrCentral
{
    /**
     * Extends LightConfiguration structure to add editor functionality
     * such as property handlers and visibility filters, as well as
     * reflection for editing.
     */
    class EditorLightConfiguration
        : public LightConfiguration
    {
    public:

        AZ_TYPE_INFO(EditorLightConfiguration, "{1D3B114F-8FB2-47BD-9C21-E089F4F37861}");

        ~EditorLightConfiguration() override {}

        static void Reflect(AZ::ReflectContext* context);

        AZ::Crc32 GetAmbientLightVisibility() const override;
        AZ::Crc32 GetPointLightVisibility() const override;
        AZ::Crc32 GetProjectorLightVisibility() const override;
        AZ::Crc32 GetProbeLightVisibility() const override;
        AZ::Crc32 GetShadowSettingsVisibility() const override;
        AZ::Crc32 GetAreaSettingVisibility() const override;

        AZ::Crc32 MinorPropertyChanged() override;
        AZ::Crc32 MajorPropertyChanged() override;
        AZ::Crc32 LightTypeChanged() override;
        AZ::Crc32 OnAnimationSettingChanged() override;
    };

    /*!
     * In-editor light component.
     * Handles previewing and activating lights in the editor.
     */
    class EditorLightComponent
        : public AzToolsFramework::Components::EditorComponentBase
        , private AzFramework::EntityDebugDisplayEventBus::Handler
        , private LightComponentEditorRequestBus::Handler
        , private RenderNodeRequestBus::Handler
    {
    private:
        using Base = AzToolsFramework::Components::EditorComponentBase;
    public:
        AZ_COMPONENT(EditorLightComponent, "{33BB1CD4-6A33-46AA-87ED-8BBB40D94B0D}", AzToolsFramework::Components::EditorComponentBase);

        EditorLightComponent();
        ~EditorLightComponent() override;

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AzFramework::EntityDebugDisplayEventBus interface implementation
        void DisplayEntity(bool& handled) override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // LightComponentEditorRequestBus::Handler interface implementation
        void SetCubemap(const char* cubemap) override;
        AZ::u32 GetCubemapResolution() override;
        void RefreshLight() override;
        const LightConfiguration& GetConfiguration() const override;
        void CacheCubemapPreviewResources() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // RenderNodeRequestBus::Handler interface implementation
        IRenderNode* GetRenderNode() override;
        float GetRenderNodeRequestBusOrder() const override;
        //////////////////////////////////////////////////////////////////////////

        void BuildGameEntity(AZ::Entity* gameEntity) override;

        //////////////////////////////////////////////////////////////////////////
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            LightComponent::GetProvidedServices(provided);
        }
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
        {
            LightComponent::GetDependentServices(dependent);
        }
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            LightComponent::GetRequiredServices(required);
        }

        static void Reflect(AZ::ReflectContext* context);
        //////////////////////////////////////////////////////////////////////////

    private:

        bool IsProbe() const;
        void GenerateCubemap();
        void OnViewCubemapChanged();
        IMaterial* CreateCubemapMaterial();
        bool IsViewCubemapReadonly();

        const char* GetGenerateCubemapButtonText() const;

        void DrawProjectionGizmo(AzFramework::EntityDebugDisplayRequests* dc, const float radius) const;

        EditorLightConfiguration m_configuration;
        bool m_viewCubemap;
        bool m_cubemapRegen;

        IStatObj* m_cubemapPreview;
        IMaterial* m_cubemapMatSrc;

        LightInstance m_light;

        //Statics that can be used by every light
        static IEditor*             m_editor;
        static IMaterialManager*    m_materialManager;
    };
} // namespace LmbrCentral

