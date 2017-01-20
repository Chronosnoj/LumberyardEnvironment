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

#include <AzCore/Component/TransformBus.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

#include <AzFramework/Entity/EntityDebugDisplayBus.h>

#include <LmbrCentral/Rendering/RenderNodeBus.h>

#include "StaticMeshComponent.h"

struct IPhysicalEntity;

namespace LmbrCentral
{
    /*!
    * In-editor mesh component.
    * Conducts some additional listening and operations to ensure immediate
    * effects when changing fields in the editor.
    */
    class EditorStaticMeshComponent
        : public AzToolsFramework::Components::EditorComponentBase
        , private MeshComponentRequestBus::Handler
        , private MaterialRequestBus::Handler
        , private MeshComponentNotificationBus::Handler
        , private RenderNodeRequestBus::Handler
        , private AZ::TransformNotificationBus::Handler
        , private AzToolsFramework::EntitySelectionEvents::Bus::Handler
        , private AzFramework::EntityDebugDisplayEventBus::Handler
        , private StaticMeshComponentRequestBus::Handler
    {
    public:

        AZ_COMPONENT(EditorStaticMeshComponent, "{FC315B86-3280-4D03-B4F0-5553D7D08432}", AzToolsFramework::Components::EditorComponentBase);

        ~EditorStaticMeshComponent() override = default;

        const float s_renderNodeRequestBusOrder = 100.f;

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AzToolsFramework::EntitySelectionEventBus::Handler interface implementation
        void OnSelected() override;
        void OnDeselected() override;
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
        // MeshComponentNotificationBus interface implementation
        void OnMeshCreated(const AZ::Data::Asset<AZ::Data::AssetData>& asset) override;
        void OnMeshDestroyed() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // RenderNodeRequestBus::Handler
        IRenderNode* GetRenderNode() override;
        float GetRenderNodeRequestBusOrder() const override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // TransformBus::Handler
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AzFramework::EntityDebugDisplayEventBus interface implementation
        void DisplayEntity(bool& handled) override;
        //////////////////////////////////////////////////////////////////////////

        //! Called when you want to change the game asset through code (like when creating components based on assets).
        void SetPrimaryAsset(const AZ::Data::AssetId& assetId) override;

        //////////////////////////////////////////////////////////////////////////
        // StaticMeshComponentRequestBus interface implementation
        IStatObj* GetStatObj() override;
        ///////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // EditorComponentBase
        void BuildGameEntity(AZ::Entity* gameEntity) override;
        ///////////////////////////////////

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("MeshService"));
            provided.push_back(AZ_CRC("StaticMeshService"));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("TransformService"));
        }
        
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("MeshService"));
            incompatible.push_back(AZ_CRC("StaticMeshService"));
        }

        static void Reflect(AZ::ReflectContext* context);

    protected:

        // Editor-specific physicalization for the attached mesh. This is needed to support
        // features in the editor that rely on edit-time collision info (i.e. object snapping).
        void CreateEditorPhysics();
        void DestroyEditorPhysics();

        StaticMeshComponentRenderNode m_mesh;     ///< IRender node implementation

        IPhysicalEntity* m_physicalEntity = nullptr;  ///< Edit-time physical entity (for object snapping).
        AZ::Transform m_physTransform;      ///< To track scale changes, which requires re-physicalizing.
    };
} // namespace LmbrCentral
