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
#include "MultiplayerProjectGameRules.h"
#include "IActorSystem.h"
#include <GridMate/GridMate.h>
#include <GridMate/Session/Session.h>
#include <GridMate/Session/LANSession.h>
#include "../CryNetwork/GridMate/NetworkGridMate.h"

namespace LYGame
{
    DECLARE_DEFAULT_COMPONENT_FACTORY(MultiplayerProjectGameRules, IGameRules)

    MultiplayerProjectGameRules::MultiplayerProjectGameRules()
    {
    }

    MultiplayerProjectGameRules::~MultiplayerProjectGameRules()
    {
        if (gEnv->pGame->GetIGameFramework()->GetILevelSystem())
        {
            gEnv->pGame->GetIGameFramework()->GetILevelSystem()->RemoveListener(this);
        }

        if (gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem())
        {
            gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->SetCurrentGameRules(nullptr);
        }
    }

    bool MultiplayerProjectGameRules::Init(IGameObject* pGameObject)
    {
        SetGameObject(pGameObject);
        if (!pGameObject->BindToNetwork())
        {
            return false;
        }

        if (gEnv == nullptr ||
            gEnv->pGame == nullptr ||
            gEnv->pGame->GetIGameFramework() == nullptr ||
            gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem() == nullptr)
        {
            return false;
        }

        gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->SetCurrentGameRules(this);
        if (gEnv->pGame->GetIGameFramework()->GetILevelSystem())
        {
            gEnv->pGame->GetIGameFramework()->GetILevelSystem()->AddListener(this);
        }

        return true;
    }

    /// Called on server when a client connects. You must create an actor for this
    /// client or the connection will fail.
    bool MultiplayerProjectGameRules::OnClientConnect(ChannelId channelId, bool isReset)
    {
        auto pPlayerActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->CreateActor(channelId, "Character", "Actor", Vec3(0, 0, 34), Quat(Ang3(0, 0, 0)), Vec3(1, 1, 1));

        return true;
    }

    /// This is called on the server after a client has finished loading the map on their
    /// end and received replication of entities, including the actor spawned for them
    /// in OnClientConnect.
    bool MultiplayerProjectGameRules::OnClientEnteredGame(ChannelId channelId, bool isReset)
    {
        CryLog("[Game] MultiplayerProjectGameRules::OnClientEnteredGame %u", channelId);
        HandleCharacterAssignment(channelId);
        return true;
    }

    void MultiplayerProjectGameRules::HandleCharacterAssignment(ChannelId channelId)
    {
        if (gEnv->bServer)
        {
            CharacterId characterId = GetOrAssignCharacterId(channelId);
            if (characterId != kInvalidCharacterId)
            {
                // notify all clients of the assignment
                RMIReqToClient_AssignCharacter(channelId, characterId);
            }
            else
            {
                // it's possible that clients will connect before any characters
                // are registered (during flowgraph init) so queue them for later assignment
                CryLog("[Game] Adding player %u to unassigned players list", channelId);
                stl::push_back_unique(m_unassignedPlayers, channelId);
            }
        }
    }

    /// Called on server when a client disconnects
    void MultiplayerProjectGameRules::OnClientDisconnect(ChannelId channelId, EDisconnectionCause cause, const char* desc, bool keepClient)
    {
        m_unassignedPlayers.remove_if(
            [this, &channelId](ChannelId testId)
        {
            return channelId == testId;
        }
            );

        for (IPossessableCharacter* character : m_characters)
        {
            if (character && character->GetOwnerChannelId() == channelId)
            {
                CryLog("[Game] Unassigning character for client %u", channelId);
                character->SetOwnerChannelId(kInvalidChannelId);
            }
        }
    }

    //! Called after ILevelSystem::OnLoadingStart() completes, before the level actually starts loading.
    void MultiplayerProjectGameRules::OnLoadingStart(ILevelInfo* pLevel)
    {
        m_characters.clear();
    }

    //! Called after a level is unloaded, before the data is freed.
    void MultiplayerProjectGameRules::OnUnloadComplete(ILevel* pLevel)
    {
        m_characters.clear();
    }

    void MultiplayerProjectGameRules::AssignCharacter(ChannelId channelId, CharacterId characterId)
    {
        for (IPossessableCharacter* character : m_characters)
        {
            if (character && character->GetCharacterId() == characterId)
            {
                CryLog("[Game] Assigning character %d to client %u", characterId, channelId);
                character->SetOwnerChannelId(channelId);
                break;
            }
        }
    }

    CharacterId MultiplayerProjectGameRules::GetOrAssignCharacterId(ChannelId channelId)
    {
        CharacterId characterId = GetAssignedCharacterId(channelId);
        if (characterId == kInvalidCharacterId)
        {
            IPossessableCharacter* character = GetUnAssignedCharacter();
            if (character)
            {
                character->SetOwnerChannelId(channelId);
                characterId = character->GetCharacterId();
                CryLog("[Game] Assigning character %d to client %u", characterId, channelId);
            }
            else
            {
                CryLog("[Game] No unassigned character found for client %u", channelId);
            }
        }

        return characterId;
    }

    IPossessableCharacter* MultiplayerProjectGameRules::GetUnAssignedCharacter()
    {
        for (IPossessableCharacter* character : m_characters)
        {
            if (character && character->GetOwnerChannelId() == kInvalidChannelId)
            {
                return character;
            }
        }

        return nullptr;
    }

    CharacterId MultiplayerProjectGameRules::GetAssignedCharacterId(ChannelId channelId)
    {
        for (IPossessableCharacter* character : m_characters)
        {
            if (character && character->GetOwnerChannelId() == channelId)
            {
                return character->GetCharacterId();
            }
        }

        return kInvalidCharacterId;
    }

    void MultiplayerProjectGameRules::AddPossessableCharacter(IPossessableCharacter* listener)
    {
        if (stl::push_back_unique(m_characters, listener))
        {
            listener->SetCharacterId(m_characters.size());
        }

        if (gEnv->bServer)
        {
            // it's possible that players connect before the map fully loads, so attempt to
            // assign those players as characters become available
            if (!m_unassignedPlayers.empty())
            {
                m_unassignedPlayers.remove_if(
                    [this](ChannelId channelId)
                {
                    CharacterId characterId = GetOrAssignCharacterId(channelId);
                    if (characterId != kInvalidCharacterId)
                    {
                        // notify all clients of the assignement
                        RMIReqToClient_AssignCharacter(channelId, characterId);
                        return true;
                    }

                    return false;
                }
                    );
            }
        }
    }

    void MultiplayerProjectGameRules::RemovePossessableCharacter(IPossessableCharacter* listener)
    {
        ChannelId channelId = listener->GetOwnerChannelId();

        stl::find_and_erase(m_characters, listener);

        // Re-assign the channelId that was possessing this character to the next available character.
        if (channelId != kInvalidChannelId)
        {
            HandleCharacterAssignment(channelId);
        }
    }

    // Method run on server to notify clients of new character assignement
    void MultiplayerProjectGameRules::RMIReqToClient_AssignCharacter(ChannelId channelId, CharacterId characterId) const
    {
        TRMIAssignCharacter Info(channelId, characterId);
        CryLog("[Game] Sending RMI to all clients.  Assign character %d to client %u", characterId, channelId);
        GetGameObject()->InvokeRMI(ClReq_AssignCharacter(), Info, eRMI_ToAllClients);
    }

    // RMI method run on client to update assignement
    IMPLEMENT_RMI(MultiplayerProjectGameRules, ClReq_AssignCharacter)
    {
        TRMIAssignCharacter Info(params);
        CryLog("[Game] Received assign character RMI from server with channel id %u, character id %d", Info.m_channelId, Info.m_characterId);
        AssignCharacter(Info.m_channelId, Info.m_characterId);
        return true;
    }
}
