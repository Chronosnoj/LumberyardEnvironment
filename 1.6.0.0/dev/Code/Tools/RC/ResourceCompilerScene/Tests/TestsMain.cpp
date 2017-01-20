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

#include <AzCore/Module/Environment.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Module/DynamicModuleHandle.h>

void __stdcall InitializeAzEnvironment(AZ::EnvironmentInstance sharedEnvironment);
void __stdcall BeforeUnloadDLL();

class ResourceCompilerSceneTestEnvironment
    : public AZ::Test::ITestEnvironment
{
public:
    virtual ~ResourceCompilerSceneTestEnvironment()
    {}

protected:
    void SetupEnvironment() override
    {
        if (!AZ::AllocatorInstance<AZ::SystemAllocator>().IsReady())
        {
            AZ::AllocatorInstance<AZ::SystemAllocator>().Create();
            m_hasLocalMemoryAllocator = true;
        }

        sceneCoreModule = AZ::DynamicModuleHandle::Create("SceneCore");
        AZ_Assert(sceneCoreModule, "ResourceCompilerScene unit tests failed to create SceneCore module");
        AZ_Assert(sceneCoreModule->Load(true), "ResourceCompilerScene unit tests failed to load SceneCore module");

        sceneDataModule = AZ::DynamicModuleHandle::Create("SceneData");
        AZ_Assert(sceneDataModule, "ResourceCompilerScene unit tests failed to create SceneData module");
        AZ_Assert(sceneDataModule->Load(true), "ResourceCompilerScene unit tests failed to load SceneData module");

        fbxSceneBuilderModule = AZ::DynamicModuleHandle::Create("FbxSceneBuilder");
        AZ_Assert(fbxSceneBuilderModule, "ResourceCompilerScene unit tests failed to create FbxSceneBuilder module");
        AZ_Assert(fbxSceneBuilderModule->Load(true), "ResourceCompilerScene unit tests failed to load FbxSceneBuilder module");
    }

    void TeardownEnvironment() override
    {
        sceneDataModule.reset();
        sceneCoreModule.reset();
        fbxSceneBuilderModule.reset();
        if (m_hasLocalMemoryAllocator)
        {
            AZ::AllocatorInstance<AZ::SystemAllocator>().Destroy();
        }
    }

private:
    bool m_hasLocalMemoryAllocator = false;
    AZStd::unique_ptr<AZ::DynamicModuleHandle> sceneCoreModule;
    AZStd::unique_ptr<AZ::DynamicModuleHandle> sceneDataModule;
    AZStd::unique_ptr<AZ::DynamicModuleHandle> fbxSceneBuilderModule;
};

AZ_UNIT_TEST_HOOK(new ResourceCompilerSceneTestEnvironment);

TEST(ResourceCompilerSceneTests, Sanity)
{
    EXPECT_EQ(1, 1);
}