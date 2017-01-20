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
#include "SimpleAnimationComponent.h"

#include <AzCore/Component/Entity.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Memory/Memory.h>
#include <AzCore/EBus/ScriptBinder.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Script/ScriptContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <LmbrCentral/Animation/CharacterAnimationBus.h>

#include <MathConversion.h>

namespace LmbrCentral
{
    AZ_SCRIPTABLE_EBUS(
        SimpleAnimationComponentRequestBus,
        SimpleAnimationComponentRequestBus,
        "{DB6651C4-8833-423A-AFC9-E01FFB6C58C2}",
        "{4EC8E72B-76CA-4938-8E2C-A9A6647C1B01}",
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(SimpleAnimationComponentRequests::Result, SimpleAnimationComponentRequests::Result::Failure, StartDefaultAnimations)
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(SimpleAnimationComponentRequests::Result, SimpleAnimationComponentRequests::Result::Failure, StartAnimation, const AnimatedLayer&)
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(SimpleAnimationComponentRequests::Result, SimpleAnimationComponentRequests::Result::Failure, StartAnimationByName, const char*, AnimatedLayer::LayerId)
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(SimpleAnimationComponentRequests::Result, SimpleAnimationComponentRequests::Result::Failure, StopAllAnimations)
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(SimpleAnimationComponentRequests::Result, SimpleAnimationComponentRequests::Result::Failure, StopAnimationsOnLayer, AnimatedLayer::LayerId, float)
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(SimpleAnimationComponentRequests::Result, SimpleAnimationComponentRequests::Result::Failure, SetPlaybackSpeed, AnimatedLayer::LayerId, float)
        )

    AZ_SCRIPTABLE_EBUS(
        SimpleAnimationComponentNotificationBus,
        SimpleAnimationComponentNotificationBus,
        "{1F6AFB38-41D3-489D-BADD-99EA07B1750A}",
        "{D29A77F9-35F0-404D-AC69-A64E8C1E8D22}",
        AZ_SCRIPTABLE_EBUS_EVENT(OnAnimationStarted, const AnimatedLayer&)
        AZ_SCRIPTABLE_EBUS_EVENT(OnAnimationStopped, const AnimatedLayer::LayerId)
        )

    void AnimatedLayer::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<AnimatedLayer>()->
                Version(1)
                ->Field("Animation Name", &AnimatedLayer::m_animationName)
                ->Field("Layer Id", &AnimatedLayer::m_layerId)
                ->Field("Looping", &AnimatedLayer::m_looping)
                ->Field("Playback Speed", &AnimatedLayer::m_playbackSpeed)
				->Field("AnimDrivenMotion", &AnimatedLayer::m_animDrivenRootMotion)
                ->Field("Layer Weight", &AnimatedLayer::m_layerWeight);

            AZ::EditContext* editContext = serializeContext->GetEditContext();

            if (editContext)
            {
                editContext->Class<AnimatedLayer>(
                    "Animated Layer", "Allows the configuration of one animation on one layer")->
                    ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Animation")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)->
                    DataElement(AZ::Edit::UIHandlers::ComboBox, &AnimatedLayer::m_animationName, "Animation name",
                    "Indicates the animation played by this component on this layer in absence of an overriding animation. ")
                        ->Attribute(AZ::Edit::Attributes::StringList, &AnimatedLayer::GetAvailableAnims)
                    ->DataElement(0, &AnimatedLayer::m_layerId, "Layer id",
                    "Indicates the layer that this animation is to be played on, animations can stomp on each other if they are not properly authored.")
                    ->DataElement(0, &AnimatedLayer::m_looping, "Looping",
                    "Indicates whether the animation should loop after its finished playing or not.")
                    ->DataElement(0, &AnimatedLayer::m_playbackSpeed, "Playback speed",
                    "Indicates the speed of animation playback.")
                    ->DataElement(0, &AnimatedLayer::m_layerWeight, "Layer weight",
                    "Indicates the weight of animations played on this layer.")
                    ->DataElement(0, &AnimatedLayer::m_animDrivenRootMotion, "Animate root",
                    "Enables animation-driven root motion during playback of this animation.")
					;
            }
        }

        AZ::ScriptContext* scriptContext = azrtti_cast<AZ::ScriptContext*>(context);
        if (scriptContext)
        {
            scriptContext->Class<AnimatedLayer, void(AZ::ScriptDataContext&), AZ::ScriptContext::SP_RAW_SCRIPT_OWN>("AnimatedLayer")
                ->Property("layerId", &AnimatedLayer::GetLayerId, nullptr)
                ->Property("animationName", &AnimatedLayer::GetAnimationName, nullptr)
                ->Property("looping", &AnimatedLayer::IsLooping, nullptr)
                ->Property("playbackSpeed", &AnimatedLayer::GetPlaybackSpeed, nullptr)
                ->Property("transitionTime", &AnimatedLayer::GetTransitionTime, nullptr)
                ->Property("layerWeight", &AnimatedLayer::m_layerWeight,nullptr);
        }
    }

    AZStd::vector<AZStd::string> AnimatedLayer::GetAvailableAnims()
    {
        const AnimationNameList* availableAnimatonNames = nullptr;
        EBUS_EVENT_ID_RESULT(availableAnimatonNames, m_entityId,
            AnimationInformationBus, GetAvailableAnimationsList);

        if (availableAnimatonNames)
        {
            return *availableAnimatonNames;
        }
        else
        {
            return AnimationNameList();
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // SimpleAnimator::ActiveAnimatedLayers Implementation

    //////////////////////////////////////////////////////////////////////////
    // Helpers
    void PopulateCryAnimParamsFromAnimatedLayer(const AnimatedLayer& animatedLayer, CryCharAnimationParams& outAnimParams)
    {
        outAnimParams.m_nLayerID = animatedLayer.GetLayerId();
        outAnimParams.m_fPlaybackSpeed = animatedLayer.GetPlaybackSpeed();
        outAnimParams.m_nFlags |= animatedLayer.IsLooping() ? CA_LOOP_ANIMATION : CA_REPEAT_LAST_KEY;
        outAnimParams.m_fTransTime = animatedLayer.GetTransitionTime();

        outAnimParams.m_nFlags |= CA_FORCE_TRANSITION_TO_ANIM | CA_ALLOW_ANIM_RESTART;
        outAnimParams.m_fAllowMultilayerAnim = true;
        outAnimParams.m_fPlaybackWeight = animatedLayer.GetLayerWeight();
    }
    //////////////////////////////////////////////////////////////////////////

    const AnimatedLayer* SimpleAnimator::AnimatedLayerManager::GetActiveAnimatedLayer(AnimatedLayer::LayerId layerId) const
    {
        const auto& activeLayer = m_activeAnimatedLayers.find(layerId);
        if (activeLayer != m_activeAnimatedLayers.end())
        {
            return &(activeLayer->second);
        }
        else
        {
            return nullptr;
        }
    }

    SimpleAnimationComponentRequests::Result SimpleAnimator::AnimatedLayerManager::ActivateAnimatedLayer(const AnimatedLayer& animatedLayer)
    {
        if (!m_characterInstance)
        {
            return SimpleAnimationComponentRequests::Result::Failure;
        }

        if (animatedLayer.GetLayerId() == AnimatedLayer::s_invalidLayerId)
        {
            return SimpleAnimationComponentRequests::Result::Failure;
        }

        // Check if the requested animation is already looping on the same layer
        const AnimatedLayer* activeAnimatedLayer = GetActiveAnimatedLayer(animatedLayer.GetLayerId());
        if (activeAnimatedLayer)
        {
            if (!animatedLayer.InterruptIfAlreadyPlaying() && *activeAnimatedLayer == animatedLayer)
            {
                return SimpleAnimationComponentRequests::Result::AnimationAlreadyPlaying;
            }
        }

        // If not then play this animation on this layer
        CryCharAnimationParams animationParamaters;
        PopulateCryAnimParamsFromAnimatedLayer(animatedLayer, animationParamaters);

        AZ::s32 animationId = m_characterInstance->GetIAnimationSet()->GetAnimIDByName(animatedLayer.GetAnimationName().c_str());

        if (animationId < 0)
        {
            return SimpleAnimationComponentRequests::Result::AnimationNotFound;
        }

        if (m_characterInstance->GetISkeletonAnim()->StartAnimationById(animationId, animationParamaters))
        {
            m_activeAnimatedLayers.insert(AZStd::make_pair(animatedLayer.GetLayerId(), animatedLayer));

            EBUS_EVENT_ID(m_attachedEntityId, SimpleAnimationComponentNotificationBus, OnAnimationStarted, animatedLayer);

            if (animatedLayer.GetAnimationDrivenRootMotion())
            {
                EBUS_EVENT_ID(m_attachedEntityId, CharacterAnimationRequestBus, SetAnimationDrivenMotion, true);
            }

            return SimpleAnimationComponentRequests::Result::Success;
        }

        return SimpleAnimationComponentRequests::Result::Failure;
    }

    SimpleAnimationComponentRequests::Result SimpleAnimator::AnimatedLayerManager::DeactivateAnimatedLayer(AnimatedLayer::LayerId layerId, float blendOutTime)
    {
        if (!m_characterInstance)
        {
            return SimpleAnimationComponentRequests::Result::Failure;
        }

        if (layerId == AnimatedLayer::s_invalidLayerId)
        {
            return SimpleAnimationComponentRequests::Result::Failure;
        }

        // Are there any active animations ?
        const AnimatedLayer* activeAnimatedLayer = GetActiveAnimatedLayer(layerId);
        if (!activeAnimatedLayer)
        {
            return SimpleAnimationComponentRequests::Result::NoAnimationPlayingOnLayer;
        }
        else
        {
            if (activeAnimatedLayer->GetAnimationDrivenRootMotion())
            {
                EBUS_EVENT_ID(m_attachedEntityId, CharacterAnimationRequestBus, SetAnimationDrivenMotion, false);
            }

            if (m_characterInstance->GetISkeletonAnim()->StopAnimationInLayer(layerId, blendOutTime))
            {
                m_activeAnimatedLayers.erase(layerId);

                EBUS_EVENT_ID(m_attachedEntityId, SimpleAnimationComponentNotificationBus, OnAnimationStopped, layerId);

                return SimpleAnimationComponentRequests::Result::Success;
            }
        }

        return SimpleAnimationComponentRequests::Result::Failure;
    }

    SimpleAnimationComponentRequests::Result SimpleAnimator::AnimatedLayerManager::DeactivateAllAnimatedLayers()
    {
        if (!m_characterInstance)
        {
            return SimpleAnimationComponentRequests::Result::Failure;
        }

        m_characterInstance->GetISkeletonAnim()->StopAnimationsAllLayers();

        for (const auto & layer : m_activeAnimatedLayers)
        {
            EBUS_EVENT_ID(m_attachedEntityId, SimpleAnimationComponentNotificationBus, OnAnimationStopped, layer.second.GetLayerId());
        }

        m_activeAnimatedLayers.clear();

        return SimpleAnimationComponentRequests::Result::Success;
    }

    bool SimpleAnimator::AnimatedLayerManager::IsLayerActive(AnimatedLayer::LayerId layerId) const
    {
        return m_activeAnimatedLayers.find(layerId) != m_activeAnimatedLayers.end();
    }

    SimpleAnimator::AnimatedLayerManager::AnimatedLayerManager(ICharacterInstance* characterInstance, AZ::EntityId entityId)
        : m_characterInstance(characterInstance)
        , m_attachedEntityId(entityId)
    {
    }

    SimpleAnimator::AnimatedLayerManager::~AnimatedLayerManager()
    {
        for (const auto & layer : m_activeAnimatedLayers)
        {
            EBUS_EVENT_ID(m_attachedEntityId, SimpleAnimationComponentNotificationBus, OnAnimationStopped, layer.second.GetLayerId());
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // SimpleAnimator Implementation

    void SimpleAnimator::Activate(ICharacterInstance* characterInstance, AZ::EntityId entityId)
    {
        if (m_attachedEntityId.IsValid())
        {
            Deactivate();
        }

        if (characterInstance)
        {
            m_activeAnimatedLayerManager = std::make_unique<AnimatedLayerManager>(SimpleAnimator::AnimatedLayerManager(characterInstance, entityId));
            m_meshCharacterInstance = characterInstance;
        }
        else
        {
            m_activeAnimatedLayerManager = nullptr;
            m_meshCharacterInstance = nullptr;
        }

        AZ_Assert(entityId.IsValid(), "[Anim Component] Entity id is invalid");
        m_attachedEntityId = entityId;

        EBUS_EVENT_ID_RESULT(m_currentEntityLocation, entityId, AZ::TransformBus, GetWorldTM);

        MeshComponentNotificationBus::Handler::BusConnect(entityId);
        AZ::TransformNotificationBus::Handler::BusConnect(entityId);
        AZ::TickBus::Handler::BusConnect();
    }

    void SimpleAnimator::OnMeshCreated(const AZ::Data::Asset<AZ::Data::AssetData>& asset)
    {
        (void)asset;
        UpdateCharacterInstance();
    }

    void SimpleAnimator::OnMeshDestroyed()
    {
        UpdateCharacterInstance();
    }

    void SimpleAnimator::UpdateCharacterInstance()
    {
        m_activeAnimatedLayerManager = nullptr;

        ICharacterInstance* newCharacterInstance = nullptr;
        EBUS_EVENT_ID_RESULT(newCharacterInstance, m_attachedEntityId, SkinnedMeshComponentRequestBus, GetCharacterInstance);

        if (newCharacterInstance)
        {
            m_activeAnimatedLayerManager = std::make_unique<AnimatedLayerManager>(SimpleAnimator::AnimatedLayerManager(newCharacterInstance, m_attachedEntityId));
            m_meshCharacterInstance = newCharacterInstance;
        }
        else
        {
            m_meshCharacterInstance = nullptr;
        }
    }

    SimpleAnimationComponentRequests::Result SimpleAnimator::StartAnimations(const AnimatedLayer::AnimatedLayerSet& animatedLayers)
    {
        int resultCounter = 0;

        for (const AnimatedLayer& animatedLayer : animatedLayers)
        {
            auto result = StartAnimation(animatedLayer);
            resultCounter += static_cast<int>(result) == 0 ? 0 : 1;
        }

        if (resultCounter == 0)
        {
            return SimpleAnimationComponentRequests::Result::Success;
        }
        else if (resultCounter < animatedLayers.size())
        {
            return SimpleAnimationComponentRequests::Result::SuccessWithErrors;
        }

        return SimpleAnimationComponentRequests::Result::Failure;
    }


    SimpleAnimationComponentRequests::Result SimpleAnimator::StartAnimation(const AnimatedLayer& animatedLayer)
    {
        if (m_activeAnimatedLayerManager)
        {
            return m_activeAnimatedLayerManager->ActivateAnimatedLayer(animatedLayer);
        }

        return SimpleAnimationComponentRequests::Result::Failure;
    }


    SimpleAnimationComponentRequests::Result SimpleAnimator::StopAnimations(const SimpleAnimationComponentRequests::LayerMask& animatedLayerIds, float blendOutTime)
    {
        if (!m_activeAnimatedLayerManager)
        {
            return SimpleAnimationComponentRequests::Result::Failure;
        }

        int resultCounter = 0;
        for (int i = 0; i < AnimatedLayer::s_maxActiveAnimatedLayers; i++)
        {
            if (animatedLayerIds[i])
            {
                auto result = StopAnimation(i, blendOutTime);
                resultCounter += static_cast<int>(result) == 0 ? 0 : 1;
            }
        }

        if (resultCounter == 0)
        {
            return SimpleAnimationComponentRequests::Result::Success;
        }
        else if (resultCounter < animatedLayerIds.count())
        {
            return SimpleAnimationComponentRequests::Result::SuccessWithErrors;
        }

        return SimpleAnimationComponentRequests::Result::Failure;
    }

    SimpleAnimationComponentRequests::Result SimpleAnimator::StopAnimation(const AnimatedLayer::LayerId animatedLayerId, float blendOutTime)
    {
        if (m_activeAnimatedLayerManager)
        {
            return m_activeAnimatedLayerManager->DeactivateAnimatedLayer(animatedLayerId, blendOutTime);
        }

        return SimpleAnimationComponentRequests::Result::Failure;
    }

    SimpleAnimationComponentRequests::Result SimpleAnimator::SetPlaybackSpeed(const AnimatedLayer::LayerId layerId, float playbackSpeed)
    {
        if (m_activeAnimatedLayerManager && m_activeAnimatedLayerManager->IsLayerActive(layerId))
        {
            ICharacterInstance* character = m_activeAnimatedLayerManager->GetCharacter();
            if (character)
            {
                character->GetISkeletonAnim()->SetLayerPlaybackScale(layerId, playbackSpeed);
            }
        }

        return SimpleAnimationComponentRequests::Result::Failure;
    }

    void SimpleAnimator::Deactivate()
    {
        StopAllAnimations();

        MeshComponentNotificationBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();

        m_meshCharacterInstance = nullptr;

        m_activeAnimatedLayerManager = nullptr;
    }

    SimpleAnimationComponentRequests::Result SimpleAnimator::StopAllAnimations()
    {
        if (m_activeAnimatedLayerManager)
        {
            return m_activeAnimatedLayerManager->DeactivateAllAnimatedLayers();
        }

        return SimpleAnimationComponentRequests::Result::Failure;
    }

    void SimpleAnimator::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        if (m_meshCharacterInstance && m_activeAnimatedLayerManager)
        {
            // Remove layers whose animations have ended and report the same
            for (auto layerIt = m_activeAnimatedLayerManager->m_activeAnimatedLayers.begin();
                 layerIt != m_activeAnimatedLayerManager->m_activeAnimatedLayers.end(); )
            {
                if (!layerIt->second.IsLooping())
                {
                    AnimatedLayer::LayerId currentLayerID = layerIt->second.GetLayerId();
                    if (std::fabs(m_meshCharacterInstance->GetISkeletonAnim()->GetLayerNormalizedTime(currentLayerID) - 1)
                        < std::numeric_limits<float>::epsilon())
                    {
                        m_meshCharacterInstance->GetISkeletonAnim()->StopAnimationInLayer(currentLayerID, 0.0f);
                        layerIt = m_activeAnimatedLayerManager->m_activeAnimatedLayers.erase(layerIt);
                        EBUS_EVENT_ID(m_attachedEntityId, SimpleAnimationComponentNotificationBus, OnAnimationStopped, currentLayerID);
                        continue;
                    }
                }
                layerIt++;
            }
        }
    }

    void SimpleAnimator::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        m_currentEntityLocation = world;
    }

    //////////////////////////////////////////////////////////////////////////
    // SimpleAnimationComponent Implementation

    void SimpleAnimationComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<SimpleAnimationComponent, AZ::Component>()
                ->Version(1)
                ->Field("Playback Entries", &SimpleAnimationComponent::m_defaultAnimationSettings);
        }

        AnimatedLayer::Reflect(context);

        AZ::ScriptContext* script = azrtti_cast<AZ::ScriptContext*>(context);
        if (script)
        {
            ScriptableEBus_SimpleAnimationComponentRequestBus::Reflect(script);
            ScriptableEBus_SimpleAnimationComponentNotificationBus::Reflect(script);
        }
    }

    void SimpleAnimationComponent::Init()
    {
        for (const AnimatedLayer& layer : m_defaultAnimationSettings)
        {
            m_defaultAnimLayerSet.insert(layer);
        }

        m_defaultAnimationSettings.clear();
    }

    void SimpleAnimationComponent::Activate()
    {
        SimpleAnimationComponentRequestBus::Handler::BusConnect(GetEntityId());
        MeshComponentNotificationBus::Handler::BusConnect(GetEntityId());

        ICharacterInstance* characterInstance = nullptr;
        EBUS_EVENT_ID_RESULT(characterInstance, GetEntityId(), SkinnedMeshComponentRequestBus, GetCharacterInstance);
        LinkToCharacterInstance(characterInstance);
    }

    void SimpleAnimationComponent::Deactivate()
    {
        SimpleAnimationComponentRequestBus::Handler::BusDisconnect();
        MeshComponentNotificationBus::Handler::BusDisconnect();

        m_animator.Deactivate();
    }

    void SimpleAnimationComponent::OnMeshCreated(const AZ::Data::Asset<AZ::Data::AssetData>& asset)
    {
        ICharacterInstance* characterInstance = nullptr;
        EBUS_EVENT_ID_RESULT(characterInstance, GetEntityId(), SkinnedMeshComponentRequestBus, GetCharacterInstance);
        LinkToCharacterInstance(characterInstance);
    }

    void SimpleAnimationComponent::LinkToCharacterInstance(ICharacterInstance* characterInstance)
    {
        m_animator.Activate(characterInstance, GetEntityId());

        if (characterInstance)
        {
            StartDefaultAnimations();
        }
    }

    SimpleAnimationComponentRequests::Result SimpleAnimationComponent::StartDefaultAnimations()
    {
        return m_animator.StartAnimations(m_defaultAnimLayerSet);
    }

    SimpleAnimationComponentRequests::Result SimpleAnimationComponent::StartAnimation(const AnimatedLayer& animatedLayer)
    {
        return m_animator.StartAnimation(animatedLayer);
    }

    SimpleAnimationComponentRequests::Result SimpleAnimationComponent::StartAnimationSet(const AnimatedLayer::AnimatedLayerSet& animationSet)
    {
        return m_animator.StartAnimations(animationSet);
    }

    SimpleAnimationComponentRequests::Result SimpleAnimationComponent::StopAllAnimations()
    {
        return m_animator.StopAllAnimations();
    }

    SimpleAnimationComponentRequests::Result SimpleAnimationComponent::StopAnimationsOnLayer(AnimatedLayer::LayerId layerId, float blendOutTime)
    {
        return m_animator.StopAnimation(layerId, blendOutTime);
    }

    SimpleAnimationComponentRequests::Result SimpleAnimationComponent::SetPlaybackSpeed(AnimatedLayer::LayerId layerId, float playbackSpeed)
    {
        return m_animator.SetPlaybackSpeed(layerId, playbackSpeed);
    }

    SimpleAnimationComponentRequests::Result SimpleAnimationComponent::StopAnimationsOnLayers(LayerMask layerIds, float blendOutTime)
    {
        return m_animator.StopAnimations(layerIds, blendOutTime);
    }

    //////////////////////////////////////////////////////////////////////////
} // namespace LmbrCentral