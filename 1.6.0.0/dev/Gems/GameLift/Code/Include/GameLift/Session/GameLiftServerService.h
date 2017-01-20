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

#if BUILD_GAMELIFT_SERVER

#include <GameLift/Session/GameLiftServerServiceBus.h>
#include <GameLift/Session/GameLiftServerServiceEventsBus.h>
#include <GameLift/Session/GameLiftSessionDefs.h>

#include <GridMate/Session/Session.h>
#include <AzCore/std/smart_ptr/scoped_ptr.h>

namespace GridMate
{
    class GameLiftSession;
    struct GameLiftSessionParams;
    struct GameLiftSessionRequestParams;

    /*!
    * GameLift server service settings.
    */
    struct GameLiftServerServiceDesc
        : public SessionServiceDesc
    {
        GameLiftServerServiceDesc()
            : m_port(0)
        {
        }

        //! Log paths the servers will write to, these will be uploaded to the gamelift dashboard. Both relative to the game root folder and absolute paths supported.
        vector<string> m_logPaths;

        //! The port the server will be listening on
        int m_port;
    };

    /*!
    * GameLift server service.
    */
    class GameLiftServerService
        : public SessionService
        , public Internal::GameLiftServerSystemEventsBus::Handler
        , public GameLiftServerServiceBus::Handler
    {
    public:

        GM_CLASS_ALLOCATOR(GameLiftServerService);
        GRIDMATE_SERVICE_ID(GameLiftServerService);

        GameLiftServerService(const GameLiftServerServiceDesc& desc);

        bool IsReady() const;

        // GameLiftServerServiceBus
        GridSession* HostSession(const GameLiftSessionParams& params, const CarrierDesc& carrierDesc) override;
        GameLiftServerSession* QueryGameLiftSession(const GridSession* session) override;

        // GameLiftServerSystemEventsBus
        void OnGameLiftGameSessionStarted(const Aws::GameLift::Server::Model::GameSession& gameSession) override;
        void OnGameLiftServerWillTerminate() override;

        // GridMateService interface
        void OnServiceRegistered(IGridMate* gridMate) override;
        void OnServiceUnregistered(IGridMate* gridMate) override;

    protected:
        // SessionService
        void Update() override;

        bool StartGameLiftServer();

        enum GameLiftStatus
        {
            GameLift_NotInited, ///< GameLift SDK is not initialized
            GameLift_Initing, ///< Pending GameLift SDK initialization
            GameLift_Ready, ///< GameLift SDK is ready to use
            GameLift_Failed, ///< GameLift SDK failed to initialize
            GameLift_Terminated ///< Current instance was force terminated by the user (only applies to the server)
        };

        GameLiftServerServiceDesc m_serviceDesc;
        GameLiftStatus m_serverStatus;
        Aws::GameLift::GenericOutcomeCallable* m_serverInitOutcome;
    };
} // namespace GridMate

#endif // BUILD_GAMELIFT_SERVER
