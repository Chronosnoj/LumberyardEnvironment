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

#include <GameLift/Session/GameLiftSearch.h>
#include <GameLift/Session/GameLiftClientService.h>

// guarding against AWS SDK & windows.h name conflict
#ifdef GetMessage
#undef GetMessage
#endif
#pragma warning(push)
#pragma warning(disable: 4819) // Invalid character not in default code page
#include <aws/gamelift/client/GameLiftClientAPI.h>
#include <aws/gamelift/model/GameSessionStatus.h>
#pragma warning(pop)

namespace GridMate
{
    GameLiftSearch::GameLiftSearch(GameLiftClientService* service, const GameLiftSearchParams& searchParams)
        : GridSearch(service)
        , m_searchParams(searchParams)
    {
        m_isDone = true;
    }

    bool GameLiftSearch::Initialize()
    {
        if (m_searchParams.m_gameInstanceId.empty())
        {
            m_searchOutcome = Aws::GameLift::Client::GetActiveGameSessionsAsync();
        }
        else
        {
            Aws::String gameSessionId(m_searchParams.m_gameInstanceId.c_str());
            m_getSessionOutcome = Aws::GameLift::Client::GetGameSessionAsync(gameSessionId);
        }

        m_isDone = false;
        return true;
    }

    unsigned int GameLiftSearch::GetNumResults() const
    {
        return static_cast<unsigned int>(m_results.size());
    }

    const SearchInfo* GameLiftSearch::GetResult(unsigned int index) const
    {
        return &m_results[index];
    }

    void GameLiftSearch::AbortSearch()
    {
        m_searchOutcome = SearchOutcome();
        m_getSessionOutcome = GetSessionOutcome();

        SearchDone();
    }

    void GameLiftSearch::SearchDone()
    {
        m_isDone = true;
    }

    void GameLiftSearch::Update()
    {
        if (m_isDone)
        {
            return;
        }

        if (m_searchOutcome.valid() && m_searchOutcome.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
        {
            auto result = m_searchOutcome.get();
            if (result.IsSuccess())
            {
                auto gameSessions = result.GetResult();
                for (auto& gameSession : gameSessions)
                {
                    ProcessGameSessionResult(gameSession);
                }
            }
            else
            {
                AZ_TracePrintf("GameLift", "Session search failed with error <%s> %s\n",
                    result.GetError().GetErrorName().c_str(),
                    result.GetError().GetErrorMessage().c_str());
            }

            SearchDone();
        }

        if (m_getSessionOutcome.valid() && m_getSessionOutcome.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
        {
            auto result = m_getSessionOutcome.get();
            if (result.IsSuccess())
            {
                ProcessGameSessionResult(result.GetResult());
            }
            else
            {
                AZ_TracePrintf("GameLift", "Failed to get session by id. Error: <%s> %s\n",
                    result.GetError().GetErrorName().c_str(),
                    result.GetError().GetErrorMessage().c_str());
            }

            SearchDone();
        }
    }

    void GameLiftSearch::ProcessGameSessionResult(const Aws::GameLift::Model::GameSession& gameSession)
    {
        GameLiftSearchInfo info;
        info.m_fleetId = gameSession.GetFleetId().c_str();
        info.m_gameInstanceId = gameSession.GetGameSessionId().c_str();
        info.m_sessionId = info.m_gameInstanceId.c_str();
        info.m_numFreePublicSlots = gameSession.GetMaximumPlayerSessionCount() - gameSession.GetCurrentPlayerSessionCount();
        info.m_numUsedPublicSlots = gameSession.GetCurrentPlayerSessionCount();
        info.m_numPlayers = gameSession.GetCurrentPlayerSessionCount();
        info.m_serverPort = gameSession.GetPort();

        auto& properties = gameSession.GetGameProperties();
        for (auto& prop : properties)
        {
            info.m_params[info.m_numParams].m_id = prop.GetKey().c_str();
            info.m_params[info.m_numParams].m_value = prop.GetValue().c_str();

            ++info.m_numParams;
        }

        m_results.push_back(info);
    }
} // namespace GridMate

#endif // BUILD_GAMELIFT_CLIENT
