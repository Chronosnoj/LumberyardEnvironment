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

#include "EditorSkinnedMeshComponent.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>

#include <MathConversion.h>

#include <IPhysics.h> // For basic physicalization at edit-time for object snapping.
#include "StaticMeshComponent.h"
#include <AzCore/Asset/AssetDatabase.h>

namespace LmbrCentral
{

    //////////////////////////////////////////////////////////////////////////
    /// Deprecated EditorMeshComponent
    //////////////////////////////////////////////////////////////////////////
    namespace ClassConverters
    {
        static bool DeprecateEditorMeshComponent(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement);
    } // namespace ClassConverters
    //////////////////////////////////////////////////////////////////////////


    void EditorSkinnedMeshComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            // Need to deprecate the old EditorMeshComponent whenever we see one.
            serializeContext->ClassDeprecate("EditorMeshComponent", "{C4C69E93-4C1F-446D-AFAB-F8835AD8EFB0}", &ClassConverters::DeprecateEditorMeshComponent);
            serializeContext->Class<EditorSkinnedMeshComponent, EditorComponentBase>()
                ->Version(1)
                ->Field("Skinned Mesh Render Node", &EditorSkinnedMeshComponent::m_mesh)
                ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();

            if (editContext)
            {
                editContext->Class<EditorSkinnedMeshComponent>("Skinned Mesh", "Attach skinned geometry to the entity.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Rendering")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/SkinnedMesh.png")
                        ->Attribute(AZ::Edit::Attributes::PrimaryAssetType, AZ::AzTypeInfo<LmbrCentral::SkinnedMeshAsset>::Uuid())
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/SkinnedMesh.png")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorSkinnedMeshComponent::m_mesh);

                editContext->Class<SkinnedMeshComponentRenderNode::SkinnedRenderOptions>(
                    "Render Options", "Rendering options for the mesh.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly"))

                    ->ClassElement(AZ::Edit::ClassElements::Group, "Options")

                    ->DataElement(AZ::Edit::UIHandlers::Slider, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_opacity, "Opacity", "Opacity value")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->Attribute(AZ::Edit::Attributes::Max, 1.f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.1f)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_maxViewDist, "Max view distance", "Maximum view distance in meters.")
                        ->Attribute(AZ::Edit::Attributes::Suffix, " m")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->Attribute(AZ::Edit::Attributes::Max, &SkinnedMeshComponentRenderNode::GetDefaultMaxViewDist)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.1f)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_viewDistMultiplier, "View distance multiplier", "Adjusts max view distance. If 1.0 then default is used. 1.1 would be 10% further than default.")
                        ->Attribute(AZ::Edit::Attributes::Suffix, "x")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Slider, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_lodRatio, "LOD distance ratio", "Controls LOD ratio over distance.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::OnChanged)
                        ->Attribute(AZ::Edit::Attributes::Min, 0)
                        ->Attribute(AZ::Edit::Attributes::Max, 255)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_castShadows, "Cast dynamic shadows", "Casts dynamic shadows (shadow maps).")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_castLightmap, "Cast static shadows", "Casts static shadows (lightmap).")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_indoorOnly, "Indoor only", "Only render in indoor areas.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::OnChanged)

                    ->ClassElement(AZ::Edit::ClassElements::Group, "Advanced")

                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_allowBloom, "Enable bloom", "Enable bloom post effect.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_allowMotionBlur, "Enable motion blur", "Enable motion blur post effect.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_rainOccluder, "Rain occluder", "Occludes dynamic raindrops.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_affectDynamicWater, "Affect dynamic water", "Will generate ripples in dynamic water.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_receiveWind, "Receive wind", "Receives wind.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_acceptDecals, "Accept decals", "Can receive decals.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_affectNavmesh, "Affect navmesh", "Will affect navmesh generation.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_visibilityOccluder, "Visibility occluder", "Is appropriate for occluding visibility of other objects.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_depthTest, "Depth test", "Require depth testing.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::OnChanged)
                    ;

                editContext->Class<SkinnedMeshComponentRenderNode>(
                    "Mesh Rendering", "Attach geometry to the entity.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkinnedMeshComponentRenderNode::m_visible, "Visible", "Is currently visible.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::RefreshRenderState)
                        ->Attribute(AZ::Edit::Attributes::NameLabelOverride, "Rendering")
                    ->DataElement("SkinnedMeshAsset", &SkinnedMeshComponentRenderNode::m_skinnedMeshAsset, "Skinned asset", "Skinned mesh asset reference")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::OnAssetPropertyChanged)
                    ->DataElement("SimpleAssetRef", &SkinnedMeshComponentRenderNode::m_material, "Material override", "Optionally specify an override material.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::OnAssetPropertyChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkinnedMeshComponentRenderNode::m_renderOptions, "Render options", "Render/draw options.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &SkinnedMeshComponentRenderNode::RefreshRenderState)
                    ;
            }
        }
    }

    void EditorSkinnedMeshComponent::Activate()
    {
        EditorComponentBase::Activate();

        m_mesh.AttachToEntity(m_entity->GetId());

        // Check current visibility and updates render flags appropriately
        bool currentVisibility = true;
        EBUS_EVENT_ID_RESULT(currentVisibility, GetEntityId(), AzToolsFramework::EditorVisibilityRequestBus, GetCurrentVisibility);
        m_mesh.UpdateAuxiliaryRenderFlags(!currentVisibility, ERF_HIDDEN);

        // Note we are purposely connecting to buses before calling m_mesh.CreateMesh().
        // m_mesh.CreateMesh() can result in events (eg: OnMeshCreated) that we want receive.
        MaterialRequestBus::Handler::BusConnect(GetEntityId());
        MeshComponentRequestBus::Handler::BusConnect(GetEntityId());
        MeshComponentNotificationBus::Handler::BusConnect(GetEntityId());
        RenderNodeRequestBus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorVisibilityNotificationBus::Handler::BusConnect(GetEntityId());
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        SkinnedMeshComponentRequestBus::Handler::BusConnect(GetEntityId());

        auto renderOptionsChangeCallback =
            [this]()
        {
            this->m_mesh.ApplyRenderOptions();
        };
        m_mesh.m_renderOptions.m_changeCallback = renderOptionsChangeCallback;

        m_mesh.CreateMesh();
    }

    void EditorSkinnedMeshComponent::Deactivate()
    {
        SkinnedMeshComponentRequestBus::Handler::BusDisconnect();
        MaterialRequestBus::Handler::BusDisconnect();
        MeshComponentRequestBus::Handler::BusDisconnect();
        MeshComponentNotificationBus::Handler::BusDisconnect();
        RenderNodeRequestBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        AzToolsFramework::EditorVisibilityNotificationBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();

        DestroyEditorPhysics();

        m_mesh.m_renderOptions.m_changeCallback = 0;

        m_mesh.DestroyMesh();
        m_mesh.AttachToEntity(AZ::EntityId());

        EditorComponentBase::Deactivate();
    }

    void EditorSkinnedMeshComponent::OnMeshCreated(const AZ::Data::Asset<AZ::Data::AssetData>& asset)
    {
        (void)asset;

        CreateEditorPhysics();

        if (m_physicalEntity)
        {
            OnTransformChanged(GetTransform()->GetLocalTM(), GetTransform()->GetWorldTM());
        }
    }

    void EditorSkinnedMeshComponent::OnMeshDestroyed()
    {
        DestroyEditorPhysics();
    }

    IRenderNode* EditorSkinnedMeshComponent::GetRenderNode()
    {
        return &m_mesh;
    }

    float EditorSkinnedMeshComponent::GetRenderNodeRequestBusOrder() const
    {
        return s_renderNodeRequestBusOrder;
    }

    void EditorSkinnedMeshComponent::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
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

    AZ::Aabb EditorSkinnedMeshComponent::GetWorldBounds()
    {
        return m_mesh.CalculateWorldAABB();
    }

    AZ::Aabb EditorSkinnedMeshComponent::GetLocalBounds()
    {
        return m_mesh.CalculateLocalAABB();
    }

    void EditorSkinnedMeshComponent::SetMeshAsset(const AZ::Data::AssetId& id)
    {
        m_mesh.SetMeshAsset(id);
        EBUS_EVENT(AzToolsFramework::ToolsApplicationRequests::Bus, AddDirtyEntity, GetEntityId());
    }

    void EditorSkinnedMeshComponent::SetMaterial(IMaterial* material)
    {
        m_mesh.SetMaterial(material);

        EBUS_EVENT(AzToolsFramework::ToolsApplicationEvents::Bus, InvalidatePropertyDisplay, AzToolsFramework::Refresh_AttributesAndValues);
    }

    IMaterial* EditorSkinnedMeshComponent::GetMaterial()
    {
        return m_mesh.GetMaterial();
    }

    void EditorSkinnedMeshComponent::SetPrimaryAsset(const AZ::Data::AssetId& id)
    {
        SetMeshAsset(id);
    }

    void EditorSkinnedMeshComponent::OnEntityVisibilityChanged(bool visibility)
    {
        m_mesh.UpdateAuxiliaryRenderFlags(!visibility, ERF_HIDDEN);
        m_mesh.RefreshRenderState();
    }

    void EditorSkinnedMeshComponent::DisplayEntity(bool& handled)
    {
        if (m_mesh.HasMesh())
        {
            // Only allow Sandbox to draw the default sphere if we don't have a
            // visible mesh.
            handled = true;
        }
    }

    void EditorSkinnedMeshComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        if (SkinnedMeshComponent* meshComponent = gameEntity->CreateComponent<SkinnedMeshComponent>())
        {
            m_mesh.CopyPropertiesTo(meshComponent->m_skinnedMeshRenderNode);
        }
    }

    void EditorSkinnedMeshComponent::CreateEditorPhysics()
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

    void EditorSkinnedMeshComponent::DestroyEditorPhysics()
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

    ICharacterInstance* EditorSkinnedMeshComponent::GetCharacterInstance()
    {
        return m_mesh.GetEntityCharacter();
    }


    //////////////////////////////////////////////////////////////////////////
    /// Deprecated EditorMeshComponent
    //////////////////////////////////////////////////////////////////////////
    namespace ClassConverters
    {
        extern void MeshComponentRenderNodeRenderOptionsVersion1To2Converter(AZ::SerializeContext&, AZ::SerializeContext::DataElementNode&);
        extern void MeshComponentRenderNodeRenderOptionsVersion2To3Converter(AZ::SerializeContext&, AZ::SerializeContext::DataElementNode&);

        static bool DeprecateEditorMeshComponent(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
        {

            //////////////////////////////////////////////////////////////////////////
            // Pull data out of the old version
            auto renderNode = classElement.GetSubElement(classElement.FindElement(AZ_CRC("Mesh")));

            auto editorBaseClass = classElement.GetSubElement(classElement.FindElement(AZ_CRC("BaseClass1")));

            auto materialOverride = renderNode.GetSubElement(renderNode.FindElement(AZ_CRC("Material Override")));

            auto renderOptions = renderNode.GetSubElement(renderNode.FindElement(AZ_CRC("Render Options")));
            MeshComponentRenderNodeRenderOptionsVersion1To2Converter(context, renderOptions);

            // There is a visible field in the render options that we might want to keep so we should grab it now
            // this will default to true if the mesh component is at version 3 and does not have a Visible render node option
            bool visible = true;
            renderOptions.GetChildData(AZ_CRC("Visible"), visible);
            MeshComponentRenderNodeRenderOptionsVersion2To3Converter(context, renderOptions);

            // Use the visible field found in the render node options as a default if we found it earlier, default to true otherwise
            renderNode.GetChildData(AZ_CRC("Visible"), visible);

            float opacity = 1.f;
            renderOptions.GetChildData(AZ_CRC("Opacity"), opacity);
            float maxViewDistance;
            renderOptions.GetChildData(AZ_CRC("MaxViewDistance"), maxViewDistance);
            float viewDIstanceMultuplier = 1.f;
            renderOptions.GetChildData(AZ_CRC("ViewDistanceMultiplier"), viewDIstanceMultuplier);
            unsigned lodRatio = 100;
            renderOptions.GetChildData(AZ_CRC("LODRatio"), lodRatio);
            bool castDynamicShadows = true;
            renderOptions.GetChildData(AZ_CRC("CastDynamicShadows"), castDynamicShadows);
            bool castLightmapShadows = true;
            renderOptions.GetChildData(AZ_CRC("CastLightmapShadows"), castLightmapShadows);
            bool indoorOnly = false;
            renderOptions.GetChildData(AZ_CRC("IndoorOnly"), indoorOnly);
            bool bloom = true;
            renderOptions.GetChildData(AZ_CRC("Bloom"), bloom);
            bool motionBlur = true;
            renderOptions.GetChildData(AZ_CRC("MotionBlur"), motionBlur);
            bool rainOccluder = false;
            renderOptions.GetChildData(AZ_CRC("RainOccluder"), rainOccluder);
            bool affectDynamicWater = false;
            renderOptions.GetChildData(AZ_CRC("AffectDynamicWater"), affectDynamicWater);
            bool receiveWind = false;
            renderOptions.GetChildData(AZ_CRC("ReceiveWind"), receiveWind);
            bool acceptDecals = true;
            renderOptions.GetChildData(AZ_CRC("AcceptDecals"), acceptDecals);
            bool affectNavmesh = true;
            renderOptions.GetChildData(AZ_CRC("AffectNavmesh"), affectNavmesh);
            bool visibilityOccluder = false;
            renderOptions.GetChildData(AZ_CRC("VisibilityOccluder"), visibilityOccluder);
            bool depthTest = true;
            renderOptions.GetChildData(AZ_CRC("DepthTest"), depthTest);
            //////////////////////////////////////////////////////////////////////////

            //////////////////////////////////////////////////////////////////////////
            // Parse the asset reference so we know if it's a static or skinned mesh
            int meshAssetElementIndex = renderNode.FindElement(AZ_CRC("Mesh"));
            AZStd::string path;
            AZ::Data::AssetId meshAssetId;
            if (meshAssetElementIndex != -1)
            {
                auto meshAssetNode = renderNode.GetSubElement(meshAssetElementIndex);
                // pull the raw data from the old asset node to get the asset id so we can create an asset of the new type
                AZStd::string rawElementString = AZStd::string(&meshAssetNode.GetRawDataElement().m_buffer[0]);
                // raw data will look something like this
                //"id={41FDB841-F602-5603-BFFA-8BAA6930347B}:0,type={202B64E8-FD3C-4812-A842-96BC96E38806}"
                //    ^           38 chars                 ^
                unsigned int beginningOfSequence = rawElementString.find("id={") + 3;
                const int guidLength = 38;
                AZStd::string assetGuidString = rawElementString.substr(beginningOfSequence, guidLength);

                meshAssetId = AZ::Data::AssetId(AZ::Uuid::CreateString(assetGuidString.c_str()));
                EBUS_EVENT_RESULT(path, AZ::Data::AssetCatalogRequestBus, GetAssetPathById, meshAssetId);
                int renderOptionsIndex = -1;
            }


            AZStd::string newComponentStringGuid;
            AZStd::string renderNodeName;
            AZStd::string meshTypeString;
            AZ::Uuid meshAssetUuId;
            AZ::Uuid renderNodeUuid;
            AZ::Uuid renderOptionUuid;
            AZ::Data::Asset<AZ::Data::AssetData> meshAssetData;

            // Switch to the new component type based on the asset type of the original
            // .cdf,.chr files become skinned mesh assets inside of skinned mesh components
            // otherwise it becomes a static mesh asset in a static mesh component
            if (path.find(CRY_CHARACTER_DEFINITION_FILE_EXT) == AZStd::string::npos)
            {
                newComponentStringGuid = "{FC315B86-3280-4D03-B4F0-5553D7D08432}";
                renderNodeName = "Static Mesh Render Node";
                renderNodeUuid = AZ::AzTypeInfo<StaticMeshComponentRenderNode>::Uuid();
                meshAssetUuId = AZ::AzTypeInfo<StaticMeshAsset>::Uuid();
                renderOptionUuid = StaticMeshComponentRenderNode::GetRenderOptionsUuid();
                meshTypeString = "Static Mesh";
            }
            else
            {
                newComponentStringGuid = "{D3E1A9FC-56C9-4997-B56B-DA186EE2D62A}";
                renderNodeName = "Skinned Mesh Render Node";
                renderNodeUuid = AZ::AzTypeInfo<SkinnedMeshComponentRenderNode>::Uuid();
                meshAssetUuId = AZ::AzTypeInfo<SkinnedMeshAsset>::Uuid();
                renderOptionUuid = SkinnedMeshComponentRenderNode::GetRenderOptionsUuid();
                meshTypeString = "Skinned Mesh";
            } 
            //////////////////////////////////////////////////////////////////////////

            //////////////////////////////////////////////////////////////////////////
            // Convert.  this will destroy the old mesh component and change the uuid to the new type
            classElement.Convert(context, newComponentStringGuid.c_str());
            //////////////////////////////////////////////////////////////////////////

            //////////////////////////////////////////////////////////////////////////
            // add data back in as appropriate
            classElement.AddElement(editorBaseClass);

            //create a static mesh render node
            int renderNodeIndex = classElement.AddElement(context, renderNodeName.c_str(), renderNodeUuid);
            auto& newrenderNode = classElement.GetSubElement(renderNodeIndex);
            AZ::Data::Asset<AZ::Data::AssetData> assetData = nullptr;
            if (!path.empty())
            {
                assetData = AZ::Data::AssetDatabase::Instance().GetAsset(meshAssetId, meshAssetUuId, false);
            }
            newrenderNode.AddElementWithData(context, meshTypeString.c_str(), assetData);
            newrenderNode.AddElement(materialOverride);
            newrenderNode.AddElementWithData(context, "Visible", visible);

            // render options
            int renderOptionsIndex = newrenderNode.AddElement(context, "Render Options", renderOptionUuid);
            auto& newRenderOptions = newrenderNode.GetSubElement(renderOptionsIndex);
            newRenderOptions.AddElementWithData(context, "Opacity", opacity);
            newRenderOptions.AddElementWithData(context, "MaxViewDistance", maxViewDistance);
            newRenderOptions.AddElementWithData(context, "ViewDistanceMultiplier", viewDIstanceMultuplier);
            newRenderOptions.AddElementWithData(context, "LODRatio", lodRatio);
            newRenderOptions.AddElementWithData(context, "CastDynamicShadows", castDynamicShadows);
            newRenderOptions.AddElementWithData(context, "CastLightmapShadows", castLightmapShadows);
            newRenderOptions.AddElementWithData(context, "IndoorOnly", indoorOnly);
            newRenderOptions.AddElementWithData(context, "Bloom", bloom);
            newRenderOptions.AddElementWithData(context, "MotionBlur", motionBlur);
            newRenderOptions.AddElementWithData(context, "RainOccluder", rainOccluder);
            newRenderOptions.AddElementWithData(context, "AffectDynamicWater", affectDynamicWater);
            newRenderOptions.AddElementWithData(context, "ReceiveWind", receiveWind);
            newRenderOptions.AddElementWithData(context, "AcceptDecals", acceptDecals);
            newRenderOptions.AddElementWithData(context, "affectNavmesh", affectNavmesh);
            newRenderOptions.AddElementWithData(context, "VisibilityOccluder", visibilityOccluder);
            newRenderOptions.AddElementWithData(context, "DepthTest", depthTest);
            //////////////////////////////////////////////////////////////////////////
            return true;
        }
    } // namespace ClassConverters
} // namespace LmbrCentral
