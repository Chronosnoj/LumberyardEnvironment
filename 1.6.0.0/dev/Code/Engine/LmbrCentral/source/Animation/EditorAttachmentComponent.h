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

#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include "AttachmentComponent.h"

namespace LmbrCentral
{
    /*!
     * In-editor attachment component.
     * \ref AttachmentComponent
     */
    class EditorAttachmentComponent
        : public AzToolsFramework::Components::EditorComponentBase
    {
    private:
        using Base = AzToolsFramework::Components::EditorComponentBase;
    public:
        AZ_COMPONENT(EditorAttachmentComponent, "{DA6072FD-E696-47D8-81D9-1F77D3464200}", Base);
        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            AttachmentComponent::GetProvidedServices(provided);
        }

        ~EditorAttachmentComponent() override = default;
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    protected:
        //////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

        AZ::u32 OnTargetIdChanged();
        AZ::u32 OnTargetBoneChanged();
        AZ::u32 OnTargetOffsetChanged();
        AZ::u32 OnAttachedInitiallyChanged();

        //! Invoked when an attachment property changes
        void AttachOrDetachAsNecessary();

        //! For populating ComboBox
        AZStd::vector<AZStd::string> GetTargetBoneOptions() const;

        //! Create runtime configuration from editor configuration
        AttachmentConfiguration CreateAttachmentConfiguration() const;

        //! Create AZ::Transform from position and rotation
        AZ::Transform GetTargetOffset() const;

        //! Attach to this entity.
        AZ::EntityId m_targetId;

        //! Attach to this bone on target entity.
        AZStd::string m_targetBoneName;

        //! Offset from target.
        AZ::Vector3 m_positionOffset = AZ::Vector3::CreateZero();

        //! Offset from target.
        AZ::Vector3 m_rotationOffset = AZ::Vector3::CreateZero();

        //! Whether to attach to target upon activation.
        //! If false, the entity remains detached until Attach() is called.
        bool m_attachedInitially = true;

        //! Implements actual attachment functionality
        LmbrCentral::BoneFollower m_boneFollower;
    };
} // namespace LmbrCentral
