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

#include "EditorParticleComponent.h"

#include <AzCore/Asset/AssetDatabase.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <Include/IEditorParticleManager.h>

// Effect properties are disabled from the edit context until we can 
// promote them into per-emitter properties. 
#define NON_EDITABLE_EFFECT_PROPERTIES (0)

namespace LmbrCentral
{
    //////////////////////////////////////////////////////////////////////////
    void EditorParticleComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<EditorParticleComponent, EditorComponentBase>()->
                Version(1)->
                Field("Visible", &EditorParticleComponent::m_visible)->
                Field("ParticleLibrary", &EditorParticleComponent::m_asset)->
                Field("SelectedEmitter", &EditorParticleComponent::m_selectedEmitter)->
                Field("LibrarySource", &EditorParticleComponent::m_librarySource)->
                Field("Particle", &EditorParticleComponent::m_settings)
            ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();

            if (editContext)
            {
                editContext->Class<EditorParticleComponent>("Particle", "")->

                    ClassElement(AZ::Edit::ClassElements::EditorData, "")->
                    Attribute(AZ::Edit::Attributes::Category, "Rendering")->
                    Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/Particle")->
                    Attribute(AZ::Edit::Attributes::PrimaryAssetType, AZ::AzTypeInfo<LmbrCentral::ParticleAsset>::Uuid())->
                    Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/Particle.png")->
                    Attribute(AZ::Edit::Attributes::AutoExpand, true)->
                    Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))->

                    DataElement(0, &EditorParticleComponent::m_visible, "Visible", "Whether or not the particle emitter is currently visible")->
                    Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorParticleComponent::OnVisibilityChanged)->

                    DataElement(0, &EditorParticleComponent::m_asset, "Particle effect library", "The library of particle effects.")->
                    Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorParticleComponent::OnAssetChanged)->
                    Attribute(AZ::Edit::Attributes::Visibility, &EditorParticleComponent::GetAssetVisibility)->

                    DataElement(AZ::Edit::UIHandlers::ComboBox, &EditorParticleComponent::m_selectedEmitter, "Emitters", "List of emitters in this library.")->
                    Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorParticleComponent::OnEmitterSelected)->
                    Attribute(AZ::Edit::Attributes::StringList, &EditorParticleComponent::GetEmitterNames)->

                    DataElement(AZ::Edit::UIHandlers::ComboBox, &EditorParticleComponent::m_librarySource, "Library source", "Particle libraries may come from an asset or from the level.")->
                    Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorParticleComponent::OnLibrarySourceChange)->
                    EnumAttribute(LibrarySource::Level, "Level")->
                    EnumAttribute(LibrarySource::File, "File")->

                    DataElement(0, &EditorParticleComponent::m_settings, "Settings", "")->
                    Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorParticleComponent::OnSpawnParamChanged)
                ;

                editContext->Class<ParticleEmitterSettings>("ParticleSettings", "")->

                    ClassElement(AZ::Edit::ClassElements::EditorData, "")->
                    Attribute(AZ::Edit::Attributes::AutoExpand, true)->

                    ClassElement(AZ::Edit::ClassElements::Group, "General")->

                    DataElement(AZ::Edit::UIHandlers::ComboBox, &ParticleEmitterSettings::m_attachType, "Attach type", "What type of object particles emitted from.")->
                    EnumAttribute(EGeomType::GeomType_None, "None")->
                    EnumAttribute(EGeomType::GeomType_BoundingBox, "Bounding box")->
                    EnumAttribute(EGeomType::GeomType_Physics, "Physics")->
                    EnumAttribute(EGeomType::GeomType_Render, "Render")->

                    DataElement(AZ::Edit::UIHandlers::ComboBox, &ParticleEmitterSettings::m_attachForm, "Attach form", "What aspect of shape emitted from.")->
                    EnumAttribute(EGeomForm::GeomForm_Surface, "Surface")->
                    EnumAttribute(EGeomForm::GeomForm_Edges, "Edges")->
                    EnumAttribute(EGeomForm::GeomForm_Vertices, "Vertices")->
                    EnumAttribute(EGeomForm::GeomForm_Volume, "Volume")->


#if NON_EDITABLE_EFFECT_PROPERTIES
                    ClassElement(AZ::Edit::ClassElements::Group, "Effect Properties")->
                    DataElement(AZ::Edit::UIHandlers::Vector3, &ParticleEmitterSettings::m_positionOffset, "Position offset", "Offsets the spawn point")->
                    DataElement(AZ::Edit::UIHandlers::Vector3, &ParticleEmitterSettings::m_randomOffset, "Random offset", "Max random range applied to the offset")->
                    DataElement(AZ::Edit::UIHandlers::Default, &ParticleEmitterSettings::m_sizeX, "Size X", "Scaling on X")->
                    DataElement(AZ::Edit::UIHandlers::Slider, &ParticleEmitterSettings::m_sizeRandomX, "Size random X", "Percent range applied to scaling on X")->
                    Attribute(AZ::Edit::Attributes::Max, 1.0f)->
                    Attribute(AZ::Edit::Attributes::Min, 0.0f)->
                    DataElement(AZ::Edit::UIHandlers::Default, &ParticleEmitterSettings::m_sizeY, "Size Y", "Scaling on Y")->
                    DataElement(AZ::Edit::UIHandlers::Slider, &ParticleEmitterSettings::m_sizeRandomY, "Size random Y", "Percent range applied to scaling on Y")->
                    Attribute(AZ::Edit::Attributes::Max, 1.0f)->
                    Attribute(AZ::Edit::Attributes::Min, 0.0f)->
                    DataElement(AZ::Edit::UIHandlers::Vector3, &ParticleEmitterSettings::m_initAngles, "Initial angles", "Initial rotation on X Y and Z")->
                    DataElement(AZ::Edit::UIHandlers::Default, &ParticleEmitterSettings::m_rotationRateX, "Rotate rate X", "Rotation rate on the X axis (deg/sec)")->
                    DataElement(AZ::Edit::UIHandlers::Slider, &ParticleEmitterSettings::m_rotationRateRandomX, "Random rate X", "Percent range applied to rotation rate on X")->
                    Attribute(AZ::Edit::Attributes::Max, 1.0f)->
                    Attribute(AZ::Edit::Attributes::Min, 0.0f)->
                    DataElement(AZ::Edit::UIHandlers::Default, &ParticleEmitterSettings::m_rotationRateY, "Rotate rate Y", "Rotation rate on the Y axis (deg/sec)")->
                    DataElement(AZ::Edit::UIHandlers::Slider, &ParticleEmitterSettings::m_rotationRateRandomY, "Random rate Y", "Percent range applied to rotation rate on Y")->
                    Attribute(AZ::Edit::Attributes::Max, 1.0f)->
                    Attribute(AZ::Edit::Attributes::Min, 0.0f)->
                    DataElement(AZ::Edit::UIHandlers::Default, &ParticleEmitterSettings::m_rotationRateZ, "Rotate rate Z", "Rotation rate on the Z axis (deg/sec)")->
                    DataElement(AZ::Edit::UIHandlers::Slider, &ParticleEmitterSettings::m_rotationRateRandomZ, "Random rate Z", "Percent range applied to rotation rate on Z")->
                    Attribute(AZ::Edit::Attributes::Max, 1.0f)->
                    Attribute(AZ::Edit::Attributes::Min, 0.0f)->
                    DataElement(AZ::Edit::UIHandlers::Vector3, &ParticleEmitterSettings::m_rotationRandomAngles, "Random angles", "Max random angle range applied to rotation")->
                    DataElement(AZ::Edit::UIHandlers::Color, &ParticleEmitterSettings::m_color, "Color", "A color to add to the particles")->
                    DataElement(AZ::Edit::UIHandlers::Default, &ParticleEmitterSettings::m_geometryAsset, "Geometry", "Geometry for 3D particles")->
#endif

                    ClassElement(AZ::Edit::ClassElements::Group, "Spawn Properties")->

                    DataElement(AZ::Edit::UIHandlers::CheckBox, &ParticleEmitterSettings::m_countPerUnit, "Count per unit", "Multiply particle count also by geometry extent (length/area/volume).")->     // todo: better name?
                    DataElement(AZ::Edit::UIHandlers::CheckBox, &ParticleEmitterSettings::m_prime, "Prime", "Set emitter as though it's been running indefinitely.")->
                    DataElement(0, &ParticleEmitterSettings::m_countScale, "Count scale", "Multiple for particle count (on top of \"Count Per Unit\" if set).")->
                    DataElement(0, &ParticleEmitterSettings::m_timeScale, "Time scale", "Multiple for emitter time evolution.")->
                    DataElement(0, &ParticleEmitterSettings::m_pulsePeriod, "Pulse period", "How often to restart emitter.")->
                    DataElement(0, &ParticleEmitterSettings::m_sizeScale, "Size scale", "Multiple for all effect sizes.")->
                    DataElement(0, &ParticleEmitterSettings::m_speedScale, "Speed scale", "Multiple for particle emission speed.")->
                    DataElement(0, &ParticleEmitterSettings::m_strength, "Strength", "Controls parameter strength curves.")->
                    DataElement(AZ::Edit::UIHandlers::CheckBox, &ParticleEmitterSettings::m_ignoreRotation, "Ignore rotation", "Ignored the entity's rotation.")->
                    DataElement(AZ::Edit::UIHandlers::CheckBox, &ParticleEmitterSettings::m_notAttached, "Not attached", "Set emitter as though it's been running indefinitely.")->
                    DataElement(AZ::Edit::UIHandlers::CheckBox, &ParticleEmitterSettings::m_registerByBBox, "Register by bounding box", "Use the Bounding Box instead of Position to Register in VisArea.")->
                    
                    ClassElement(AZ::Edit::ClassElements::Group, "Audio")->
                    DataElement(AZ::Edit::UIHandlers::CheckBox, &ParticleEmitterSettings::m_enableAudio, "Enable audio", "Used by particle effect instances to indicate whether audio should be updated or not.")->
                    DataElement(0, &ParticleEmitterSettings::m_audioRTPC, "Audio RTPC", "Indicates what audio RTPC this particle effect instance drives.")
                ;
            }
        }
    }

    EditorParticleComponent::EditorParticleComponent()
    {
        m_visible = true;
    }

    EditorParticleComponent::~EditorParticleComponent()
    {
    }

    void EditorParticleComponent::Init()
    {
        EditorComponentBase::Init();
    }

    void EditorParticleComponent::Activate()
    {
        EditorComponentBase::Activate();

        // Populates the emitter dropdown.
        OnAssetChanged();

        ResolveLevelEmitter();

        m_selectedEmitter = m_settings.m_selectedEmitter;
        SetDirty();

        m_emitter.AttachToEntity(m_entity->GetId());
        m_emitter.Set(m_selectedEmitter, m_settings);

        m_emitter.SetVisibility(m_visible);

        ParticleComponentRequestBus::Handler::BusConnect(m_entity->GetId());
        RenderNodeRequestBus::Handler::BusConnect(m_entity->GetId());
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
    }

    void EditorParticleComponent::Deactivate()
    {
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        RenderNodeRequestBus::Handler::BusDisconnect();
        ParticleComponentRequestBus::Handler::BusDisconnect();

        m_emitter.Clear();
        m_emitter.AttachToEntity(AZ::EntityId());

        EditorComponentBase::Deactivate();
    }

    IRenderNode* EditorParticleComponent::GetRenderNode()
    {
        return m_emitter.GetRenderNode();
    }

    float EditorParticleComponent::GetRenderNodeRequestBusOrder() const
    {
        return ParticleComponent::s_renderNodeRequestBusOrder;
    }

    void EditorParticleComponent::DisplayEntity(bool& handled)
    {
        // Don't draw extra visualization unless selected.
        if (!IsSelected() || m_emitter.IsCreated())
        {
            handled = true;
        }
    }

    void EditorParticleComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        ParticleComponent* component = gameEntity->CreateComponent<ParticleComponent>();

        if (component)
        {
            // Copy the editor-side settings to the game entity to be exported.
            component->m_settings = m_settings;
        }
    }

    void EditorParticleComponent::OnEmitterSelected()
    {
        SetDirty();

        m_settings.m_selectedEmitter = m_selectedEmitter;
        m_emitter.Set(m_selectedEmitter, m_settings);

        EBUS_EVENT(AzToolsFramework::ToolsApplicationEvents::Bus, InvalidatePropertyDisplay, AzToolsFramework::Refresh_AttributesAndValues);
    }

    void EditorParticleComponent::OnSpawnParamChanged()
    {
        m_emitter.SpawnEmitter(m_settings);
    }

    void EditorParticleComponent::OnAssetReady(const AZ::Data::Asset<AZ::Data::AssetData>& asset)
    {
        if (asset.GetId() == m_asset.GetId())
        {
            // Grab emitter list from asset and refresh properties.
            m_emitterNames = static_cast<ParticleAsset*>(asset.Get())->m_emitterNames;

            // If there is no selected emitter, we will automatically select the first one to ensure we see something right away.
            if (!m_emitterNames.empty() && m_selectedEmitter.empty())
            {
                m_selectedEmitter = m_emitterNames[0];
                OnEmitterSelected();
            }
            else
            {
                EBUS_EVENT(AzToolsFramework::ToolsApplicationEvents::Bus, InvalidatePropertyDisplay, AzToolsFramework::Refresh_AttributesAndValues);
            }
        }
    }

    void EditorParticleComponent::OnAssetReloaded(const AZ::Data::Asset<AZ::Data::AssetData>& asset)
    {
        m_asset = asset;
        OnAssetReady(asset);
    }

    void EditorParticleComponent::OnAssetChanged()
    {
        m_emitterNames.clear();
        m_emitter.Clear();

        if (AZ::Data::AssetBus::Handler::BusIsConnected())
        {
            AZ::Data::AssetBus::Handler::BusDisconnect();
        }

        if (m_asset.GetId().IsValid())
        {
            AZ::Data::AssetBus::Handler::BusConnect(m_asset.GetId());
            m_asset.QueueLoad();
        }

        EBUS_EVENT(AzToolsFramework::ToolsApplicationEvents::Bus, InvalidatePropertyDisplay, AzToolsFramework::Refresh_AttributesAndValues);
    }

    void EditorParticleComponent::SetPrimaryAsset(const AZ::Data::AssetId& id)
    {
        m_asset.Create(id, true);
        EBUS_EVENT(AzToolsFramework::ToolsApplicationRequests::Bus, AddDirtyEntity, GetEntityId());
        OnAssetChanged();
    }

    AZ::u32 EditorParticleComponent::GetAssetVisibility() const
    {
        return m_librarySource == LibrarySource::File ?
               AZ_CRC("PropertyVisibility_Show") : AZ_CRC("PropertyVisibility_Hide");
    }

    AZ::u32 EditorParticleComponent::OnLibrarySourceChange()
    {
        m_emitter.Clear();
        m_emitterNames.clear();
        m_selectedEmitter.clear();
        m_asset = AZ::Data::Asset<ParticleAsset>();

        if (m_librarySource == LibrarySource::Level)
        {
            ResolveLevelEmitter();
        }

        return AZ::Edit::PropertyRefreshLevels::EntireTree;
    }

    void EditorParticleComponent::ResolveLevelEmitter()
    {
        if (m_librarySource == LibrarySource::Level)
        {
            IEditor* editor = nullptr;
            EBUS_EVENT_RESULT(editor, AzToolsFramework::EditorRequests::Bus, GetEditor);
            IEditorParticleManager* pPartManager = editor->GetParticleManager();
            if (pPartManager)
            {
                IDataBaseLibrary* lib = pPartManager->GetLevelLibrary();
                {
                    for (int i = 0; i < lib->GetItemCount(); ++i)
                    {
                        IDataBaseItem* item = lib->GetItem(i);
                        auto itemName = item->GetName();

                        AZStd::string effectName = AZStd::string::format("Level.%s", itemName.GetString());
                        m_emitterNames.push_back(effectName);
                    }
                }

                if (!m_emitterNames.empty())
                {
                    // If we did not have a previously selected emitter, or the selected emitter is not in the list of Level emitters, use the first emitter in the list
                    if (m_selectedEmitter.empty() || AZStd::find(m_emitterNames.begin(), m_emitterNames.end(), m_selectedEmitter) == m_emitterNames.end())
                    {
                        m_selectedEmitter = m_emitterNames[0];
                    }

                    OnEmitterSelected();
                }
            }
        }
    }

    AZ::u32 EditorParticleComponent::OnVisibilityChanged()
    {
        m_settings.m_visible = m_visible;

        OnSpawnParamChanged();

        return AZ_CRC("RefreshValues");
    }
} // namespace LmbrCentral
