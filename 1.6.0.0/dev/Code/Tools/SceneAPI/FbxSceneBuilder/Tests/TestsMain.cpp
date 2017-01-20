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
#include <SceneAPI/FbxSceneBuilder/FbxSceneBuilderStandaloneAllocator.h>


class FbxSceneBuilderTestEnvironment
    : public AZ::Test::ITestEnvironment
{
public:
    virtual ~FbxSceneBuilderTestEnvironment()
    {}

protected:
    void SetupEnvironment() override
    {
        AZ::SceneAPI::FbxSceneBuilderStandaloneAllocator::Initialize();

        sceneCoreModule = AZ::DynamicModuleHandle::Create("SceneCore");
        AZ_Assert(sceneCoreModule, "FbxSceneBuilder unit tests failed to create SceneCore module");
        AZ_Assert(sceneCoreModule->Load(true), "FbxSceneBuilder unit tests failed to load SceneCore module");

        sceneDataModule = AZ::DynamicModuleHandle::Create("SceneData");
        AZ_Assert(sceneDataModule, "FbxSceneBuilder unit tests failed to create SceneData module");
        AZ_Assert(sceneDataModule->Load(true), "FbxSceneBuilder unit tests failed to load SceneData module");
    }

    void TeardownEnvironment() override
    {
        sceneDataModule.reset();
        sceneCoreModule.reset();
        AZ::SceneAPI::FbxSceneBuilderStandaloneAllocator::TearDown();
    }

private:
    AZStd::unique_ptr<AZ::DynamicModuleHandle> sceneCoreModule;
    AZStd::unique_ptr<AZ::DynamicModuleHandle> sceneDataModule;
};

AZ_UNIT_TEST_HOOK(new FbxSceneBuilderTestEnvironment);