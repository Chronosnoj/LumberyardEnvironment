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
#include "EditorLookAtComponent.h"
#include "LookAtComponent.h"

#include <AzCore/Serialization/EditContext.h>

namespace LmbrCentral
{
    //=========================================================================
    void EditorLookAtComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorLookAtComponent, AZ::Component>()
                ->Version(1)
                ->Field("Target", &EditorLookAtComponent::m_targetId)
                ->Field("ForwardAxis", &EditorLookAtComponent::m_forwardAxis)
                ;

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<EditorLookAtComponent>("Look At", "Force an entity to always look at a given target")

                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Game")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/LookAt.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/LookAt.png")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)

                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorLookAtComponent::m_targetId, "Target", "The entity to look at")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorLookAtComponent::OnTargetChanged)

                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &EditorLookAtComponent::m_forwardAxis, "Forward Axis", "The local axis that should point at the target")
                        ->EnumAttribute(AzFramework::Axis::YPositive, "Y+")
                        ->EnumAttribute(AzFramework::Axis::YNegative, "Y-")
                        ->EnumAttribute(AzFramework::Axis::XPositive, "X+")
                        ->EnumAttribute(AzFramework::Axis::XNegative, "X-")
                        ->EnumAttribute(AzFramework::Axis::ZPositive, "Z+")
                        ->EnumAttribute(AzFramework::Axis::ZNegative, "Z-")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorLookAtComponent::RecalculateTransform)
                    ;
            }
        }
    }

    //=========================================================================
    void EditorLookAtComponent::Activate()
    {
        if (m_targetId.IsValid())
        {
            AZ::EntityBus::Handler::BusConnect(m_targetId);
        }
    }

    //=========================================================================
    void EditorLookAtComponent::Deactivate()
    {
        AZ::TransformNotificationBus::MultiHandler::BusDisconnect();
        AZ::EntityBus::Handler::BusDisconnect();
    }

    //=========================================================================
    void EditorLookAtComponent::OnEntityActivated(AZ::Entity* /*entity*/)
    {
        AZ::TransformNotificationBus::MultiHandler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::MultiHandler::BusConnect(m_targetId);
    }

    //=========================================================================
    void EditorLookAtComponent::OnEntityDeactivate(AZ::Entity* /*entity*/)
    {
        AZ::TransformNotificationBus::MultiHandler::BusDisconnect(GetEntityId());
        AZ::TransformNotificationBus::MultiHandler::BusDisconnect(m_targetId);
    }

    //=========================================================================
    void EditorLookAtComponent::OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world)
    {
        // We need to defer the Look-At transform change.  Can't flush it through here because
        // that will cause a feedback loop and the originator of the transform change might
        // not be finished broadcasting out to listeners.  If we set look-at here, the look-at
        // transform can be stomped later by the original data.

        // Method 1: Connect to the Tick bus for a frame.  In the next OnTick we set the
        // Look-At transform and disconnect.
        AZ::TickBus::Handler::BusConnect();


        // Method 2: We may want to stay connected to the TickBus if the transform is constantly
        // changing.  Without a good heuristic to detect this case, we'll stick with Method 1.
        // Here's a gist of this method:

        // connect/disconnect to TickBus in OnEntityActivated/OnEntityDeactivate.
        // set a 'shouldRecalc' flag here in OnTransformChanged to true;
        // in OnTick do this:
        // if (shouldRecalc)
        // {
        //      RecalculateTransform();
        //      shouldRecalc = false;
        // }
    }

    //=========================================================================
    void EditorLookAtComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        LookAtComponent* lookAtComponent = gameEntity->CreateComponent<LookAtComponent>();

        if (lookAtComponent)
        {
            lookAtComponent->m_targetId = m_targetId;
            lookAtComponent->m_forwardAxis = m_forwardAxis;
        }
    }

    //=========================================================================
    void EditorLookAtComponent::OnTargetChanged()
    {
        if (m_oldTargetId.IsValid())
        {
            // Disconnect from the old target entity
            AZ::TransformNotificationBus::MultiHandler::BusDisconnect(m_oldTargetId);
            AZ::EntityBus::Handler::BusDisconnect(m_oldTargetId);
            m_oldTargetId = AZ::EntityId();
        }

        if (m_targetId.IsValid())
        {
            // Connect to the new target entity
            // Won't connect to the new target's transform bus until we receive notification
            // that target is activated via the EntityBus.
            AZ::EntityBus::Handler::BusConnect(m_targetId);
            m_oldTargetId = m_targetId;

            RecalculateTransform();
        }
        else
        {
            // If the target is invalid (nothing to look at), stop listening to everything
            AZ::TransformNotificationBus::MultiHandler::BusDisconnect();
            AZ::EntityBus::Handler::BusDisconnect();
        }
    }

    //=========================================================================
    void EditorLookAtComponent::OnTick(float /*deltaTime*/, AZ::ScriptTimePoint /*time*/)
    {
        RecalculateTransform();
        AZ::TickBus::Handler::BusDisconnect();
    }

    //=========================================================================
    void EditorLookAtComponent::RecalculateTransform()
    {
        if (m_targetId.IsValid())
        {
            AZ::TransformNotificationBus::MultiHandler::BusDisconnect(GetEntityId());
            {
                AZ::Transform currentTM = AZ::Transform::CreateIdentity();
                EBUS_EVENT_ID_RESULT(currentTM, GetEntityId(), AZ::TransformBus, GetWorldTM);

                AZ::Transform targetTM = AZ::Transform::CreateIdentity();
                EBUS_EVENT_ID_RESULT(targetTM, m_targetId, AZ::TransformBus, GetWorldTM);

                AZ::Transform lookAtTransform = AzFramework::CreateLookAt(
                    currentTM.GetPosition(),
                    targetTM.GetPosition(),
                    m_forwardAxis
                    );

                EBUS_EVENT_ID(GetEntityId(), AZ::TransformBus, SetWorldTM, lookAtTransform);
            }
            AZ::TransformNotificationBus::MultiHandler::BusConnect(GetEntityId());
        }
    }

}//namespace LmbrCentral
