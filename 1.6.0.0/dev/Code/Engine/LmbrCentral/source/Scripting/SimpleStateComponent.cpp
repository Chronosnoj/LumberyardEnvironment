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

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/EBus/ScriptBinder.h>

#include <AzFramework/Entity/GameEntityContextBus.h>

#include "SimpleStateComponent.h"

namespace LmbrCentral
{
    AZ_SCRIPTABLE_EBUS(
        SimpleStateComponentRequestBus,
        SimpleStateComponentRequestBus,
        "{861E46CC-DBC0-48C5-A1C2-CBD76EF40B6E}",
        "{17C9B885-E5EA-478E-8AD7-E7978628AD08}",
        AZ_SCRIPTABLE_EBUS_EVENT(SetState, const char*)
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(const char*,nullptr, GetCurrentState)
        )

    AZ_SCRIPTABLE_EBUS(
        SimpleStateComponentNotificationBus,
        SimpleStateComponentNotificationBus,
        "{9BF3EA15-DCF8-4C40-9878-7580C4AE57BF}",
        "{F935125C-AE4E-48C1-BB60-24A0559BC4D2}",
        AZ_SCRIPTABLE_EBUS_EVENT(OnStateChanged, const char*, const char*)
        )

    //=========================================================================
    // ForEachEntity
    //=========================================================================
    template <class Func>
    void ForEachEntity(AZStd::vector<AZ::EntityId>& entitiyIds, Func entityFunction)
    {
        for (const AZ::EntityId& currId : entitiyIds)
        {
            AZ::Entity* entity = nullptr;
            EBUS_EVENT_RESULT(entity, AZ::ComponentApplicationBus, FindEntity, currId);
            if (entity)
            {
                entityFunction(entity);
            }
        }
    }

    //=========================================================================
    // State::Init
    //=========================================================================
    void State::Init()
    {
        UpdateNameCrc();
        for (const AZ::EntityId& currId : m_entityIds)
        {
            EBUS_EVENT(AzFramework::GameEntityContextRequestBus, MarkEntityForNoActivation, currId);
        }
    }

    //=========================================================================
    // State::Activate
    //=========================================================================
    void State::Activate()
    {
        ForEachEntity(m_entityIds,
            [](AZ::Entity* entity)
            {
                if (entity->GetState() != AZ::Entity::ES_ACTIVE)
                {
                    entity->Activate();
                }
            }
        );
    }

    //=========================================================================
    // State::Deactivate
    //=========================================================================
    void State::Deactivate()
    {
        ForEachEntity(m_entityIds,
            [](AZ::Entity* entity)
            {
                if (entity->GetState() == AZ::Entity::ES_ACTIVE)
                {
                    entity->Deactivate();
                }
            }
        );
    }

    //=========================================================================
    // State::UpdateNameCrc
    //=========================================================================
    void State::UpdateNameCrc()
    {
        m_nameCrc = AZ::Crc32(m_name.c_str());
    }

    //=========================================================================
    // GetStateFromName
    //=========================================================================
    State* State::FindWithName(AZStd::vector<State>& states, const char* stateName)
    {
        if (stateName)
        {
            const AZ::Crc32 stateNameCrc(stateName);
            for (State& currState : states)
            {
                if (currState.m_nameCrc == stateNameCrc)
                {
                    AZ_Assert(strcmp(stateName, currState.m_name.c_str()) == 0, "Crc32 state name collision between '%s' and '%s'", stateName, currState.m_name.c_str());
                    return &currState;
                }
            }

            AZ_Error("Script", false, "StateName '%s' does not map to any existing states", stateName);
        }

        return nullptr;
    }

    //=========================================================================
    // SimpleStateComponent::Reflect
    //=========================================================================
    void State::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<State>()
                ->Version(1)
                ->Field("Name", &State::m_name)
                ->Field("EntityIds", &State::m_entityIds);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<State>("State", "A state includes a name and set of entities that will be activated when the state is entered and deactivated when the state is left.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &State::m_name, "Name", "The name of this state")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"))
                    ->DataElement(0, &State::m_entityIds, "Entities", "The list of entities referenced by this state")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"));
            }
        }
    }

    //=========================================================================
    // SimpleStateComponent::Reflect
    //=========================================================================
    void SimpleStateComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        State::Reflect(serializeContext);

        if (serializeContext)
        {
            serializeContext->Class<SimpleStateComponent, AZ::Component>()
                ->Version(1)
                ->Field("InitialStateName", &SimpleStateComponent::m_initialStateName)
                ->Field("ResetOnActivate", &SimpleStateComponent::m_resetStateOnActivate)
                ->Field("States", &SimpleStateComponent::m_states);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<SimpleStateComponent>("Simple State", "The simple state component provides a simple state machine, where each state is represented by zero or more entities to activate when entered, deactivate when left.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Game")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/SimpleState.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/SimpleState.png")
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &SimpleStateComponent::m_initialStateName, "Initial state", "The initial active state")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"))
                        ->Attribute(AZ::Edit::Attributes::StringList, &SimpleStateComponent::GetStateNames)
                    ->DataElement(0, &SimpleStateComponent::m_resetStateOnActivate, "Reset on activate", "If set, SimpleState will return to the configured initial state when activated, and not the state held prior to being deactivated.")
                    ->DataElement(0, &SimpleStateComponent::m_states, "States", "The list of states")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshAttributesAndValues"));
            }
        }

        AZ::ScriptContext* script = azrtti_cast<AZ::ScriptContext*>(context);
        if (script)
        {
            ScriptableEBus_SimpleStateComponentRequestBus::Reflect(script);
            ScriptableEBus_SimpleStateComponentNotificationBus::Reflect(script);
        }
    }

    //=========================================================================
    // SimpleStateComponent::Init
    //=========================================================================
    void SimpleStateComponent::Init()
    {
        for (State& currState : m_states)
        {
            currState.Init();
        }
        m_initialState = State::FindWithName(m_states, m_initialStateName.c_str());
        m_currentState = m_initialState;
    }

    //=========================================================================
    // SimpleStateComponent::Activate
    //=========================================================================
    void SimpleStateComponent::Activate()
    {
        if (m_resetStateOnActivate)
        {
            m_currentState = m_initialState;
        }

        for (State& currState : m_states)
        {
            if (&currState != m_currentState)
            {
                currState.Deactivate();
            }
        }

        if (m_currentState)
        {
            m_currentState->Activate();
        }

        SimpleStateComponentRequestBus::Handler::BusConnect(GetEntityId());

        if (m_currentState)
        {
            // Notify newly activated State
            // Note: Even if !m_resetStateOnActivate, the prior state to Activation is NullState
            EBUS_EVENT_ID(GetEntityId(), SimpleStateComponentNotificationBus, OnStateChanged, SimpleStateConstants::NullStateName, m_currentState->GetNameCStr());
        }
    }

    //=========================================================================
    // SimpleStateComponent::Deactivate
    //=========================================================================
    void SimpleStateComponent::Deactivate()
    {
        SimpleStateComponentRequestBus::Handler::BusDisconnect();

        if (m_currentState)
        {
            m_currentState->Deactivate();
        }
    }

    //=========================================================================
    // SimpleStateComponent::SetState
    //=========================================================================
    void SimpleStateComponent::SetState(const char* stateName)
    {
        // Out with the old
        const char* oldName = SimpleStateConstants::NullStateName;
        if (m_currentState)
        {
            oldName = m_currentState->GetNameCStr();
            m_currentState->Deactivate();
        }

        // In with the new
        const char* newName = SimpleStateConstants::NullStateName;
        State* newState = State::FindWithName(m_states, stateName);
        if (newState)
        {
            newName = newState->GetNameCStr();
            newState->Activate();
        }

        if (m_currentState != newState)
        {
            m_currentState = newState;

            EBUS_EVENT_ID(GetEntityId(), SimpleStateComponentNotificationBus, OnStateChanged, oldName, newName);
        }
    }

    //=========================================================================
    // SimpleStateComponent::GetState
    //=========================================================================
    const char* SimpleStateComponent::GetCurrentState()
    {
        return m_currentState->GetNameCStr();
    }

    //=========================================================================
    // SimpleStateComponent::GetStateNames
    //=========================================================================
    AZStd::vector<AZStd::string> SimpleStateComponent::GetStateNames() const
    {
        AZStd::vector<AZStd::string> names;
        names.reserve(m_states.size());
        for (const State& currState : m_states)
        {
            names.emplace_back(currState.GetName());
        }
        return names;
    }
} // namespace LmbrCentral
