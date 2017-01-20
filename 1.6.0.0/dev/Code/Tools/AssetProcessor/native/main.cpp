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
#if !defined(BATCH_MODE)
#include "utilities/GUIApplicationManager.h"
#else
#include "utilities/BatchApplicationManager.h"
#endif

#include <AzCore/PlatformDef.h>

static AZ_THREAD_LOCAL AZ::s64 s_currentJobId = 0;

namespace AssetProcessor
{
    AZ::s64 GetThreadLocalJobId()
    {
        return s_currentJobId;
    }

    void SetThreadLocalJobId(AZ::s64 jobId)
    {
        s_currentJobId = jobId;
    }
}

int main(int argc, char* argv[])
{
#if defined(BATCH_MODE)
    BatchApplicationManager applicationManager(argc, argv);
#else
    GUIApplicationManager applicationManager(argc, argv);
#endif

    ApplicationManager::BeforeRunStatus status = applicationManager.BeforeRun();

    if (status != ApplicationManager::BeforeRunStatus::Status_Success)
    {
        if (status == ApplicationManager::BeforeRunStatus::Status_Restarting)
        {
            //AssetProcessor will restart
            return 0;
        }
        else
        {
            //Initialization failed
            return 1;
        }
    }

    return applicationManager.Run() ? 0 : 1;
}

