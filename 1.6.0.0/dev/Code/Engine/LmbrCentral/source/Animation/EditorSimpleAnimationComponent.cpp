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
#include "EditorSimpleAnimationComponent.h"

#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/TransformBus.h>
#include <MathConversion.h>

namespace LmbrCentral
{
    void EditorSimpleAnimationComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<EditorSimpleAnimationComponent, EditorComponentBase>()
                ->Version(1)
                ->Field("Playback Settings", &EditorSimpleAnimationComponent::m_defaultAnimationSettings);

            AZ::EditContext* editContext = serializeContext->GetEditContext();

            if (editContext)
            {
                editContext->Class<EditorSimpleAnimationComponent>(
                    "Simple Animation", "Allows a Mesh Entity to do Simple Animations")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Animation")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/SimpleAnimation.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/Animation.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &EditorSimpleAnimationComponent::m_defaultAnimationSettings,
                    "Playback Settings", "A set of configured animation layers that can be played on this entity")
                        ->Attribute("AddNotify", &EditorSimpleAnimationComponent::OnLayersChanged);
            }
        }
    }

    void EditorSimpleAnimationComponent::DisplayEntity(bool& handled)
    {
        // Don't draw extra visualization unless selected.
        if (!IsSelected())
        {
            handled = true;
        }
    }

    void EditorSimpleAnimationComponent::Activate()
    {
        EditorComponentBase::Activate();

        UpdateCharacterInstance();
        ICharacterInstance* characterInstance = nullptr;
        EBUS_EVENT_ID_RESULT(characterInstance, GetEntityId(), SkinnedMeshComponentRequestBus, GetCharacterInstance);
        if (characterInstance)
        {
            characterInstance->GetIAnimationSet()->RegisterListener(this);
        }

        AnimationInformationBus::Handler::BusConnect(GetEntityId());
        MeshComponentNotificationBus::Handler::BusConnect(GetEntityId());
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
    }

    void EditorSimpleAnimationComponent::Deactivate()
    {
        AnimationInformationBus::Handler::BusDisconnect();
        MeshComponentNotificationBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();

        EditorComponentBase::Deactivate();
    }

    void EditorSimpleAnimationComponent::OnMeshCreated(const AZ::Data::Asset<AZ::Data::AssetData>& asset)
    {
        UpdateCharacterInstance();
        ICharacterInstance* characterInstance = nullptr;
        EBUS_EVENT_ID_RESULT(characterInstance, GetEntityId(), SkinnedMeshComponentRequestBus, GetCharacterInstance);
        if (characterInstance)
        {
            characterInstance->GetIAnimationSet()->RegisterListener(this);
        }
    }

    void EditorSimpleAnimationComponent::OnMeshDestroyed()
    {
        UpdateCharacterInstance();
    }

    void EditorSimpleAnimationComponent::UpdateCharacterInstance()
    {
        ICharacterInstance* characterInstance = nullptr;
        EBUS_EVENT_ID_RESULT(characterInstance, GetEntityId(), SkinnedMeshComponentRequestBus, GetCharacterInstance);

        m_availableAnimations.clear();

        if (characterInstance)
        {
            FetchCharacterAnimations(characterInstance);
        }

        OnLayersChanged();
    }

    void EditorSimpleAnimationComponent::FetchCharacterAnimations(ICharacterInstance* characterInstance)
    {
        EBUS_EVENT_ID_RESULT(characterInstance, GetEntityId(), SkinnedMeshComponentRequestBus, GetCharacterInstance);
        AZ::u32 animationCount = characterInstance->GetIAnimationSet()->GetAnimationCount();

        for (int i = 0; i < animationCount; i++)
        {
            m_availableAnimations.push_back(characterInstance->GetIAnimationSet()->GetNameByAnimID(i));
        }
    }

    void EditorSimpleAnimationComponent::OnLayersChanged()
    {
        for (AnimatedLayer& animatedLayer : m_defaultAnimationSettings)
        {
            animatedLayer.SetEntityId(GetEntityId());
        }

        EBUS_EVENT(AzToolsFramework::ToolsApplicationEvents::Bus,
            InvalidatePropertyDisplay, AzToolsFramework::Refresh_AttributesAndValues);
    }

    void EditorSimpleAnimationComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        SimpleAnimationComponent* simpleAnimationComponent = gameEntity->CreateComponent<SimpleAnimationComponent>();

        if (simpleAnimationComponent)
        {
            simpleAnimationComponent->m_defaultAnimationSettings = m_defaultAnimationSettings;
        }
    }

    void EditorSimpleAnimationComponent::OnAnimationSetReload()
    {
        UpdateCharacterInstance();
    }

    const AnimatedLayer::AnimationNameList* EditorSimpleAnimationComponent::GetAvailableAnimationsList()
    {
        return &m_availableAnimations;
    }
} // namespace LmbrCentral