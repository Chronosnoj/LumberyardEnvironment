
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
#include <SceneAPI/FbxSceneBuilder/FbxImporter.h>
#include <SceneAPI/FbxSDKWrapper/Mocks/MockFbxSceneWrapper.h>
#include <SceneAPI/FbxSDKWrapper/Mocks/MockFbxNodeWrapper.h>
#include <SceneAPI/FbxSDKWrapper/Mocks/MockFbxSystemUnitWrapper.h>
#include <SceneAPI/FbxSDKWrapper/Mocks/MockFbxAxisSystemWrapper.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace FbxSceneImporter
        {
            using ::testing::NiceMock;
            using ::testing::Return;
            using ::testing::ReturnRef;
            using ::testing::Matcher;
            using ::testing::_;

            TEST(FbxImporterSimpleTests, GetDefaultFileExtension_Called_ReturnsFbx)
            {
                FbxImporter testFbxImporter;
                const char* fileExtension = testFbxImporter.GetDefaultFileExtension();
                EXPECT_TRUE(strcmp("fbx", fileExtension) == 0);
            }

            class FbxImporterFakeIOTests
                : public ::testing::Test
            {
            public:
                FbxImporterFakeIOTests()
                    : m_stubFbxScene(new NiceMock<FbxSDKWrapper::MockFbxSceneWrapper>())
                    , m_stubFbxNode(std::make_shared<NiceMock<FbxSDKWrapper::MockFbxNodeWrapper> >())
                    , m_testOutScene("testScene")
                    , m_stubFbxSystemUnit(new NiceMock<FbxSDKWrapper::MockFbxSystemUnitWrapper>())
                    , m_stubFbxAxisSystem(new NiceMock<FbxSDKWrapper::MockFbxAxisSystemWrapper>())
                {
                    FbxSDKWrapper::FbxSceneWrapper* basePtr = m_stubFbxScene;
                    std::unique_ptr<FbxSDKWrapper::FbxSceneWrapper> sceneUniquePtr(basePtr);
                    m_testFbxImporter.reset(new FbxImporter(std::move(sceneUniquePtr)));
                }

            protected:
                virtual void SetUp()
                {
                    m_identityMatrix = Transform::Identity();
                    m_zeroVector3 = Vector3::CreateZero();
                    m_oneVector3 = Vector3(1.0f, 1.0f, 1.0f);

                    ON_CALL(*m_stubFbxSystemUnit, GetConversionFactorTo(Matcher<FbxSDKWrapper::FbxSystemUnitWrapper::Unit>(_)))
                        .WillByDefault(Return(1.0f));
                    ON_CALL(*m_stubFbxAxisSystem, GetUpVector(Matcher<int>(_)))
                        .WillByDefault(Return(FbxSDKWrapper::FbxAxisSystemWrapper::Z));
                    ON_CALL(*m_stubFbxAxisSystem, CalculateConversionTransform(Matcher<FbxSDKWrapper::FbxAxisSystemWrapper::UpVector>(_)))
                        .WillByDefault(Return(Transform::CreateIdentity()));
                    ON_CALL(*m_stubFbxNode, GetName())
                        .WillByDefault(Return(m_fakeFbxRootName));
                    ON_CALL(*m_stubFbxNode, GetMesh())
                        .WillByDefault(Return(nullptr));
                    ON_CALL(*m_stubFbxNode, EvaluateLocalTransform())
                        .WillByDefault(Return(m_identityMatrix));
                    ON_CALL(*m_stubFbxNode, GetGeometricTransform())
                        .WillByDefault(Return(m_identityMatrix));
                    ON_CALL(*m_stubFbxNode, GetChildCount())
                        .WillByDefault(Return(0));
                    ON_CALL(*m_stubFbxScene, GetSystemUnit())
                        .WillByDefault(Return(m_stubFbxSystemUnit));
                    ON_CALL(*m_stubFbxScene, GetAxisSystem())
                        .WillByDefault(Return(m_stubFbxAxisSystem));

                }

                virtual void TearDown()
                {
                }

                bool IsCorrectlyNamedEmptyRootNode(const Containers::SceneGraph& sceneGraph, Containers::SceneGraph::NodeIndex nodeIndex)
                {
                    if (!nodeIndex.IsValid())
                    {
                        return false;
                    }

                    if (sceneGraph.HasNodeChild(nodeIndex))
                    {
                        return false;
                    }

                    if (sceneGraph.HasNodeSibling(nodeIndex))
                    {
                        return false;
                    }

                    if (strcmp(sceneGraph.GetNodeName(nodeIndex).c_str(), m_fakeFbxRootName) != 0)
                    {
                        return false;
                    }

                    return true;
                }

                // Note: m_stubScene is managed as a unique_ptr in the FbxImporter, and will be cleaned up when that object goes out of scope
                NiceMock<FbxSDKWrapper::MockFbxSceneWrapper>* m_stubFbxScene;
                const char* m_fakeFbxRootName = "fakeRoot";
                std::shared_ptr<NiceMock<FbxSDKWrapper::MockFbxNodeWrapper> > m_stubFbxNode;
                AZStd::shared_ptr<NiceMock<FbxSDKWrapper::MockFbxSystemUnitWrapper>> m_stubFbxSystemUnit;
                AZStd::shared_ptr<NiceMock<FbxSDKWrapper::MockFbxAxisSystemWrapper>> m_stubFbxAxisSystem;
                std::unique_ptr<FbxImporter> m_testFbxImporter;
                Transform m_identityMatrix;
                Vector3 m_zeroVector3;
                Vector3 m_oneVector3;
                Containers::Scene m_testOutScene;
                
            };

            // Tests
            // GetDefaultSceneExtension
            // PopulateSceneFromFile
            //  FBX SDK Wrapper can't load
            //  ConvertFbxScene
            //      Invalid FBX Scene Root
            //      FBX Scene not changed
            //      Single Root, no children
            //      AppendGraphChildRecursive call count is expected based on input
            //



            TEST_F(FbxImporterFakeIOTests, PopulateFromFile_FbxSDKCannotLoadFile_ReturnsFalse)
            {
                ON_CALL(*m_stubFbxScene, LoadSceneFromFile(Matcher<const char*>(_)))
                    .WillByDefault(Return(false));

                bool populateSuccess = m_testFbxImporter->PopulateFromFile("fake.fbx", m_testOutScene);
                EXPECT_FALSE(populateSuccess);
            }

            TEST_F(FbxImporterFakeIOTests, PopulateSceneFromFile_FbxSceneHasNoRoot_ReturnsFalse)
            {
                ON_CALL(*m_stubFbxScene, LoadSceneFromFile(Matcher<const char*>(_)))
                    .WillByDefault(Return(true));
                ON_CALL(*m_stubFbxScene, GetRootNode())
                    .WillByDefault(Return(std::shared_ptr<FbxSDKWrapper::FbxNodeWrapper>(nullptr)));

                bool populateSuccess = m_testFbxImporter->PopulateFromFile("fake.fbx", m_testOutScene);
                EXPECT_FALSE(populateSuccess);
            }

            TEST_F(FbxImporterFakeIOTests, PopulateSceneFromFile_FbxSceneHasRoot_ReturnsTrue)
            {
                ON_CALL(*m_stubFbxScene, LoadSceneFromFile(Matcher<const char*>(_)))
                    .WillByDefault(Return(true));
                ON_CALL(*m_stubFbxScene, GetRootNode())
                    .WillByDefault(Return(m_stubFbxNode));

                bool populateSuccess = m_testFbxImporter->PopulateFromFile("fake.fbx", m_testOutScene);
                EXPECT_TRUE(populateSuccess);
            }

            TEST_F(FbxImporterFakeIOTests, PopulateSceneFromFile_FbxSceneHasRootNoChildren_SceneGraphHasRootNodeNoChildren)
            {
                ON_CALL(*m_stubFbxScene, LoadSceneFromFile(Matcher<const char*>(_)))
                    .WillByDefault(Return(true));
                ON_CALL(*m_stubFbxScene, GetRootNode())
                    .WillByDefault(Return(m_stubFbxNode));

                bool populateSuccess = m_testFbxImporter->PopulateFromFile("fake.fbx", m_testOutScene);

                const Containers::SceneGraph& sceneGraph = m_testOutScene.GetGraph();
                Containers::SceneGraph::NodeIndex nodeIndex = sceneGraph.GetRoot();

                // We expect a single FBX Scene with only a root to be equivalent to a SceneGraph root with a single child.
                // The child should be equivalent to the FBX Root Node
                ASSERT_TRUE(populateSuccess && sceneGraph.HasNodeChild(nodeIndex));

                Containers::SceneGraph::NodeIndex fbxRootIndex = sceneGraph.GetNodeChild(nodeIndex);

                EXPECT_TRUE(IsCorrectlyNamedEmptyRootNode(sceneGraph, fbxRootIndex));
            }
        }
    }
}

