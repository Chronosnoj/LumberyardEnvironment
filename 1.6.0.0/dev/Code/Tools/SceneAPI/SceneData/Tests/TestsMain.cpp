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

#include <AzCore/Module/DynamicModuleHandle.h>
#include <SceneAPI/SceneData/SceneDataStandaloneAllocator.h>

class SceneDataTestEnvironment
    : public AZ::Test::ITestEnvironment
{
public:
    virtual ~SceneDataTestEnvironment()
    {}

protected:
    void SetupEnvironment() override
    {
        AZ::SceneAPI::SceneDataStandaloneAllocator::Initialize();

        sceneCoreModule = AZ::DynamicModuleHandle::Create("SceneCore");
        AZ_Assert(sceneCoreModule, "SceneData unit tests failed to create SceneCore module");
        AZ_Assert(sceneCoreModule->Load(true), "SceneData unit tests failed to load SceneCore module");
    }

    void TeardownEnvironment() override
    {
        sceneCoreModule.reset();
        AZ::SceneAPI::SceneDataStandaloneAllocator::TearDown();
    }

private:
    AZStd::unique_ptr<AZ::DynamicModuleHandle> sceneCoreModule;
};

AZ_UNIT_TEST_HOOK(new SceneDataTestEnvironment);