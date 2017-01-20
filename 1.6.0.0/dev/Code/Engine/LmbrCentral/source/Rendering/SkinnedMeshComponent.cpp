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
#include "SkinnedMeshComponent.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Asset/AssetDatabase.h>

#include <MathConversion.h>

#include <I3DEngine.h>
#include <RenderObjectDefines.h>
#include <ICryAnimation.h>
#include "StaticMeshComponent.h"

namespace LmbrCentral
{

    //////////////////////////////////////////////////////////////////////////
    /// Deprecated MeshComponent

    namespace ClassConverters
    {
        static bool DeprecateMeshComponent(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement);
    } // namespace ClassConverters

    //////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////

    void SkinnedMeshComponent::Reflect(AZ::ReflectContext* context)
    {
        SkinnedMeshComponentRenderNode::Reflect(context);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            // Need to deprecate the old MeshComponent whenever we see one.
            serializeContext->ClassDeprecate("MeshComponent", "{9697D425-3D28-4414-93DD-1890E576AB4B}", &ClassConverters::DeprecateMeshComponent);

            serializeContext->Class<SkinnedMeshComponent, AZ::Component>()
                ->Version(1)
                ->Field("Skinned Mesh Render Node", &SkinnedMeshComponent::m_skinnedMeshRenderNode);
        }
    }

    //////////////////////////////////////////////////////////////////////////

    void SkinnedMeshComponentRenderNode::SkinnedRenderOptions::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<SkinnedMeshComponentRenderNode::SkinnedRenderOptions>()
                ->Version(1)
                ->Field("Opacity", &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_opacity)
                ->Field("MaxViewDistance", &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_maxViewDist)
                ->Field("ViewDistanceMultiplier", &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_viewDistMultiplier)
                ->Field("LODRatio", &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_lodRatio)
                ->Field("CastDynamicShadows", &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_castShadows)
                ->Field("CastLightmapShadows", &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_castLightmap)
                ->Field("IndoorOnly", &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_indoorOnly)
                ->Field("Bloom", &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_allowBloom)
                ->Field("MotionBlur", &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_allowMotionBlur)
                ->Field("RainOccluder", &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_rainOccluder)
                ->Field("AffectDynamicWater", &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_affectDynamicWater)
                ->Field("ReceiveWind", &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_receiveWind)
                ->Field("AcceptDecals", &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_acceptDecals)
                ->Field("AffectNavmesh", &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_affectNavmesh)
                ->Field("VisibilityOccluder", &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_visibilityOccluder)
                ->Field("DepthTest", &SkinnedMeshComponentRenderNode::SkinnedRenderOptions::m_depthTest)
            ;
        }
    }

    void SkinnedMeshComponentRenderNode::Reflect(AZ::ReflectContext* context)
    {
        SkinnedRenderOptions::Reflect(context);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<SkinnedMeshComponentRenderNode>()
                ->Version(1)
                ->Field("Visible", &SkinnedMeshComponentRenderNode::m_visible)
                ->Field("Skinned Mesh", &SkinnedMeshComponentRenderNode::m_skinnedMeshAsset)
                ->Field("Material Override", &SkinnedMeshComponentRenderNode::m_material)
                ->Field("Render Options", &SkinnedMeshComponentRenderNode::m_renderOptions)
            ;
        }
    }

    float SkinnedMeshComponentRenderNode::GetDefaultMaxViewDist()
    {
        if (gEnv && gEnv->p3DEngine)
        {
            return gEnv->p3DEngine->GetMaxViewDistance(false);
        }

        // In the editor and the game, the dynamic lookup above should *always* hit.
        // This case essentially means no renderer (not even the null renderer) is present.
        return FLT_MAX;
    }

    SkinnedMeshComponentRenderNode::SkinnedRenderOptions::SkinnedRenderOptions()
        : m_opacity(1.f)
        , m_viewDistMultiplier(1.f)
        , m_lodRatio(100)
        , m_indoorOnly(false)
        , m_castShadows(true)
        , m_castLightmap(true)
        , m_rainOccluder(false)
        , m_affectNavmesh(true)
        , m_affectDynamicWater(false)
        , m_acceptDecals(true)
        , m_receiveWind(false)
        , m_visibilityOccluder(false)
        , m_depthTest(true)
        , m_allowBloom(true)
        , m_allowMotionBlur(true)
    {
        m_maxViewDist = GetDefaultMaxViewDist();
    }

    SkinnedMeshComponentRenderNode::SkinnedMeshComponentRenderNode()
        : m_characterInstance(nullptr)
        , m_materialOverride(nullptr)
        , m_auxiliaryRenderFlags(0)
        , m_auxiliaryRenderFlagsHistory(0)
        , m_lodDistance(0.f)
        , m_isRegisteredWithRenderer(false)
        , m_objectMoved(false)
        , m_skinnedMeshAsset(static_cast<AZ::u8>(AZ::Data::AssetFlags::OBJECTSTREAM_QUEUE_LOAD))
        , m_visible(true)
    {
        m_localBoundingBox.Reset();
        m_worldBoundingBox.Reset();
        m_worldTransform = AZ::Transform::CreateIdentity();
        m_renderTransform = Matrix34::CreateIdentity();
    }

    SkinnedMeshComponentRenderNode::~SkinnedMeshComponentRenderNode()
    {
        DestroyMesh();
    }

    void SkinnedMeshComponentRenderNode::CopyPropertiesTo(SkinnedMeshComponentRenderNode& rhs) const
    {
        rhs.m_visible = m_visible;
        rhs.m_materialOverride = m_materialOverride;
        rhs.m_skinnedMeshAsset = m_skinnedMeshAsset;
        rhs.m_material = m_material;
        rhs.m_renderOptions = m_renderOptions;
    }

    void SkinnedMeshComponentRenderNode::AttachToEntity(AZ::EntityId id)
    {
        if (AZ::TransformNotificationBus::Handler::BusIsConnectedId(m_attachedToEntityId))
        {
            AZ::TransformNotificationBus::Handler::BusDisconnect(m_attachedToEntityId);
        }

        if (id.IsValid())
        {
            if (!AZ::TransformNotificationBus::Handler::BusIsConnectedId(id))
            {
                AZ::TransformNotificationBus::Handler::BusConnect(id);
            }

            AZ::Transform entityTransform = AZ::Transform::CreateIdentity();
            EBUS_EVENT_ID_RESULT(entityTransform, id, AZ::TransformBus, GetWorldTM);
            UpdateWorldTransform(entityTransform);
        }

        m_attachedToEntityId = id;
    }

    void SkinnedMeshComponentRenderNode::OnAssetPropertyChanged()
    {
        if (HasMesh())
        {
            DestroyMesh();
        }

        if (AZ::Data::AssetBus::Handler::BusIsConnected())
        {
            AZ::Data::AssetBus::Handler::BusDisconnect();
        }

        CreateMesh();
    }

    void SkinnedMeshComponentRenderNode::RefreshRenderState()
    {
        if (gEnv->IsEditor())
        {
            UpdateLocalBoundingBox();

            AZ::Transform parentTransform = AZ::Transform::CreateIdentity();
            EBUS_EVENT_ID_RESULT(parentTransform, m_attachedToEntityId, AZ::TransformBus, GetWorldTM);
            OnTransformChanged(AZ::Transform::CreateIdentity(), parentTransform);

            m_renderOptions.OnChanged();

            if (HasMesh())
            {
                // Re-register with the renderer, as some render settings/flags require it.
                // Note that this is editor-only behavior (hence the guard above).
                if (m_isRegisteredWithRenderer)
                {
                    RegisterWithRenderer(false);
                    RegisterWithRenderer(true);
                }
            }
        }
    }

    void SkinnedMeshComponentRenderNode::SetAuxiliaryRenderFlags(uint32 flags)
    {
        m_auxiliaryRenderFlags = flags;
        m_auxiliaryRenderFlagsHistory |= flags;
    }

    void SkinnedMeshComponentRenderNode::UpdateAuxiliaryRenderFlags(bool on, uint32 mask)
    {
        if (on)
        {
            m_auxiliaryRenderFlags |= mask;
        }
        else
        {
            m_auxiliaryRenderFlags &= ~mask;
        }
        m_auxiliaryRenderFlagsHistory |= mask;
    }

    void SkinnedMeshComponentRenderNode::CreateMesh()
    {
        if (m_skinnedMeshAsset.GetId().IsValid())
        {
            if (!AZ::Data::AssetBus::Handler::BusIsConnected())
            {
                AZ::Data::AssetBus::Handler::BusConnect(m_skinnedMeshAsset.GetId());
            }

            if (m_skinnedMeshAsset.IsReady())
            {
                OnAssetReady(m_skinnedMeshAsset);
            }
            else
            {
                m_skinnedMeshAsset.QueueLoad();
            }
        }
    }

    void SkinnedMeshComponentRenderNode::DestroyMesh()
    {
        if (AZ::Data::AssetBus::Handler::BusIsConnected())
        {
            AZ::Data::AssetBus::Handler::BusDisconnect();
        }

        RegisterWithRenderer(false);

        if (m_characterInstance)
        {
            EBUS_EVENT(SkinnedMeshInformationBus, OnSkinnedMeshDestroyed, m_attachedToEntityId);
        }
        m_characterInstance = nullptr;

        EBUS_EVENT_ID(m_attachedToEntityId, MeshComponentNotificationBus, OnMeshDestroyed);

    }

    bool SkinnedMeshComponentRenderNode::HasMesh() const
    {
        return m_characterInstance != nullptr;
    }

    void SkinnedMeshComponentRenderNode::SetMeshAsset(const AZ::Data::AssetId& id)
    {
        m_skinnedMeshAsset.Create(id);
        OnAssetPropertyChanged();
    }


    void SkinnedMeshComponentRenderNode::GetMemoryUsage(class ICrySizer* pSizer) const
    {
        pSizer->AddObjectSize(this);
    }

    void SkinnedMeshComponentRenderNode::OnTransformChanged(const AZ::Transform&, const AZ::Transform& parentWorld)
    {
        // The entity to which we're attached has moved.
        UpdateWorldTransform(parentWorld);
    }

    void SkinnedMeshComponentRenderNode::OnAssetReady(const AZ::Data::Asset<AZ::Data::AssetData>& asset)
    {
        if (asset == m_skinnedMeshAsset)
        {
            m_characterInstance = m_skinnedMeshAsset.Get()->m_characterInstance;

            if (HasMesh())
            {
                m_characterInstance->SetFlags(m_characterInstance->GetFlags() | CS_FLAG_UPDATE);
                const AZStd::string& materialOverridePath = m_material.GetAssetPath();
                if (!materialOverridePath.empty())
                {
                    m_materialOverride = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(materialOverridePath.c_str());

                    AZ_Warning("MeshComponent", m_materialOverride,
                        "Failed to load override material \"%s\".",
                        materialOverridePath.c_str());
                }

                UpdateLocalBoundingBox();
                UpdateLodDistance(gEnv->p3DEngine->GetFrameLodInfo());

                RegisterWithRenderer(true);

                // Inform listeners that the mesh has been changed
                EBUS_EVENT_ID(m_attachedToEntityId, MeshComponentNotificationBus, OnMeshCreated, asset);

                EBUS_EVENT(SkinnedMeshInformationBus, OnSkinnedMeshCreated, m_characterInstance, m_attachedToEntityId);
            }
        }
    }


    void SkinnedMeshComponentRenderNode::UpdateWorldTransform(const AZ::Transform& entityTransform)
    {
        m_worldTransform = entityTransform;

        m_renderTransform = AZTransformToLYTransform(m_worldTransform);

        UpdateWorldBoundingBox();

        m_objectMoved = true;
    }

    void SkinnedMeshComponentRenderNode::UpdateLocalBoundingBox()
    {
        m_localBoundingBox.Reset();

        if (HasMesh())
        {
            m_localBoundingBox.Add(m_characterInstance->GetAABB());
        }

        UpdateWorldBoundingBox();
    }

    void SkinnedMeshComponentRenderNode::UpdateWorldBoundingBox()
    {
        m_worldBoundingBox.SetTransformedAABB(m_renderTransform, m_localBoundingBox);

        if (m_isRegisteredWithRenderer)
        {
            // Re-register with the renderer to update culling info
            gEnv->p3DEngine->RegisterEntity(this);
        }
    }

    void SkinnedMeshComponentRenderNode::RegisterWithRenderer(bool registerWithRenderer)
    {
        if (gEnv && gEnv->p3DEngine)
        {
            if (registerWithRenderer)
            {
                if (!m_isRegisteredWithRenderer)
                {
                    ApplyRenderOptions();

                    gEnv->p3DEngine->RegisterEntity(this);

                    m_isRegisteredWithRenderer = true;
                }
            }
            else
            {
                if (m_isRegisteredWithRenderer)
                {
                    gEnv->p3DEngine->FreeRenderNodeState(this);
                    m_isRegisteredWithRenderer = false;
                }
            }
        }
    }

    namespace SkinnedMeshInternal
    {
        void UpdateRenderFlag(bool enable, int mask, unsigned int& flags)
        {
            if (enable)
            {
                flags |= mask;
            }
            else
            {
                flags &= ~mask;
            }
        }
    }

    void SkinnedMeshComponentRenderNode::ApplyRenderOptions()
    {
        using SkinnedMeshInternal::UpdateRenderFlag;
        unsigned int flags = GetRndFlags();

        // Turn off any flag which has ever been set via auxiliary render flags.
        UpdateRenderFlag(false, m_auxiliaryRenderFlagsHistory, flags);

        // Update flags according to current render settings
        UpdateRenderFlag(m_renderOptions.m_indoorOnly == false, ERF_OUTDOORONLY, flags);
        UpdateRenderFlag(m_renderOptions.m_castShadows, ERF_CASTSHADOWMAPS | ERF_HAS_CASTSHADOWMAPS, flags);
        UpdateRenderFlag(m_renderOptions.m_castLightmap, ERF_DYNAMIC_DISTANCESHADOWS, flags);
        UpdateRenderFlag(m_renderOptions.m_rainOccluder, ERF_RAIN_OCCLUDER, flags);
        UpdateRenderFlag(m_visible == false, ERF_HIDDEN, flags);
        UpdateRenderFlag(m_renderOptions.m_receiveWind, ERF_RECVWIND, flags);
        UpdateRenderFlag(m_renderOptions.m_visibilityOccluder, ERF_GOOD_OCCLUDER, flags);

        UpdateRenderFlag(false == m_renderOptions.m_affectNavmesh, ERF_EXCLUDE_FROM_TRIANGULATION, flags);
        UpdateRenderFlag(false == m_renderOptions.m_affectDynamicWater, ERF_NODYNWATER, flags);
        UpdateRenderFlag(false == m_renderOptions.m_acceptDecals, ERF_NO_DECALNODE_DECALS, flags);

        // Apply current auxiliary render flags
        UpdateRenderFlag(true, m_auxiliaryRenderFlags, flags);

        m_fWSMaxViewDist = m_renderOptions.m_maxViewDist;

        SetViewDistanceMultiplier(m_renderOptions.m_viewDistMultiplier);

        SetLodRatio(static_cast<int>(m_renderOptions.m_lodRatio));

        SetRndFlags(flags);
    }

    CLodValue SkinnedMeshComponentRenderNode::ComputeLOD(int wantedLod, const SRenderingPassInfo& passInfo)
    {
        if (m_characterInstance)
        {
            return m_characterInstance->ComputeLod(wantedLod, passInfo);
        }

        return CLodValue(wantedLod);
    }

    AZ::Aabb SkinnedMeshComponentRenderNode::CalculateWorldAABB() const
    {
        AZ::Aabb aabb = AZ::Aabb::CreateNull();
        if (!m_worldBoundingBox.IsReset())
        {
            aabb.AddPoint(LYVec3ToAZVec3(m_worldBoundingBox.min));
            aabb.AddPoint(LYVec3ToAZVec3(m_worldBoundingBox.max));
        }
        return aabb;
    }

    AZ::Aabb SkinnedMeshComponentRenderNode::CalculateLocalAABB() const
    {
        AZ::Aabb aabb = AZ::Aabb::CreateNull();
        if (!m_localBoundingBox.IsReset())
        {
            aabb.AddPoint(LYVec3ToAZVec3(m_localBoundingBox.min));
            aabb.AddPoint(LYVec3ToAZVec3(m_localBoundingBox.max));
        }
        return aabb;
    }

    /*IRenderNode*/ void SkinnedMeshComponentRenderNode::Render(const struct SRendParams& inRenderParams, const struct SRenderingPassInfo& passInfo)
    {
        SRendParams rParams(inRenderParams);

        rParams.fAlpha = m_renderOptions.m_opacity;

        IMaterial* previousMaterial = rParams.pMaterial;
        const int previousObjectFlags = rParams.dwFObjFlags;

        if (m_materialOverride)
        {
            rParams.pMaterial = m_materialOverride;
        }

        if (m_renderOptions.m_depthTest)
        {
            rParams.nCustomFlags |= COB_HUD_REQUIRE_DEPTHTEST;
        }

        if (m_objectMoved)
        {
            rParams.dwFObjFlags |= FOB_DYNAMIC_OBJECT;
            m_objectMoved = false;
        }

        rParams.pMatrix = &m_renderTransform;
        if (rParams.pMatrix->IsValid())
        {
            rParams.lodValue = ComputeLOD(inRenderParams.lodValue.LodA(), passInfo);
            if (m_characterInstance)
            {
                QuatTS identity;
                identity.SetIdentity();
                m_characterInstance->Render(rParams, identity, passInfo);
            }
        }

        rParams.pMaterial = previousMaterial;
        rParams.dwFObjFlags = previousObjectFlags;
    }

    /*IRenderNode*/ bool SkinnedMeshComponentRenderNode::GetLodDistances(const SFrameLodInfo& frameLodInfo, float* distances) const
    {
        const float lodRatio = GetLodRatioNormalized();
        if (lodRatio > 0.0f)
        {
            const float distMultiplier = 1.f / (lodRatio * frameLodInfo.fTargetSize);

            for (int lodIndex = 0; lodIndex < SMeshLodInfo::s_nMaxLodCount; ++lodIndex)
            {
                distances[lodIndex] = m_lodDistance * (lodIndex + 1) * distMultiplier;
            }
        }
        else
        {
            for (int lodIndex = 0; lodIndex < SMeshLodInfo::s_nMaxLodCount; ++lodIndex)
            {
                distances[lodIndex] = FLT_MAX;
            }
        }

        return true;
    }

    void SkinnedMeshComponentRenderNode::UpdateLodDistance(const SFrameLodInfo& frameLodInfo)
    {
        SMeshLodInfo lodInfo;

        if (m_characterInstance)
        {
            m_characterInstance->ComputeGeometricMean(lodInfo);
        }

        m_lodDistance = sqrt(lodInfo.fGeometricMean);
    }

    /*IRenderNode*/ EERType SkinnedMeshComponentRenderNode::GetRenderNodeType()
    {
        return eERType_RenderComponent;
    }


    /*IRenderNode*/ const char* SkinnedMeshComponentRenderNode::GetName() const
    {
        return "SkinnedMeshComponentRenderNode";
    }

    /*IRenderNode*/ const char* SkinnedMeshComponentRenderNode::GetEntityClassName() const
    {
        return "SkinnedMeshComponentRenderNode";
    }


    /*IRenderNode*/ Vec3 SkinnedMeshComponentRenderNode::GetPos(bool bWorldOnly /*= true*/) const
    {
        return m_renderTransform.GetTranslation();
    }

    /*IRenderNode*/ const AABB SkinnedMeshComponentRenderNode::GetBBox() const
    {
        return m_worldBoundingBox;
    }

    /*IRenderNode*/ void SkinnedMeshComponentRenderNode::SetBBox(const AABB& WSBBox)
    {
        m_worldBoundingBox = WSBBox;
    }

    /*IRenderNode*/ void SkinnedMeshComponentRenderNode::OffsetPosition(const Vec3& delta)
    {
        // Recalculate local transform
        AZ::Transform localTransform = AZ::Transform::CreateIdentity();
        EBUS_EVENT_ID_RESULT(localTransform, m_attachedToEntityId, AZ::TransformBus, GetLocalTM);

        localTransform.SetTranslation(localTransform.GetTranslation() + LYVec3ToAZVec3(delta));
        EBUS_EVENT_ID(m_attachedToEntityId, AZ::TransformBus, SetLocalTM, localTransform);

        m_objectMoved = true;
    }

    /*IRenderNode*/ struct IPhysicalEntity* SkinnedMeshComponentRenderNode::GetPhysics() const
    {
        return nullptr;
    }

    /*IRenderNode*/ void SkinnedMeshComponentRenderNode::SetPhysics(IPhysicalEntity* pPhys)
    {
    }

    /*IRenderNode*/ void SkinnedMeshComponentRenderNode::SetMaterial(IMaterial* pMat)
    {
        m_materialOverride = pMat;

        if (pMat)
        {
            m_material.SetAssetPath(pMat->GetName());
        }
        else
        {
            // If no material is provided, we intend to reset to the original material so we treat
            // it as an asset reset to recreate the mesh.
            m_material.SetAssetPath("");
            OnAssetPropertyChanged();
        }
    }

    /*IRenderNode*/ IMaterial* SkinnedMeshComponentRenderNode::GetMaterial(Vec3* pHitPos /*= nullptr*/)
    {
        if (m_materialOverride)
        {
            return m_materialOverride;
        }

        if (HasMesh())
        {
            return m_characterInstance->GetIMaterial();
        }

        return nullptr;
    }

    /*IRenderNode*/ IMaterial* SkinnedMeshComponentRenderNode::GetMaterialOverride()
    {
        return m_materialOverride;
    }

    /*IRenderNode*/ float SkinnedMeshComponentRenderNode::GetMaxViewDist()
    {
        return(m_renderOptions.m_maxViewDist * 0.75f * GetViewDistanceMultiplier());
    }

    /*IRenderNode*/ IStatObj* SkinnedMeshComponentRenderNode::GetEntityStatObj(unsigned int nPartId, unsigned int nSubPartId, Matrix34A* pMatrix, bool bReturnOnlyVisible)
    {
        return nullptr;
    }

    /*IRenderNode*/ IMaterial* SkinnedMeshComponentRenderNode::GetEntitySlotMaterial(unsigned int nPartId, bool bReturnOnlyVisible, bool* pbDrawNear)
    {
        if (0 == nPartId)
        {
            return m_materialOverride;
        }

        return nullptr;
    }

    /*IRenderNode*/ ICharacterInstance* SkinnedMeshComponentRenderNode::GetEntityCharacter(unsigned int nSlot, Matrix34A* pMatrix, bool bReturnOnlyVisible)
    {
        if (0 == nSlot)
        {
            if (pMatrix)
            {
                *pMatrix = m_renderTransform;
            }

            return m_characterInstance;
        }

        return nullptr;
    }
    //////////////////////////////////////////////////////////////////////////




    //////////////////////////////////////////////////////////////////////////
    // SkinnedMeshComponent
    const float SkinnedMeshComponent::s_renderNodeRequestBusOrder = 100.f;

    void SkinnedMeshComponent::Activate()
    {
        m_skinnedMeshRenderNode.AttachToEntity(m_entity->GetId());

        // Note we are purposely connecting to buses before calling m_mesh.CreateMesh().
        // m_mesh.CreateMesh() can result in events (eg: OnMeshCreated) that we want receive.
        MaterialRequestBus::Handler::BusConnect(m_entity->GetId());
        MeshComponentRequestBus::Handler::BusConnect(m_entity->GetId());
        RenderNodeRequestBus::Handler::BusConnect(m_entity->GetId());
        m_skinnedMeshRenderNode.CreateMesh();
        SkinnedMeshComponentRequestBus::Handler::BusConnect(GetEntityId());
    }

    void SkinnedMeshComponent::Deactivate()
    {
        SkinnedMeshComponentRequestBus::Handler::BusDisconnect();
        MaterialRequestBus::Handler::BusDisconnect();
        MeshComponentRequestBus::Handler::BusDisconnect();
        RenderNodeRequestBus::Handler::BusDisconnect();

        m_skinnedMeshRenderNode.DestroyMesh();
        m_skinnedMeshRenderNode.AttachToEntity(AZ::EntityId());
    }

    AZ::Aabb SkinnedMeshComponent::GetWorldBounds()
    {
        return m_skinnedMeshRenderNode.CalculateWorldAABB();
    }

    AZ::Aabb SkinnedMeshComponent::GetLocalBounds()
    {
        return m_skinnedMeshRenderNode.CalculateLocalAABB();
    }

    void SkinnedMeshComponent::SetMeshAsset(const AZ::Data::AssetId& id)
    {
        m_skinnedMeshRenderNode.SetMeshAsset(id);
    }

    void SkinnedMeshComponent::SetMaterial(IMaterial* material)
    {
        m_skinnedMeshRenderNode.SetMaterial(material);
    }

    IMaterial* SkinnedMeshComponent::GetMaterial()
    {
        return m_skinnedMeshRenderNode.GetMaterial();
    }

    IRenderNode* SkinnedMeshComponent::GetRenderNode()
    {
        return &m_skinnedMeshRenderNode;
    }

    float SkinnedMeshComponent::GetRenderNodeRequestBusOrder() const
    {
        return s_renderNodeRequestBusOrder;
    }

    ICharacterInstance* SkinnedMeshComponent::GetCharacterInstance()
    {
        return m_skinnedMeshRenderNode.GetEntityCharacter();
    }


    //////////////////////////////////////////////////////////////////////////
    /// Deprecated MeshComponent

    namespace ClassConverters
    {
        /// Convert MeshComponentRenderNode::RenderOptions to latest version.
        void MeshComponentRenderNodeRenderOptionsVersion1To2Converter(AZ::SerializeContext& context,
            AZ::SerializeContext::DataElementNode& classElement)
        {
            // conversion from version 1:
            // - Need to convert Hidden to Visible
            // - Need to convert OutdoorOnly to IndoorOnly
            if (classElement.GetVersion() <= 1)
            {
                int hiddenIndex = classElement.FindElement(AZ_CRC("Hidden"));
                int visibleIndex = classElement.FindElement(AZ_CRC("Visible"));

                // There was a brief time where hidden became visible but the version was not patched and this will handle that case
                // This should also be a reminder to always up the version at the time of renaming or removing parameters
                if (visibleIndex <= -1)
                {
                    if (hiddenIndex > -1)
                    {
                        //Invert hidden and rename to visible
                        AZ::SerializeContext::DataElementNode& hidden = classElement.GetSubElement(hiddenIndex);
                        bool hiddenData;

                        hidden.GetData<bool>(hiddenData);
                        hidden.SetData<bool>(context, !hiddenData);

                        hidden.SetName("Visible");
                    }
                }

                int outdoorOnlyIndex = classElement.FindElement(AZ_CRC("OutdoorOnly"));
                if (outdoorOnlyIndex > -1)
                {
                    //Invert outdoor only and rename to indoor only
                    AZ::SerializeContext::DataElementNode& outdoorOnly = classElement.GetSubElement(outdoorOnlyIndex);
                    bool outdoorOnlyData;

                    outdoorOnly.GetData<bool>(outdoorOnlyData);
                    outdoorOnly.SetData<bool>(context, !outdoorOnlyData);

                    outdoorOnly.SetName("IndoorOnly");
                }
            }
        }
        /// Convert MeshComponentRenderNode::RenderOptions to latest version.
        void MeshComponentRenderNodeRenderOptionsVersion2To3Converter(AZ::SerializeContext& context,
            AZ::SerializeContext::DataElementNode& classElement)
        {
            // conversion from version 2:
            // - Need to remove Visible
            if (classElement.GetVersion() <= 2)
            {
                int visibleIndex = classElement.FindElement(AZ_CRC("Visible"));

                classElement.RemoveElement(visibleIndex);
            }
        }

        static bool DeprecateMeshComponent(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
        {
            //////////////////////////////////////////////////////////////////////////
            // Pull data out of the old version
            auto renderNode = classElement.GetSubElement(classElement.FindElement(AZ_CRC("Mesh")));

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
                newComponentStringGuid = "{2F4BAD46-C857-4DCB-A454-C412DE67852A}";
                renderNodeName = "Static Mesh Render Node";
                renderNodeUuid = AZ::AzTypeInfo<StaticMeshComponentRenderNode>::Uuid();
                meshAssetUuId = AZ::AzTypeInfo<StaticMeshAsset>::Uuid();
                renderOptionUuid = StaticMeshComponentRenderNode::GetRenderOptionsUuid();
                meshTypeString = "Static Mesh";
            }
            else
            {
                newComponentStringGuid = "{C99EB110-CA74-4D95-83F0-2FCDD1FF418B}";
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
            //create a static mesh render node
            int renderNodeIndex = classElement.AddElement(context, renderNodeName.c_str(), renderNodeUuid);
            auto& newrenderNode = classElement.GetSubElement(renderNodeIndex);
            AZ::Data::Asset<AZ::Data::AssetData> assetData = nullptr;
            if (!path.empty())
            {
                assetData = AZ::Data::AssetDatabase::Instance().GetAsset(meshAssetId, meshAssetUuId);
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
