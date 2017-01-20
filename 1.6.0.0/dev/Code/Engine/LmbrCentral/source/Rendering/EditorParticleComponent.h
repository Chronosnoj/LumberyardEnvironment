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

#include <AzCore/Asset/AssetCommon.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>

#include <LmbrCentral/Rendering/ParticleAsset.h>

#include "ParticleComponent.h"

namespace LmbrCentral
{
    /*!
    * In-editor particle component.
    * Loads available emitters from a specified particle effect library.
    */
    class EditorParticleComponent
        : public AzToolsFramework::Components::EditorComponentBase
        , public ParticleComponentRequestBus::Handler
        , public RenderNodeRequestBus::Handler
        , private AzFramework::EntityDebugDisplayEventBus::Handler
        , private AZ::Data::AssetBus::Handler
    {
    public:

        friend class EditorParticleComponentFactory;

        AZ_COMPONENT(EditorParticleComponent, "{0F35739E-1B40-4497-860D-D6FF5D87A9D9}", AzToolsFramework::Components::EditorComponentBase);

        EditorParticleComponent();
        ~EditorParticleComponent() override;

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // RenderNodeRequestBus interface implementation
        IRenderNode* GetRenderNode() override;
        float GetRenderNodeRequestBusOrder() const override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AzFramework::EntityDebugDisplayEventBus interface implementation
        void DisplayEntity(bool& handled) override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AssetEventHandler
        void OnAssetReady(const AZ::Data::Asset<AZ::Data::AssetData>& asset) override;
        void OnAssetReloaded(const AZ::Data::Asset<AZ::Data::AssetData>& asset) override;
        //////////////////////////////////////////////////////////////////////////

        //! Called during export for the game-side entity creation.
        void BuildGameEntity(AZ::Entity* gameEntity) override;

        //! Called when you want to change the game asset through code (like when creating components based on assets).
        void SetPrimaryAsset(const AZ::Data::AssetId& assetId) override;

        //! Invoked in the editor when the user assigns a new asset.
        void OnAssetChanged();

        //! Invoked in the editor when the user selects an emitter from the combo box.
        void OnEmitterSelected();

        //! Invoked when the library source enum is changed
        AZ::u32 OnLibrarySourceChange();

        //! Invoked in the editor when the user adjusts any properties.
        void OnSpawnParamChanged();

        //! Used to populate the emitter combo box.
        AZStd::vector<AZStd::string> GetEmitterNames() const { return m_emitterNames; }

        // ParticleComponentRequestBus
        void Show() override { m_emitter.Show(); }
        void Hide() override { m_emitter.Hide(); }
        void SetVisibility(bool visible) override { m_emitter.SetVisibility(visible); }
        //////////////////////////////////////////////////////////////////////////

    protected:

        //! Particle libraries may be assets or part of the level.
        enum class LibrarySource
        {
            Level,
            File
        };

        LibrarySource m_librarySource = LibrarySource::File;

        AZ::u32 GetAssetVisibility() const;

        void ResolveLevelEmitter();

        AZ::u32 OnVisibilityChanged();

        //! Emitter visibility
        bool m_visible;

        //! Particle library asset.
        AZ::Data::Asset<ParticleAsset> m_asset;

        //! Emitter to use.
        AZStd::string m_selectedEmitter;

        //! Non serialized list (editor only)
        AZStd::vector<AZStd::string> m_emitterNames;

        ParticleEmitterSettings m_settings;
        ParticleEmitter m_emitter;

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("ParticleService"));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("TransformService"));
        }

        static void Reflect(AZ::ReflectContext* context);
    };
} // namespace LmbrCentral
