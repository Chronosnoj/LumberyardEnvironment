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

#include <INetwork.h>
#include <IConsole.h>

namespace GridMate
{
    class GameLiftSessionService;
    struct InviteInfo;
}

namespace LYGame
{
    class LobbySessionListener;
    ////////////////////////////////////////////////////////////////////////////////
    /// Manages game-specific components of the lobby system
    ////////////////////////////////////////////////////////////////////////////////
    class Lobby
    {
        friend class LobbySessionListener;

    public:
        Lobby();

        void OnInit();
        void OnShutdown();
        void Update(float dt);

    private:
        void OnSessionCreated(GridMate::GridSession* session);

        LobbySessionListener* m_sessionListener;

        float m_countdown;
        int m_lastSessionCount;
        bool m_onePlayerJoined;
    };
} // namespace LYGame
