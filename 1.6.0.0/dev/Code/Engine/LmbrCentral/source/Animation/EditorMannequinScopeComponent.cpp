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
#include "EditorMannequinScopeComponent.h"
#include "MannequinScopeComponent.h"

namespace LmbrCentral
{
    void EditorMannequinScopeComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<EditorMannequinScopeComponent, EditorComponentBase>()
                ->Version(1)
                ->Field("Animation Database", &EditorMannequinScopeComponent::m_animationDatabase)
                ->Field("Scope name", &EditorMannequinScopeComponent::m_scopeContextName)
                ->Field("Target Entity", &EditorMannequinScopeComponent::m_targetEntityId);

            AZ::EditContext* editContext = serializeContext->GetEditContext();

            if (editContext)
            {
                editContext->Class<EditorMannequinScopeComponent>(
                    "Mannequin Scope Context", "Allows you to attach mannequin scope contexts to adb files")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Animation")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/MannequinScopeContext")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/MannequinScopeContext.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &EditorMannequinScopeComponent::m_animationDatabase, "Animation Database", "Animation database file that has to be attached to this scope context")
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &EditorMannequinScopeComponent::m_scopeContextName, "Context Name", "Name of the Scope that the context is to be set for")
                        ->Attribute(AZ::Edit::Attributes::StringList, &EditorMannequinScopeComponent::GetAvailableScopeContextNames)
                    ->DataElement(0, &EditorMannequinScopeComponent::m_targetEntityId, "Target Entity", "Entity that this scope context setting is bound to; Empty for this entity");
            }
        }
    }

    EditorMannequinScopeComponent::EditorMannequinScopeComponent()
        : m_scopeContextName("")
    {
    }

    void EditorMannequinScopeComponent::Activate()
    {
        EditorComponentBase::Activate();
        RefreshAvailableScopeContextNames();
        MannequinAssetNotificationBus::Handler::BusConnect(GetEntityId());
    }

    void EditorMannequinScopeComponent::Deactivate()
    {
        MannequinAssetNotificationBus::Handler::BusDisconnect();
        EditorComponentBase::Deactivate();
    }

    void EditorMannequinScopeComponent::OnControllerDefinitionsChanged()
    {
        RefreshAvailableScopeContextNames();
        EBUS_EVENT(AzToolsFramework::ToolsApplicationEvents::Bus,
                   InvalidatePropertyDisplay, AzToolsFramework::Refresh_AttributesAndValues);
    }

    void EditorMannequinScopeComponent::RefreshAvailableScopeContextNames()
    {
        EBUS_EVENT_ID(GetEntityId(), MannequinInformationBus, FetchAvailableScopeContextNames, m_scopeContextNames);

        if (m_scopeContextNames.size() > 0)
        {
            if (AZStd::find(m_scopeContextNames.begin(), m_scopeContextNames.end(), m_scopeContextName) == m_scopeContextNames.end())
            {
                m_scopeContextName = m_scopeContextNames[0];
            }
        }
        else
        {
            m_scopeContextName.clear();
        }
    }

    AZStd::vector<AZStd::string> EditorMannequinScopeComponent::GetAvailableScopeContextNames()
    {
        return m_scopeContextNames;
    }

    void EditorMannequinScopeComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        MannequinScopeComponent* mannequinScopeComponent = gameEntity->CreateComponent<MannequinScopeComponent>();
        mannequinScopeComponent->m_animationDatabase = m_animationDatabase;
        mannequinScopeComponent->m_scopeContextName = m_scopeContextName;
        mannequinScopeComponent->m_targetEntityId = m_targetEntityId;
    }

} // namespace LmbrCentral
