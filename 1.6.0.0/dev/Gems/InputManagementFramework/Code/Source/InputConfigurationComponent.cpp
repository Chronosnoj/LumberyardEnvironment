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
#include "InputConfigurationComponent.h"
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Serialization/ObjectStream.h>
#include <InputRequestBus.h>

namespace Input
{
    void InputConfigurationComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("InputConfigurationService"));
    }

    void InputConfigurationComponent::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<InputConfigurationComponent>()
                ->Version(1)
                ->Field("Input Event Bindings", &InputConfigurationComponent::m_inputEventBindingsAsset)
                ->Field("Source EntityId", &InputConfigurationComponent::m_sourceEntity);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<InputConfigurationComponent>("Input",
                    "The Input Configuration allows a user to modify how raw inputs translate into higher level events.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Game")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/InputConfig.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/InputConfig.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                    ->DataElement(0, &InputConfigurationComponent::m_inputEventBindingsAsset, "Input to event bindings",
                        "Asset containing input to event binding information.")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &InputConfigurationComponent::m_sourceEntity, "Source EntityId", "All input for a single player must share a common source.");
            }
        }
    }

    void InputConfigurationComponent::Init()
    {
    }

    void InputConfigurationComponent::Activate()
    {
        AZ::Data::AssetBus::Handler::BusConnect(m_inputEventBindingsAsset.GetId());
    }

    void InputConfigurationComponent::Deactivate()
    {
        AZ::Data::AssetBus::Handler::BusDisconnect();
        AZ::EntityId sourceId = m_sourceEntity.IsValid() ? m_sourceEntity : GetEntityId();
        m_inputEventBindings.Deactivate(sourceId);

    }

    void InputConfigurationComponent::OnAssetReady(const AZ::Data::Asset<AZ::Data::AssetData>& asset)
    {
        AZ_Assert(asset.GetAs<InputEventBindingsAsset>(), "Input bindings asset is not the correct type.");
        m_inputEventBindings = asset.GetAs<InputEventBindingsAsset>()->m_bindings;

        AZ::EntityId sourceId = m_sourceEntity.IsValid() ? m_sourceEntity : GetEntityId();
        EBUS_EVENT(AZ::InputRequestsBus, RequestDeviceMapping, sourceId);
        m_inputEventBindings.Activate(sourceId);
    }
}
