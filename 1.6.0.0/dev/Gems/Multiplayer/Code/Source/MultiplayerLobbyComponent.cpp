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

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/string/string.h>

#include <GameLift/GameLiftBus.h>
#include <GameLift/Session/GameLiftClientService.h>
#include <GameLift/Session/GameLiftSessionDefs.h>
#include <GameLift/Session/GameLiftSessionRequest.h>

#include <GridMate/Carrier/Driver.h>
#include <GridMate/NetworkGridMate.h>
#include <GridMate/Session/LANSession.h>

#if defined(DURANGO)
#include <GridMate/Session/XBone/XBoneSessionService.h>
#elif defined(ORBIS)
#include <GridMate/Session/PS4/PSNSessionService.h>
#endif

#include <LyShine/Bus/UiButtonBus.h>
#include <LyShine/Bus/UiElementBus.h>
#include <LyShine/Bus/UiInteractableBus.h>
#include <LyShine/Bus/UiTextBus.h>
#include <LyShine/Bus/UiTextInputBus.h>

#include "Multiplayer/MultiplayerLobbyComponent.h"

#include "IHardwareMouse.h"
#include "Multiplayer/IMultiplayerGem.h"
#include "Multiplayer/MultiplayerUtils.h"


namespace Multiplayer
{
    ///////////////////////////
    // ServerListingResultRow
    ///////////////////////////
    
    MultiplayerLobbyComponent::ServerListingResultRow::ServerListingResultRow(const AZ::EntityId& canvas, int row, int text, int highlight)
        : m_canvas(canvas)
        , m_row(row)
        , m_text(text)
        , m_highlight(highlight)
    {
    }
    
    int MultiplayerLobbyComponent::ServerListingResultRow::GetRowID()
    {
        return m_row;
    }
    
    void MultiplayerLobbyComponent::ServerListingResultRow::Select()
    {
        AZ::Entity* element = nullptr;
        EBUS_EVENT_ID_RESULT(element,m_canvas,UiCanvasBus, FindElementById, m_highlight);
        
        if (element != nullptr)
        {
            EBUS_EVENT_ID(element->GetId(), UiElementBus, SetIsEnabled, true);
        }
    }
    
    void MultiplayerLobbyComponent::ServerListingResultRow::Deselect()
    {
        AZ::Entity* element = nullptr;
        EBUS_EVENT_ID_RESULT(element,m_canvas,UiCanvasBus, FindElementById, m_highlight);
        
        if (element != nullptr)
        {
            EBUS_EVENT_ID(element->GetId(), UiElementBus, SetIsEnabled, false);
        }
    }

    void MultiplayerLobbyComponent::ServerListingResultRow::DisplayResult(const GridMate::SearchInfo* searchInfo)
    {
        char displayString[64];

        const char* serverName = nullptr;

        for (unsigned int i=0; i < searchInfo->m_numParams; ++i)
        {
            const GridMate::GridSessionParam& param = searchInfo->m_params[i];

            if (param.m_id == "sv_name")
            {
                serverName = param.m_value.c_str();
                break;
            }
        }

        azsnprintf(displayString,AZ_ARRAY_SIZE(displayString),"%s (%u/%u)",serverName,searchInfo->m_numUsedPublicSlots,searchInfo->m_numFreePublicSlots + searchInfo->m_numUsedPublicSlots);

        AZ::Entity* element = nullptr;
        
        EBUS_EVENT_ID_RESULT(element,m_canvas,UiCanvasBus, FindElementById, m_text);

        if (element != nullptr)
        {            
            LyShine::StringType textString(displayString);
            EBUS_EVENT_ID(element->GetId(),UiTextBus,SetText,textString);
        }
    }
    
    void MultiplayerLobbyComponent::ServerListingResultRow::ResetDisplay()
    {
        SetTitle("");
        Deselect();
    }

    void MultiplayerLobbyComponent::ServerListingResultRow::SetTitle(const char* title)
    {        
        AZ::Entity* element = nullptr;
        EBUS_EVENT_ID_RESULT(element,m_canvas,UiCanvasBus, FindElementById, m_text);
        
        if (element != nullptr)
        {
            LyShine::StringType textString(title);
            EBUS_EVENT_ID(element->GetId(),UiTextBus,SetText,textString);
        }
    }

    //////////////////////////////
    // MultiplayerLobbyComponent
    //////////////////////////////    
    
    // List of widgets we want to specifically references
    
    // Lobby Selection
    static const int k_lobbySelectionLANButton = 8;
    static const int k_lobbySelectionLANText = 9;
    static const int k_lobbySelectionGameliftButton = 12;
    static const int k_lobbySelectionGameliftText = 13;
    static const int k_lobbySelectionErrorWindow = 15;
    static const int k_lobbySelectionErrorMessage = 20;
    static const int k_lobbySelectionBusyScreen = 14;
    static const int k_lobbySelectionGameLiftWindow = 22;

    // Gamelift Config Controls
    static const int k_lobbySelectionGameliftAWSAccesKeyInput = 131;
    static const int k_lobbySelectionGameliftAWSSecretKeyInput = 136;
    static const int k_lobbySelectionGameliftAWSRegionInput = 141;
    static const int k_lobbySelectionGameliftFleetIDInput = 146;
    static const int k_lobbySelectionGameliftEndPointInput = 151;
    static const int k_lobbySelectionGameliftAliasIDInput = 156;
    static const int k_lobbySelectionGameliftPlayerIDInput = 161;
    static const int k_lobbySelectionGameliftConnectButton = 55;
    static const int k_lobbySelectionGameliftCancelButton = 57;
    
    // ServerListing Lobby
    static const int k_serverListingTitle = 20;
    static const int k_serverListingRefreshButton = 2;
    static const int k_serverListingConnectButton = 5;
    static const int k_serverCreateButton = 24;
    static const int k_serverListingServerName = 26;
    static const int k_serverListingMapName = 29;
    static const int k_serverListingErrorWindow = 46;
    static const int k_serverListingErrorMessage = 51;
    static const int k_serverListingBusyScreen = 56;
    static const int k_serverListingJoinButton = 5;

    // XBox overlays
    static const int k_serverListingConnectXBOverlay = 60;
    static const int k_serverListingCreateXBOverlay = 62;
    static const int k_serverListingRefreshXBOverlay = 64;
    static const int k_serverListingReturnXBOverlay = 58;

    // PS4 overlays
    static const int k_serverListingConnectPSOverlay = 59;
    static const int k_serverListingCreatePSOverlay = 61;
    static const int k_serverListingRefreshPSOverlay = 63;
    static const int k_serverListingReturnPSOverlay = 57;
    
    void MultiplayerLobbyComponent::Reflect(AZ::ReflectContext* reflectContext)
    {
        AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(reflectContext);

        if (serialize)
        {
            serialize->Class<MultiplayerLobbyComponent,AZ::Component>()
                ->Version(1)
                ->Field("MaxPlayers",&MultiplayerLobbyComponent::m_maxPlayers)
                ->Field("Port",&MultiplayerLobbyComponent::m_port)
                ->Field("EnableDisconnectDetection",&MultiplayerLobbyComponent::m_enableDisconnectDetection)
                ->Field("ConnectionTimeout",&MultiplayerLobbyComponent::m_connectionTimeoutMS)
                ->Field("DefaultMap",&MultiplayerLobbyComponent::m_defaultMap)
                ->Field("DefaultServer",&MultiplayerLobbyComponent::m_defaultServerName)
            ;

            AZ::EditContext* editContext = serialize->GetEditContext();

            if (editContext)
            {
                editContext->Class<MultiplayerLobbyComponent>("Multiplayer Lobby Component","This component will load up and manage a simple lobby for connecting for LAN and GameLift sessions.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData,"")
                        ->Attribute(AZ::Edit::Attributes::Category,"MultiplayerSample")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand,true)
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MultiplayerLobbyComponent::m_maxPlayers,"Max Players","The total number of players that can join in the game.")
                        ->Attribute(AZ::Edit::Attributes::Min,0)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MultiplayerLobbyComponent::m_port,"Port","The port on which the game service will create connections through.")
                        ->Attribute(AZ::Edit::Attributes::Min,1)
                        ->Attribute(AZ::Edit::Attributes::Max,65534)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MultiplayerLobbyComponent::m_enableDisconnectDetection,"Enable Disconnect Detection","Enables disconnecting players if they do not respond within the Timeout window.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MultiplayerLobbyComponent::m_connectionTimeoutMS,"Timeout","The length of time a client has to respond before being disconnected(if disconnection detection is enabled.")
                        ->Attribute(AZ::Edit::Attributes::Suffix,"ms")
                        ->Attribute(AZ::Edit::Attributes::Min,0)
                        ->Attribute(AZ::Edit::Attributes::Max,60000)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MultiplayerLobbyComponent::m_defaultMap,"DefaultMap", "The default value that will be added to the map field when loading the lobby.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &MultiplayerLobbyComponent::m_defaultServerName,"DefaultServerName","The default value that will be added to the server name field when loading the lobby.")
                ;                
            }
        }
    }
    
    MultiplayerLobbyComponent::MultiplayerLobbyComponent()
        : m_maxPlayers(8)
        , m_port(SERVER_DEFAULT_PORT)
        , m_enableDisconnectDetection(true)
        , m_connectionTimeoutMS(500)
        , m_defaultMap("")
        , m_defaultServerName("MyServer")
        , m_selectionLobbyID()
        , m_serverListingID()
        , m_unregisterGameliftServiceOnErrorDismiss(false)        
        , m_hasGameliftSession(false)
        , m_isShowingBusy(true)
        , m_isShowingError(true)
        , m_isShowingGameLiftConfig(true)
        , m_lobbyMode(LobbyMode::Unknown)
        , m_selectedServerResult(-1)
        , m_joinSearch(nullptr)
        , m_listSearch(nullptr)
        
        , m_gameliftCreationSearch(nullptr)
    {
    }
    
    MultiplayerLobbyComponent::~MultiplayerLobbyComponent()
    {
    }

    void MultiplayerLobbyComponent::Init()
    {
    }
    
    void MultiplayerLobbyComponent::Activate()
    {        
        IGameFramework* pGameFramework = gEnv->pGame ? gEnv->pGame->GetIGameFramework() : nullptr;
        IActionMapManager* actionMapManager = pGameFramework ? pGameFramework->GetIActionMapManager() : nullptr;

        if (actionMapManager)
        {            
            actionMapManager->EnableActionMap("lobby",true);
            actionMapManager->AddExtraActionListener(this, "lobby");
        }
        
        m_selectionLobbyID = LoadCanvas("ui/Canvases/selection_lobby.uicanvas");
        AZ_Error("MultiplayerLobbyComponent",m_selectionLobbyID.IsValid(),"Missing UI file for ServerType Selection Lobby.");
        
        m_serverListingID = LoadCanvas("ui/Canvases/listing_lobby.uicanvas");
        AZ_Error("MultiplayerLobbyComponent",m_serverListingID.IsValid(),"Missing UI file for Server Listing Lobby.");
        
        if (m_serverListingID.IsValid())
        {
            m_listingRows.emplace_back(m_serverListingID,10,11,32);
            m_listingRows.emplace_back(m_serverListingID,12,13,33);
            m_listingRows.emplace_back(m_serverListingID,14,15,34);
            m_listingRows.emplace_back(m_serverListingID,16,17,35);
            m_listingRows.emplace_back(m_serverListingID,18,19,36);
        }
        
#if BUILD_GAMELIFT_CLIENT
        ShowSelectionLobby();
#else
        SetElementInputEnabled(m_selectionLobbyID,k_lobbySelectionGameliftButton,false);
        ShowServerListingLobby(LobbyMode::LANLobby);
#endif

        gEnv->pHardwareMouse->IncrementCounter();

        SetupControllerOverlay();

        if (gEnv->pNetwork)
        {
            GridMate::IGridMate* gridMate = gEnv->pNetwork->GetGridMate();

            if (gridMate)
            {
                GridMate::SessionEventBus::Handler::BusConnect(gridMate);
            }
        }
    }
    
    void MultiplayerLobbyComponent::Deactivate()
    {
        UiCanvasNotificationBus::Handler::BusDisconnect();
        GridMate::SessionEventBus::Handler::BusDisconnect();
        
        if (m_selectionLobbyID.IsValid())
        {
            gEnv->pLyShine->ReleaseCanvas(m_selectionLobbyID, false);
        }
        
        if (m_serverListingID.IsValid())
        {
            gEnv->pLyShine->ReleaseCanvas(m_serverListingID, false);
        }

        IGameFramework* pGameFramework = gEnv->pGame ? gEnv->pGame->GetIGameFramework() : nullptr;
        IActionMapManager* actionMapManager = pGameFramework ? pGameFramework->GetIActionMapManager() : nullptr;

        if (actionMapManager)
        {            
            actionMapManager->EnableActionMap("lobby",false);
            actionMapManager->RemoveExtraActionListener(this, "lobby");
        }

        gEnv->pHardwareMouse->DecrementCounter();
    }
    
    void MultiplayerLobbyComponent::OnAction(AZ::EntityId entityId, const LyShine::ActionName& actionName)
    {
        // Enforcing modality here, because there is currently not a good way to do modality in the UI System.
        // Once that gets entered, most of this should be condensed down for just a safety measure.
        if (m_isShowingBusy)
        {
            return;
        }
        else if (m_isShowingError)
        {
            if (actionName == "OnDismissErrorMessage")
            {
                if (m_unregisterGameliftServiceOnErrorDismiss)
                {
                    m_unregisterGameliftServiceOnErrorDismiss = false;

#if BUILD_GAMELIFT_CLIENT
                    StopGameLiftSession();
#endif
                }

                DismissError();
            }

            return;
        }
        else if (m_isShowingGameLiftConfig)
        {
            if (actionName == "OnGameliftConnect")
            {
                SaveGameLiftConfig();
                DismissGameLiftConfig();

#if BUILD_GAMELIFT_CLIENT
                ShowServerListingLobby(LobbyMode::GameliftLobby);
#else
                AZ_Assert(false,"Trying to use GameLift on unsupported Platform.");
#endif
            }
            else if (actionName == "OnGameliftCancel")
            {
                DismissGameLiftConfig();
            }

            return;
        }

        if (actionName == "OnCreateServer")
        {
            CreateServer();
        }
        else if (actionName == "OnSelectServer")
        {
            LyShine::ElementId elementId = 0;
            EBUS_EVENT_ID_RESULT(elementId, entityId, UiElementBus, GetElementId);

            SelectId(elementId);
        }
        else if (actionName == "OnJoinServer")
        {            
            JoinServer();
        }
        else if (actionName == "OnRefresh")
        {
            ListServers();
        }
        else if (actionName == "OnListGameliftServers")
        {
            ShowGameLiftConfig();
        }
        else if (actionName == "OnListLANServers")
        {
            ShowServerListingLobby(LobbyMode::LANLobby);
        }
        else if (actionName == "OnDismissErrorMessage")
        {
            
        }
        else if (actionName == "OnReturn")
        {
            ShowSelectionLobby();
        }
    }

    void MultiplayerLobbyComponent::OnAction(const ActionId& action, int activationMode, float value)
    {
        if (m_isShowingBusy)
        {
            return;
        }

        if (m_lobbyMode == LobbyMode::LobbySelection)
        {
            OnLobbySelectionAction(action,activationMode,value);
        }
        else
        {
            OnServerListingAction(action, activationMode, value);
        }
    }

    void MultiplayerLobbyComponent::OnSessionCreated(GridMate::GridSession* session)
    {
        GridMate::GridSession* gridSession = nullptr;
        EBUS_EVENT_RESULT(gridSession,Multiplayer::MultiplayerRequestBus,GetSession);

        if (gridSession == session && session->IsHost())
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

            ICVar* mapVar = gEnv->pConsole->GetCVar("sv_map");

            if (mapVar)
            {
                const char* mapName = mapVar->GetString();

                // If we have an actual level to load, load it.
                if (strcmp(mapName,"nolevel") != 0)
                {
                    AZStd::string loadCommand = "map ";
                    loadCommand += mapName;
                    gEnv->pConsole->ExecuteString(loadCommand.c_str(),false,true);
                }
            }
        }
    }

    void MultiplayerLobbyComponent::OnSessionError(GridMate::GridSession* session,const GridMate::string& errorMsg)
    {
        DismissBusyScreen();
        ShowError(errorMsg.c_str());
    }
    
    void MultiplayerLobbyComponent::OnGridSearchComplete(GridMate::GridSearch* search)
    {
        if (search == m_gameliftCreationSearch)
        {            
            GridMate::Network* network = static_cast<GridMate::Network*>(gEnv->pNetwork);
            
            if (network)
            {                
                if (search->GetNumResults() == 0)
                {
                    ShowError("Error creating GameLift Session");
                }                
            }
            
            m_gameliftCreationSearch->Release();
            m_gameliftCreationSearch = nullptr;            
        }
        else if (search == m_listSearch)
        {
            for (unsigned int i=0; i < search->GetNumResults(); ++i)
            {
                // Screen is currently not dynamically populated, so we are stuck with a fixed size
                // amount of results for now.
                if (i >= m_listingRows.size())
                {
                    break;
                }

                const GridMate::SearchInfo* searchInfo = search->GetResult(i);

                ServerListingResultRow& resultRow = m_listingRows[i];

                resultRow.DisplayResult(searchInfo);
            }            
         
            DismissBusyScreen();            
        }       
    }    
    
    void MultiplayerLobbyComponent::ShowSelectionLobby()
    {
        const bool forceHide = true;

        if (m_lobbyMode != LobbyMode::LobbySelection)
        {
            ClearSearches();
            UnregisterService();            

            HideLobby();
            m_lobbyMode = LobbyMode::LobbySelection;
            UiCanvasNotificationBus::Handler::BusConnect(m_selectionLobbyID);            

            EBUS_EVENT_ID(m_selectionLobbyID, UiCanvasBus, SetEnabled, true);

            DismissGameLiftConfig();
            DismissError(forceHide);
            DismissBusyScreen(forceHide);
        }
    }
    
    void MultiplayerLobbyComponent::ShowServerListingLobby(LobbyMode lobbyMode)
    {
        if (lobbyMode == LobbyMode::LobbySelection)
        {
            ShowSelectionLobby();
        }
        else if (m_lobbyMode == LobbyMode::LobbySelection)
        {
            bool showLobby = true;
            if (lobbyMode == LobbyMode::GameliftLobby)
            {
#if BUILD_GAMELIFT_CLIENT
                showLobby = StartGameLiftSession();
#else
                showLobby = false;
                AZ_Assert(false,"Trying to use GameLift on unsupported Platform");
#endif
            }
            else
            {
                StartSessionService(gEnv->pNetwork->GetGridMate());
            }

            if (showLobby)
            {
                HideLobby();
                m_lobbyMode = lobbyMode;
                UiCanvasNotificationBus::Handler::BusConnect(m_serverListingID);                

                ClearServerListings();            
                EBUS_EVENT_ID(m_serverListingID, UiCanvasBus, SetEnabled, true);            

                const bool forceHide = true;
                DismissError(forceHide);
                DismissBusyScreen(forceHide);

                SetupServerListingDisplay();
            }
        }
    }
    
    void MultiplayerLobbyComponent::HideLobby()
    {
        UiCanvasNotificationBus::Handler::BusDisconnect();
        m_lobbyMode = LobbyMode::Unknown;

        EBUS_EVENT_ID(m_selectionLobbyID, UiCanvasBus, SetEnabled, false);
        EBUS_EVENT_ID(m_serverListingID, UiCanvasBus, SetEnabled, false);
    }
    
    void MultiplayerLobbyComponent::ListServers()
    {
        ClearServerListings();        
        
        if (SanityCheck())
        {
            if (m_lobbyMode == LobbyMode::GameliftLobby)
            {
#if BUILD_GAMELIFT_CLIENT
                if (SanityCheckGameLift())
                {
                    ListGameLiftServers();
                }
#else
                AZ_Assert(false,"Trying to use Gamelift lobby on unsupported platform.")
#endif
            }
            else if (m_lobbyMode == LobbyMode::LANLobby)
            {
                if (SanityCheckLAN())
                {
                    ListLANServers();
                }
            }
        }
    }
    
    void MultiplayerLobbyComponent::ClearServerListings()
    {
        m_selectedServerResult = -1;

        for (ServerListingResultRow& serverResultRow : m_listingRows)
        {
            serverResultRow.ResetDisplay();
        }

        SetElementInputEnabled(m_serverListingID,k_serverListingJoinButton,false);        
    }    
    
    void MultiplayerLobbyComponent::SelectId(int rowId)
    {
        if (m_listSearch == nullptr || !m_listSearch->IsDone())
        {
            return;
        }

        SetElementInputEnabled(m_serverListingID,k_serverListingJoinButton,false);
        int lastSelection = m_selectedServerResult;
        
        m_selectedServerResult = -1;        
        for (int i=0; i < static_cast<int>(m_listingRows.size()); ++i)        
        {
            ServerListingResultRow& resultRow = m_listingRows[i];
            
            if (resultRow.GetRowID() == rowId)
            {
                if (i < m_listSearch->GetNumResults())
                {
                    SetElementInputEnabled(m_serverListingID,k_serverListingJoinButton,true);
                    m_selectedServerResult = i;
                    resultRow.Select();
                }
                else
                {
                    resultRow.Deselect();
                }
            }
            else
            {
                resultRow.Deselect();
            }
        }
        
        // Double click to join.
        if (m_selectedServerResult >= 0 && lastSelection == m_selectedServerResult)
        {
            JoinServer();
        }
    }
    
    void MultiplayerLobbyComponent::JoinServer()
    {
        if (m_lobbyMode == LobbyMode::LobbySelection)
        {
            return;
        }
        
        if (   m_listSearch == nullptr
            || !m_listSearch->IsDone()
            || m_selectedServerResult < 0
            || m_listSearch->GetNumResults() <= m_selectedServerResult)
        {
            ShowError("No Server Selected to Join.");
            return;
        }
        
        const GridMate::SearchInfo* searchInfo = m_listSearch->GetResult(m_selectedServerResult);                

        if (!SanityCheck())
        {
            return;
        }
        else if (searchInfo == nullptr)
        {
            ShowError("Invalid Server Selection.");
            return;
        }        
        else if (m_lobbyMode == LobbyMode::LANLobby) 
        {
            if (!SanityCheckLAN())
            {
                return;
            }
            else
            {
                StartSessionService(gEnv->pNetwork->GetGridMate());
            }
        }
        else if (m_lobbyMode == LobbyMode::GameliftLobby)
        {
#if BUILD_GAMELIFT_CLIENT
            if (!SanityCheckGameLift())
            {
                return;
            }
#else
            AZ_Assert(false,"Trying to use Gamelift lobby on unsupported platform.")
            return;
#endif
        }

        ShowBusyScreen();
        
        if (!JoinSession(searchInfo))
        {
            DismissBusyScreen();
            ShowError("Found a game session, but failed to join.");
        }
    }
    
    void MultiplayerLobbyComponent::CreateServer()
    {
        if (m_lobbyMode == LobbyMode::LobbySelection)
        {
            return;
        }
        else if (SanityCheck())
        {
            if (GetMapName().empty())
            {
                ShowError("Invalid Map Name");
            }
            else if (GetServerName().empty())
            {
                ShowError("Invalid Server Name");
            }
            else if (m_lobbyMode == LobbyMode::GameliftLobby)
            {
#if BUILD_GAMELIFT_CLIENT
                if (SanityCheckGameLift())
                {
                    CreateGameLiftServer();
                }
#else
                AZ_Assert(false,"Trying to use Gamelift on unsupported platform.");                
#endif
            }
            else if (m_lobbyMode == LobbyMode::LANLobby)
            {
                if (SanityCheckLAN())
                {
                    CreateLANServer();
                }
            }
        }
    }

    void MultiplayerLobbyComponent::UnregisterService()
    {
        // Stop whatever session we may have been using, if any
        if (m_lobbyMode == LobbyMode::LANLobby)
        {
            if (gEnv->pNetwork && gEnv->pNetwork->GetGridMate())
            {
                GridMate::IGridMate* gridMate = gEnv->pNetwork->GetGridMate();

#if defined(DURANGO)
                GridMate::StopGridMateService<GridMate::XBoneSessionService>(gridMate);                
#elif defined(ORBIS)
                GridMate::StopGridMateService<GridMate::PSNSessionService>(gridMate);
#else
                GridMate::StopGridMateService<GridMate::LANSessionService>(gridMate);
#endif
            }
        }
        else if (m_lobbyMode == LobbyMode::GameliftLobby)
        {
#if BUILD_GAMELIFT_CLIENT
            StopGameLiftSession();
            m_hasGameliftSession = false;
#else
            AZ_Assert(false,"Trying to use Gamelift on Unsupported platform.");
#endif
        }
    }

    void MultiplayerLobbyComponent::ClearSearches()
    {
        if (m_listSearch)
        {
            if (!m_listSearch->IsDone())
            {
                m_listSearch->AbortSearch();
            }

            m_listSearch->Release();
            m_listSearch = nullptr;
        }
            
        if (m_gameliftCreationSearch)
        {
            if (!m_gameliftCreationSearch->IsDone())
            {
                m_gameliftCreationSearch->AbortSearch();
            }

            m_gameliftCreationSearch->Release();
            m_gameliftCreationSearch = nullptr;
        }
    }    

    void MultiplayerLobbyComponent::SetupControllerOverlay()
    {
        bool isPS4 = false;
        bool isXBox = false;

        SetElementEnabled(m_serverListingID,k_serverListingConnectPSOverlay,isPS4);
        SetElementEnabled(m_serverListingID,k_serverListingCreatePSOverlay,isPS4);
        SetElementEnabled(m_serverListingID,k_serverListingRefreshPSOverlay,isPS4);
        SetElementEnabled(m_serverListingID,k_serverListingReturnPSOverlay,isPS4);

        SetElementEnabled(m_serverListingID,k_serverListingConnectXBOverlay,isXBox);
        SetElementEnabled(m_serverListingID,k_serverListingCreateXBOverlay,isXBox);
        SetElementEnabled(m_serverListingID,k_serverListingRefreshXBOverlay,isXBox);
        SetElementEnabled(m_serverListingID,k_serverListingReturnXBOverlay,isXBox);
    }

    void MultiplayerLobbyComponent::SetElementEnabled(const AZ::EntityId& canvasID, int elementId, bool enabled)
    {
        AZ::Entity* element = nullptr;
        EBUS_EVENT_ID_RESULT(element,canvasID,UiCanvasBus, FindElementById, elementId);
        
        if (element != nullptr)
        {        
            EBUS_EVENT_ID(element->GetId(),UiElementBus,SetIsEnabled,enabled);
        }
    }

    void MultiplayerLobbyComponent::SetElementInputEnabled(const AZ::EntityId& canvasID, int elementId, bool enabled)
    {
        AZ::Entity* element = nullptr;
        EBUS_EVENT_ID_RESULT(element,canvasID,UiCanvasBus, FindElementById, elementId);

        if (element != nullptr)
        {
            EBUS_EVENT_ID(element->GetId(),UiInteractableBus,SetIsHandlingEvents,enabled);
        }
    }

    void MultiplayerLobbyComponent::SetElementText(const AZ::EntityId& canvasID, int elementId, const char* text)
    {
        AZ::Entity* element = nullptr;
        EBUS_EVENT_ID_RESULT(element,canvasID,UiCanvasBus, FindElementById, elementId);

        if (element != nullptr)
        {
            if (UiTextInputBus::FindFirstHandler(element->GetId()))
            {
                EBUS_EVENT_ID(element->GetId(),UiTextInputBus,SetText,text);
            }
            else if (UiTextBus::FindFirstHandler(element->GetId()))
            {
                EBUS_EVENT_ID(element->GetId(),UiTextBus,SetText,text);
            }
        }
    }        

    LyShine::StringType MultiplayerLobbyComponent::GetElementText(const AZ::EntityId& canvasID, int elementId) const
    {
        LyShine::StringType retVal;
        AZ::Entity* element = nullptr;
        EBUS_EVENT_ID_RESULT(element,canvasID,UiCanvasBus, FindElementById, elementId);

        if (element != nullptr)
        {
            if (UiTextInputBus::FindFirstHandler(element->GetId()))
            {
                EBUS_EVENT_ID_RESULT(retVal,element->GetId(),UiTextInputBus,GetText);
            }
            else if (UiTextBus::FindFirstHandler(element->GetId()))
            {
                EBUS_EVENT_ID_RESULT(retVal,element->GetId(),UiTextBus,GetText);                
            }
        }

        return retVal;
    }

    void MultiplayerLobbyComponent::StartSessionService(GridMate::IGridMate* gridMate)
    {        

#if defined(DURANGO)
        if (!GridMate::HasGridMateService<GridMate::XBoneSessionService>(gridMate))
        {
            Multiplayer::Durango::StartSessionService(gridMate);
        }
#elif defined(ORBIS)
        if (!GridMate::HasGridMateService<GridMate::PSNSessionService>(gridMate))
        {
            Multiplayer::Orbis::StartSessionService(gridMate);
        }
#else
        if (!GridMate::HasGridMateService<GridMate::LANSessionService>(gridMate))        
        {
            Multiplayer::LAN::StartSessionService(gridMate);
        }
#endif
    }

    bool MultiplayerLobbyComponent::JoinSession(const GridMate::SearchInfo* searchInfo)
    {
        GridMate::GridSession* session = nullptr;
        GridMate::CarrierDesc carrierDesc;

        Multiplayer::Utils::InitCarrierDesc(carrierDesc);
        Multiplayer::NetSec::ConfigureCarrierDescForJoin(carrierDesc);

        GridMate::JoinParams joinParams;

#if defined(DURANGO)
        EBUS_EVENT_ID_RESULT(session,gEnv->pNetwork->GetGridMate(),GridMate::XBoneSessionServiceBus,JoinSessionBySearchInfo,static_cast<const GridMate::XBoneSearchInfo&>(*searchInfo),joinParams,carrierDesc);
#elif defined(ORBIS)
        EBUS_EVENT_ID_RESULT(session,gEnv->pNetwork->GetGridMate(),GridMate::PSNSessionServiceBus,JoinSessionBySearchInfo,static_cast<const GridMate::PSNSearchInfo&>(*searchInfo),joinParams,carrierDesc);
#else
        const GridMate::LANSearchInfo& lanSearchInfo = static_cast<const GridMate::LANSearchInfo&>(*searchInfo);
        EBUS_EVENT_ID_RESULT(session,gEnv->pNetwork->GetGridMate(),GridMate::LANSessionServiceBus,JoinSessionBySearchInfo,lanSearchInfo,joinParams,carrierDesc);
#endif

        if (session != nullptr)
        {
            EBUS_EVENT(Multiplayer::MultiplayerRequestBus,RegisterSession,session);
        }

        return session != nullptr;
    }

    void MultiplayerLobbyComponent::OnLobbySelectionAction(const ActionId& action, int activationMode, float value)
    {
    
    }

    void MultiplayerLobbyComponent::OnServerListingAction(const ActionId& action, int activationMode, float value)
    {

    }
    
    AZ::EntityId MultiplayerLobbyComponent::LoadCanvas(const char* canvasName) const
    {
        AZ::EntityId canvasEntityId = gEnv->pLyShine->LoadCanvas(canvasName);
        
        return canvasEntityId;
    }
    
    void MultiplayerLobbyComponent::SetupServerListingDisplay()
    {
        AZ::Entity* element = nullptr;
        
        EBUS_EVENT_ID_RESULT(element,m_serverListingID,UiCanvasBus,FindElementById,k_serverListingTitle);        
        
        LyShine::StringType textString;
            
        if (m_lobbyMode == LobbyMode::GameliftLobby)
        {
            textString = "GameLift Servers";
        }
        else if (m_lobbyMode == LobbyMode::LANLobby)
        {
            textString = "LAN Servers";
        }
        else
        {
            textString = "Unknown";
        }
            
        EBUS_EVENT_ID(element->GetId(),UiTextBus,SetText,textString);

        element = nullptr;

        EBUS_EVENT_ID_RESULT(element,m_serverListingID,UiCanvasBus,FindElementById,k_serverListingMapName);

        if (element)
        {
            textString = m_defaultMap.c_str();

            EBUS_EVENT_ID(element->GetId(),UiTextInputBus,SetText,textString);
        }

        element = nullptr;

        EBUS_EVENT_ID_RESULT(element,m_serverListingID,UiCanvasBus,FindElementById,k_serverListingServerName);

        if (element)
        {
            textString = m_defaultServerName.c_str();

            EBUS_EVENT_ID(element->GetId(),UiTextInputBus,SetText,textString);
        }
    }

    LyShine::StringType MultiplayerLobbyComponent::GetServerName() const
    {
        return GetElementText(m_serverListingID,k_serverListingServerName);
    }

    LyShine::StringType MultiplayerLobbyComponent::GetMapName() const
    {
        return GetElementText(m_serverListingID,k_serverListingMapName);        
    }
    
    bool MultiplayerLobbyComponent::SanityCheckLAN()
    {
        return true;
    }    
    
    void MultiplayerLobbyComponent::CreateLANServer()
    {
        GridMate::GridSession* gridSession = nullptr;
        EBUS_EVENT_RESULT(gridSession,Multiplayer::MultiplayerRequestBus,GetSession);
        
        GridMate::IGridMate* gridMate = gEnv->pNetwork->GetGridMate();
        
        if (!gridSession)
        {
            StartSessionService(gridMate);            
            
            GridMate::CarrierDesc carrierDesc;

            Multiplayer::Utils::InitCarrierDesc(carrierDesc);
            Multiplayer::NetSec::ConfigureCarrierDescForHost(carrierDesc);            

#if defined(DURANGO)
            GridMate::XBoneSessionParams sp;
            sp.m_templateName = gEnv->pConsole->GetCVar("gm_durango_session_template")->GetString();
            sp.m_matchingHopperName = gEnv->pConsole->GetCVar("gm_durango_match_hopper")->GetString();
            EBUS_EVENT_ID_RESULT(sp.m_localUser, gridMate, GridMate::XBoneUserServiceBus, GetUser, 0);
            AZ_Assert(sp.m_localUser, "Can't find valid xbox user in slot 0!");
            
            carrierDesc.m_securityData = gEnv->pConsole->GetCVar("gm_securityData")->GetString();
            carrierDesc.m_familyType = GridMate::Driver::BSD_AF_INET6;
#elif defined(ORBIS)
            GridMate::PSNSessionParams sp;
            EBUS_EVENT_ID_RESULT(sp.m_localMember, gridMate, GridMate::PS4UserServiceBus, GetUser, 0);
            AZ_Assert(sp.m_localMember, "Can't find valid xbox user in slot 0!");
            
            carrierDesc.m_familyType = GridMate::Driver::BSD_AF_INET;                    
#else
            // Attempt to start a hosted LAN session. If we do, we're now a server and in multiplayer mode.
            GridMate::LANSessionParams sp;
            sp.m_port = m_port + 1; // Listen for searches on port + 1
#endif

            /////
            // Common Session configuration
            sp.m_topology = GridMate::ST_CLIENT_SERVER;
            sp.m_numPublicSlots = m_maxPlayers + (gEnv->IsDedicated() ? 1 : 0); // One slot for server member.
            sp.m_numPrivateSlots = 0;
            sp.m_peerToPeerTimeout = 60000;
            sp.m_flags = 0;
            /////
            
            /////
            // Common Carrier configuration                    
            carrierDesc.m_port = m_port;
            carrierDesc.m_enableDisconnectDetection = m_enableDisconnectDetection;
            carrierDesc.m_connectionTimeoutMS = m_connectionTimeoutMS;
            carrierDesc.m_threadUpdateTimeMS = 30;
            /////
            
            sp.m_numParams = 0;
            sp.m_params[sp.m_numParams].m_id = "sv_name";
            sp.m_params[sp.m_numParams].SetValue(GetServerName().c_str());
            sp.m_numParams++;
            
            sp.m_params[sp.m_numParams].m_id = "sv_map";
            sp.m_params[sp.m_numParams].SetValue(GetMapName().c_str());
            sp.m_numParams++;

            ShowBusyScreen();

            GridMate::GridSession* session = nullptr;

#if defined(DURANGO)
            EBUS_EVENT_ID_RESULT(session,gridMate,GridMate::XBoneSessionServiceBus,HostSession,sp,carrierDesc);
#elif defined(ORBIS)
            EBUS_EVENT_ID_RESULT(session,gridMate,GridMate::PSNSessionServiceBus,HostSession,sp,carrierDesc);
#else
            EBUS_EVENT_ID_RESULT(session,gridMate,GridMate::LANSessionServiceBus,HostSession,sp,carrierDesc);
#endif
            
            if (session == nullptr)            
            {
                DismissBusyScreen();
                ShowError("Error while hosting Session.");
            }
            else
            {
                EBUS_EVENT(Multiplayer::MultiplayerRequestBus,RegisterSession,session);
            }
        }            
        else
        {
            ShowError("Invalid Gem Session");
        }
    }
    
    void MultiplayerLobbyComponent::ListLANServers()
    {
        GridMate::IGridMate* gridMate = gEnv->pNetwork->GetGridMate();

        if (gridMate)
        {
            ShowBusyScreen();
            StartSessionService(gridMate);            
            
            if (m_listSearch)
            {
                m_listSearch->AbortSearch();
                m_listSearch->Release();
                m_listSearch = nullptr;
            }            
            
#if defined(DURANGO)
            GridMate::XBoneSearchParams searchParams;
            searchParams.m_templateName = gEnv->pConsole->GetCVar("gm_durango_session_template")->GetString();
            searchParams.m_matchingHopperName = gEnv->pConsole->GetCVar("gm_durango_match_hopper")->GetString();
            searchParams.m_searchTicketTimeout = 30;
            searchParams.m_numParams = 0;
            EBUS_EVENT_ID_RESULT(searchParams.m_localUser, gridMate, GridMate::XBoneUserServiceBus, GetUser, 0);
            AZ_Assert(searchParams.m_localUser, "Can't find valid xbox user in slot 0!");

            EBUS_EVENT_ID_RESULT(m_listSearch, gridMate, GridMate::XBoneSessionServiceBus, StartGridSearch, searchParams);
#elif defined(ORBIS)
            GridMate::PSNSearchParams searchParams;
            EBUS_EVENT_ID_RESULT(searchParams.m_localMember, gridMate, GridMate::PS4UserServiceBus, GetUser, 0);
            AZ_Assert(searchParams.m_localMember, "Can't find valid PSN user in slot 0!");

            EBUS_EVENT_ID_RESULT(m_listSearch, gridMate, GridMate::PSNSessionServiceBus, StartGridSearch, searchParams);
#else
            GridMate::LANSearchParams searchParams;
            searchParams.m_serverPort = m_port + 1;
            searchParams.m_listenPort = 0;

            EBUS_EVENT_ID_RESULT(m_listSearch, gridMate, GridMate::LANSessionServiceBus, StartGridSearch, searchParams);
#endif            
            
            if (m_listSearch == nullptr)
            {
                DismissBusyScreen();
                ShowError("ListServers failed to start a GridSearch.");
            }
        }
        else
        {
            ShowError("Missing Online Service.");
        }
    }
    
    const char* MultiplayerLobbyComponent::GetGameLiftParam(const char* param)
    {
        ICVar* cvar = gEnv->pConsole->GetCVar(param);

        if (cvar)
        {
            return cvar->GetString();
        }

        return nullptr;        
    }

    void MultiplayerLobbyComponent::SetGameLiftParam(const char* param, const char* value)
    {
        ICVar* cvar = gEnv->pConsole->GetCVar(param);

        if (cvar)
        {
            cvar->Set(value);            
        }        
    }

    void MultiplayerLobbyComponent::ShowGameLiftConfig()
    {
        if (!m_isShowingGameLiftConfig)
        {
            m_isShowingGameLiftConfig = true;

            SetElementEnabled(m_selectionLobbyID,k_lobbySelectionGameLiftWindow,true);
            SetElementText(m_selectionLobbyID,k_lobbySelectionGameliftAWSAccesKeyInput,GetGameLiftParam("gamelift_aws_access_key"));
            SetElementText(m_selectionLobbyID,k_lobbySelectionGameliftAWSSecretKeyInput,GetGameLiftParam("gamelift_aws_secret_key"));
            SetElementText(m_selectionLobbyID,k_lobbySelectionGameliftAWSRegionInput,GetGameLiftParam("gamelift_aws_region"));
            SetElementText(m_selectionLobbyID,k_lobbySelectionGameliftFleetIDInput,GetGameLiftParam("gamelift_fleet_id"));
            SetElementText(m_selectionLobbyID,k_lobbySelectionGameliftEndPointInput,GetGameLiftParam("gamelift_endpoint"));
            SetElementText(m_selectionLobbyID,k_lobbySelectionGameliftAliasIDInput,GetGameLiftParam("gamelift_alias_id"));
            SetElementText(m_selectionLobbyID,k_lobbySelectionGameliftPlayerIDInput,GetGameLiftParam("gamelift_player_id"));
        }
    }

    void MultiplayerLobbyComponent::DismissGameLiftConfig()
    {
        if (m_isShowingGameLiftConfig)
        {
            m_isShowingGameLiftConfig = false;

            SetElementEnabled(m_selectionLobbyID,k_lobbySelectionGameLiftWindow,false);
        }
    }

    void MultiplayerLobbyComponent::SaveGameLiftConfig()
    {
        LyShine::StringType param;
        
        param = GetElementText(m_selectionLobbyID,k_lobbySelectionGameliftAWSAccesKeyInput);
        SetGameLiftParam("gamelift_aws_access_key",param.c_str());

        param = GetElementText(m_selectionLobbyID,k_lobbySelectionGameliftAWSSecretKeyInput);
        SetGameLiftParam("gamelift_aws_secret_key",param.c_str());

        param = GetElementText(m_selectionLobbyID,k_lobbySelectionGameliftAWSRegionInput);
        SetGameLiftParam("gamelift_aws_region",param.c_str());

        param = GetElementText(m_selectionLobbyID,k_lobbySelectionGameliftFleetIDInput);
        SetGameLiftParam("gamelift_fleet_id",param.c_str());

        param = GetElementText(m_selectionLobbyID,k_lobbySelectionGameliftEndPointInput);
        SetGameLiftParam("gamelift_endpoint",param.c_str());

        param = GetElementText(m_selectionLobbyID,k_lobbySelectionGameliftAliasIDInput);
        SetGameLiftParam("gamelift_alias_id",param.c_str());

        param = GetElementText(m_selectionLobbyID,k_lobbySelectionGameliftPlayerIDInput);
        SetGameLiftParam("gamelift_player_id",param.c_str());        
    }

#if BUILD_GAMELIFT_CLIENT
    
    bool MultiplayerLobbyComponent::SanityCheckGameLift()
    {
        GridMate::IGridMate* gridMate = gEnv->pNetwork->GetGridMate();

        // This should already be errored by the previous sanity check.
        if (gridMate == nullptr)
        {
            return false;
        }
        else if (!GridMate::HasGridMateService<GridMate::GameLiftClientService>(gridMate))
        {
            ShowError("MultiplayerService is missing.");
            return false;
        }
        
        return true;
    }   

    bool MultiplayerLobbyComponent::StartGameLiftSession()
    {
        GridMate::IGridMate* gridMate = gEnv->pNetwork->GetGridMate();

        // Not sure what happens if we start this once and it fails to be created...
        // calling it again causes an assert.
        if (gridMate && !m_hasGameliftSession)
        {
            ShowBusyScreen();

            GridMate::GameLiftClientServiceEventsBus::Handler::BusConnect(gridMate);

            GridMate::GameLiftClientServiceDesc serviceDesc;
            serviceDesc.m_accessKey = GetGameLiftParam("gamelift_aws_access_key");
            serviceDesc.m_secretKey = GetGameLiftParam("gamelift_aws_secret_key");
            serviceDesc.m_fleetId = GetGameLiftParam("gamelift_fleet_id");
            serviceDesc.m_endpoint = GetGameLiftParam("gamelift_endpoint");
            serviceDesc.m_region = GetGameLiftParam("gamelift_aws_region");
            serviceDesc.m_aliasId = GetGameLiftParam("gamelift_alias_id");
            serviceDesc.m_playerId = GetGameLiftParam("gamelift_player_id");
            EBUS_EVENT(GameLift::GameLiftRequestBus, StartClientService, serviceDesc);
        }

        return m_hasGameliftSession;
    }

    void MultiplayerLobbyComponent::StopGameLiftSession()
    {        
        EBUS_EVENT(GameLift::GameLiftRequestBus, StopClientService);
    }
    
    void MultiplayerLobbyComponent::CreateGameLiftServer()
    {        
        GridMate::IGridMate* gridMate = gEnv->pNetwork->GetGridMate();        
        
        if (m_gameliftCreationSearch)
        {
            m_gameliftCreationSearch->AbortSearch();
            m_gameliftCreationSearch->Release();
            m_gameliftCreationSearch = nullptr;
        }        
        
        // Request a new Gamelift Game
        GridMate::GameLiftSessionRequestParams reqParams;
        reqParams.m_instanceName = GetServerName().c_str();
        reqParams.m_numPublicSlots = m_maxPlayers;
        reqParams.m_numParams = 0;
        reqParams.m_params[reqParams.m_numParams].m_id = "sv_name";
        reqParams.m_params[reqParams.m_numParams].m_value = GetServerName().c_str();
        reqParams.m_numParams++;
        reqParams.m_params[reqParams.m_numParams].m_id = "sv_map";
        reqParams.m_params[reqParams.m_numParams].m_value = GetMapName().c_str();
        reqParams.m_numParams++;

        ShowBusyScreen();
        
        EBUS_EVENT_ID_RESULT(m_gameliftCreationSearch, gridMate, GridMate::GameLiftClientServiceBus, RequestSession, reqParams);
    }
    
    void MultiplayerLobbyComponent::ListGameLiftServers()
    {
        ShowBusyScreen();
        GridMate::Network* network = static_cast<GridMate::Network*>(gEnv->pNetwork);
        GridMate::IGridMate* gridMate = network->GetGridMate();
        
        if (m_listSearch)
        {
            m_listSearch->AbortSearch();
            m_listSearch->Release();
            m_listSearch = nullptr;
        }

        EBUS_EVENT_ID_RESULT(m_listSearch,gridMate,GridMate::GameLiftClientServiceBus, StartSearch, GridMate::GameLiftSearchParams());
            
        if (m_listSearch == nullptr)
        {        
            DismissBusyScreen();
            ShowError("Failed to start a GridSearch");
        }
    }    

    void MultiplayerLobbyComponent::OnGameLiftSessionServiceReady(GridMate::GameLiftClientService* service)
    {
        DismissBusyScreen();

        m_hasGameliftSession = true;
        ShowServerListingLobby(LobbyMode::GameliftLobby);
    }

    void MultiplayerLobbyComponent::OnGameLiftSessionServiceFailed(GridMate::GameLiftClientService* service)
    {
        DismissBusyScreen();

        m_hasGameliftSession = false;
        
        m_unregisterGameliftServiceOnErrorDismiss = true;
        ShowError("GameLift Service Failed");
    }
#endif
    
    bool MultiplayerLobbyComponent::SanityCheck()
    {
        if (gEnv->IsEditor())
        {
            ShowError("Unsupported action inside of Editor.");
            return false;
        }
        else if (gEnv->pNetwork == nullptr)
        {
            ShowError("Network Environment is null");
            return false;
        }        
        else
        {
            GridMate::IGridMate* gridMate = gEnv->pNetwork->GetGridMate();
            
            if (gridMate == nullptr)
            {
                ShowError("GridMate is null.");
                return false;
            }
        }
     
        return true;
    }    
    
    void MultiplayerLobbyComponent::ShowError(const char* message)
    {
        if (!m_isShowingError)
        {
            int errorWindow = 0;
            int errorMessage = 0;
            AZ::EntityId canvasID = 0;

            m_isShowingError = true;

            if (m_lobbyMode == LobbyMode::GameliftLobby || m_lobbyMode == LobbyMode::LANLobby)
            {
                errorWindow = k_serverListingErrorWindow;   
                errorMessage = k_serverListingErrorMessage;
                canvasID = m_serverListingID;
            }
            else if (m_lobbyMode == LobbyMode::LobbySelection)
            {
                errorWindow = k_lobbySelectionErrorWindow;   
                errorMessage = k_lobbySelectionErrorMessage;
                canvasID = m_selectionLobbyID;
            }

            SetElementEnabled(canvasID,errorWindow,true);

            AZ::Entity* element = nullptr;            
            EBUS_EVENT_ID_RESULT(element,canvasID,UiCanvasBus,FindElementById,errorMessage);

            if (element != nullptr)
            {
                LyShine::StringType textString(message);
                EBUS_EVENT_ID(element->GetId(),UiTextBus,SetText,textString);
            }
        }
    }

    void MultiplayerLobbyComponent::DismissError(bool force)
    {
        if (m_isShowingError || force)
        {
            int errorWindow = 0;
            AZ::EntityId canvasID = 0;

            m_isShowingError = false;

            if (m_lobbyMode == LobbyMode::GameliftLobby || m_lobbyMode == LobbyMode::LANLobby)
            {
                errorWindow = k_serverListingErrorWindow;
                canvasID = m_serverListingID;
            }
            else if (m_lobbyMode == LobbyMode::LobbySelection)
            {
                errorWindow = k_lobbySelectionErrorWindow;
                canvasID = m_selectionLobbyID;
            }

            SetElementEnabled(canvasID,errorWindow,false);           
        }
    }

    void MultiplayerLobbyComponent::ShowBusyScreen()
    {
        if (!m_isShowingBusy)
        {
            int busyWindow = 0;
            AZ::EntityId canvasID = 0;

            m_isShowingBusy = true;

            if (m_lobbyMode == LobbyMode::GameliftLobby || m_lobbyMode == LobbyMode::LANLobby)
            {
                busyWindow = k_serverListingBusyScreen;
                canvasID = m_serverListingID;
            }
            else if (m_lobbyMode == LobbyMode::LobbySelection)
            {
                busyWindow = k_lobbySelectionBusyScreen;
                canvasID = m_selectionLobbyID;
            }

            SetElementEnabled(canvasID,busyWindow,true);            
        }
    }

    void MultiplayerLobbyComponent::DismissBusyScreen(bool force)
    {
        if (m_isShowingBusy || force)
        {
            int busyWindow = 0;
            AZ::EntityId canvasID = 0;

            m_isShowingBusy = false;

            if (m_lobbyMode == LobbyMode::GameliftLobby || m_lobbyMode == LobbyMode::LANLobby)
            {
                busyWindow = k_serverListingBusyScreen;
                canvasID = m_serverListingID;
            }
            else if (m_lobbyMode == LobbyMode::LobbySelection)
            {                
                busyWindow = k_lobbySelectionBusyScreen;
                canvasID = m_selectionLobbyID;
            }

            SetElementEnabled(canvasID,busyWindow,false);
        }
    }    
}