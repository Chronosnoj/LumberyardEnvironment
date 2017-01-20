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
#include <platform_impl.h>
#include "GameEffectSystemGem.h"
#include <FlowSystem/Nodes/FlowBaseNode.h>

GameEffectSystemGem::GameEffectSystemGem()
    : CryHooksModule()
    , m_gameEffectSystem(nullptr)
    , g_gameFXSystemDebug(0)
{
    GameEffectSystemRequestBus::Handler::BusConnect();
}

GameEffectSystemGem::~GameEffectSystemGem()
{
    GameEffectSystemRequestBus::Handler::BusDisconnect();
}

void GameEffectSystemGem::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
    switch (event)
    {
    case ESYSTEM_EVENT_FLOW_SYSTEM_REGISTER_EXTERNAL_NODES:
        RegisterExternalFlowNodes();
        break;

    case ESYSTEM_EVENT_GAME_POST_INIT:
        // Put your init code here
        // All other Gems will exist at this point
        REGISTER_CVAR(g_gameFXSystemDebug, 0, 0, "Toggles game effects system debug state");

        m_gameEffectSystem = new CGameEffectsSystem();
        m_gameEffectSystem->Initialize();
        m_gameEffectSystem->LoadData();
        break;

    case ESYSTEM_EVENT_FULL_SHUTDOWN:
    case ESYSTEM_EVENT_FAST_SHUTDOWN:
        if (m_gameEffectSystem)
        {
            m_gameEffectSystem->ReleaseData();
            m_gameEffectSystem->Destroy();

            delete m_gameEffectSystem;
            m_gameEffectSystem = nullptr;
        }
        break;
    }
}

IGameEffectSystem* GameEffectSystemGem::GetIGameEffectSystem()
{
    return m_gameEffectSystem;
}

AZ_DECLARE_MODULE_CLASS(GameEffectSystem_d378b5a7b47747d0a7aa741945df58f3, GameEffectSystemGem)
