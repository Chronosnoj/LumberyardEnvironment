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
#pragma once

#include <AzCore/std/containers/vector.h>
#include <AzCore/Component/Component.h>
#include <LmbrCentral/Scripting/SimpleStateComponentBus.h>

namespace LmbrCentral
{
    /**
    * State
    *
    * Structure describing a single state
    */
    class State
    {
    public:
        AZ_TYPE_INFO(State, "{97BCF9D8-A76D-456F-A4B8-98EFF6897CE7}");

        void Init();
        void Activate();
        void Deactivate();

        const char* GetNameCStr() const
        {
            return m_name.c_str();
        }

        const AZStd::string& GetName() const
        {
            return m_name;
        }

        static State* FindWithName(AZStd::vector<State>& states, const char* stateName);
        static void Reflect(AZ::ReflectContext* context);

    private:
        void UpdateNameCrc();

        // runtime value, not serialized
        AZ::Crc32 m_nameCrc;

        // serialized values
        AZStd::string m_name;
        AZStd::vector<AZ::EntityId> m_entityIds;
    };


    /**
     * SimpleStateComponent
     *
     * SimpleState provides a simple state machine.
     *
     * Each state is represented by a name and zero or more entities to activate when entered and deactivate when the state is left.
     */
    class SimpleStateComponent
        : public AZ::Component
        , public SimpleStateComponentRequestBus::Handler
    {
    public:
        AZ_COMPONENT(SimpleStateComponent, "{242D4707-BC72-4245-AC96-BCEE38BBC1B7}");

        ~SimpleStateComponent() override {}

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // SimpleStateComponentRequestBus::Handler
        //////////////////////////////////////////////////////////////////////////
        void SetState(const char* stateName) override;
        const char* GetCurrentState() override;

    private:

        //////////////////////////////////////////////////////////////////////////
        // Component descriptor
        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("SimpleStateService"));
        }

        //////////////////////////////////////////////////////////////////////////
        // Helpers
        AZStd::vector<AZStd::string> GetStateNames() const;

        //////////////////////////////////////////////////////////////////////////
        // Runtime state, not serialized
        State* m_initialState = nullptr;
        State* m_currentState = nullptr;

        // Serialized
        AZStd::string m_initialStateName;
        AZStd::vector<State> m_states;
        bool m_resetStateOnActivate = true;
    };
} // namespace LmbrCentral
