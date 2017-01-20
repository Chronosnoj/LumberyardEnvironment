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
#include "GameplaySampleGame.h"
#include "IGameFramework.h"
#include "IGameRulesSystem.h"
#include "GameplaySampleGameRules.h"
#include "IPlatformOS.h"
#include <functional>

using namespace LYGame;

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

#define REGISTER_FACTORY(host, name, impl, isAI) \
    (host)->RegisterFactory((name), (impl*)0, (isAI), (impl*)0)

GameplaySampleGame* LYGame::g_Game = NULL;

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

GameplaySampleGame::GameplaySampleGame()
    : m_clientEntityId(INVALID_ENTITYID)
    , m_gameRules(NULL)
    , m_gameFramework(NULL)
    , m_defaultActionMap(NULL)
    , m_platformInfo()
{
    g_Game = this;
    GetISystem()->SetIGame(this);
}

GameplaySampleGame::~GameplaySampleGame()
{
    m_gameFramework->EndGameContext(false);

    // Remove self as listener.
    m_gameFramework->UnregisterListener(this);
    m_gameFramework->GetILevelSystem()->RemoveListener(this);
    gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);

    g_Game = NULL;
    GetISystem()->SetIGame(NULL);

    ReleaseActionMaps();
}

namespace LYGame
{
    void GameInit();
    void GameShutdown();
}

bool GameplaySampleGame::Init(IGameFramework* framework)
{
    m_gameFramework = framework;

    // Register the actor class so actors can spawn.
    // #TODO If you create a new actor, make sure to register a factory here.
    REGISTER_FACTORY(m_gameFramework, "Actor", Actor, false);

    // Listen to system events, so we know when levels load/unload, etc.
    gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this);
    m_gameFramework->GetILevelSystem()->AddListener(this);

    // Listen for game framework events (level loaded/unloaded, etc.).
    m_gameFramework->RegisterListener(this, "Game", FRAMEWORKLISTENERPRIORITY_GAME);

    // Load actions maps.
    LoadActionMaps("config/input/actionmaps.xml");

    // Register game rules wrapper.
    REGISTER_FACTORY(framework, "GameplaySampleGameRules", GameplaySampleGameRules, false);
    IGameRulesSystem* pGameRulesSystem = g_Game->GetIGameFramework()->GetIGameRulesSystem();
    pGameRulesSystem->RegisterGameRules("DummyRules", "GameplaySampleGameRules");

    GameInit();

    return true;
}

bool GameplaySampleGame::CompleteInit()
{
    return true;
}

int GameplaySampleGame::Update(bool hasFocus, unsigned int updateFlags)
{
    const float frameTime = gEnv->pTimer->GetFrameTime();

    const bool continueRunning = m_gameFramework->PreUpdate(true, updateFlags);
    m_gameFramework->PostUpdate(true, updateFlags);

    return static_cast<int>(continueRunning);
}

void GameplaySampleGame::PlayerIdSet(EntityId playerId)
{
    m_clientEntityId = playerId;
}

void GameplaySampleGame::Shutdown()
{
    GameShutdown();

    this->~GameplaySampleGame();
}

void GameplaySampleGame::LoadActionMaps(const char* fileName)
{
    if (g_Game->GetIGameFramework()->IsGameStarted())
    {
        CryLogAlways("[Profile] Can't change configuration while game is running (yet)");
        return;
    }

    XmlNodeRef rootNode = m_gameFramework->GetISystem()->LoadXmlFromFile(fileName);
    if (rootNode && ReadProfile(rootNode))
    {
        IActionMapManager* actionMapManager = m_gameFramework->GetIActionMapManager();
        actionMapManager->SetLoadFromXMLPath(fileName);
        m_defaultActionMap = actionMapManager->GetActionMap("default");
    }
    else
    {
        CryLogAlways("[Profile] Warning: Could not open configuration file");
    }
}

void GameplaySampleGame::ReleaseActionMaps()
{
    if (m_defaultActionMap)
    {
        IActionMapManager* actionMapManager = m_gameFramework->GetIActionMapManager();
        actionMapManager->RemoveActionMap(m_defaultActionMap->GetName());
        m_defaultActionMap = NULL;
    }
}

bool GameplaySampleGame::ReadProfile(XmlNodeRef& rootNode)
{
    bool successful = false;

    if (IActionMapManager* actionMapManager = m_gameFramework->GetIActionMapManager())
    {
        actionMapManager->Clear();

        // Load platform information in.
        XmlNodeRef platforms = rootNode->findChild("platforms");
        if (!platforms || !ReadProfilePlatform(platforms, GetPlatform()))
        {
            CryLogAlways("[Profile] Warning: No platform information specified!");
        }

        successful = actionMapManager->LoadFromXML(rootNode);
    }

    return successful;
}

bool GameplaySampleGame::ReadProfilePlatform(XmlNodeRef& platformsNode, LYGame::Platform platformId)
{
    // Platform names.
    static char const* s_PlatformNames[ePlatform_Count] =
    {
        "Unknown",
        "PC",
        "Xbox",
        "PS4",
        "Android",
        "iOS"
    };

    bool successful = false;

    if (platformsNode && (platformId > ePlatform_Unknown) && (platformId < ePlatform_Count))
    {
        if (XmlNodeRef platform = platformsNode->findChild(s_PlatformNames[platformId]))
        {
            // Extract which devices we want.
            if (!strcmp(platform->getAttr("keyboard"), "0"))
            {
                m_platformInfo.Devices &= ~eAID_KeyboardMouse;
            }

            if (!strcmp(platform->getAttr("xboxpad"), "0"))
            {
                m_platformInfo.Devices &= ~eAID_XboxPad;
            }

            if (!strcmp(platform->getAttr("ps4pad"), "0"))
            {
                m_platformInfo.Devices &= ~eAID_PS4Pad;
            }

            if (!strcmp(platform->getAttr("androidkey"), "0"))
            {
                m_platformInfo.Devices &= ~eAID_AndroidKey;
            }

            // Map the devices we want.
            IActionMapManager* actionMapManager = m_gameFramework->GetIActionMapManager();

            if (m_platformInfo.Devices & eAID_KeyboardMouse)
            {
                actionMapManager->AddInputDeviceMapping(eAID_KeyboardMouse, "keyboard");
            }

            if (m_platformInfo.Devices & eAID_XboxPad)
            {
                actionMapManager->AddInputDeviceMapping(eAID_XboxPad, "xboxpad");
            }

            if (m_platformInfo.Devices & eAID_PS4Pad)
            {
                actionMapManager->AddInputDeviceMapping(eAID_PS4Pad, "ps4pad");
            }

            if (m_platformInfo.Devices & eAID_AndroidKey)
            {
                actionMapManager->AddInputDeviceMapping(eAID_AndroidKey, "androidkey");
            }

            successful = true;
        }
        else
        {
            GameWarning("GameplaySampleGame::ReadProfilePlatform: Failed to find platform, action mappings loading will fail");
        }
    }

    return successful;
}

LYGame::Platform GameplaySampleGame::GetPlatform() const
{
    LYGame::Platform platform = ePlatform_Unknown;

#if defined(ANDROID)
    platform = ePlatform_Android;
#elif defined(IOS)
    platform = ePlatform_iOS;
#elif defined(WIN32) || defined(WIN64) || defined(DURANGO) || defined(APPLE) || defined(LINUX)
    platform = ePlatform_PC;
#endif

    return platform;
}

// #TODO Use OnSystemEvent to intercept important system events, a few
// common ones have been stubbed out here.
void GameplaySampleGame::OnSystemEvent(ESystemEvent event, UINT_PTR wParam, UINT_PTR lParam)
{
    switch (event)
    {
    case ESYSTEM_EVENT_LEVEL_LOAD_START:
        break;

    case ESYSTEM_EVENT_LEVEL_LOAD_END:
        break;

    case ESYSTEM_EVENT_LEVEL_UNLOAD:
        break;

    case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
        break;

    case ESYSTEM_EVENT_TIME_OF_DAY_SET:
        break;
    }
}

void GameplaySampleGame::OnActionEvent(const SActionEvent& event)
{
    switch (event.m_event)
    {
    case eAE_unloadLevel:
        break;
    }
}

void GameplaySampleGame::GetMemoryStatistics(ICrySizer* sizer)
{
}
