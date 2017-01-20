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
#include <AzTest/AzTest.h>
#include <AzCore/Memory/AllocatorManager.h>
#include <AzCore/Memory/OSAllocator.h>
#include <AzCore/Memory/SystemAllocator.h>

class GemRegistryTestEnvironment
    : public AZ::Test::ITestEnvironment
{
public:
    virtual ~GemRegistryTestEnvironment()
    {}

protected:
    void SetupEnvironment() override
    {
        AZ::AllocatorInstance<AZ::OSAllocator>::Create();
        AZ::AllocatorInstance<AZ::SystemAllocator>::Create();
    }

    void TeardownEnvironment() override
    {
        AZ::AllocatorInstance<AZ::OSAllocator>::Destroy();
        AZ::AllocatorInstance<AZ::SystemAllocator>::Destroy();
    }
};

AZ_UNIT_TEST_HOOK(new GemRegistryTestEnvironment);