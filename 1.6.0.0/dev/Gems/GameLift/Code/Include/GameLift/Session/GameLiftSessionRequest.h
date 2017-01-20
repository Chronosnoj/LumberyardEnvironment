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

#if BUILD_GAMELIFT_CLIENT

#include <GameLift/Session/GameLiftSearch.h>
#include <GameLift/Session/GameLiftSessionDefs.h>

#ifdef GetMessage
#undef GetMessage
#endif

#pragma warning(push)
#pragma warning(disable: 4819) // Invalid character not in default code page
#pragma warning(disable: 4355) // <future> includes ppltasks.h which throws a C4355 warning: 'this' used in base member initializer list
#include <aws/gamelift/client/GameLiftClientAPI.h>
#pragma warning(pop)

namespace GridMate
{
    class GameLiftClientService;

    /*!
    * Sends request to GameLift backend to create new session with given parameters.
    * When request is completed - ordinary session search ebus events are issued.
    */
    class GameLiftSessionRequest : public GameLiftSearch
    {
        friend class GameLiftClientService;

    public:
        GM_CLASS_ALLOCATOR(GameLiftSessionRequest);

        void AbortSearch() override;

    private:
        GameLiftSessionRequest(GameLiftClientService* service, const GameLiftSessionRequestParams& params);
        bool Initialize();

        void Update() override;
        void SearchDone();

        typedef Aws::GameLift::Client::CreateGameSessionOutcomeCallable CreateOutcome;

        CreateOutcome m_createOutcome;
        GameLiftSessionRequestParams m_requestParams;
    };
} // namespace GridMate

#endif
