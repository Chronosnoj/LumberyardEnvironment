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
#include "StaticDataGem.h"
#include <FlowSystem/Nodes/FlowBaseNode.h>
#include <AzCore/std/smart_ptr/make_shared.h>

// This is a temporary workaround for the fact that we don't have NativeSDK on all platforms yet
#if defined(PLATFORM_SUPPORTS_AWS_NATIVE_SDK)

#include "StaticDataManager.h"

#endif

void StaticData::StaticDataGem::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
    using namespace StaticData;

    switch (event)
    {
    case ESYSTEM_EVENT_FLOW_SYSTEM_REGISTER_EXTERNAL_NODES:
        RegisterExternalFlowNodes();
        break;

    case ESYSTEM_EVENT_GAME_POST_INIT:
        // Put your init code here
        // All other Gems will exist at this point
    {
#if defined(PLATFORM_SUPPORTS_AWS_NATIVE_SDK)
        // This is a temporary workaround for the fact that we don't have the NativeSDK on all platforms yet
        m_staticDataManager = AZStd::make_shared<CloudCanvas::StaticData::StaticDataManager>();
#endif
    }
        break;

    case ESYSTEM_EVENT_FULL_SHUTDOWN:
    case ESYSTEM_EVENT_FAST_SHUTDOWN:
        // Put your shutdown code here
        // Other Gems may have been shutdown already, but none will have destructed
#if defined(PLATFORM_SUPPORTS_AWS_NATIVE_SDK)
        // This is a temporary workaround for the fact that we don't have the NativeSDK on platforms other than windows
        m_staticDataManager.reset();
#endif
        break;
    }
}

AZ_DECLARE_MODULE_CLASS(StaticData_4a4bf593603c4f329c76c2a10779311b, StaticData::StaticDataGem)
