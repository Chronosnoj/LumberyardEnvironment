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

#if BUILD_GAMELIFT_CLIENT

#include <GameLift/Session/GameLiftClientService.h>
#include <GameLift/Session/GameLiftClientSession.h>
#include <GameLift/Session/GameLiftSearch.h>
#include <GameLift/Session/GameLiftSessionRequest.h>

#pragma warning(push)
#pragma warning(disable:4251)
#include <aws/core/auth/AWSCredentialsProvider.h>
#pragma warning(pop)

#include <aws/core/client/ClientConfiguration.h>
#include <aws/gamelift/client/GameLiftClientAPI.h>

namespace
{
    // AWS Native sdk should have access to region names list to avoid doing this in Session code
    Aws::Region GetAwsRegionByName(const char* regionName)
    {
        if (!strcmp(Aws::RegionMapper::GetRegionName(Aws::Region::US_EAST_1), regionName))
        {
            return Aws::Region::US_EAST_1;
        }
        else if (!strcmp(Aws::RegionMapper::GetRegionName(Aws::Region::US_WEST_1), regionName))
        {
            return Aws::Region::US_WEST_1;
        }
        else if (!strcmp(Aws::RegionMapper::GetRegionName(Aws::Region::US_WEST_2), regionName))
        {
            return Aws::Region::US_WEST_2;
        }
        else if (!strcmp(Aws::RegionMapper::GetRegionName(Aws::Region::EU_WEST_1), regionName))
        {
            return Aws::Region::EU_WEST_1;
        }
        else if (!strcmp(Aws::RegionMapper::GetRegionName(Aws::Region::EU_CENTRAL_1), regionName))
        {
            return Aws::Region::EU_CENTRAL_1;
        }
        else if (!strcmp(Aws::RegionMapper::GetRegionName(Aws::Region::AP_SOUTHEAST_1), regionName))
        {
            return Aws::Region::AP_SOUTHEAST_1;
        }
        else if (!strcmp(Aws::RegionMapper::GetRegionName(Aws::Region::AP_SOUTHEAST_2), regionName))
        {
            return Aws::Region::AP_SOUTHEAST_2;
        }
        else if (!strcmp(Aws::RegionMapper::GetRegionName(Aws::Region::AP_NORTHEAST_1), regionName))
        {
            return Aws::Region::AP_NORTHEAST_1;
        }
        else if (!strcmp(Aws::RegionMapper::GetRegionName(Aws::Region::AP_NORTHEAST_2), regionName))
        {
            return Aws::Region::AP_NORTHEAST_2;
        }
        else if (!strcmp(Aws::RegionMapper::GetRegionName(Aws::Region::SA_EAST_1), regionName))
        {
            return Aws::Region::SA_EAST_1;
        }

        AZ_Assert(false, "Unsupported AWS Region: %s\n", regionName);
        return Aws::Region();
    }

    //This is used when a playerId is not specified when initializing GameLiftSDK with developer credentials.
    const char* DEFAULT_PLAYER_ID = "AnonymousPlayerId";
}

namespace GridMate
{
    GameLiftClientService::GameLiftClientService(const GameLiftClientServiceDesc& desc)
        : SessionService(desc)
        , m_serviceDesc(desc)
        , m_clientStatus(GameLift_NotInited)
        , m_clientInitOutcome(nullptr)
    {
    }

    bool GameLiftClientService::IsReady() const
    {
        return m_clientStatus == GameLift_Ready;
    }

    void GameLiftClientService::OnServiceRegistered(IGridMate* gridMate)
    {
        SessionService::OnServiceRegistered(gridMate);

        GameLiftClientSession::RegisterReplicaChunks();

        if (!StartGameLiftClient())
        {
            EBUS_EVENT_ID(m_gridMate, GameLiftClientServiceEventsBus, OnGameLiftSessionServiceFailed, this);
        }

        GameLiftClientServiceBus::Handler::BusConnect(gridMate);
    }

    void GameLiftClientService::OnServiceUnregistered(IGridMate* gridMate)
    {
        GameLiftClientServiceBus::Handler::BusDisconnect();

        if (m_clientStatus == GameLift_Ready)
        {
            Aws::GameLift::Client::Destroy();
            m_clientStatus = GameLift_NotInited;
        }

        delete m_clientInitOutcome;

        SessionService::OnServiceUnregistered(gridMate);
    }

    void GameLiftClientService::Update()
    {
        if (m_clientInitOutcome
            && m_clientInitOutcome->valid()
            && m_clientInitOutcome->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
        {
            auto result = m_clientInitOutcome->get();
            if (result.IsSuccess())
            {
                AZ_TracePrintf("GameLift", "Initialized GameLift client successfully.\n");
                m_clientStatus = GameLift_Ready;

                if (IsReady())
                {
                    EBUS_EVENT_ID(m_gridMate, GameLiftClientServiceEventsBus, OnGameLiftSessionServiceReady, this);
                    EBUS_DBG_EVENT(Debug::SessionDrillerBus, OnSessionServiceReady);
                    EBUS_EVENT_ID(m_gridMate, SessionEventBus, OnSessionServiceReady);
                }
            }
            else
            {
                AZ_TracePrintf("GameLift", "Failed to initialize GameLift client: %s, %s\n", result.GetError().GetErrorName().c_str(), result.GetError().GetErrorMessage());
                m_clientStatus = GameLift_Failed;
                EBUS_EVENT_ID(m_gridMate, GameLiftClientServiceEventsBus, OnGameLiftSessionServiceFailed, this);
            }
        }

        SessionService::Update();
    }

    GridSession* GameLiftClientService::JoinSessionBySearchInfo(const GameLiftSearchInfo& searchInfo, const CarrierDesc& carrierDesc)
    {
        if (m_clientStatus != GameLift_Ready)
        {
            AZ_TracePrintf("GameLift", "Client API is not initialized.\n");
            return nullptr;
        }

        GameLiftClientSession* session = aznew GameLiftClientSession(this);
        if (!session->Initialize(searchInfo, JoinParams(), carrierDesc))
        {
            delete session;
            return nullptr;
        }

        return session;
    }

    GameLiftSessionRequest* GameLiftClientService::RequestSession(const GameLiftSessionRequestParams& params)
    {
        if (m_clientStatus != GameLift_Ready)
        {
            AZ_TracePrintf("GameLift", "Client API is not initialized.\n");
            return nullptr;
        }

        GameLiftSessionRequest* request = aznew GameLiftSessionRequest(this, params);
        if (!request->Initialize())
        {
            delete request;
            return nullptr;
        }

        return request;
    }

    GameLiftSearch* GameLiftClientService::StartSearch(const GameLiftSearchParams& params)
    {
        if (m_clientStatus != GameLift_Ready)
        {
            AZ_TracePrintf("GameLift", "Client API is not initialized.\n");
            return nullptr;
        }

        GameLiftSearch* search = aznew GameLiftSearch(this, params);
        if (!search->Initialize())
        {
            delete search;
            return nullptr;
        }

        return search;
    }

    GameLiftClientSession* GameLiftClientService::QueryGameLiftSession(const GridSession* session)
    {
        for (GridSession* s : m_sessions)
        {
            if (s == session)
            {
                return static_cast<GameLiftClientSession*>(s);
            }
        }

        return nullptr;
    }

    GameLiftSearch* GameLiftClientService::QueryGameLiftSearch(const GridSearch* search)
    {
        for (GridSearch* s : m_activeSearches)
        {
            if (s == search)
            {
                return static_cast<GameLiftSearch*>(s);
            }
        }

        for (GridSearch* s : m_completedSearches)
        {
            if (s == search)
            {
                return static_cast<GameLiftSearch*>(s);
            }
        }

        return nullptr;
    }

    bool GameLiftClientService::StartGameLiftClient()
    {
        if (m_clientStatus == GameLift_NotInited)
        {
            if (!ValidateServiceDesc())
            {
                m_clientStatus = GameLift_Failed;
                AZ_TracePrintf("GameLift", "GameLift client failed to start. Invalid set of gamelift_* parameters.\n");
                return false;
            }

            Aws::Client::ClientConfiguration config = Aws::Client::ClientConfiguration();
            config.region = GetAwsRegionByName(m_serviceDesc.m_region.c_str());
            config.endpointOverride = m_serviceDesc.m_endpoint.c_str();
            config.verifySSL = true;

            Aws::GameLift::Client::ClientInitializeOutcome outcome = Aws::GameLift::Client::Initialize(config);

            if (outcome.IsSuccess())
            {
                Aws::String playerId;
                if (!m_serviceDesc.m_playerId.empty())
                {
                    playerId.assign(m_serviceDesc.m_playerId.c_str());
                }
                else
                {
                    playerId.assign(DEFAULT_PLAYER_ID);
                }

                m_clientStatus = GameLift_Initing;

                Aws::String accessKey(m_serviceDesc.m_accessKey.c_str());
                Aws::String secretKey(m_serviceDesc.m_secretKey.c_str());
                Aws::Auth::AWSCredentials cred = Aws::Auth::AWSCredentials(accessKey, secretKey);

                if (m_serviceDesc.m_aliasId.empty())
                {
                    //Using FleetId
                    Aws::String fleetId(m_serviceDesc.m_fleetId.c_str());
                    m_clientInitOutcome = new Aws::GameLift::GenericOutcomeCallable(Aws::GameLift::Client::SetTargetFleetAsync(fleetId, cred, playerId));
                }
                else
                {
                    //Using Alias Id
                    Aws::String aliasId(m_serviceDesc.m_aliasId.c_str());
                    m_clientInitOutcome = new Aws::GameLift::GenericOutcomeCallable(Aws::GameLift::Client::SetTargetAliasIdAsync(aliasId, cred, playerId));
                }
            }
            else
            {
                m_clientStatus = GameLift_Failed;
                AZ_TracePrintf("GameLift", "Initialization failed. GameLiftSDK failed to initialize: %s\n", outcome.GetError().GetErrorName().c_str());
            }
        }

        return m_clientStatus != GameLift_Failed;
    }

    bool GameLiftClientService::ValidateServiceDesc()
    {
        //Validation on inputs.
        //Service still supports the use of developers credentials on top of the player credentials.
        if (m_serviceDesc.m_fleetId.empty() && m_serviceDesc.m_aliasId.empty())
        {
            AZ_TracePrintf("GameLift", "You need to provide at least [gamelift_aliasid, gamelift_aws_access_key, gamelift_aws_secret_key] or [gamelift_fleetid, gamelift_aws_access_key, gamelift_aws_secret_key]\n");
            return false;
        }

        //If the user is a developer.
        if (!m_serviceDesc.m_fleetId.empty() && (m_serviceDesc.m_accessKey.empty() || m_serviceDesc.m_secretKey.empty()))
        {
            AZ_TracePrintf("GameLift", "Initialize failed. Trying to set the fleetId without providing gamelift_aws_access_key and gamelift_aws_secret_key.\n");
            return false;
        }

        //If the user is a player.
        if (!m_serviceDesc.m_aliasId.empty() && (m_serviceDesc.m_accessKey.empty() || m_serviceDesc.m_secretKey.empty()))
        {
            AZ_TracePrintf("GameLift", "Initialize failed. Trying to set the aliasId without providing gamelift_aws_access_key and gamelift_aws_secret_key.\n");
            return false;
        }

        return true;
    }
} // namespace GridMate

#endif // BUILD_GAMELIFT_CLIENT
