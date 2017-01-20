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
#include <AzCore/Component/Entity.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/TransformBus.h>

#include <IEntityRenderState.h>

#include <LmbrCentral/Rendering/MeshComponentBus.h>
#include <LmbrCentral/Rendering/RenderNodeBus.h>
#include <LmbrCentral/Rendering/MaterialAsset.h>
#include <LmbrCentral/Rendering/MeshAsset.h>

namespace LmbrCentral
{
    /*!
     * RenderNode implementation responsible for integrating with the renderer.
     * The node owns render flags, the mesh instance, and the render transform.
     */
    class SkinnedMeshComponentRenderNode
        : public IRenderNode
        , public AZ::TransformNotificationBus::Handler
        , public AZ::Data::AssetBus::Handler
    {
        friend class EditorSkinnedMeshComponent;
    public:
        using MaterialPtr = _smart_ptr<IMaterial>;
        using CharacterPtr = _smart_ptr<ICharacterInstance>;

        AZ_TYPE_INFO(SkinnedMeshComponentRenderNode, "{AE5CFE2B-6CFF-4B66-9B9C-C514BFDB8A88}");

        SkinnedMeshComponentRenderNode();
        ~SkinnedMeshComponentRenderNode() override;

        void CopyPropertiesTo(SkinnedMeshComponentRenderNode& rhs) const;

        //! Notifies render node which entity owns it, for subscribing to transform
        //! bus, etc.
        void AttachToEntity(AZ::EntityId id);

        //! Instantiate mesh instance.
        void CreateMesh();

        //! Destroy mesh instance.
        void DestroyMesh();

        //! Returns true if the node has geometry assigned.
        bool HasMesh() const;

        //! Assign a new mesh asset
        void SetMeshAsset(const AZ::Data::AssetId& id);

        //! Invoked in the editor when the user assigns a new asset.
        void OnAssetPropertyChanged();

        //! Render the mesh
        void RenderMesh(const struct SRendParams& inRenderParams, const struct SRenderingPassInfo& passInfo);

        //! Updates the render node's world transform based on the entity's.
        void UpdateWorldTransform(const AZ::Transform& entityTransform);

        //! Computes world-space AABB.
        AZ::Aabb CalculateWorldAABB() const;

        //! Computes local-space AABB.
        AZ::Aabb CalculateLocalAABB() const;

        //////////////////////////////////////////////////////////////////////////
        // AZ::Data::AssetBus::Handler
        void OnAssetReady(const AZ::Data::Asset<AZ::Data::AssetData>& asset) override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AZ::TransformNotificationBus::Handler interface implementation
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // IRenderNode interface implementation
        void Render(const struct SRendParams& inRenderParams, const struct SRenderingPassInfo& passInfo) override;
        bool GetLodDistances(const SFrameLodInfo& frameLodInfo, float* distances) const override;
        EERType GetRenderNodeType() override;
        const char* GetName() const override;
        const char* GetEntityClassName() const override;
        Vec3 GetPos(bool bWorldOnly = true) const override;
        const AABB GetBBox() const override;
        void SetBBox(const AABB& WSBBox) override;
        void OffsetPosition(const Vec3& delta) override;
        struct IPhysicalEntity* GetPhysics() const override;
        void SetPhysics(IPhysicalEntity* pPhys) override;
        void SetMaterial(IMaterial* pMat) override;
        IMaterial* GetMaterial(Vec3* pHitPos = nullptr) override;
        IMaterial* GetMaterialOverride() override;
        IStatObj* GetEntityStatObj(unsigned int nPartId = 0, unsigned int nSubPartId = 0, Matrix34A* pMatrix = nullptr, bool bReturnOnlyVisible = false) override;
        IMaterial* GetEntitySlotMaterial(unsigned int nPartId, bool bReturnOnlyVisible = false, bool* pbDrawNear = nullptr) override;
        ICharacterInstance* GetEntityCharacter(unsigned int nSlot = 0, Matrix34A* pMatrix = nullptr, bool bReturnOnlyVisible = false) override;
        float GetMaxViewDist() override;
        void GetMemoryUsage(class ICrySizer* pSizer) const override;
        //////////////////////////////////////////////////////////////////////////

        //! Invoked in the editor when a property requiring render state refresh
        //! has changed.
        void RefreshRenderState();

        //! Set/get auxiliary render flags.
        void SetAuxiliaryRenderFlags(uint32 flags);
        uint32 GetAuxiliaryRenderFlags() const { return m_auxiliaryRenderFlags; }
        void UpdateAuxiliaryRenderFlags(bool on, uint32 mask); ///< turn particular bits on/off

        static void Reflect(AZ::ReflectContext* context);

        static float GetDefaultMaxViewDist();

        static AZ::Uuid GetRenderOptionsUuid() { return AZ::AzTypeInfo<SkinnedRenderOptions>::Uuid(); }
    protected:

        //! Calculates base LOD distance based on mesh characteristics.
        //! We do this each time the mesh resource changes.
        void UpdateLodDistance(const SFrameLodInfo& frameLodInfo);

        //! Computes desired LOD level for the assigned mesh instance.
        CLodValue ComputeLOD(int wantedLod, const SRenderingPassInfo& passInfo);

        //! Computes the entity-relative (local space) bounding box for
        //! the assigned mesh.
        virtual void UpdateLocalBoundingBox();

        //! Updates the world-space bounding box and world space transform
        //! for the assigned mesh.
        void UpdateWorldBoundingBox();

        //! Registers or unregisters our render node with the render.
        void RegisterWithRenderer(bool registerWithRenderer);

        //! Applies configured render options to the render node.
        void ApplyRenderOptions();


        class SkinnedRenderOptions
        {
        public:

            AZ_TYPE_INFO(SkinnedRenderOptions, "{33E69F1C-518F-4DD2-88D1-DF6D12ECA54E}")

                SkinnedRenderOptions();

            float m_opacity; //!< Alpha/opacity value for rendering.
            float m_maxViewDist; //!< Maximum draw distance.
            float m_viewDistMultiplier; //!< Adjusts max view distance. If 1.0 then default max view distance is used.
            AZ::u32 m_lodRatio; //!< Controls LOD distance ratio.
            bool m_indoorOnly; //!< Indoor only.
            bool m_castShadows; //!< Casts dynamic shadows.
            bool m_castLightmap; //!< Casts shadows in lightmap.
            bool m_rainOccluder; //!< Occludes raindrops.
            bool m_affectNavmesh; //!< Cuts out of the navmesh.
            bool m_affectDynamicWater; //!< Affects dynamic water (ripples).
            bool m_acceptDecals; //!< Accepts decals.
            bool m_receiveWind; //!< Receives wind.
            bool m_visibilityOccluder; //!< Appropriate for visibility occluding.
            bool m_depthTest; //!< Enables depth testing.
            bool m_allowBloom; //!< Allows bloom post effect.
            bool m_allowMotionBlur; //!< Allows motion blur post effect.

            AZStd::function<void()> m_changeCallback;

            void OnChanged()
            {
                if (m_changeCallback)
                {
                    m_changeCallback();
                }
            }

            static void Reflect(AZ::ReflectContext* context);
        };

        //! Should be visible.
        bool m_visible;

        //! User-specified material override.
        AzFramework::SimpleAssetReference<MaterialAsset> m_material;

        //! Render flags/options.
        SkinnedRenderOptions m_renderOptions;

        //! Currently-assigned material. Null if no material is manually assigned.
        MaterialPtr m_materialOverride;

        //! The Id of the entity we're associated with, for bus subscription.
        AZ::EntityId m_attachedToEntityId;

        //! World and render transforms.
        //! These are equivalent, but for different math libraries.
        AZ::Transform m_worldTransform;
        Matrix34 m_renderTransform;

        //! Local and world bounding boxes.
        AABB m_localBoundingBox;
        AABB m_worldBoundingBox;

        //! Additional render flags -- for special editor behavior, etc.
        uint32 m_auxiliaryRenderFlags;

        //! Remember which flags have ever been toggled externally so that we can shut them off.
        uint32 m_auxiliaryRenderFlagsHistory;

        //! Reference to current asset
        AZ::Data::Asset<SkinnedMeshAsset> m_skinnedMeshAsset;
        CharacterPtr m_characterInstance;

        //! Computed LOD distance.
        float m_lodDistance;

        //! Identifies whether we've already registered our node with the renderer.
        bool m_isRegisteredWithRenderer;

        //! Tracks if the object was moved so we can notify the renderer.
        bool m_objectMoved;
    };

    class SkinnedMeshComponent
        : public AZ::Component
        , public MeshComponentRequestBus::Handler
        , public MaterialRequestBus::Handler
        , public RenderNodeRequestBus::Handler
        , public SkinnedMeshComponentRequestBus::Handler
    {
    public:
        friend class EditorSkinnedMeshComponent;

        AZ_COMPONENT(SkinnedMeshComponent, "{C99EB110-CA74-4D95-83F0-2FCDD1FF418B}");

        ~SkinnedMeshComponent() override = default;

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // MeshComponentRequestBus interface implementation
        AZ::Aabb GetWorldBounds() override;
        AZ::Aabb GetLocalBounds() override;
        void SetMeshAsset(const AZ::Data::AssetId& id) override;
        ///////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // MaterialRequestBus interface implementation
        void SetMaterial(IMaterial*) override;
        IMaterial* GetMaterial() override;
        ///////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // RenderNodeRequestBus
        IRenderNode* GetRenderNode() override;
        float GetRenderNodeRequestBusOrder() const override;
        static const float s_renderNodeRequestBusOrder;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // SkinnedMeshComponentRequestBus interface implementation
        ICharacterInstance* GetCharacterInstance() override;
        ///////////////////////////////////
    protected:

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("MeshService"));
            provided.push_back(AZ_CRC("SkinnedMeshService"));
        }

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("MeshService"));
            incompatible.push_back(AZ_CRC("SkinnedMeshService"));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("TransformService"));
        }

        static void Reflect(AZ::ReflectContext* context);

        //////////////////////////////////////////////////////////////////////////
        // Reflected Data
        SkinnedMeshComponentRenderNode m_skinnedMeshRenderNode;
    };

} // namespace LmbrCentral
