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

#include <INetwork.h>
#include "MultiplayerCVars.h"
#include "MultiplayerGem.h"
#include "Multiplayer/MultiplayerUtils.h"

#include <GameLift/GameLiftBus.h>
#include <GameLift/Session/GameLiftServerService.h>
#include <CertificateManager/ICertificateManagerGem.h>
#include <CertificateManager/DataSource/FileDataSourceBus.h>


#include <GridMate/Carrier/DefaultSimulator.h>
#include <GridMate/Session/LANSession.h>

namespace Multiplayer
{
#if BUILD_GAMELIFT_CLIENT
    static void StopGameLiftClient(IConsoleCmdArgs *args)
    {
        EBUS_EVENT(GameLift::GameLiftRequestBus, StopClientService);
    }
#endif

#if BUILD_GAMELIFT_SERVER
    static void StartGameLiftServer(IConsoleCmdArgs *args)
    {
        CRY_ASSERT(gEnv->pConsole);

        // set the sys_dump_type to 2 so error dump files don't exceed GameLift size limits 
        ICVar* sys_dump_type_cvar = gEnv->pConsole->GetCVar("sys_dump_type");
        if (sys_dump_type_cvar)
        {
            sys_dump_type_cvar->Set(2);
        }

        GridMate::GameLiftServerServiceDesc serviceDesc;

        if (gEnv->pLog && gEnv->pLog->GetFileName() && gEnv->pLog->GetFileName()[0] != '\0') // have log file ?
        {
            char resolvedPath[AZ_MAX_PATH_LEN];
            if (gEnv->pFileIO->ResolvePath(gEnv->pLog->GetFileName(), resolvedPath, AZ_ARRAY_SIZE(resolvedPath)))
            {
                serviceDesc.m_logPaths.push_back(resolvedPath);
            }
        }

        if (gEnv->pConsole->GetCVar("sv_port"))
        {
            serviceDesc.m_port = gEnv->pConsole->GetCVar("sv_port")->GetIVal();
        }

        EBUS_EVENT(GameLift::GameLiftRequestBus, StartServerService, serviceDesc);
    }

    static void StopGameLiftServer(IConsoleCmdArgs *args)
    {
        EBUS_EVENT(GameLift::GameLiftRequestBus, StopServerService);
    }
#endif
    //-----------------------------------------------------------------------------
    void CmdNetSimulator(IConsoleCmdArgs* args)
    {
        GridMate::Simulator* simulator = nullptr;
        EBUS_EVENT_RESULT(simulator,Multiplayer::MultiplayerRequestBus,GetSimulator);

        GridMate::GridSession* session = nullptr;
        EBUS_EVENT_RESULT(session,Multiplayer::MultiplayerRequestBus,GetSession);

        if (!simulator && session)
        {
            CryLogAlways("Simulator should be enabled before GridMate session starts. Use 'mpdisconnect' to destroy the session.");
            return;
        }

        if (args->GetArgCount() == 2 && CryStringUtils::ToYesNoType(args->GetArg(1)) == CryStringUtils::YesNoType::No)
        {
            EBUS_EVENT(Multiplayer::MultiplayerRequestBus,DisableSimulator);
            return;
        }

        if (args->GetArgCount() == 2 && !stricmp(args->GetArg(1), "help"))
        {
            CryLogAlways("gm_net_simulator off      - Disable simulator");
            CryLogAlways("gm_net_simulator param1:value1 param2:value2, ...      - Enable simulator with given parameters");
            CryLogAlways("Available parameters:");
            CryLogAlways("oLatMin, oLatMax      - Outgoing latency in milliseconds");
            CryLogAlways("iLatMin, iLatMax      - Incoming latency in milliseconds");
            CryLogAlways("oBandMin, oBandMax      - Outgoing bandwidth in Kbps");
            CryLogAlways("iBandMin, iBandMax      - Incoming bandwidth in Kbps");
            CryLogAlways("oLossMin, oLossMax      - Outgoing packet loss, will lose one packet every interval");
            CryLogAlways("iLossMin, iLossMax      - Incoming packet loss, will lose one packet every interval");
            CryLogAlways("oDropMin, oDropMax, oDropPeriodMin, oDropPeriodMax      - Outgoing packet drop, will periodically lose packets for given interval");
            CryLogAlways("iDropMin, iDropMax, iDropPeriodMin, iDropPeriodMax      - Incoming packet drop, will periodically lose packets for given interval");
            CryLogAlways("oReorder      - [0|1] Outgoing packet reordering. You need to enable latency to reorder packets.");
            CryLogAlways("iReorder      - [0|1] Incoming packet reordering. You need to enable latency to reorder packets.");
            return;
        }

        if (args->GetArgCount() > 1)
        {
            EBUS_EVENT(Multiplayer::MultiplayerRequestBus,EnableSimulator);            
        }

        GridMate::DefaultSimulator* sim = static_cast<GridMate::DefaultSimulator*>(simulator);

        if (sim)
        {
            unsigned int oLatMin, oLatMax;
            unsigned int iLatMin, iLatMax;
            unsigned int oBandMin, oBandMax;
            unsigned int iBandMin, iBandMax;
            unsigned int oLossMin, oLossMax;
            unsigned int iLossMin, iLossMax;
            unsigned int oDropMin, oDropMax, oDropPeriodMin, oDropPeriodMax;
            unsigned int iDropMin, iDropMax, iDropPeriodMin, iDropPeriodMax;
            bool oReorder, iReorder;

            sim->GetOutgoingLatency(oLatMin, oLatMax);
            sim->GetIncomingLatency(iLatMin, iLatMax);
            sim->GetOutgoingBandwidth(oBandMin, oBandMax);
            sim->GetIncomingBandwidth(iBandMin, iBandMax);
            sim->GetOutgoingPacketLoss(oLossMin, oLossMax);
            sim->GetIncomingPacketLoss(iLossMin, iLossMax);
            sim->GetOutgoingPacketDrop(oDropMin, oDropMax, oDropPeriodMin, oDropPeriodMax);
            sim->GetIncomingPacketDrop(iDropMin, iDropMax, iDropPeriodMin, iDropPeriodMax);
            oReorder = sim->IsOutgoingReorder();
            iReorder = sim->IsIncomingReorder();

            for (int i = 1; i < args->GetArgCount(); ++i)
            {
                const char* arg = args->GetArg(i);

                unsigned int param;
                char key[64];

                int numParams = sscanf(arg, "%64[^:]:%u", key, &param) - 1;

                if (numParams <= 0)
                {
                    CryLogAlways("ERROR: Invalid argument format: %s. Should be 'key:value'. Bailing out.", arg);
                    return;
                }

                if (!stricmp(key, "oLatMin"))
                {
                    oLatMin = param;
                }
                else if (!stricmp(key, "oLatMax"))
                {
                    oLatMax = param;
                }
                else if (!stricmp(key, "iLatMin"))
                {
                    iLatMin = param;
                }
                else if (!stricmp(key, "iLatMax"))
                {
                    iLatMax = param;
                }
                else if (!stricmp(key, "oBandMin"))
                {
                    oBandMin = param;
                }
                else if (!stricmp(key, "oBandMax"))
                {
                    oBandMax = param;
                }
                else if (!stricmp(key, "iBandMin"))
                {
                    iBandMin = param;
                }
                else if (!stricmp(key, "iBandMax"))
                {
                    iBandMax = param;
                }
                else if (!stricmp(key, "oLossMin"))
                {
                    oLossMin = param;
                }
                else if (!stricmp(key, "oLossMax"))
                {
                    oLossMax = param;
                }
                else if (!stricmp(key, "iLossMin"))
                {
                    iLossMin = param;
                }
                else if (!stricmp(key, "iLossMax"))
                {
                    iLossMax = param;
                }
                else if (!stricmp(key, "oDropMin"))
                {
                    oDropMin = param;
                }
                else if (!stricmp(key, "oDropMax"))
                {
                    oDropMax = param;
                }
                else if (!stricmp(key, "oDropPeriodMin"))
                {
                    oDropPeriodMin = param;
                }
                else if (!stricmp(key, "oDropPeriodMax"))
                {
                    oDropPeriodMax = param;
                }
                else if (!stricmp(key, "iDropMin"))
                {
                    iDropMin = param;
                }
                else if (!stricmp(key, "iDropMax"))
                {
                    iDropMax = param;
                }
                else if (!stricmp(key, "iDropPeriodMin"))
                {
                    iDropPeriodMin = param;
                }
                else if (!stricmp(key, "iDropPeriodMax"))
                {
                    iDropPeriodMax = param;
                }
                else if (!stricmp(key, "oReorder"))
                {
                    oReorder = param != 0;
                }
                else if (!stricmp(key, "iReorder"))
                {
                    iReorder = param != 0;
                }
                else
                {
                    CryLogAlways("ERROR: Invalid argument: %s. Bailing out.", key);
                    return;
                }
            }

            sim->SetOutgoingLatency(oLatMin, oLatMax);
            sim->SetIncomingLatency(iLatMin, iLatMax);
            sim->SetOutgoingBandwidth(oBandMin, oBandMax);
            sim->SetIncomingBandwidth(iBandMin, iBandMax);
            sim->SetOutgoingPacketLoss(oLossMin, oLossMax);
            sim->SetIncomingPacketLoss(iLossMin, iLossMax);
            sim->SetOutgoingPacketDrop(oDropMin, oDropMax, oDropPeriodMin, oDropPeriodMax);
            sim->SetIncomingPacketDrop(iDropMin, iDropMax, iDropPeriodMin, iDropPeriodMax);
            sim->SetOutgoingReorder(oReorder);
            sim->SetIncomingReorder(iReorder);

            CryLogAlways("Simulator settings:");
            CryLogAlways("OutgoingLatency: (%u, %u)", oLatMin, oLatMax);
            CryLogAlways("IncomingLatency: (%u, %u)", iLatMin, iLatMax);
            CryLogAlways("OutgoingBandwidth: (%u, %u)", oBandMin, oBandMax);
            CryLogAlways("IncomingBandwidth: (%u, %u)", iBandMin, iBandMax);
            CryLogAlways("OutgoingPacketLoss: (%u, %u)", oLossMin, oLossMax);
            CryLogAlways("IncomingPacketLoss: (%u, %u)", iLossMin, iLossMax);
            CryLogAlways("OutgoingPacketDrop: (%u, %u, %u, %u)", oDropMin, oDropMax, oDropPeriodMin, oDropPeriodMax);
            CryLogAlways("IncomingPacketDrop: (%u, %u, %u, %u)", iDropMin, iDropMax, iDropPeriodMin, iDropPeriodMax);
            CryLogAlways("OutgoingReorder: %s", oReorder ? "on" : "off");
            CryLogAlways("IncomingReorder: %s", iReorder ? "on" : "off");
        }
        else
        {
            CryLogAlways("Simulator is disabled.");
        }
    }

#if defined(NET_SUPPORT_SECURE_SOCKET_DRIVER)

    void MultiplayerCVars::OnPrivateKeyChanged(ICVar* filename)
    {
        if (filename && filename->GetString() && filename->GetString()[0])
        {
            CreateFileDataSource();
            EBUS_EVENT(CertificateManager::FileDataSourceConfigurationBus,ConfigurePrivateKey,filename->GetString());
        }
        else
        {
            AZ_Warning("CertificateManager", "Failed to load Private Key '%s'.", filename->GetString());
        }
    }

    void MultiplayerCVars::OnCertificateChanged(ICVar* filename)
    {
        if (filename && filename->GetString() && filename->GetString()[0])
        {
            CreateFileDataSource();
            EBUS_EVENT(CertificateManager::FileDataSourceConfigurationBus,ConfigureCertificate,filename->GetString());
        }
        else
        {
            AZ_Warning("CertificateManager", "Failed to load Certificate '%s'.", filename->GetString());
        }
    }

    void MultiplayerCVars::OnCAChanged(ICVar* filename)
    {
        if (filename && filename->GetString() && filename->GetString()[0])
        {
            CreateFileDataSource();
            EBUS_EVENT(CertificateManager::FileDataSourceConfigurationBus,ConfigureCertificateAuthority,filename->GetString());
            }
        else
        {
            AZ_Warning("CertificateManager", "Failed to load CA '%s'.", filename->GetString());
        }
    }

    void MultiplayerCVars::CreateFileDataSource()
    {
        if (CertificateManager::FileDataSourceConfigurationBus::FindFirstHandler() == nullptr)
        {            
            EBUS_EVENT(CertificateManager::FileDataSourceCreationBus,CreateFileDataSource);

            if (CertificateManager::FileDataSourceConfigurationBus::FindFirstHandler() == nullptr)
            {                
                AZ_Assert(false,"Unable to create File Data Source");
            }
        }

        AZ_Assert(CertificateManager::FileDataSourceConfigurationBus::FindFirstHandler() != nullptr,"Incorrect DataSource configured for File Based CVars");
    }
#endif

    static void OnDisconnectDetectionChanged(ICVar* cvar)
    {
        GridMate::GridSession* session = nullptr;
        EBUS_EVENT_RESULT(session,Multiplayer::MultiplayerRequestBus,GetSession);
        if (!session)
        {
            return;
        }

        if (!session->IsHost())
        {
            CryLogAlways("Will not apply to the active session, only host can control disconnect detection mode for a game in progress.");
            return;
        }

        session->DebugEnableDisconnectDetection(cvar->GetIVal() != 0);
    }

    static void OnReplicasSendTimeChanged(ICVar* cvar)
    {
        GridMate::GridSession* session = nullptr;
        EBUS_EVENT_RESULT(session,Multiplayer::MultiplayerRequestBus,GetSession);

        if (!session)
        {
            return;
        }

        session->GetReplicaMgr()->SetSendTimeInterval(cvar->GetIVal());
    }

    static void OnReplicasSendLimitChanged(ICVar* cvar)
    {
        GridMate::GridSession* session = nullptr;
        EBUS_EVENT_RESULT(session,Multiplayer::MultiplayerRequestBus,GetSession);

        if (!session)
        {
            return;
        }

        session->GetReplicaMgr()->SetSendLimit(cvar->GetIVal());
    }

    static void OnReplicasBurstRangeChanged(ICVar* cvar)
    {
        GridMate::GridSession* session = nullptr;
        EBUS_EVENT_RESULT(session,Multiplayer::MultiplayerRequestBus,GetSession);

        if (!session)
        {
            return;
        }

        session->GetReplicaMgr()->SetSendLimitBurstRange(cvar->GetFVal());
    }

    //-----------------------------------------------------------------------------
    MultiplayerCVars* MultiplayerCVars::s_instance = nullptr;

    //-----------------------------------------------------------------------------
    MultiplayerCVars::MultiplayerCVars()
        : m_autoJoin(false)
        , m_search(nullptr)
    {
        s_instance = this;
    }

    //-----------------------------------------------------------------------------
    MultiplayerCVars::~MultiplayerCVars()
    {
        if (s_instance == this)
        {
            s_instance = nullptr;
        }
    }

    //------------------------------------------------------------------------
    void MultiplayerCVars::RegisterCVars()
    {
        if (gEnv && !gEnv->IsEditor())
        {
            REGISTER_COMMAND("mphost", MPHostLANCmd, 0, "begin hosting a LAN session");
            REGISTER_COMMAND("mpjoin", MPJoinLANCmd, 0, "try to join a LAN session");
            REGISTER_COMMAND("mpsearch", MPJoinLANCmd, 0, "try to find a LAN session");
            REGISTER_COMMAND("mpdisconnect", MPDisconnectCmd, 0, "disconnect from our session");

            REGISTER_INT("gm_version", 1, 0, "Set the gridmate version number.");

#ifdef NET_SUPPORT_SECURE_SOCKET_DRIVER
            REGISTER_CVAR2("gm_netsec_enable", &MultiplayerModule::s_NetsecEnabled, MultiplayerModule::s_NetsecEnabled, VF_NULL,
                "Enable network level encryption. Must be called before hosting or joining a session (e.g. by using mphost or mpjoin).");
            REGISTER_STRING_CB("gm_netsec_private_key", nullptr, VF_DEV_ONLY,
                "Set the private key file (PEM format) to use when establishing a secure network connection.", OnPrivateKeyChanged);
            REGISTER_STRING_CB("gm_netsec_certificate", nullptr, VF_DEV_ONLY,
                "Set the certificate file (PEM format) to use when establishing a secure network connection.", OnCertificateChanged);
            REGISTER_STRING_CB("gm_netsec_ca", nullptr, VF_DEV_ONLY,
                "Set the CA certificate file (PEM format) to use when establishing a secure network connection.", OnCAChanged);
            REGISTER_CVAR2("gm_netsec_verify_client", &MultiplayerModule::s_NetsecVerifyClient, MultiplayerModule::s_NetsecVerifyClient, VF_NULL,
                "Enable client authentication. If not set only the server will be authenticated. Only needs to be called on the server!");
#endif

            REGISTER_COMMAND("gm_net_simulator", CmdNetSimulator, VF_DEV_ONLY, "Setup network simulator. See 'gm_net_simulator help' for available options.");
            REGISTER_INT_CB("gm_disconnectDetection", 1, VF_NULL, "GridMate disconnect detection.", OnDisconnectDetectionChanged);
            REGISTER_FLOAT("gm_disconnectDetectionRttThreshold", 500.0f, VF_NULL, "Rtt threshold in milliseconds, connection will be dropped once actual rtt is bigger than this value");
            REGISTER_FLOAT("gm_disconnectDetectionPacketLossThreshold", 0.3f, VF_NULL, "Packet loss percentage threshold (0.0..1.0, 1.0 is 100%), connection will be dropped once actual packet loss exceeds this value");

            REGISTER_STRING("gm_ipversion", "IPv4", 0, "IP protocol version. (Can be 'IPv4' or 'IPv6')");
            REGISTER_STRING("gm_securityData", "", 0, "Security data for session.(For example on XBox One this is the Security template name)");
            REGISTER_INT_CB("gm_replicasSendTime", 0, VF_NULL, "Time interval between replicas sends (in milliseconds), 0 will bound sends to GridMate tick rate", OnReplicasSendTimeChanged);
            REGISTER_INT_CB("gm_replicasSendLimit", 0, VF_DEV_ONLY, "Replica data send limit in bytes per second. 0 - limiter turned off. (Dev build only)", OnReplicasSendLimitChanged);
            REGISTER_FLOAT_CB("gm_burstTimeLimit", 10.f, VF_DEV_ONLY, "Burst in bandwidth will be allowed for the given amount of time(in seconds). Burst will only be allowed if bandwidth is not capped at the time of burst. (Dev build only)", OnReplicasBurstRangeChanged);

#if defined(DURANGO)
            REGISTER_STRING("gm_durango_session_template", "GroupBuildingLobby", 0, "XBox live session template name for smart matchmaking");
            REGISTER_STRING("gm_durango_match_hopper", "DefaultHopper", 0, "XBox live hopper name for smart matchmaking");
#endif

#if BUILD_GAMELIFT_CLIENT
            REGISTER_STRING("gamelift_fleet_id", "", VF_DUMPTODISK, "Id of GameLift Fleet to use with this client.");
            REGISTER_STRING("gamelift_aws_access_key", "", VF_DUMPTODISK, "AWS Access Key.");
            REGISTER_STRING("gamelift_aws_secret_key", "", VF_DUMPTODISK, "AWS Secret Key.");
            REGISTER_STRING("gamelift_aws_region", "us-west-2", VF_DUMPTODISK, "AWS Region to use for GameLift.");
            REGISTER_STRING("gamelift_endpoint", "gamelift.us-west-2.amazonaws.com", VF_DUMPTODISK, "GameLift service endpoint.");
            REGISTER_STRING("gamelift_alias_id", "", VF_DUMPTODISK, "Id of GameLift alias to use with the client.");

            // player IDs must be unique and anonymous 
            bool includeBrackets = false;
            bool includeDashes = true;
            AZStd::string defaultPlayerId = AZ::Uuid::CreateRandom().ToString<AZStd::string>(includeBrackets, includeDashes);
            REGISTER_STRING("gamelift_player_id", defaultPlayerId.c_str(), VF_DUMPTODISK, "Player Id.");
            REGISTER_COMMAND("gamelift_stop_client", StopGameLiftClient, VF_NULL, "Stops gamelift session service and terminates the session if it had one.");
#endif

#if BUILD_GAMELIFT_SERVER
            REGISTER_COMMAND("gamelift_start_server", StartGameLiftServer, VF_NULL, "Start up the gamelift server. This will initialize gameLift server API.\nThe session will start after GameLift intialization");
            REGISTER_COMMAND("gamelift_stop_server", StopGameLiftServer, VF_NULL, "Stops gamelift session service and terminates the session if it had one.");
#endif
        }
    }
    //------------------------------------------------------------------------
    void MultiplayerCVars::UnregisterCVars()
    {
        if (gEnv && !gEnv->IsEditor())
        {
#if BUILD_GAMELIFT_CLIENT
            if (gEnv->pConsole)
            {
                gEnv->pConsole->RemoveCommand("gamelift_start_client");
            }
            UNREGISTER_CVAR("gamelift_player_id");
            UNREGISTER_CVAR("gamelift_alias_id");
            UNREGISTER_CVAR("gamelift_endpoint");
            UNREGISTER_CVAR("gamelift_aws_region");
            UNREGISTER_CVAR("gamelift_aws_secret_key");
            UNREGISTER_CVAR("gamelift_aws_access_key");
            UNREGISTER_CVAR("gamelift_fleet_id");
#endif

#if BUILD_GAMELIFT_SERVER
            if (gEnv->pConsole)
            {
                gEnv->pConsole->RemoveCommand("gamelift_stop_server");
                gEnv->pConsole->RemoveCommand("gamelift_start_server");
            }
#endif

#if defined(DURANGO)
            UNREGISTER_CVAR("gm_durango_match_hopper");
            UNREGISTER_CVAR("gm_durango_session_template");
#endif


            UNREGISTER_CVAR("gm_burstTimeLimit");
            UNREGISTER_CVAR("gm_replicasSendLimit");
            UNREGISTER_CVAR("gm_replicasSendTime");
            UNREGISTER_CVAR("gm_securityData");
            UNREGISTER_CVAR("gm_ipversion");
            UNREGISTER_CVAR("gm_disconnectDetectionPacketLossThreshold");
            UNREGISTER_CVAR("gm_disconnectDetectionRttThreshold");
            UNREGISTER_CVAR("gm_disconnectDetection");

            if (gEnv->pConsole)
            {
                gEnv->pConsole->RemoveCommand("gm_net_simulator");
            }

#ifdef NET_SUPPORT_SECURE_SOCKET_DRIVER
            UNREGISTER_CVAR("gm_netsec_ca");
            UNREGISTER_CVAR("gm_netsec_certificate");
            UNREGISTER_CVAR("gm_netsec_private_key");
            UNREGISTER_CVAR("gm_netsec_enable");
#endif
            UNREGISTER_CVAR("gm_version");

            if (gEnv->pConsole)
            {
                gEnv->pConsole->RemoveCommand("mpdisconnect");
                gEnv->pConsole->RemoveCommand("mpsearch");
                gEnv->pConsole->RemoveCommand("mpjoin");
                gEnv->pConsole->RemoveCommand("mphost");
            }
        }
    }

    //------------------------------------------------------------------------
    void MultiplayerCVars::MPHostLANCmd(IConsoleCmdArgs* args)
    {
        (void)args;

        GridMate::IGridMate* gridMate = gEnv->pNetwork->GetGridMate();
        if (!gridMate)
        {
            CryLogAlways("GridMate has not been initialized.");
            return;
        }

        GridMate::GridSession* gridSession = nullptr;
        EBUS_EVENT_RESULT(gridSession,Multiplayer::MultiplayerRequestBus,GetSession);

        if (gridSession)
        {
            CryLogAlways("You're already part of a session. Use 'mpdisconnect' first.");
            return;
        }

        if (!GridMate::HasGridMateService<GridMate::LANSessionService>(gridMate))
        {   
            Multiplayer::LAN::StartSessionService(gridMate);            
        }

        // Attempt to start a hosted LAN session. If we do, we're now a server and in multiplayer mode.
        GridMate::LANSessionParams sp;
        sp.m_topology = GridMate::ST_CLIENT_SERVER;
        sp.m_numPublicSlots = gEnv->pConsole->GetCVar("sv_maxplayers")->GetIVal() + (gEnv->IsDedicated() ? 1 : 0); // One slot for server member.
        sp.m_numPrivateSlots = 0;
        sp.m_port = gEnv->pConsole->GetCVar("sv_port")->GetIVal() + 1; // Listen for searches on sv_port + 1
        sp.m_peerToPeerTimeout = 60000;
        sp.m_flags = 0;
        sp.m_numParams = 0;

        GridMate::CarrierDesc carrierDesc;

        Multiplayer::Utils::InitCarrierDesc(carrierDesc);
        Multiplayer::NetSec::ConfigureCarrierDescForHost(carrierDesc);        

        carrierDesc.m_port = static_cast<uint16>(gEnv->pConsole->GetCVar("sv_port")->GetIVal());
        carrierDesc.m_enableDisconnectDetection = !!gEnv->pConsole->GetCVar("gm_disconnectDetection")->GetIVal();
        carrierDesc.m_connectionTimeoutMS = 10000;
        carrierDesc.m_threadUpdateTimeMS = 30;
        carrierDesc.m_version = gEnv->pConsole->GetCVar("gm_version")->GetIVal();
        carrierDesc.m_disconnectDetectionRttThreshold = gEnv->pConsole->GetCVar("gm_disconnectDetectionRttThreshold")->GetFVal();
        carrierDesc.m_disconnectDetectionPacketLossThreshold = gEnv->pConsole->GetCVar("gm_disconnectDetectionPacketLossThreshold")->GetFVal();
        
        GridMate::GridSession* session = nullptr;
        EBUS_EVENT_ID_RESULT(session,gridMate,GridMate::LANSessionServiceBus,HostSession,sp,carrierDesc);

        if (session)
        {
            EBUS_EVENT(Multiplayer::MultiplayerRequestBus,RegisterSession,session);
        }        
    }

    //------------------------------------------------------------------------
    void MultiplayerCVars::MPJoinLANCmd(IConsoleCmdArgs* args)
    {
        GridMate::IGridMate* gridMate = gEnv->pNetwork->GetGridMate();
        if (!gridMate)
        {
            CryLogAlways("GridMate has not been initialized.");
            return;
        }

        GridMate::GridSession* gridSession = nullptr;
        EBUS_EVENT_RESULT(gridSession,Multiplayer::MultiplayerRequestBus,GetSession);

        if (gridSession)
        {
            CryLogAlways("You're already part of a session. Use 'mpdisconnect' first.");
            return;
        }

        if (GridMate::LANSessionServiceBus::FindFirstHandler(gridMate) == nullptr)
        {
            Multiplayer::LAN::StartSessionService(gridMate);
        }

        // Parse optional arguments
        if (args->GetArgCount() > 1)
        {
            gEnv->pConsole->GetCVar("cl_serveraddr")->Set(args->GetArg(1));

            // check if a port was provided
            if (args->GetArgCount()>2)
            {
                gEnv->pConsole->GetCVar("cl_serverport")->Set(args->GetArg(2));
            }
        }

        const char* serveraddr = gEnv->pConsole->GetCVar("cl_serveraddr")->GetString();
        // LANSession doesn't support names. At least handle localhost here.
        if (!serveraddr || 0 == stricmp("localhost", serveraddr))
        {
            serveraddr = "127.0.0.1";
        }

        const bool autoJoin = (nullptr != CryStringUtils::stristr(args->GetArg(0), "join"));

        CryLogAlways("Attempting to '%s' server with search key \"%s\"...",
            args->GetArg(0), serveraddr);

        // Attempt to join the session at the specified address. Should we succeed, we're
        // now a client and in multiplayer mode.
        s_instance->BusConnect(gridMate);
        s_instance->m_autoJoin = autoJoin;

        GridMate::LANSearchParams searchParams;
        searchParams.m_serverAddress = serveraddr;
        searchParams.m_serverPort = gEnv->pConsole->GetCVar("cl_serverport")->GetIVal() + 1;
        searchParams.m_listenPort = 0; // Always use ephemeral port for searches for the time being, until we change the API to allow users to customize this. 

        s_instance->m_search = nullptr;

        EBUS_EVENT_ID_RESULT(s_instance->m_search,gridMate,GridMate::LANSessionServiceBus,StartGridSearch,searchParams);
    }

    //------------------------------------------------------------------------
    void MultiplayerCVars::MPDisconnectCmd(IConsoleCmdArgs* args)
    {
        (void)args;

        GridMate::GridSession* gridSession = nullptr;
        EBUS_EVENT_RESULT(gridSession,Multiplayer::MultiplayerRequestBus,GetSession);

        if (!gridSession)
        {
            CryLogAlways("You're not in any MP session.");
            return;
        }

        gridSession->Leave(false);
    }

    //------------------------------------------------------------------------
    void MultiplayerCVars::OnGridSearchComplete(GridMate::GridSearch* search)
    {
        if (search == m_search)
        {
            m_search = nullptr;

            GridMate::SessionEventBus::Handler::BusDisconnect();

            if (m_autoJoin)
            {
                m_autoJoin = false;

                if (search->GetNumResults() > 0)
                {                    
                    const GridMate::SearchInfo* searchInfo = search->GetResult(0);

                    GridMate::GridSession* session = nullptr;

                    GridMate::CarrierDesc carrierDesc;

                    Multiplayer::Utils::InitCarrierDesc(carrierDesc);
                    Multiplayer::NetSec::ConfigureCarrierDescForJoin(carrierDesc);                    

                    GridMate::JoinParams joinParams;

                    const GridMate::LANSearchInfo& lanSearchInfo = static_cast<const GridMate::LANSearchInfo&>((*searchInfo));
                    EBUS_EVENT_ID_RESULT(session,gEnv->pNetwork->GetGridMate(),GridMate::LANSessionServiceBus,JoinSessionBySearchInfo,lanSearchInfo,joinParams,carrierDesc);

                    if (session != nullptr)
                    {
                        EBUS_EVENT(Multiplayer::MultiplayerRequestBus,RegisterSession,session);
                        CryLogAlways("Successfully joined game session.");
                    }
                    else
                    {
                        CryLogAlways("Found a game session, but failed to join.");
                    }
                }
                else
                {
                    CryLogAlways("No game sessions found.");
                }
            }
        }
    }

} // namespace Multiplayer
