
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

#include <SceneAPI/FbxSceneBuilder/Tests/FbxMeshBuilderTests.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace FbxSceneImporter
        {
            using ::testing::Return;
            using ::testing::NiceMock;
            using ::testing::_;

            TEST(FbxMeshBuilderSimpleTest, BuildMesh_FbxNodeHasNoMesh_ReturnsFalse)
            {
                Containers::SceneGraph graph;
                std::shared_ptr<FbxSceneSystem> fbxSceneSystem = std::make_shared<FbxSceneSystem>();
                FbxMeshBuilder testFbxMeshBuilder(nullptr, graph, graph.GetRoot(), fbxSceneSystem);
                EXPECT_FALSE(testFbxMeshBuilder.BuildMesh());
                EXPECT_FALSE(graph.HasNodeContent(graph.GetRoot()));
            }

            TEST(FbxMeshBuilderSimpleTest, BuildMesh_FbxMeshHasNoPolygon_ReturnsFalse)
            {
                std::shared_ptr<NiceMock<FbxSDKWrapper::MockFbxMeshWrapper> > stubFbxMesh =
                    std::make_shared<NiceMock<FbxSDKWrapper::MockFbxMeshWrapper> >();
                ON_CALL(*stubFbxMesh, GetPolygonCount())
                    .WillByDefault(Return(0));

                Containers::SceneGraph graph;
                std::shared_ptr<FbxSceneSystem> fbxSceneSystem = std::make_shared<FbxSceneSystem>();
                FbxMeshBuilder testFbxMeshBuilder(stubFbxMesh, graph, graph.GetRoot(), fbxSceneSystem);
                EXPECT_FALSE(testFbxMeshBuilder.BuildMesh());
                EXPECT_FALSE(graph.HasNodeContent(graph.GetRoot()));
            }

            TEST_F(FbxMeshBuilderIntegrationTests, BuildMesh_MeshInputStandardSquareNoMaterials_EquivalentMeshData)
            {
                BuildSquare();
                ASSERT_TRUE(m_testFbxMeshBuilder->BuildMesh());
                CheckBuildMeshData();
            }

            TEST_F(FbxMeshBuilderIntegrationTests, BuildMesh_MeshInputStandardCubeNoMaterials_EquivalentMeshData)
            {
                BuildCube();
                ASSERT_TRUE(m_testFbxMeshBuilder->BuildMesh());
                CheckBuildMeshData();
            }
        }
    }
}