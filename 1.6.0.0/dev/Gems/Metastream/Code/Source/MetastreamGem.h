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

#include <Metastream/MetastreamBus.h>
#include "BaseHttpServer.h"
#include "DataCache.h"

namespace Metastream
{
    class MetastreamGem
        : public CryHooksModule
        , public MetastreamRequestBus::Handler
    {
        AZ_RTTI(MetastreamGem, "{0BACF38B-9774-4771-89E2-B099EA9E3FE7}", CryHooksModule);

    public:
        MetastreamGem();
        ~MetastreamGem() override;

        void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;

        virtual void AddToCache(const char* table, const char* key, const char* value) override;

    private:
        // CVars
        int m_serverEnabled;
        int m_serverPort;
        ICVar* m_serverDocRootCVar;

        std::auto_ptr<BaseHttpServer> m_server;
        std::auto_ptr<DataCache> m_cache;
    };
} // namespace Metastream
