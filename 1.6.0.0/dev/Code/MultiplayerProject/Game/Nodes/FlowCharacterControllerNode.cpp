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
#include "Game/Actor.h"
#include <IGameObject.h>
#include "Nodes/GameFlowBaseNode.h"
#include <GridMate/GridMate.h>
#include "../CryNetwork/GridMate/NetworkGridMate.h"
#include "../Core/MultiplayerProjectGameRules.h"

namespace
{
    const string g_MultiplayerCharacterControllerNodePath = "Multiplayer:CharacterController";
}

namespace LYGame
{
    /*!
     * A FlowGraph node that registers with gamerules and is assigned a unique character id
     * When a player connects, they are assigned a character id and the node that has the same
     * character id is notified, causing it to activate.  When a player disconnects, gamerules notifies
     * the node that was owned by that player so it can activate the 'Disconnected' output port.
     */
    class CFlowNode_MultiplayerCharacterController
        : public CFlowBaseNode <eNCT_Instanced>
        , public IPossessableCharacter
    {
    public:
        enum OutputPorts
        {
            ChannelId = 0,
            CharacterId,
            IsLocalPlayer,
            Connected,
            Disconnected
        };

        CFlowNode_MultiplayerCharacterController(SActivationInfo* pActInfo)
            : m_characterId(kInvalidCharacterId)
            , m_ownerChannelId(kInvalidChannelId)
        {
        }

        ~CFlowNode_MultiplayerCharacterController()
        {
            if (gEnv->pGame && gEnv->pGame->GetIGameFramework())
            {
                MultiplayerProjectGameRules* gamerules = static_cast<MultiplayerProjectGameRules*>(gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules());
                if (gamerules)
                {
                    gamerules->RemovePossessableCharacter(this);
                }
            }
        }

        IFlowNodePtr Clone(SActivationInfo* pActInfo)
        {
            return new CFlowNode_MultiplayerCharacterController(pActInfo);
        }

        void GetConfiguration(SFlowNodeConfig& config) override
        {
            static const SInputPortConfig inputs[] =
            {
                { 0 }
            };
            static const SOutputPortConfig outputs[] =
            {
                OutputPortConfig<int>("ChannelId", _HELP("Channel Id of the player that will assume control")),
                OutputPortConfig<int>("CharacterId", _HELP("Character Id to be controlled (not an EntityId)")),
                OutputPortConfig<bool>("IsLocalPlayer", _HELP("This character should be controlled by the local player")),
                OutputPortConfig<bool>("Connected", _HELP("True if connected")),
                OutputPortConfig<bool>("Disconnected", _HELP("True if disconnected")),
                { 0 }
            };

            config.pInputPorts = inputs;
            config.pOutputPorts = outputs;
            config.sDescription = _HELP("When a player connects or disconnects, one instance of this node will fire. Character Id is 0..max players.");
            config.SetCategory(EFLN_ADVANCED);
        }

        void GetMemoryUsage(ICrySizer* s) const override
        {
            s->Add(*this);
        }

        //! MultiplayerProjectGameRules calls this method to assign a player (owner channel id)
        void SetOwnerChannelId(::ChannelId channelId) override
        {
            m_ownerChannelId = channelId;

            // register for updates because we need to wait till the init phase is over
            // before triggering other flowgraph nodes
            if (m_activationInfo.pGraph)
            {
                m_activationInfo.pGraph->SetRegularlyUpdated(m_activationInfo.myID, true);
            }
        }

        ::ChannelId GetOwnerChannelId() override
        {
            return m_ownerChannelId;
        }

        //! MultiplayerProjectGameRules calls this method to give this node a unique character id
        void SetCharacterId(LYGame::CharacterId characterId) override
        {
            m_characterId = characterId;
        }

        LYGame::CharacterId GetCharacterId() override
        {
            return m_characterId;
        }

        void ProcessEvent(EFlowEvent event, SActivationInfo* activationInfo) override
        {
            m_activationInfo = *activationInfo;

            switch (event)
            {
            case eFE_Initialize:
            {
                MultiplayerProjectGameRules* gamerules = static_cast<MultiplayerProjectGameRules*>(gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules());
                if (gamerules)
                {
                    gamerules->AddPossessableCharacter(this);
                }
                break;
            }
            case eFE_Update:
            {
                if (m_ownerChannelId != kInvalidChannelId)
                {
                    CryLogAlways("[Game] Player %ul owns character %d", m_ownerChannelId, m_characterId);

                    ActivateOutput(&m_activationInfo, ChannelId, m_ownerChannelId);
                    ActivateOutput(&m_activationInfo, CharacterId, m_characterId);
                    if (gEnv->pNetwork && gEnv->IsClient() && gEnv->pNetwork->GetLocalChannelId() == m_ownerChannelId)
                    {
                        CryLogAlways("[Game] Assigning character %d to local player", m_characterId);
                        ActivateOutput(&m_activationInfo, IsLocalPlayer, true);
                    }
                    ActivateOutput(&m_activationInfo, Connected, true);
                }
                else
                {
                    CryLogAlways("[Game] Character %d no longer owned", m_characterId);
                    ActivateOutput(&m_activationInfo, ChannelId, m_ownerChannelId);
                    ActivateOutput(&m_activationInfo, CharacterId, m_characterId);
                    ActivateOutput(&m_activationInfo, Disconnected, true);
                }

                if (m_activationInfo.pGraph)
                {
                    m_activationInfo.pGraph->SetRegularlyUpdated(m_activationInfo.myID, false);
                }
            }
            }
        }

    private:

        SActivationInfo m_activationInfo;

        LYGame::CharacterId m_characterId;
        ::ChannelId m_ownerChannelId;
    };

    REGISTER_FLOW_NODE(g_MultiplayerCharacterControllerNodePath, CFlowNode_MultiplayerCharacterController);
};

