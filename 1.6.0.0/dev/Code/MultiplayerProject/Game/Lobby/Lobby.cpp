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
#include <AzCore/Memory/Memory.h>
#include <chrono>
#include "Lobby.h"
#include "../Game/Core/MultiplayerProjectGame.h"
#include <GridMate/NetworkGridMate.h>
#include <GridMate/Session/Session.h>

namespace LYGame
{
    class LobbySessionListener
        : public GridMate::SessionEventBus::Handler
    {
    public:

        AZ_CLASS_ALLOCATOR(LobbySessionListener, AZ::SystemAllocator, 0);

        LobbySessionListener(Lobby* parent)
            : m_parent(parent)
        {
            GridMate::SessionEventBus::Handler::BusConnect(gEnv->pNetwork->GetGridMate());
        }

        void OnSessionCreated(GridMate::GridSession* session) override
        {
            m_parent->OnSessionCreated(session);
        }

        Lobby* m_parent;
    };
}   // namespace LYGame

namespace LYGame
{
    Lobby::Lobby()
        : m_sessionListener(nullptr)
        , m_countdown(0.0f)
        , m_lastSessionCount(0)
        , m_onePlayerJoined(false)
    {
    }

    void Lobby::OnInit()
    {
        REGISTER_FLOAT("sv_idle_seconds", -1.f, VF_DUMPTODISK, "Sets the number of seconds the server will idle without players connected before shutting down. Negative value means disabled.");

        m_sessionListener = aznew LobbySessionListener(this);

    }

    void Lobby::OnShutdown()
    {
        UNREGISTER_CVAR("sv_idle_seconds");

        delete m_sessionListener;
        m_sessionListener = nullptr;
    }

    void Lobby::OnSessionCreated(GridMate::GridSession* session)
    {
        GridMate::Network* gm = static_cast<GridMate::Network*>(gEnv->pNetwork);
        if (gEnv->bServer && gm->GetCurrentSession() == session)
        {
            // Push any session parameters we may have received back to any matching CVars.
            for (unsigned int i = 0; i < session->GetNumParams(); ++i)
            {
                ICVar* var = gEnv->pConsole->GetCVar(session->GetParam(i).m_id.c_str());
                if (var)
                {
                    var->Set(session->GetParam(i).m_value.c_str());
                }
                else
                {
                    CryLogAlways("Unable to bind session property '%s:%s' to CVar. CVar does not exist.", session->GetParam(i).m_id.c_str(), session->GetParam(i).m_value.c_str());
                }
            }

            // If sv_map is set, load this map.
            const char* map = gEnv->pConsole->GetCVar("sv_map")->GetString();
            if (strcmp(map, "nolevel"))
            {
                string cmd = "map ";
                cmd += map;
                gEnv->pConsole->ExecuteString(cmd.c_str(), false, true);
            }
        }
    }

    void Lobby::Update(float dt)
    {
        if (gEnv->bServer && gEnv->pConsole->GetCVar("sv_idle_seconds")->GetFVal() > 0.f) // if server and disconnect on idle is enabled
        {
            GridMate::Network* gm = static_cast<GridMate::Network*>(gEnv->pNetwork);
            if (gm && gm->GetCurrentSession() && gm->GetCurrentSession()->IsReady())
            {
                int currentCount = gm->GetCurrentSession()->GetNumberOfMembers();

                //Someone has joined!
                if (currentCount > m_lastSessionCount)
                {
                    CryLogAlways("A player has joined.");
                    m_lastSessionCount = currentCount;
                    m_onePlayerJoined = true;
                    //Canceling countdown for shutdown
                    if (m_countdown > 0.0f)
                    {
                        CryLogAlways("Canceling countdown for shutdown.");
                        m_countdown = 0.0f;
                    }
                    return;
                }

                //Checking against 1 here because the server counts itself as 1 member in GridMate.
                if (currentCount <= 1 && m_onePlayerJoined)
                {
                    //At least one player joined and none are left on the server
                    //Incrementing the countdown.
                    m_countdown += dt;
                    float idleTimeSeconds = gEnv->pConsole->GetCVar("sv_idle_seconds")->GetFVal();
                    if (m_countdown > idleTimeSeconds)
                    {
                        CryLogAlways("Countdown reached, terminating the game session.");

                        // leave the GridMate session.  This will disconnect our clients as well.
                        GridMate::GridSession* session = gm->GetCurrentSession();
                        if (session)
                        {
                            session->Leave(false);
                        }

                        m_countdown = 0.0f;
                    }
                }

                if (m_lastSessionCount != currentCount)
                {
                    m_lastSessionCount = currentCount;
                }
            }
        }
    }
} // namespace LYGame
