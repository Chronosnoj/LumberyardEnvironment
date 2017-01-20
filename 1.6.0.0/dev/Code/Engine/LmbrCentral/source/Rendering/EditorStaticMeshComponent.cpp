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

#include "EditorStaticMeshComponent.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Asset/AssetDatabase.h>

#include <MathConversion.h>

#include <IPhysics.h> // For basic physicalization at edit-time for object snapping.

namespace LmbrCentral
{
    void EditorStaticMeshComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<EditorStaticMeshComponent, EditorComponentBase>()
                ->Version(1)
                ->Field("Static Mesh Render Node", &EditorStaticMeshComponent::m_mesh)
                ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();

            if (editContext)
            {
                editContext->Class<EditorStaticMeshComponent>("Static Mesh", "Attach static geometry to the entity.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Rendering")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/StaticMesh.png")
                        ->Attribute(AZ::Edit::Attributes::PrimaryAssetType, AZ::AzTypeInfo<LmbrCentral::StaticMeshAsset>::Uuid())
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/StaticMesh.png")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorStaticMeshComponent::m_mesh);

                editContext->Class<StaticMeshComponentRenderNode::StaticMeshRenderOptions>(
                    "Render Options", "Rendering options for the mesh.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly"))

                    ->ClassElement(AZ::Edit::ClassElements::Group, "Options")

                    ->DataElement(AZ::Edit::UIHandlers::Slider, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_opacity, "Opacity", "Opacity value")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->Attribute(AZ::Edit::Attributes::Max, 1.f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.1f)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_maxViewDist, "Max view distance", "Maximum view distance in meters.")
                        ->Attribute(AZ::Edit::Attributes::Suffix, " m")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->Attribute(AZ::Edit::Attributes::Max, &StaticMeshComponentRenderNode::GetDefaultMaxViewDist)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.1f)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_viewDistMultiplier, "View distance multiplier", "Adjusts max view distance. If 1.0 then default is used. 1.1 would be 10% further than default.")
                        ->Attribute(AZ::Edit::Attributes::Suffix, "x")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Slider, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_lodRatio, "LOD distance ratio", "Controls LOD ratio over distance.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                        ->Attribute(AZ::Edit::Attributes::Min, 0)
                        ->Attribute(AZ::Edit::Attributes::Max, 255)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_castShadows, "Cast dynamic shadows", "Casts dynamic shadows (shadow maps).")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_castLightmap, "Cast static shadows", "Casts static shadows (lightmap).")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_indoorOnly, "Indoor only", "Only render in indoor areas.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)

                    ->ClassElement(AZ::Edit::ClassElements::Group, "Advanced")

                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_allowBloom, "Enable bloom", "Enable bloom post effect.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_allowMotionBlur, "Enable motion blur", "Enable motion blur post effect.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_rainOccluder, "Rain occluder", "Occludes dynamic raindrops.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_affectDynamicWater, "Affect dynamic water", "Will generate ripples in dynamic water.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_receiveWind, "Receive wind", "Receives wind.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_acceptDecals, "Accept decals", "Can receive decals.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_affectNavmesh, "Affect navmesh", "Will affect navmesh generation.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_visibilityOccluder, "Visibility occluder", "Is appropriate for occluding visibility of other objects.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_depthTest, "Depth test", "Require depth testing.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ;

                editContext->Class<StaticMeshComponentRenderNode>(
                    "Mesh Rendering", "Attach geometry to the entity.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::m_visible, "Visible", "Is currently visible.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::RefreshRenderState)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::m_staticMeshAsset, "Static asset", "Static mesh asset reference")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::OnAssetPropertyChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::m_material, "Material override", "Optionally specify an override material.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::OnAssetPropertyChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::m_renderOptions, "Render options", "Render/draw options.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::RefreshRenderState)
                    ;
            }
        }
    }

    void EditorStaticMeshComponent::Activate()
    {
        EditorComponentBase::Activate();

        m_mesh.AttachToEntity(m_entity->GetId());

        // Note we are purposely connecting to buses before calling m_mesh.CreateMesh().
        // m_mesh.CreateMesh() can result in events (eg: OnMeshCreated) that we want receive.
        MaterialRequestBus::Handler::BusConnect(m_entity->GetId());
        MeshComponentRequestBus::Handler::BusConnect(m_entity->GetId());
        MeshComponentNotificationBus::Handler::BusConnect(m_entity->GetId());
        RenderNodeRequestBus::Handler::BusConnect(m_entity->GetId());
        AzToolsFramework::EntitySelectionEvents::Bus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(m_entity->GetId());
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());

        auto renderOptionsChangeCallback =
            [this]()
        {
            this->m_mesh.ApplyRenderOptions();
        };
        m_mesh.m_renderOptions.m_changeCallback = renderOptionsChangeCallback;

        m_mesh.CreateMesh();
    }

    void EditorStaticMeshComponent::Deactivate()
    {
        MaterialRequestBus::Handler::BusDisconnect();
        MeshComponentRequestBus::Handler::BusDisconnect();
        MeshComponentNotificationBus::Handler::BusDisconnect();
        RenderNodeRequestBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();

        DestroyEditorPhysics();

        m_mesh.m_renderOptions.m_changeCallback = 0;

        m_mesh.DestroyMesh();
        m_mesh.AttachToEntity(AZ::EntityId());

        EditorComponentBase::Deactivate();
    }

    void EditorStaticMeshComponent::OnMeshCreated(const AZ::Data::Asset<AZ::Data::AssetData>& asset)
    {
        (void)asset;

        CreateEditorPhysics();

        if (m_physicalEntity)
        {
            OnTransformChanged(GetTransform()->GetLocalTM(), GetTransform()->GetWorldTM());
        }
    }

    void EditorStaticMeshComponent::OnMeshDestroyed()
    {
        DestroyEditorPhysics();
    }

    IRenderNode* EditorStaticMeshComponent::GetRenderNode()
    {
        return &m_mesh;
    }

    float EditorStaticMeshComponent::GetRenderNodeRequestBusOrder() const
    {
        return s_renderNodeRequestBusOrder;
    }

    void EditorStaticMeshComponent::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        if (m_physicalEntity)
        {
            if ((m_physTransform.RetrieveScale() - world.RetrieveScale()).GetLengthSq() > FLT_EPSILON)
            {
                // Scale changes require re-physicalizing.
                DestroyEditorPhysics();
                CreateEditorPhysics();
            }
            else
            {
                Matrix34 transform = AZTransformToLYTransform(world);

                pe_params_pos par_pos;
                par_pos.pMtx3x4 = &transform;
                m_physicalEntity->SetParams(&par_pos);
            }

            m_physTransform = world;
        }
    }

    AZ::Aabb EditorStaticMeshComponent::GetWorldBounds()
    {
        return m_mesh.CalculateWorldAABB();
    }

    AZ::Aabb EditorStaticMeshComponent::GetLocalBounds()
    {
        return m_mesh.CalculateLocalAABB();
    }

    void EditorStaticMeshComponent::SetMeshAsset(const AZ::Data::AssetId& id)
    {
        m_mesh.SetMeshAsset(id);
        EBUS_EVENT(AzToolsFramework::ToolsApplicationRequests::Bus, AddDirtyEntity, GetEntityId());
    }

    void EditorStaticMeshComponent::SetMaterial(IMaterial* material)
    {
        m_mesh.SetMaterial(material);

        EBUS_EVENT(AzToolsFramework::ToolsApplicationEvents::Bus, InvalidatePropertyDisplay, AzToolsFramework::Refresh_AttributesAndValues);
    }

    IMaterial* EditorStaticMeshComponent::GetMaterial()
    {
        return m_mesh.GetMaterial();
    }

    void EditorStaticMeshComponent::SetPrimaryAsset(const AZ::Data::AssetId& id)
    {
        SetMeshAsset(id);
    }

    void EditorStaticMeshComponent::DisplayEntity(bool& handled)
    {
        if (m_mesh.HasMesh())
        {
            // Only allow Sandbox to draw the default sphere if we don't have a
            // visible mesh.
            handled = true;
        }
    }

    void EditorStaticMeshComponent::OnSelected()
    {
        m_mesh.SetAuxiliaryRenderFlags(ERF_SELECTED);
    }

    void EditorStaticMeshComponent::OnDeselected()
    {
        m_mesh.SetAuxiliaryRenderFlags(m_mesh.GetAuxiliaryRenderFlags() & ~ERF_SELECTED);
    }

    void EditorStaticMeshComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        if (StaticMeshComponent* meshComponent = gameEntity->CreateComponent<StaticMeshComponent>())
        {
            m_mesh.CopyPropertiesTo(meshComponent->m_staticMeshRenderNode);
        }
    }

    void EditorStaticMeshComponent::CreateEditorPhysics()
    {
        DestroyEditorPhysics();

        if (!GetTransform())
        {
            return;
        }

        IStatObj* geometry = m_mesh.GetEntityStatObj();
        if (!geometry)
        {
            return;
        }

        if (gEnv->pPhysicalWorld)
        {
            m_physicalEntity = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_STATIC, nullptr, &m_mesh, PHYS_FOREIGN_ID_STATIC);
            m_physicalEntity->AddRef();

            pe_geomparams params;
            geometry->Physicalize(m_physicalEntity, &params);
        }
    }

    void EditorStaticMeshComponent::DestroyEditorPhysics()
    {
        // If physics is completely torn down, all physical entities are by extension completely invalid (dangling pointers).
        // It doesn't matter that we held a reference.
        if (gEnv->pPhysicalWorld)
        {
            if (m_physicalEntity)
            {
                m_physicalEntity->Release();
                gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_physicalEntity);
            }
        }

        m_physicalEntity = nullptr;
        m_physTransform = AZ::Transform::CreateIdentity();
    }

    IStatObj* EditorStaticMeshComponent::GetStatObj()
    {
        return m_mesh.GetEntityStatObj();
    }
} // namespace LmbrCentral
