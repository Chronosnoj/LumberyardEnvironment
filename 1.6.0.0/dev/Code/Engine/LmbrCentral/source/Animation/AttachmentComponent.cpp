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
#include "AttachmentComponent.h"
#include <ICryAnimation.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/EBus/ScriptBinder.h>
#include <AzCore/Component/Entity.h>
#include <MathConversion.h>
#include <LmbrCentral/Rendering/MeshAsset.h>

namespace LmbrCentral
{
    AZ_SCRIPTABLE_EBUS(
        AttachmentComponentRequestBus,
        AttachmentComponentRequestBus,
        "{35025B25-28C0-48A2-8A7A-7E89D01A2796}",
        "{52DF0D6B-65D9-4138-9364-387A5896BD0F}",
        AZ_SCRIPTABLE_EBUS_EVENT(Attach, AZ::EntityId, const char*, const AZ::Transform &)
        AZ_SCRIPTABLE_EBUS_EVENT(Detach)
        AZ_SCRIPTABLE_EBUS_EVENT(SetAttachmentOffset, const AZ::Transform &)
        )

    AZ_SCRIPTABLE_EBUS(
        AttachmentComponentNotificationBus,
        AttachmentComponentNotificationBus,
        "{5C9C5B79-F028-4D04-8BED-3AFE9FAC457A}",
        "{636B95A0-5C7D-4EE7-8645-955665315451}",
        AZ_SCRIPTABLE_EBUS_EVENT(OnAttached, AZ::EntityId)
        AZ_SCRIPTABLE_EBUS_EVENT(OnDetached, AZ::EntityId)
        )

    void AttachmentConfiguration::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<AttachmentConfiguration>()
                ->Version(1)
                ->Field("Target ID", &AttachmentConfiguration::m_targetId)
                ->Field("Target Bone Name", &AttachmentConfiguration::m_targetBoneName)
                ->Field("Target Offset", &AttachmentConfiguration::m_targetOffset)
                ->Field("Attached Initially", &AttachmentConfiguration::m_attachedInitially)
            ;
        }

        AZ::ScriptContext* script = azrtti_cast<AZ::ScriptContext*>(context);
        if (script)
        {
            ScriptableEBus_AttachmentComponentRequestBus::Reflect(script);
            ScriptableEBus_AttachmentComponentNotificationBus::Reflect(script);
        }
    }

    void AttachmentComponent::Reflect(AZ::ReflectContext* context)
    {
        AttachmentConfiguration::Reflect(context);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<AttachmentComponent, AZ::Component>()
                ->Version(1)
                ->Field("Configuration", &AttachmentComponent::m_initialConfiguration)
            ;
        }
    }

    //=========================================================================
    // BoneFollower
    //=========================================================================

    void BoneFollower::Activate(AZ::Entity* owner, const AttachmentConfiguration& configuration, bool targetCanAnimate)
    {
        AZ_Assert(owner, "owner is required");
        AZ_Assert(!m_owner, "BoneFollower is already Activated");

        m_owner = owner;
        m_targetCanAnimate = targetCanAnimate;

        if (configuration.m_attachedInitially)
        {
            Attach(configuration.m_targetId, configuration.m_targetBoneName.c_str(), configuration.m_targetOffset);
        }

        AttachmentComponentRequestBus::Handler::BusConnect(m_owner->GetId());
    }

    void BoneFollower::Deactivate()
    {
        AZ_Assert(m_owner, "BoneFollower was never Activated");

        AttachmentComponentRequestBus::Handler::BusDisconnect();
        Detach();
        m_owner = nullptr;
    }

    void BoneFollower::Attach(AZ::EntityId targetId, const char* targetBoneName, const AZ::Transform& offset)
    {
        AZ_Assert(m_owner, "BoneFollower must be Activated to use.")

        // safe to try and detach, even if we weren't attached
        Detach();

        if (!targetId.IsValid())
        {
            return;
        }

        if (targetId == m_owner->GetId())
        {
            AZ_Error(m_owner->GetName().c_str(), false, "AttachmentComponent cannot target self");
            return;
        }

        // Note: the target entity may not be activated yet. That's ok.
        // When mesh is ready we are notified via MeshComponentEvents::OnMeshCreated
        // When transform is ready we are notified via TransformNotificationBus::OnTransformChanged

        m_targetId = targetId;
        m_targetBoneName = targetBoneName;
        m_targetOffset = offset;

        // reset character values
        ICharacterInstance* character = nullptr;
        EBUS_EVENT_ID_RESULT(character, m_targetId, SkinnedMeshComponentRequestBus, GetCharacterInstance);
        ResetCharacter(character);

        // initial query of transforms
        m_targetBoneTransform = QueryBoneTransform();
        EBUS_EVENT_ID_RESULT(m_targetEntityTransform, m_targetId, AZ::TransformBus, GetWorldTM);

        // Force update of owner's transform
        m_cachedOwnerTransform = AZ::Transform::CreateZero(); // impossible cached value
        UpdateOwnerTransformIfNecessary();

        // connect to buses that let us follow target
        MeshComponentNotificationBus::Handler::BusConnect(m_targetId);
        AZ::TransformNotificationBus::Handler::BusConnect(m_targetId);
        if (m_targetCanAnimate)
        {
            // no need for per-frame updates when target can't animate
            AZ::TickBus::Handler::BusConnect();
        }

        // alert others that we've attached
        EBUS_EVENT_ID(m_owner->GetId(), AttachmentComponentNotificationBus, OnAttached, m_targetId);
    }

    void BoneFollower::Detach()
    {
        AZ_Assert(m_owner, "BoneFollower must be Activated to use.");

        if (m_targetId.IsValid())
        {
            // alert others that we're detaching
            EBUS_EVENT_ID(m_owner->GetId(), AttachmentComponentNotificationBus, OnDetached, m_targetId);

            MeshComponentNotificationBus::Handler::BusDisconnect();
            AZ::TransformNotificationBus::Handler::BusDisconnect();
            AZ::TickBus::Handler::BusDisconnect();

            m_targetId.SetInvalid();
        }
    }


    void BoneFollower::SetAttachmentOffset(const AZ::Transform& offset)
    {
        AZ_Assert(m_owner, "BoneFollower must be Activated to use.");

        if (m_targetId.IsValid())
        {
            m_targetOffset = offset;
            UpdateOwnerTransformIfNecessary();
        }
    }

    void BoneFollower::OnMeshCreated(const AZ::Data::Asset<AZ::Data::AssetData>& asset)
    {
        // reset character values
        ICharacterInstance* character = nullptr;
        if (SkinnedMeshAsset* meshAsset = asset.GetAs<SkinnedMeshAsset>())
        {
            character = meshAsset->m_characterInstance;
        }
        ResetCharacter(character);
        m_targetBoneTransform = QueryBoneTransform();

        // move owner if necessary
        UpdateOwnerTransformIfNecessary();
    }

    void BoneFollower::ResetCharacter(ICharacterInstance* character)
    {
        if (character)
        {
            m_skeletonPose = character->GetISkeletonPose();
            m_targetBoneId = character->GetIDefaultSkeleton().GetJointIDByName(m_targetBoneName.c_str());
        }
        else
        {
            m_skeletonPose = nullptr;

            enum
            {
                InvalidBoneId = -1
            };                          // negative value means bone not found
            m_targetBoneId = InvalidBoneId;
        }
    }

    void BoneFollower::UpdateOwnerTransformIfNecessary()
    {
        AZ::Transform ownerTransform = m_targetEntityTransform * m_targetBoneTransform;
        ownerTransform.ExtractScale(); // don't adopt target's scale
        ownerTransform *= m_targetOffset;

        if (!m_cachedOwnerTransform.IsClose(ownerTransform))
        {
            EBUS_EVENT_ID(m_owner->GetId(), AZ::TransformBus, SetWorldTM, ownerTransform);
            m_cachedOwnerTransform = ownerTransform;
        }
    }

    AZ::Transform BoneFollower::QueryBoneTransform() const
    {
        if (m_skeletonPose && m_targetBoneId >= 0)
        {
            QuatT boneOffsetQuatT = m_skeletonPose->GetAbsJointByID(m_targetBoneId);
            return LYQuatTToAZTransform(boneOffsetQuatT);
        }

        return AZ::Transform::Identity();
    }

    // fires when target's transform changes
    void BoneFollower::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        m_targetEntityTransform = world;
        UpdateOwnerTransformIfNecessary();
    }

    void BoneFollower::OnTick(float /*deltaTime*/, AZ::ScriptTimePoint /*time*/)
    {
        m_targetBoneTransform = QueryBoneTransform();
        UpdateOwnerTransformIfNecessary();
    }

    //=========================================================================
    // AttachmentComponent
    //=========================================================================

    void AttachmentComponent::Activate()
    {
        m_boneFollower.Activate(GetEntity(), m_initialConfiguration, true);
    }


    void AttachmentComponent::Deactivate()
    {
        m_boneFollower.Deactivate();
    }
} // namespace LmbrCentral
