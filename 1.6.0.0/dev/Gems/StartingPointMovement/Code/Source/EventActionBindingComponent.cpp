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
#include "StdAfx.h"
#include "EventActionBindingComponent.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>

// reflect methods
#include "MoveEntityAction.h"
#include "RotateEntityAction.h"
#include "AddPhysicsImpulseAction.h"

namespace Movement
{
    void EventActionBindingComponent::Reflect(AZ::ReflectContext* reflection)
    {
        Movement::MoveEntityAction::Reflect(reflection);
        Movement::RotateEntityAction::Reflect(reflection);
        Movement::AddPhysicsInpulseAction::Reflect(reflection);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<EventActionBindingComponent>()
                ->Version(1)
                ->Field("Event Action Bindings", &EventActionBindingComponent::m_eventActionBindings)
                ->Field("Source EntityId", &EventActionBindingComponent::m_sourceEntity);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<EventActionBindingComponent>("Event Action Binding"
                    , "Allows a user to bind high level events to actions. Ex 'jump' -> MoveEntityAction")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Game")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/EventBind.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/EventBind.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                    ->DataElement(0, &EventActionBindingComponent::m_eventActionBindings, "Event to action bindings",
                    "A list of Event action bindings for this entity")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &EventActionBindingComponent::m_sourceEntity, "Entity Event Channel",
                        "The entity id to use for receiving events");
            }
        }
    }

    void EventActionBindingComponent::Init()
    {
        for (IEventActionSubComponent* subComponent : m_eventActionBindings)
        {
            subComponent->Init();
        }
    }

    void EventActionBindingComponent::Activate()
    {
        AZ::EntityId sourceId = m_sourceEntity.IsValid() ? m_sourceEntity : GetEntityId();
        for (IEventActionSubComponent* subComponent : m_eventActionBindings)
        {
            subComponent->Activate(GetEntityId(), sourceId);
        }
    }

    void EventActionBindingComponent::Deactivate()
    {
        AZ::EntityId sourceId = m_sourceEntity.IsValid() ? m_sourceEntity : GetEntityId();
        for (IEventActionSubComponent* subComponent : m_eventActionBindings)
        {
            subComponent->Deactivate(sourceId);
        }
    }
} //namespace Movement
