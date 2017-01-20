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

#include <GridMate/Session/Session.h>

#if BUILD_GAMELIFT_SERVER

#pragma warning(push)
#pragma warning(disable: 4355) // <future> includes ppltasks.h which throws a C4355 warning: 'this' used in base member initializer list
#include <aws/gamelift/server/GameLiftServerAPI.h>
#pragma warning(pop)

namespace GridMate
{
    /*!
    * GameLift specific session parameters placeholder used for hosting the session
    */
    struct GameLiftSessionParams
        : public SessionParams
    {
        GameLiftSessionParams()
            : m_gameSession(nullptr)
        {
        }

        const Aws::GameLift::Server::Model::GameSession* m_gameSession; ///< Pointer to GameSession object returned by GameLift session service
    };
}
#endif // BUILD_GAMELIFT_SERVER

#if BUILD_GAMELIFT_CLIENT

namespace GridMate
{
    /*!
    * Session creation request parameters
    */
    struct GameLiftSessionRequestParams
        : public SessionParams
    {
        string m_instanceName; //! Name of the game instance. Title players will see when browsing for instances.
    };

    /*!
    * Info returned from a GameLift search, required for joining the instance.
    */
    struct GameLiftSearchInfo
        : public SearchInfo
    {
        string m_fleetId; //< GameLift fleet Id
        string m_gameInstanceId; //< GameLift game instance id
        int m_serverPort; //< Server's port to connect to
    };

    /*!
    * GameLift specific search parameters placeholder
    */
    struct GameLiftSearchParams
        : public SearchParams
    {
        string m_gameInstanceId; //< if not empty, only specific session with the given instance id will be returned
    };
} // namespace GridMate

#endif // BUILD_GAMELIFT_CLIENT

