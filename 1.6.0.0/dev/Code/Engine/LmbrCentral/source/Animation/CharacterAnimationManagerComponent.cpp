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

#include <ISystem.h>
#include <ICryAnimation.h>
#include <MathConversion.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/EBus/ScriptBinder.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Script/ScriptContext.h>
#include <AzCore/Math/Plane.h>

#include <Animation/MotionExtraction.h>

#include <LmbrCentral/Physics/CryCharacterPhysicsBus.h>
#include <LmbrCentral/Physics/PhysicsComponentBus.h>

#include "CharacterAnimationManagerComponent.h"

namespace LmbrCentral
{
    //////////////////////////////////////////////////////////////////////////
    AZ_SCRIPTABLE_EBUS(
        CharacterAnimationRequestBus,
        CharacterAnimationRequestBus,
        "{C798C05D-596A-410A-A059-07795BD73121}",
        "{1671064D-D104-4285-8973-CA499DEC8235}",
        AZ_SCRIPTABLE_EBUS_EVENT(SetBlendParameter, AZ::u32, float)
        AZ_SCRIPTABLE_EBUS_EVENT(SetAnimationDrivenMotion, bool)
        )

    //////////////////////////////////////////////////////////////////////////

    CharacterAnimationManagerComponent::CharacterInstanceEntry::CharacterInstanceEntry(const AZ::EntityId& entityId, ICharacterInstance* characterInstance)
        : m_entityId(entityId)
        , m_characterInstance(characterInstance)
        , m_currentWorldTransform(AZ::Transform::Identity())
        , m_previousWorldTransform(AZ::Transform::Identity())
        , m_useAnimDrivenMotion(false)
    {
    }

    CharacterAnimationManagerComponent::CharacterInstanceEntry::~CharacterInstanceEntry()
    {
        Deactivate();
    }

    void CharacterAnimationManagerComponent::CharacterInstanceEntry::Activate()
    {
        AZ::TransformNotificationBus::Handler::BusConnect(m_entityId);
        CharacterAnimationRequestBus::Handler::BusConnect(m_entityId);

        PhysicsSystemEventBus::Handler::BusConnect();

        EBUS_EVENT_ID_RESULT(m_currentWorldTransform, m_entityId, AZ::TransformBus, GetWorldTM);
        m_previousWorldTransform = m_currentWorldTransform;

        if (m_characterInstance && m_characterInstance->GetISkeletonAnim())
        {
            // This simply enables the ability to sample root motion. It does not alone turn on animation-driven root motion.
            m_characterInstance->GetISkeletonAnim()->SetAnimationDrivenMotion(true);
        }
    }

    void CharacterAnimationManagerComponent::CharacterInstanceEntry::Deactivate()
    {
        PhysicsSystemEventBus::Handler::BusDisconnect();

        CharacterAnimationRequestBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
    }

    void CharacterAnimationManagerComponent::CharacterInstanceEntry::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        m_currentWorldTransform = world;
    }

    bool CharacterAnimationManagerComponent::CharacterInstanceEntry::TickAnimation(float deltaTime)
    {
        if (m_characterInstance)
        {
            // Compute motion delta and update parametric blending.
            const AZ::Transform frameMotionDelta = m_previousWorldTransform.GetInverseFull() * m_currentWorldTransform;
            UpdateParametricBlendParameters(deltaTime, frameMotionDelta);

            // Queue anim jobs for this character.
            auto transform = AZTransformToLYTransform(m_currentWorldTransform);
            SAnimationProcessParams params;
            params.locationAnimation = QuatTS(transform);
            m_characterInstance->StartAnimationProcessing(params);

            m_previousWorldTransform = m_currentWorldTransform;

            return true;
        }
        return false;
    }

    void CharacterAnimationManagerComponent::CharacterInstanceEntry::UpdateParametricBlendParameters(float deltaTime, const AZ::Transform& frameMotionDelta)
    {
        ISkeletonAnim* skeletonAnim = m_characterInstance ? m_characterInstance->GetISkeletonAnim() : nullptr;
        if (skeletonAnim)
        {
            pe_status_living livingStatus;
            livingStatus.groundSlope.Set(0.f, 0.f, 1.f);
            EBUS_EVENT_ID(m_entityId, CryPhysicsComponentRequestBus, GetPhysicsStatus, livingStatus);

            const Animation::MotionParameters params = Animation::ExtractMotionParameters(
                deltaTime, 
                m_currentWorldTransform, 
                frameMotionDelta, 
                LYVec3ToAZVec3(livingStatus.groundSlope));

            skeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelSlope, params.m_groundAngleSigned, deltaTime);
            skeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelAngle, params.m_travelAngle, deltaTime);
            skeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelDist, params.m_travelDistance, deltaTime);
            skeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelSpeed, params.m_travelSpeed, deltaTime);
            skeletonAnim->SetDesiredMotionParam(eMotionParamID_TurnAngle, params.m_turnAngle, deltaTime);
            skeletonAnim->SetDesiredMotionParam(eMotionParamID_TurnSpeed, params.m_turnSpeed, deltaTime);
        }
    }

    void CharacterAnimationManagerComponent::CharacterInstanceEntry::SetBlendParameter(AZ::u32 blendParameter, float value)
    {
        ISkeletonAnim* skeletonAnim = m_characterInstance ? m_characterInstance->GetISkeletonAnim() : nullptr;
        if (skeletonAnim)
        {
            if (blendParameter >= eMotionParamID_BlendWeight && blendParameter <= eMotionParamID_BlendWeight_Last)
            {
                float frameDeltaTime = 0.f;
                EBUS_EVENT_RESULT(frameDeltaTime, AZ::TickRequestBus, GetTickDeltaTime);
                skeletonAnim->SetDesiredMotionParam(static_cast<EMotionParamID>(blendParameter), AZ::GetClamp(value, 0.f, 1.f), frameDeltaTime);
            }
            else
            {
                AZ_Warning("Character Animation", false, 
                           "Unable to set out-of-range blend parameter %u. Use eMotionParamID_BlendWeight through eMotionParamID_BlendWeight7.",
                           blendParameter);
            }
        }
    }
    
    void CharacterAnimationManagerComponent::CharacterInstanceEntry::SetAnimationDrivenMotion(bool useAnimDrivenMotion)
    {
        if (!useAnimDrivenMotion && m_useAnimDrivenMotion)
        {
            // Issue a stop if we're exiting animation-driven root motion.
            EBUS_EVENT_ID(m_entityId, CryCharacterPhysicsRequestBus, Move, AZ::Vector3::CreateZero(), 0);
        }

        m_useAnimDrivenMotion = useAnimDrivenMotion;
    }

    void CharacterAnimationManagerComponent::CharacterInstanceEntry::OnPrePhysicsUpdate()
    {
        if (m_useAnimDrivenMotion)
        {
            ISkeletonAnim* skeletonAnim = m_characterInstance->GetISkeletonAnim();

            if (skeletonAnim)
            {
                AZ::Transform entityTransform;
                EBUS_EVENT_ID_RESULT(entityTransform, m_entityId, AZ::TransformBus, GetWorldTM);

                float frameDeltaTime = 0.f;
                EBUS_EVENT_RESULT(frameDeltaTime, AZ::TickRequestBus, GetTickDeltaTime);
                const float frameDeltaTimeInv = (frameDeltaTime > 0.f) ? (1.f / frameDeltaTime) : 0.f;

                // Apply relative translation to the character via physics.
                const AZ::Transform motion = LYQuatTToAZTransform(skeletonAnim->CalculateRelativeMovement(frameDeltaTime, 0));
                const AZ::Vector3 relativeTranslation = motion.GetTranslation();
                if (!relativeTranslation.IsZero())
                {
                    const AZ::Quaternion characterOrientation = AZ::Quaternion::CreateFromTransform(entityTransform);
                    EBUS_EVENT_ID(m_entityId, CryCharacterPhysicsRequestBus, Move, 
                        characterOrientation * (relativeTranslation * frameDeltaTimeInv), 0);
                }

                // Rotation is applied to the entity's transform directly, since physics only handles translation.
                const AZ::Quaternion& rotation = AZ::Quaternion::CreateFromTransform(motion);
                if (!rotation.IsIdentity())
                {
                    entityTransform = entityTransform * AZ::Transform::CreateFromQuaternion(rotation);
                    entityTransform.Orthogonalize();
                    EBUS_EVENT_ID(m_entityId, AZ::TransformBus, SetWorldTM, entityTransform);
                }
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////

    void CharacterAnimationManagerComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<CharacterAnimationManagerComponent, AZ::Component>()
                ->Version(1)
                ->SerializerForEmptyClass();
        }

        AZ::ScriptContext* script = azrtti_cast<AZ::ScriptContext*>(context);
        if (script)
        {
            // Expose constants to script.
            // BlendParameterX are used with SetBlendParameter() API.
            script->Constant("eMotionParamID_BlendWeight", eMotionParamID_BlendWeight);
            script->Constant("eMotionParamID_BlendWeight2", eMotionParamID_BlendWeight2);
            script->Constant("eMotionParamID_BlendWeight3", eMotionParamID_BlendWeight3);
            script->Constant("eMotionParamID_BlendWeight4", eMotionParamID_BlendWeight4);
            script->Constant("eMotionParamID_BlendWeight5", eMotionParamID_BlendWeight5);
            script->Constant("eMotionParamID_BlendWeight6", eMotionParamID_BlendWeight6);
            script->Constant("eMotionParamID_BlendWeight7", eMotionParamID_BlendWeight7);

            // Reflect character animation bus.
            ScriptableEBus_CharacterAnimationRequestBus::Reflect(script);
        }
    }

    void CharacterAnimationManagerComponent::Activate()
    {
        for (auto& meshCharacterInstance : m_characterInstanceMap)
        {
            meshCharacterInstance.second.Activate();
        }

        SkinnedMeshInformationBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();
    }

    void CharacterAnimationManagerComponent::Deactivate()
    {
        SkinnedMeshInformationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();

        for (auto& meshCharacterInstance : m_characterInstanceMap)
        {
            meshCharacterInstance.second.Deactivate();
        }
    }

    void CharacterAnimationManagerComponent::OnSkinnedMeshCreated(ICharacterInstance* characterInstance, const AZ::EntityId& entityId)
    {
        auto result = m_characterInstanceMap.insert(AZStd::make_pair<AZ::EntityId, CharacterInstanceEntry>(entityId, CharacterInstanceEntry(entityId, characterInstance)));

        if(result.second)
        {
            result.first->second.Activate();
        }
    }
    
    void CharacterAnimationManagerComponent::OnSkinnedMeshDestroyed(const AZ::EntityId& entityId)
    {
        const auto& entry = m_characterInstanceMap.find(entityId);
        if (entry != m_characterInstanceMap.end())
        {
            m_characterInstanceMap.erase(entry);
        }
    }

    void CharacterAnimationManagerComponent::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        float frameDeltaTime = 0.f;
        EBUS_EVENT_RESULT(frameDeltaTime, AZ::TickRequestBus, GetTickDeltaTime);

        for (auto characterIter = m_characterInstanceMap.begin(); characterIter != m_characterInstanceMap.end();)
        {
            bool wasTicked = characterIter->second.TickAnimation(frameDeltaTime);
            if (!wasTicked)
            {
                characterIter = m_characterInstanceMap.erase(characterIter);
            }
            else
            {
                ++characterIter;
            }
        }
    }

} // namespace LmbrCentral
