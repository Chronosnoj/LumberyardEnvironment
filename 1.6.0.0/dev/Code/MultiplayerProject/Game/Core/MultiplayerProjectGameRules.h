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

#include <IGameRulesSystem.h>
#include <ILevelSystem.h>

namespace LYGame
{
    typedef int32 CharacterId;
    static const CharacterId kInvalidCharacterId = CharacterId(-1);

    struct TRMIAssignCharacter
    {
        CharacterId m_characterId;
        ChannelId m_channelId;

        TRMIAssignCharacter()
            : m_characterId(kInvalidCharacterId)
            , m_channelId(kInvalidChannelId)
        {}

        TRMIAssignCharacter(ChannelId channelId, CharacterId characterId)
            : m_channelId(channelId)
            , m_characterId(characterId)
        {}

        void SerializeWith(TSerialize serializer)
        {
            serializer.Value("channelId", m_channelId);
            serializer.Value("characterId", m_characterId);
        }
    };

    /*!
    * Interface for classes that wish to be notified when the server tells this client to possess
    * a character.  The CFlowNode_MultiplayerCharacterController uses this be notified when a
    * character is assigned so it can trigger appropriate FlowGraph gameplay logic.
    */
    class IPossessableCharacter
    {
    public:
        virtual void SetOwnerChannelId(ChannelId channelId) = 0;
        virtual ChannelId GetOwnerChannelId() = 0;

        virtual void SetCharacterId(CharacterId characterId) = 0;
        virtual CharacterId GetCharacterId() = 0;
    };

    class MultiplayerProjectGameRules
        : public CGameObjectExtensionHelper < MultiplayerProjectGameRules, IGameRules >
        , ILevelSystemListener
    {
    public:
        MultiplayerProjectGameRules();
        virtual ~MultiplayerProjectGameRules();

        //////////////////////////////////////////////////////////////////////////
        //! IGameObjectExtension
        bool Init(IGameObject* pGameObject) override;
        void ProcessEvent(SEntityEvent& event) override { };
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // IGameRules
        bool OnClientConnect(ChannelId channelId, bool isReset) override;
        bool OnClientEnteredGame(ChannelId channelId, bool isReset) override;
        void OnClientDisconnect(ChannelId channelId, EDisconnectionCause cause, const char* desc, bool keepClient) override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        //! ILevelSystemListener
        void OnLoadingStart(ILevelInfo* pLevel) override;
        void OnUnloadComplete(ILevel* pLevel) override;
        //////////////////////////////////////////////////////////////////////////

        //! CharacterController nodes add possessable characters
        void AddPossessableCharacter(IPossessableCharacter* character);
        void RemovePossessableCharacter(IPossessableCharacter* character);

        //! Sends an RMI to notifying the client gamerules of character assignement
        void RMIReqToClient_AssignCharacter(ChannelId channelId, CharacterId characterId) const;

        //! RMI method that gets run on the client
        DECLARE_CLIENT_RMI_NOATTACH(ClReq_AssignCharacter, TRMIAssignCharacter, eNRT_ReliableOrdered);

    private:

        //! Safeguards the logic needed to assign a channelId to a character, or to put it into the unassigned list.
        void HandleCharacterAssignment(ChannelId channelId);

        //! Assign a character to a channel id (client)
        void AssignCharacter(ChannelId channelId, CharacterId characterId);

        //! Returns the character id assigned to this channel id, or assigns one
        CharacterId GetOrAssignCharacterId(ChannelId channelId);

        //! Returns the character id assigned to this channel id (client)
        CharacterId GetAssignedCharacterId(ChannelId channelId);

        //! Returns an available character id or kInvalidCharacterId
        IPossessableCharacter* GetUnAssignedCharacter();

        std::vector<IPossessableCharacter*> m_characters;
        std::list<ChannelId> m_unassignedPlayers;
    };
} // namespace LYGame