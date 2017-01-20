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
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Transform.h>

#include <MathConversion.h>

#include <IEntityRenderState.h>

#include <LmbrCentral/Rendering/DecalComponentBus.h>
#include <LmbrCentral/Rendering/RenderNodeBus.h>
#include <LmbrCentral/Rendering/MaterialAsset.h>

namespace LmbrCentral
{
    /*!
    * Contains properties used to create decals, these properties are later propagated to the
    * 3D engine decal system.
    */
    class DecalConfiguration
    {
    public:
        AZ_TYPE_INFO(DecalConfiguration, "{47082F75-428F-4353-AC82-FAE8AB017F3B}");

        virtual ~DecalConfiguration() {}

        virtual AZ::u32 MajorPropertyChanged() { return AZ_CRC("RefreshNone"); }
        virtual AZ::u32 MinorPropertyChanged() { return AZ_CRC("RefreshNone"); }

        SDecalProperties::EProjectionType   m_projectionType = SDecalProperties::EProjectionType::ePlanar;
        AZ::Vector3                         m_position = AZ::Vector3::CreateZero();
        AZ::Transform                       m_explicitRightUpFront = AZ::Transform::CreateIdentity();
        AZ::u32                             m_sortPriority = 16;
        float                               m_depth = 1.f;
        float                               m_viewDistanceMultiplier = 1.f;
        bool                                m_visible = true;
        bool                                m_deferred = false;
        AZStd::string                       m_deferredString = "";
        float                               m_opacity = 1.0f;
        float                               m_maxViewDist = 8000.0f;
        EngineSpec                          m_minSpec = EngineSpec::Low;

        //! User-specified material override.
        AzFramework::SimpleAssetReference<MaterialAsset> m_material;

        AZ::EntityId m_editorEntityId; // Editor-only, not reflected.

        static void Reflect(AZ::ReflectContext* context);

        SDecalProperties GetDecalProperties(const AZ::Transform& transform)
        {
            SDecalProperties decalProperties;
            decalProperties.m_projectionType = m_projectionType;
            decalProperties.m_radius = 1.f;

            if (m_projectionType != SDecalProperties::ePlanar)
            {
                decalProperties.m_radius = decalProperties.m_normal.GetLength();
            }

            Matrix34 wtm = AZTransformToLYTransform(transform);
            Matrix33 rotation(wtm);
            decalProperties.m_explicitRightUpFront = rotation;
            decalProperties.m_pos = wtm.TransformPoint(AZVec3ToLYVec3(m_position));
            decalProperties.m_normal = Vec3(0, 0, 1);
            decalProperties.m_pMaterialName = m_material.GetAssetPath().c_str();
            decalProperties.m_sortPrio = m_sortPriority;
            decalProperties.m_deferred = m_deferred;
            decalProperties.m_opacity = m_opacity;
            decalProperties.m_depth = m_depth;
            decalProperties.m_maxViewDist = m_maxViewDist;
            decalProperties.m_minSpec = m_minSpec;

            return decalProperties;
        }

    private:

        static bool VersionConverter(AZ::SerializeContext& context,
            AZ::SerializeContext::DataElementNode& classElement);
    };

    /*!
    * Spawns decals as setup during edit time.
    */
    class DecalComponent
        : public AZ::Component
        , public DecalComponentRequestBus::Handler
        , public RenderNodeRequestBus::Handler
    {
    public:

        AZ_COMPONENT(DecalComponent, "{1C2CEAA8-786F-4684-8202-CA7D940D627B}");

        DecalComponent();

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component implementation.
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // DecalComponentRequests implementation.
        void Show() override;
        void Hide() override;
        void SetVisibility(bool visible) override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // RenderNodeRequests implementation
        IRenderNode* GetRenderNode() override;
        float GetRenderNodeRequestBusOrder() const override;
        static const float s_renderNodeRequestBusOrder;
        //////////////////////////////////////////////////////////////////////////

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("DecalService"));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("TransformService"));
        }

        static void Reflect(AZ::ReflectContext* context);

        void SetConfiguration(const DecalConfiguration& configuration)
        {
            m_configuration = configuration;
        }

    protected:

        IDecalRenderNode* m_decalRenderNode;
        DecalConfiguration m_configuration;
    };
} // namespace LmbrCentral
