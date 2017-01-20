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
#include "BehaviorTreeComponent.h"
#include <BehaviorTree/BehaviorTreeManagerRequestBus.h>
#include <AzCore/EBus/ScriptBinder.h>


namespace LmbrCentral
{
    static AZStd::vector<AZ::Crc32> s_defaultEmptyVariableIds;
    AZ_SCRIPTABLE_EBUS(
        BehaviorTreeComponentRequestBus,
        BehaviorTreeComponentRequestBus,
        "{2C20D1CD-88D2-4A47-B01B-2E23F2D80615}",
        "{5FF91790-FE50-427D-A0FC-BE7D10CCC0C9}",
        AZ_SCRIPTABLE_EBUS_EVENT(StartBehaviorTree)
        AZ_SCRIPTABLE_EBUS_EVENT(StopBehaviorTree)
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(AZStd::vector<AZ::Crc32>, s_defaultEmptyVariableIds, GetVariableNameCrcs)
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(bool, false, GetVariableValue, AZ::Crc32)
        AZ_SCRIPTABLE_EBUS_EVENT(SetVariableValue, AZ::Crc32, bool)
        )

    void BehaviorTreeComponent::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<BehaviorTreeComponent, AZ::Component>()
                ->Version(1)
                ->Field("Behavior Tree Asset", &BehaviorTreeComponent::m_behaviorTreeAsset)
                ->Field("Enabled Initially", &BehaviorTreeComponent::m_enabledInitially);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<BehaviorTreeComponent>("Behavior tree component", "Associates a behavior tree with an entity")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "AI")
                    ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/BehaviorTree.png")
                    ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/BehaviorTree.png")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &BehaviorTreeComponent::m_behaviorTreeAsset, "Behavior tree asset", "The behavior tree asset")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &BehaviorTreeComponent::m_enabledInitially, "Enabled initially", "When true the behavior tree will be loaded and activated with the entity");
            }
        }
        if (AZ::ScriptContext* script = azrtti_cast<AZ::ScriptContext*>(reflection))
        {
            ScriptableEBus_BehaviorTreeComponentRequestBus::Reflect(script);
        }
    }


    void BehaviorTreeComponent::Activate()
    {
        if (m_enabledInitially)
        {
            StartBehaviorTree();
        }
        BehaviorTreeComponentRequestBus::Handler::BusConnect(GetEntityId());
    }

    void BehaviorTreeComponent::Deactivate()
    {
        BehaviorTreeComponentRequestBus::Handler::BusDisconnect();
        StopBehaviorTree();
    }

    void BehaviorTreeComponent::StartBehaviorTree()
    {
        EBUS_EVENT(BehaviorTree::BehaviorTreeManagerRequestBus, StartBehaviorTree, GetEntityId(), m_behaviorTreeAsset.GetAssetPath().c_str());
    }

    void BehaviorTreeComponent::StopBehaviorTree()
    {
        EBUS_EVENT(BehaviorTree::BehaviorTreeManagerRequestBus, StopBehaviorTree, GetEntityId());
    }

    AZStd::vector<AZ::Crc32> BehaviorTreeComponent::GetVariableNameCrcs()
    { 
        AZStd::vector<AZ::Crc32> returnValue;
        EBUS_EVENT_RESULT(returnValue, BehaviorTree::BehaviorTreeManagerRequestBus, GetVariableNameCrcs, GetEntityId());
        return returnValue;
    }
    
    bool BehaviorTreeComponent::GetVariableValue(AZ::Crc32 variableNameCrc)
    {
        bool returnValue = false;
        EBUS_EVENT_RESULT(returnValue, BehaviorTree::BehaviorTreeManagerRequestBus, GetVariableValue, GetEntityId(), variableNameCrc);
        return returnValue;
    }

    void BehaviorTreeComponent::SetVariableValue(AZ::Crc32 variableNameCrc, bool newValue)
    {
        EBUS_EVENT(BehaviorTree::BehaviorTreeManagerRequestBus, SetVariableValue, GetEntityId(), variableNameCrc, newValue);
    }
} // namespace LmbrCentral