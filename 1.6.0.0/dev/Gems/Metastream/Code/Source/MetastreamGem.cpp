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
#include "MetastreamGem.h"
#include <FlowSystem/Nodes/FlowBaseNode.h>

#if defined(AZ_PLATFORM_WINDOWS_X64)
#include "CivetHttpServer.h"
#endif // Windows x64

#include <sstream>

#define METASTREAM_DEFAULT_SERVER_PORT 8082
#undef DEDICATED_SERVER

using namespace Metastream;

MetastreamGem::MetastreamGem()
    : m_serverEnabled(0)
    , m_serverPort(METASTREAM_DEFAULT_SERVER_PORT)
    , m_serverDocRootCVar(nullptr)
{
    MetastreamRequestBus::Handler::BusConnect();
}
MetastreamGem::~MetastreamGem()
{
    MetastreamRequestBus::Handler::BusDisconnect();
}

void MetastreamGem::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
    using namespace Metastream;

    switch (event)
    {
    case ESYSTEM_EVENT_FLOW_SYSTEM_REGISTER_EXTERNAL_NODES:
        RegisterExternalFlowNodes();
        break;

    case ESYSTEM_EVENT_GAME_POST_INIT:
        // Put your init code here
        // All other Gems will exist at this point
        m_serverDocRootCVar = REGISTER_STRING("metastream_docroot", "Gems/Metastream/Files", VF_NULL, "The document root for the Metastream HTTP Server.");
        REGISTER_CVAR2("metastream_enabled", &m_serverEnabled, 0, VF_NULL, "Enable or disable the Metastream feature.");
        REGISTER_CVAR2("metastream_serverPort", &m_serverPort, METASTREAM_DEFAULT_SERVER_PORT, VF_NULL, "The port to use for the Metastream HTTP server.");

        if (m_serverEnabled == 0)
        {
            // Do not enable the feature
            break;
        }

        // Initialise the cache
        m_cache = std::auto_ptr<DataCache>(new DataCache());
#if defined(AZ_PLATFORM_WINDOWS_X64)
#if !defined(DEDICATED_SERVER)
        {
            // Initialise and start the HTTP server
            m_server = std::auto_ptr<BaseHttpServer>(new CivetHttpServer(m_cache.get()));
            string serverDocRoot = m_serverDocRootCVar->GetString();
            CryLogAlways("Initializing Metastream Port=%u DocRoot=\"%s\"", m_serverPort, serverDocRoot.c_str());
            m_server->Start(m_serverPort, serverDocRoot.c_str());
        }
#endif
#endif // Windows x64
        break;

    case ESYSTEM_EVENT_FULL_SHUTDOWN:
    case ESYSTEM_EVENT_FAST_SHUTDOWN:
        // Put your shutdown code here
        // Other Gems may have been shutdown already, but none will have destructed
        if (m_server.get())
        {
            m_server->Stop();
        }
        break;
    }
}

void MetastreamGem::AddToCache(const char* table, const char* key, const char* value)
{
    if (m_cache.get())
    {
        m_cache->AddToCache(table, key, value);
    }
}

AZ_DECLARE_MODULE_CLASS(Metastream_c02d7efe05134983b5699d9ee7594c3a, Metastream::MetastreamGem)