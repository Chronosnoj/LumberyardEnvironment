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

#include <LmbrCentral/Animation/AttachmentComponentBus.h>
#include <LmbrCentral/Rendering/MeshComponentBus.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Transform.h>

struct ISkeletonPose;

namespace LmbrCentral
{
    /*!
     * Configuration data for AttachmentComponent.
     */
    struct AttachmentConfiguration
    {
        AZ_TYPE_INFO(AttachmentConfiguration, "{74B5DC69-DE44-4640-836A-55339E116795}");

        virtual ~AttachmentConfiguration() = default;

        static void Reflect(AZ::ReflectContext* context);

        //! Attach to this entity.
        AZ::EntityId m_targetId;

        //! Attach to this bone on target entity.
        AZStd::string m_targetBoneName;

        //! Offset from target.
        AZ::Transform m_targetOffset = AZ::Transform::Identity();

        //! Whether to attach to target upon activation.
        //! If false, the entity remains detached until Attach() is called.
        bool m_attachedInitially = true;
    };

    /*
     * Common functionality for game and editor attachment components.
     * The BoneFollower tracks movement of the target's bone and
     * updates the owning entity's TransformComponent to follow.
     * This class should be a member within the attachment component
     * and be activated/deactivated along with the component.
     * \ref AttachmentComponent
     */
    class BoneFollower
        : public AttachmentComponentRequestBus::Handler
        , public AZ::TransformNotificationBus::Handler
        , public AZ::TickBus::Handler
        , public MeshComponentNotificationBus::Handler
    {
    public:
        void Activate(AZ::Entity* owner, const AttachmentConfiguration& initialConfiguration, bool targetCanAnimate);
        void Deactivate();

        ////////////////////////////////////////////////////////////////////////
        // AttachmentComponentRequests
        void Attach(AZ::EntityId targetId, const char* targetBoneName, const AZ::Transform& offset) override;
        void Detach() override;
        void SetAttachmentOffset(const AZ::Transform& offset) override;
        ////////////////////////////////////////////////////////////////////////

    private:

        ////////////////////////////////////////////////////////////////////////
        // AZ::TickBus
        //! Check bone transform every frame
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZ::TransformNotificationBus
        //! When target's transform changes
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // MeshComponentEvents
        //! When target's mesh changes
        void OnMeshCreated(const AZ::Data::Asset<AZ::Data::AssetData>& asset) override;
        ////////////////////////////////////////////////////////////////////////

        //! Update cached character values
        void ResetCharacter(ICharacterInstance* character);

        AZ::Transform QueryBoneTransform() const;

        void UpdateOwnerTransformIfNecessary();

        //! Entity which which is being attached.
        AZ::Entity* m_owner = nullptr;

        //! Whether to query bone position per-frame (false while in editor)
        bool m_targetCanAnimate = false;

        AZ::EntityId    m_targetId;
        AZStd::string   m_targetBoneName;
        AZ::Transform   m_targetOffset; //!< local transform
        AZ::Transform   m_targetBoneTransform; //!< local transform of bone
        AZ::Transform   m_targetEntityTransform; //!< world transform of target

        //! Cached value, so we don't update owner's position unnecessarily.
        AZ::Transform   m_cachedOwnerTransform;

        // Cached character values to avoid repeated lookup.
        // These are set by calling ResetCharacter()
        ISkeletonPose*  m_skeletonPose; //!< nullptr when target has no skeleton
        int             m_targetBoneId; //!< negative when bone not found
    };

    /*!
     * The AttachmentComponent lets an entity stick to a particular bone on
     * a target entity. This is achieved by tracking movement of the target's
     * bone and updating the entity's TransformComponent accordingly.
     */
    class AttachmentComponent
        : public AZ::Component
    {
    public:
        AZ_COMPONENT(AttachmentComponent, "{2D17A64A-7AC5-4C02-AC36-C5E8141FFDDF}");

        friend class EditorAttachmentComponent;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("AttachmentService"));
        }

        ~AttachmentComponent() override = default;

    private:
        ////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

        //! Initial configuration for m_attachment
        AttachmentConfiguration m_initialConfiguration;

        //! Implements actual attachment functionality
        BoneFollower m_boneFollower;
    };
} // namespace LmbrCentral
