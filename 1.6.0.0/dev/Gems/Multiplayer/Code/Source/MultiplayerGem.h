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

#include "Multiplayer/IMultiplayerGem.h"
#include "MultiplayerCVars.h"
#include "CrySystemBus.h"

namespace GridMate
{
    class SecureSocketDriver;
}

namespace Multiplayer
{
    class GameLiftListener;

    class MultiplayerModule
        : public CryHooksModule
        , public MultiplayerRequestBus::Handler
        , public GridMate::SessionEventBus::Handler
    {
        friend class MultiplayerCVars;
    public:

        AZ_RTTI(MultiplayerModule, "{946D16FF-7C9D-4134-88F9-03FAE5D5803A}", CryHooksModule);

        MultiplayerModule();
        ~MultiplayerModule() override;

        ////////////////////
        // IMultiplayerGem
        bool IsNetSecEnabled() const override;
        bool IsNetSecVerifyClient() const override;

#if defined(NET_SUPPORT_SECURE_SOCKET_DRIVER)
        void RegisterSecureDriver(GridMate::SecureSocketDriver* driver) override;
#endif
                        
        GridMate::GridSession* GetSession() override;
        void RegisterSession(GridMate::GridSession* session) override;
        GridMate::Simulator* GetSimulator() override;
        void EnableSimulator() override;
        void DisableSimulator() override;
        ////////////////////

    private:

        void OnCrySystemInitialized(ISystem&, const SSystemInitParams&) override;

        void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;

        void OnSessionCreated(GridMate::GridSession* session);
        void OnSessionDelete(GridMate::GridSession* session);

        //! Current game session
        GridMate::GridSession* m_session;

        //! Secure driver
        GridMate::SecureSocketDriver* m_secureDriver;

        //! Network specific commands and cvars.
        MultiplayerCVars m_cvars;

        GridMate::DefaultSimulator* m_simulator;
        GameLiftListener* m_gameLiftListener;

        static int s_NetsecEnabled;
        static int s_NetsecVerifyClient;        
    };
} // namespace Multiplayer
