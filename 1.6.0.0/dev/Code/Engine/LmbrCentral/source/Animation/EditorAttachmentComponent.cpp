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
#include "EditorAttachmentComponent.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Quaternion.h>

#include <ICryAnimation.h>

namespace LmbrCentral
{
    void EditorAttachmentComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<EditorAttachmentComponent, EditorComponentBase>()
                ->Version(1)
                ->Field("Target ID", &EditorAttachmentComponent::m_targetId)
                ->Field("Target Bone Name", &EditorAttachmentComponent::m_targetBoneName)
                ->Field("Position Offset", &EditorAttachmentComponent::m_positionOffset)
                ->Field("Rotation Offset", &EditorAttachmentComponent::m_rotationOffset)
                ->Field("Attached Initially", &EditorAttachmentComponent::m_attachedInitially)
            ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<EditorAttachmentComponent>(
                    "Attachment", "Lets an entity stick to a particular joint on a target entity")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Animation")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/Attachment.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/Attachment.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &EditorAttachmentComponent::m_targetId,
                    "Target entity", "Attach to this entity.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorAttachmentComponent::OnTargetIdChanged)
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &EditorAttachmentComponent::m_targetBoneName,
                    "Joint name", "Attach to this joint on target entity.")
                        ->Attribute(AZ::Edit::Attributes::StringList, &EditorAttachmentComponent::GetTargetBoneOptions)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorAttachmentComponent::OnTargetBoneChanged)
                    ->DataElement(0, &EditorAttachmentComponent::m_positionOffset,
                    "Position offset", "Local offset from target")
                        ->Attribute(AZ::Edit::Attributes::Suffix, "m")
                        ->Attribute(AZ::Edit::Attributes::Step, 0.01f)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorAttachmentComponent::OnTargetOffsetChanged)
                    ->DataElement(0, &EditorAttachmentComponent::m_rotationOffset,
                    "Rotation offset", "Local offset from target")
                        ->Attribute(AZ::Edit::Attributes::Suffix, "deg")
                        ->Attribute(AZ::Edit::Attributes::Step, 0.01f)
                        ->Attribute(AZ::Edit::Attributes::Min, -AZ::RadToDeg(AZ::Constants::TwoPi))
                        ->Attribute(AZ::Edit::Attributes::Max, AZ::RadToDeg(AZ::Constants::TwoPi))
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorAttachmentComponent::OnTargetOffsetChanged)
                    ->DataElement(0, &EditorAttachmentComponent::m_attachedInitially,
                    "Attached initially", "Whether to attach to target upon activation.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorAttachmentComponent::OnAttachedInitiallyChanged)
                ;
            }
        }
    }

    void EditorAttachmentComponent::Activate()
    {
        Base::Activate();
        m_boneFollower.Activate(GetEntity(),
            CreateAttachmentConfiguration(),
            false);                     // Entity's don't animate in Editor
    }

    void EditorAttachmentComponent::Deactivate()
    {
        m_boneFollower.Deactivate();
        Base::Deactivate();
    }

    void EditorAttachmentComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        AttachmentComponent* component = gameEntity->CreateComponent<AttachmentComponent>();
        if (component)
        {
            component->m_initialConfiguration = CreateAttachmentConfiguration();
        }
    }

    AttachmentConfiguration EditorAttachmentComponent::CreateAttachmentConfiguration() const
    {
        AttachmentConfiguration configuration;
        configuration.m_targetId = m_targetId;
        configuration.m_targetBoneName = m_targetBoneName;
        configuration.m_targetOffset = GetTargetOffset();
        configuration.m_attachedInitially = m_attachedInitially;
        return configuration;
    }

    AZ::Transform EditorAttachmentComponent::GetTargetOffset() const
    {
        AZ::Quaternion rotation = AZ::Quaternion::CreateRotationX(AZ::DegToRad(m_rotationOffset.GetX()))
            * AZ::Quaternion::CreateRotationY(AZ::DegToRad(m_rotationOffset.GetY()))
            * AZ::Quaternion::CreateRotationZ(AZ::DegToRad(m_rotationOffset.GetZ()));
        return AZ::Transform::CreateFromQuaternionAndTranslation(rotation, m_positionOffset);
    }

    AZStd::vector<AZStd::string> EditorAttachmentComponent::GetTargetBoneOptions() const
    {
        AZStd::vector<AZStd::string> names;

        // insert blank entry, so user may choose to bind to NO bone.
        names.push_back("");

        // track whether currently-set bone is found
        bool currentTargetBoneFound = false;

        // Get character and iterate over bones
        const ICharacterInstance* character = nullptr;
        EBUS_EVENT_ID_RESULT(character, m_targetId, SkinnedMeshComponentRequestBus, GetCharacterInstance);
        if (character)
        {
            const IDefaultSkeleton& skeleton = character->GetIDefaultSkeleton();
            AZ::u32 jointCount = skeleton.GetJointCount();
            for (AZ::u32 jointI = 0; jointI < jointCount; ++jointI)
            {
                names.push_back(skeleton.GetJointNameByID(static_cast<int>(jointI)));

                if (!currentTargetBoneFound)
                {
                    currentTargetBoneFound = (m_targetBoneName == names.back());
                }
            }
        }

        // If we never found currently-set bone name,
        // stick it at top of list, just in case user wants to keep it anyway
        if (!currentTargetBoneFound && !m_targetBoneName.empty())
        {
            names.insert(names.begin(), m_targetBoneName);
        }

        return names;
    }

    AZ::u32 EditorAttachmentComponent::OnTargetIdChanged()
    {
        // Give warning, rather than error that occurs in deeper systems.
        if (m_targetId == GetEntityId())
        {
            AZ_Warning(GetEntity()->GetName().c_str(), false, "AttachmentComponent cannot target self.")
            m_targetId.SetInvalid();
        }

        AttachOrDetachAsNecessary();

        return AZ::Edit::PropertyRefreshLevels::AttributesAndValues; // refresh bone options
    }

    AZ::u32 EditorAttachmentComponent::OnTargetBoneChanged()
    {
        AttachOrDetachAsNecessary();
        return AZ::Edit::PropertyRefreshLevels::None;
    }

    AZ::u32 EditorAttachmentComponent::OnTargetOffsetChanged()
    {
        EBUS_EVENT_ID(GetEntityId(), AttachmentComponentRequestBus, SetAttachmentOffset, GetTargetOffset());
        return AZ::Edit::PropertyRefreshLevels::None;
    }

    AZ::u32 EditorAttachmentComponent::OnAttachedInitiallyChanged()
    {
        AttachOrDetachAsNecessary();
        return AZ::Edit::PropertyRefreshLevels::None;
    }

    void EditorAttachmentComponent::AttachOrDetachAsNecessary()
    {
        if (m_attachedInitially && m_targetId.IsValid())
        {
            EBUS_EVENT_ID(GetEntityId(), AttachmentComponentRequestBus, Attach, m_targetId, m_targetBoneName.c_str(), GetTargetOffset());
        }
        else
        {
            EBUS_EVENT_ID(GetEntityId(), AttachmentComponentRequestBus, Detach);
        }
    }
} // namespace LmbrCentral
