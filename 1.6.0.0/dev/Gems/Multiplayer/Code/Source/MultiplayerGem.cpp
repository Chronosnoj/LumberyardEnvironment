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
#include "MultiplayerGem.h"
#include "GameLiftListener.h"
#include <GridMate/NetworkGridMate.h>
#include <FlowSystem/Nodes/FlowBaseNode.h>
#include <GridMate/Carrier/Driver.h>
#include <CertificateManager/ICertificateManagerGem.h>

#include <GridMate/Carrier/DefaultSimulator.h>
#include <AzFramework/Network/NetBindingSystemBus.h>

#include <AzCore/Component/ComponentApplicationBus.h>

#include "Multiplayer/MultiplayerLobbyComponent.h"


#ifdef NET_SUPPORT_SECURE_SOCKET_DRIVER
#   include <GridMate/Carrier/SecureSocketDriver.h>
#endif

namespace
{
    void ApplyDisconnectDetectionSettings(GridMate::CarrierDesc& carrierDesc)
    {
        carrierDesc.m_enableDisconnectDetection = !!gEnv->pConsole->GetCVar("gm_disconnectDetection")->GetIVal();

        if (gEnv->pConsole->GetCVar("gm_disconnectDetectionRttThreshold"))
        {
            carrierDesc.m_disconnectDetectionRttThreshold = gEnv->pConsole->GetCVar("gm_disconnectDetectionRttThreshold")->GetFVal();
        }

        if (gEnv->pConsole->GetCVar("gm_disconnectDetectionPacketLossThreshold"))
        {
            carrierDesc.m_disconnectDetectionPacketLossThreshold = gEnv->pConsole->GetCVar("gm_disconnectDetectionPacketLossThreshold")->GetFVal();
        }
    }
}

namespace Multiplayer
{

    int MultiplayerModule::s_NetsecEnabled = 0;
    int MultiplayerModule::s_NetsecVerifyClient = 0;

    MultiplayerModule::MultiplayerModule() 
        :  CryHooksModule()
        , m_session(nullptr)
        , m_secureDriver(nullptr)
        , m_simulator(nullptr)
        , m_gameLiftListener(nullptr)
    {    
        m_descriptors.insert(m_descriptors.end(), {
            MultiplayerLobbyComponent::CreateDescriptor(),
        });
    }

    MultiplayerModule::~MultiplayerModule() 
    {
        delete m_simulator;
    }

    void MultiplayerModule::OnCrySystemInitialized(ISystem& system, const SSystemInitParams& systemInitParams)
    {
        CryHooksModule::OnCrySystemInitialized(system, systemInitParams);
        m_cvars.RegisterCVars();
    }

    void MultiplayerModule::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
    {
        switch (event)
        {
            case ESYSTEM_EVENT_FLOW_SYSTEM_REGISTER_EXTERNAL_NODES:
                RegisterExternalFlowNodes();            
                break;

            case ESYSTEM_EVENT_GAME_POST_INIT:
            {
#if BUILD_GAMELIFT_SERVER
                m_gameLiftListener = aznew GameLiftListener();
#endif                
                AZ_Assert(gEnv->pNetwork->GetGridMate(), "No GridMate");
                GridMate::SessionEventBus::Handler::BusConnect(gEnv->pNetwork->GetGridMate());
                MultiplayerRequestBus::Handler::BusConnect();
            }
            break;

            case ESYSTEM_EVENT_FULL_SHUTDOWN:
            case ESYSTEM_EVENT_FAST_SHUTDOWN:
                MultiplayerRequestBus::Handler::BusDisconnect();
                GridMate::SessionEventBus::Handler::BusDisconnect();
                m_cvars.UnregisterCVars();            

#if BUILD_GAMELIFT_SERVER
                delete m_gameLiftListener;
                m_gameLiftListener = nullptr;
#endif
                break;

            default:
                (void)event;
        }
    }

    bool MultiplayerModule::IsNetSecEnabled() const
    {
        return s_NetsecEnabled != 0;
    }

    bool MultiplayerModule::IsNetSecVerifyClient() const
    {
        return s_NetsecVerifyClient != 0;
    }

#if defined(NET_SUPPORT_SECURE_SOCKET_DRIVER)
    void MultiplayerModule::RegisterSecureDriver(GridMate::SecureSocketDriver* driver)
    {
        AZ_Assert(m_secureDriver == nullptr,"Trying to Register two secure driver's at once. Unsupported behavior");

        m_secureDriver = driver;
    }
#endif

    GridMate::GridSession* MultiplayerModule::GetSession()
    {
        return m_session;
    }

    void MultiplayerModule::RegisterSession(GridMate::GridSession* session)
    {
        if (m_session != nullptr && session != nullptr)
        {
            CryLog("Already participating in the session '%s'. Leave existing session first!", m_session->GetId().c_str());
            return;
        }    

        m_session = session;
    }

    void MultiplayerModule::OnSessionCreated(GridMate::GridSession* session)
    {
        CryLog("Session %s has been created.", session->GetId().c_str());

        if (session == m_session)
        {
            session->GetReplicaMgr()->SetSendTimeInterval(gEnv->pConsole->GetCVar("gm_replicasSendTime")->GetIVal());

            if (gEnv->pConsole->GetCVar("gm_replicasSendLimit"))
            {
                session->GetReplicaMgr()->SetSendLimit(gEnv->pConsole->GetCVar("gm_replicasSendLimit")->GetIVal());
            }

            if (gEnv->pConsole->GetCVar("gm_burstTimeLimit"))
            {
                session->GetReplicaMgr()->SetSendLimitBurstRange(gEnv->pConsole->GetCVar("gm_burstTimeLimit")->GetFVal());
            }

            EBUS_EVENT(AzFramework::NetBindingSystemEventsBus, OnNetworkSessionActivated, session);
        }
    }

    //-----------------------------------------------------------------------------
    void MultiplayerModule::OnSessionDelete(GridMate::GridSession* session)
    {
        CryLog("Session %s has been deleted.", session->GetId().c_str());

        if (session == m_session)
        {
            EBUS_EVENT(AzFramework::NetBindingSystemEventsBus, OnNetworkSessionDeactivated, session);
            m_session = nullptr;

#ifdef NET_SUPPORT_SECURE_SOCKET_DRIVER
            delete m_secureDriver;
            m_secureDriver = nullptr;
#endif
        }
    }

    //-----------------------------------------------------------------------------
    GridMate::Simulator* MultiplayerModule::GetSimulator()
    {
        return m_simulator;
    }

    //-----------------------------------------------------------------------------
    void MultiplayerModule::EnableSimulator()
    {
        if (!m_simulator)
        {
            m_simulator = aznew GridMate::DefaultSimulator();
        }

        m_simulator->Enable();
    }

    //-----------------------------------------------------------------------------
    void MultiplayerModule::DisableSimulator()
    {
        if (m_simulator)
        {
            m_simulator->Disable();
        }
    }
}

AZ_DECLARE_MODULE_CLASS(Multiplayer_d3ed407a19bb4c0d92b7c4872313d600, Multiplayer::MultiplayerModule)
