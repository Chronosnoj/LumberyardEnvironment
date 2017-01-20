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
#include "MultiplayerProjectGame.h"
#include "IGameFramework.h"
#include "IGameRulesSystem.h"
#include "MultiplayerProjectGameRules.h"
#include <functional>
#include "Lobby/Lobby.h"
#include "Nodes/GameFlowBaseNode.h"
#include "IPlatformOS.h"

using namespace LYGame;

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

#define REGISTER_FACTORY(host, name, impl, isAI) \
    (host)->RegisterFactory((name), (impl*)0, (isAI), (impl*)0)

MultiplayerProjectGame* LYGame::g_Game = nullptr;

enum eGameObjectRegistrationFlags
{
    eRF_None = 0x0,
    eRF_HiddenInEditor = 0x1,
    eRF_NoEntityClass = 0x2,
    eRF_InstanceProperties = 0x4,
};

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

MultiplayerProjectGame::MultiplayerProjectGame()
    : m_clientEntityId(INVALID_ENTITYID)
    , m_gameRules(nullptr)
    , m_gameFramework(nullptr)
    , m_defaultActionMap(nullptr)
    , m_platformInfo()
    , m_lobby(nullptr)
{
    g_Game = this;
    GetISystem()->SetIGame(this);
}

MultiplayerProjectGame::~MultiplayerProjectGame()
{
    m_gameFramework->EndGameContext(false);

    // Remove self as listener.
    m_gameFramework->UnregisterListener(this);
    m_gameFramework->GetILevelSystem()->RemoveListener(this);
    gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);

    g_Game = nullptr;
    GetISystem()->SetIGame(nullptr);

    ReleaseActionMaps();
}

namespace LYGame
{
    void GameInit();
    void GameShutdown();
}

bool MultiplayerProjectGame::Init(IGameFramework* framework)
{
    m_gameFramework = framework;

    // Register the actor class so actors can spawn.
    REGISTER_FACTORY(m_gameFramework, "Actor", Actor, false);

    // Listen to system events, so we know when levels load/unload, etc.
    gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this);
    m_gameFramework->GetILevelSystem()->AddListener(this);

    // Listen for game framework events (level loaded/unloaded, etc.).
    m_gameFramework->RegisterListener(this, "Game", FRAMEWORKLISTENERPRIORITY_GAME);

    // Load actions maps.
    LoadActionMaps("config/input/actionmaps.xml");

    // Register game rules wrapper.
    REGISTER_FACTORY(framework, "MultiplayerProjectGameRules", MultiplayerProjectGameRules, false);
    IGameRulesSystem* pGameRulesSystem = g_Game->GetIGameFramework()->GetIGameRulesSystem();
    pGameRulesSystem->RegisterGameRules("DummyRules", "MultiplayerProjectGameRules");

    GameInit();

    GetISystem()->GetPlatformOS()->UserDoSignIn(0);

    m_lobby = new Lobby();
    m_lobby->OnInit();

    return true;
}

bool MultiplayerProjectGame::CompleteInit()
{
    return true;
}

int MultiplayerProjectGame::Update(bool hasFocus, unsigned int updateFlags)
{
    const float frameTime = gEnv->pTimer->GetFrameTime();

    const bool continueRunning = m_gameFramework->PreUpdate(true, updateFlags);
    if (m_lobby)
    {
        m_lobby->Update(frameTime);
    }
    m_gameFramework->PostUpdate(true, updateFlags);

    return static_cast<int>(continueRunning);
}

void MultiplayerProjectGame::PlayerIdSet(EntityId playerId)
{
    m_clientEntityId = playerId;
}

void MultiplayerProjectGame::Shutdown()
{
    if (m_lobby)
    {
        m_lobby->OnShutdown();
    }

    GameShutdown();

    this->~MultiplayerProjectGame();
}

void MultiplayerProjectGame::LoadActionMaps(const char* fileName)
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

void MultiplayerProjectGame::ReleaseActionMaps()
{
    if (m_defaultActionMap && m_gameFramework)
    {
        IActionMapManager* actionMapManager = m_gameFramework->GetIActionMapManager();
        actionMapManager->RemoveActionMap(m_defaultActionMap->GetName());
        m_defaultActionMap = nullptr;
    }
}

bool MultiplayerProjectGame::ReadProfile(const XmlNodeRef& rootNode)
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

bool MultiplayerProjectGame::ReadProfilePlatform(const XmlNodeRef& platformsNode, LYGame::Platform platformId)
{
    bool successful = false;

    if (platformsNode && (platformId > ePlatform_Unknown) && (platformId < ePlatform_Count))
    {
        if (XmlNodeRef platform = platformsNode->findChild(s_PlatformNames[platformId]))
        {
            // Extract which Devices we want.
            if (!strcmp(platform->getAttr("keyboard"), "0"))
            {
                m_platformInfo.m_devices &= ~eAID_KeyboardMouse;
            }

            if (!strcmp(platform->getAttr("xboxpad"), "0"))
            {
                m_platformInfo.m_devices &= ~eAID_XboxPad;
            }

            if (!strcmp(platform->getAttr("ps4pad"), "0"))
            {
                m_platformInfo.m_devices &= ~eAID_PS4Pad;
            }

            if (!strcmp(platform->getAttr("androidkey"), "0"))
            {
                m_platformInfo.m_devices &= ~eAID_AndroidKey;
            }

            // Map the Devices we want.
            IActionMapManager* actionMapManager = m_gameFramework->GetIActionMapManager();

            if (m_platformInfo.m_devices & eAID_KeyboardMouse)
            {
                actionMapManager->AddInputDeviceMapping(eAID_KeyboardMouse, "keyboard");
            }

            if (m_platformInfo.m_devices & eAID_XboxPad)
            {
                actionMapManager->AddInputDeviceMapping(eAID_XboxPad, "xboxpad");
            }

            if (m_platformInfo.m_devices & eAID_PS4Pad)
            {
                actionMapManager->AddInputDeviceMapping(eAID_PS4Pad, "ps4pad");
            }

            if (m_platformInfo.m_devices & eAID_AndroidKey)
            {
                actionMapManager->AddInputDeviceMapping(eAID_AndroidKey, "androidkey");
            }

            successful = true;
        }
        else
        {
            GameWarning("MultiplayerProjectGame::ReadProfilePlatform: Failed to find platform, action mappings loading will fail");
        }
    }

    return successful;
}

LYGame::Platform MultiplayerProjectGame::GetPlatform() const
{
    Platform platform = ePlatform_Unknown;

#if defined(ANDROID)
    platform = ePlatform_Android;
#elif defined(IOS)
    platform = ePlatform_iOS;
#elif defined(WIN32) || defined(WIN64) || defined(DURANGO) || defined(APPLE) || defined(LINUX)
    platform = ePlatform_PC;
#endif

    return platform;
}

void MultiplayerProjectGame::OnActionEvent(const SActionEvent& event)
{
    switch (event.m_event)
    {
    case eAE_unloadLevel:
        break;
    }
}

Lobby* MultiplayerProjectGame::GetLobby()
{
    return m_lobby;
}

template<class T>
struct CObjectCreator
    : public IGameObjectExtensionCreatorBase
{
    IGameObjectExtensionPtr Create()
    {
        return gEnv->pEntitySystem->CreateComponent<T>();
    }

    void GetGameObjectExtensionRMIData(void** ppRMI, size_t* nCount)
    {
        T::GetGameObjectExtensionRMIData(ppRMI, nCount);
    }
};

template<class T>
void MultiplayerProjectGame::RegisterGameObject(const string& name, const string& script, uint32 flags)
{
    bool registerClass = ((flags & eRF_NoEntityClass) == 0);
    IEntityClassRegistry::SEntityClassDesc clsDesc;
    clsDesc.sName = name.c_str();
    clsDesc.sScriptFile = script.c_str();

    static CObjectCreator<T> _creator;

    gEnv->pGame->GetIGameFramework()->GetIGameObjectSystem()->RegisterExtension(name.c_str(), &_creator, registerClass ? &clsDesc : nullptr);
    if ((flags & eRF_HiddenInEditor) != 0)
    {
        IEntityClass* pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name.c_str());
        pEntityClass->SetFlags(pEntityClass->GetFlags() | ECLF_INVISIBLE);
    }
}

void MultiplayerProjectGame::RegisterGameFlowNodes()
{
    if (IFlowSystem* flowSystem = gEnv->pGame->GetIGameFramework()->GetIFlowSystem())
    {
        CGameAutoRegFlowNodeBase* flowNodeFactory = CGameAutoRegFlowNodeBase::m_pFirst;
        while (flowNodeFactory)
        {
            flowSystem->RegisterType(flowNodeFactory->m_sClassName, flowNodeFactory);
            flowNodeFactory = flowNodeFactory->m_pNext;
        }
    }
}
