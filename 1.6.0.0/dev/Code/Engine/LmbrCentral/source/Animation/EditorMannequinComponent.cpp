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
#include "EditorMannequinComponent.h"
#include "MannequinComponent.h"

#include "../../CryEngine/CryAction/ICryMannequin.h"

namespace LmbrCentral
{
   
    void EditorMannequinComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<EditorMannequinComponent, EditorComponentBase>()
                ->Version(1)
                ->Field("Controller Definition", &EditorMannequinComponent::m_controllerDefinition);

            AZ::EditContext* editContext = serializeContext->GetEditContext();

            if (editContext)
            {
                editContext->Class<EditorMannequinComponent>(
                    "Mannequin", "Allows an Entity to animate using Mannequin")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Animation")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/Mannequin")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/Mannequin.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &EditorMannequinComponent::m_controllerDefinition, "Controller Definition", "The mannequin controller definition file")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorMannequinComponent::ControllerDefinitionChanged);
            }
        }
    }

    void EditorMannequinComponent::Activate()
    {
        EditorComponentBase::Activate();
        UpdateScopeContextNames();
        MannequinInformationBus::Handler::BusConnect(GetEntityId());
    }

    void EditorMannequinComponent::ControllerDefinitionChanged()
    {
        UpdateScopeContextNames();
        EBUS_EVENT_ID(GetEntityId(), MannequinAssetNotificationBus, OnControllerDefinitionsChanged);
    }

    void EditorMannequinComponent::UpdateScopeContextNames()
    {
        m_scopeContextNames.clear();
        AZStd::string controllerDefinitionFilePath = m_controllerDefinition.GetAssetPath();
        if (!controllerDefinitionFilePath.empty())
        {
            const SControllerDef* controllerDefinition = MannequinComponent::LoadControllerDefinition(m_controllerDefinition.GetAssetPath().c_str());
            if (controllerDefinition)
            {
                controllerDefinition->m_scopeContexts.FetchTagNamesAsVector(m_scopeContextNames);
            }
        }
    }

    void EditorMannequinComponent::FetchAvailableScopeContextNames(AZStd::vector<AZStd::string>& scopeContextNames) const
    {
        scopeContextNames = m_scopeContextNames;
    }

    void EditorMannequinComponent::Deactivate()
    {
        EditorComponentBase::Deactivate();
        MannequinInformationBus::Handler::BusDisconnect();
    }

    void EditorMannequinComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        MannequinComponent* mannequinComponent = gameEntity->CreateComponent<MannequinComponent>();
        mannequinComponent->m_controllerDefinition = m_controllerDefinition;
    }

} // namespace LmbrCentral
