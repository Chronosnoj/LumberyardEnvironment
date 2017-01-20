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

#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <SceneAPI/FbxSceneBuilder/FbxSkinBuilder.h>
#include <SceneAPI/FbxSceneBuilder/Tests/TestFbxSkin.h>
#include <SceneAPI/SceneData/GraphData/SkinMeshData.h>
#include <SceneAPI/SceneData/GraphData/SkinWeightData.h>
#include <SceneAPI/FbxSceneBuilder/Tests/FbxMeshBuilderTests.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace FbxSceneImporter
        {
            using ::testing::Return;
            using ::testing::NiceMock;
            
            TEST(FbxSkinBuilderSimpleTest, BuildSkin_FbxNodeHasNoMesh_ReturnsFalse)
            {
                Containers::SceneGraph graph;
                std::shared_ptr<NiceMock<FbxSDKWrapper::MockFbxMeshWrapper>> stubFbxMesh =
                    std::make_shared<NiceMock<FbxSDKWrapper::MockFbxMeshWrapper>>();
                std::shared_ptr<FbxSceneSystem> fbxSceneSystem = std::make_shared<FbxSceneSystem>();
                FbxSkinBuilder testFbxSkinBuilder(nullptr, graph, graph.GetRoot(), fbxSceneSystem);
                EXPECT_FALSE(testFbxSkinBuilder.BuildSkin());
                EXPECT_FALSE(graph.HasNodeContent(graph.GetRoot()));
            }
            
            TEST(FbxSkinBuilderSimpleTest, BuildSkin_FbxMeshHasNoSkinWeight_ReturnsFalse)
            {
                Containers::SceneGraph graph;
                std::shared_ptr<NiceMock<FbxSDKWrapper::MockFbxMeshWrapper>> stubFbxMesh =
                    std::make_shared<NiceMock<FbxSDKWrapper::MockFbxMeshWrapper>>();
                ON_CALL(*stubFbxMesh, GetDeformerCount())
                    .WillByDefault(Return(0));
                std::shared_ptr<FbxSceneSystem> fbxSceneSystem = std::make_shared<FbxSceneSystem>();
                FbxSkinBuilder testFbxSkinBuilder(stubFbxMesh, graph, graph.GetRoot(), fbxSceneSystem);
                EXPECT_FALSE(testFbxSkinBuilder.BuildSkin());
                EXPECT_FALSE(graph.HasNodeContent(graph.GetRoot()));
            }
            
            class FbxSkinBuilderIntegrationTests
                : public FbxMeshBuilderIntegrationTests
            {
            public:
                FbxSkinBuilderIntegrationTests()
                    : FbxMeshBuilderIntegrationTests()
                    , m_stubFbxSkin(new FbxSDKWrapper::TestFbxSkin())
                    , m_testFbxSkinBuilder(new FbxSceneImporter::FbxSkinBuilder(m_stubFbxMesh, m_graph, m_graph.GetRoot(), m_stubFbxSceneSystem))
                {
                }

                virtual ~FbxSkinBuilderIntegrationTests() = default;
                
            protected:
                virtual void SetUp()
                {
                }

                virtual void TearDown()
                {
                }

                void BuildSquareSkin()
                {
                    BuildSquare();
                    m_stubFbxSkin->SetName("skin0");
                    AZStd::vector<AZStd::string> boneNames = { "bone0", "bone1" };
                    AZStd::vector<AZStd::vector<double>> weights =
                    {
                        { 0.1, 0.2, 0.3, 0.4 },
                        { 0.9, 0.8, 0.7, 0.6 }
                    };
                    AZStd::vector<AZStd::vector<int>> controlPointIndices =
                    {
                        { 0, 1, 2, 3 },
                        { 0, 1, 2, 3 }
                    };
                    m_stubFbxSkin->CreateSkinWeightData(boneNames, weights, controlPointIndices);
                    
                    // Create expected output SkinData
                    AZStd::vector<AZStd::vector<int>> expectedOutputBoneIds =
                    {
                        { 0, 1 }, { 0, 1 }, { 0, 1 }, { 0, 1 }
                    };
                    AZStd::vector<AZStd::vector<float>> expectedOutputWeights =
                    {
                        { 0.1f, 0.9f }, { 0.2f, 0.8f }, { 0.3f, 0.7f }, { 0.4f, 0.6f }
                    };
                    m_stubFbxSkin->CreateExpectSkinWeightData(expectedOutputBoneIds, expectedOutputWeights);
                    m_stubFbxMesh->SetSkin(m_stubFbxSkin);
                }

                void BuildCubeSkin()
                {
                    BuildCube();

                    m_stubFbxSkin->SetName("skin0");
                    AZStd::vector<AZStd::string> boneNames = { "bone0", "bone1", "bone2", "bone3" };
                    AZStd::vector<AZStd::vector<double>> weights =
                    {
                        { 0.1, 0.2, 0.3, 0.4 },
                        { 0.9, 0.8, 0.5, 0.6 },
                        { 0.7, 0.6, 0.9, 0.8 },
                        { 0.5, 0.4, 0.1, 0.2 }
                    };
                    AZStd::vector<AZStd::vector<int>> controlPointIndices =
                    {
                        { 0, 1, 2, 3 },
                        { 0, 1, 4, 5 },
                        { 2, 3, 6, 7 },
                        { 4, 5, 6, 7 }
                    };
                    m_stubFbxSkin->CreateSkinWeightData(boneNames, weights, controlPointIndices);

                    // Create expected output SkinData
                    AZStd::vector<AZStd::vector<int>> expectedOutputBoneIds =
                    {
                        { 0, 1 }, { 0, 1 }, { 0, 2 }, { 0, 2 },
                        { 1, 3 }, { 1, 3 }, { 2, 3 }, { 2, 3 }
                    };
                    AZStd::vector<AZStd::vector<float>> expectedOutputWeights = 
                    {
                        { 0.1f, 0.9f }, { 0.2f, 0.8f }, { 0.3f, 0.7f }, { 0.4f, 0.6f },
                        { 0.5f, 0.5f }, { 0.6f, 0.4f }, { 0.9f, 0.1f }, { 0.8f, 0.2f }
                    };
                    m_stubFbxSkin->CreateExpectSkinWeightData(expectedOutputBoneIds, expectedOutputWeights);
                    m_stubFbxMesh->SetSkin(m_stubFbxSkin);
                }

                void CheckBuildSkinData()
                {
                    AZStd::shared_ptr<DataTypes::IGraphObject> buildObject = m_graph.GetNodeContent(m_graph.GetRoot());
                    EXPECT_NE(nullptr, buildObject);

                    AZStd::shared_ptr<const SceneData::GraphData::SkinMeshData> buildSkin = azrtti_cast<const SceneData::GraphData::SkinMeshData*>(buildObject);
                    EXPECT_NE(nullptr, buildSkin);

                    Containers::SceneGraph::NodeIndex index = m_graph.GetRoot();
                    index = m_graph.GetNodeChild(index);
                    EXPECT_TRUE(index.IsValid());
                    
                    AZStd::shared_ptr<const SceneData::GraphData::SkinWeightData> buildSkinWeight = azrtti_cast<const SceneData::GraphData::SkinWeightData*>(m_graph.GetNodeContent(index));
                    EXPECT_NE(nullptr, buildSkinWeight);

                    EXPECT_EQ(buildSkinWeight->GetVertexCount(), m_stubFbxSkin->GetExpectedVertexCount());
                    for (size_t vertexIndex = 0; vertexIndex < buildSkinWeight->GetVertexCount(); ++vertexIndex)
                    {
                        EXPECT_EQ(buildSkinWeight->GetLinkCount(vertexIndex), m_stubFbxSkin->GetExpectedLinkCount(vertexIndex));
                        for (size_t linkIndex = 0; linkIndex < buildSkinWeight->GetLinkCount(vertexIndex); ++linkIndex)
                        {
                            const DataTypes::ISkinWeightData::Link link = buildSkinWeight->GetLink(vertexIndex, linkIndex);
                            EXPECT_EQ(link.boneId, m_stubFbxSkin->GetExpectedSkinLinkBoneId(vertexIndex, linkIndex));
                            EXPECT_EQ(link.weight, m_stubFbxSkin->GetExpectedSkinLinkWeight(vertexIndex, linkIndex));
                        }
                    }
                }

                AZStd::shared_ptr<FbxSDKWrapper::TestFbxSkin> m_stubFbxSkin;
                AZStd::unique_ptr<FbxSkinBuilder> m_testFbxSkinBuilder;
            };

            TEST_F(FbxSkinBuilderIntegrationTests, BuildSkin_SkinMeshInputStandardSquare_EquivalentSkinWeightData)
            {
                BuildSquareSkin();
                ASSERT_TRUE(m_testFbxSkinBuilder->BuildSkin());
                CheckBuildSkinData();
            }

            TEST_F(FbxSkinBuilderIntegrationTests, BuildSkin_SkinMeshInputStandardCube_EquivalentSkinWeightData)
            {
                BuildCubeSkin();
                ASSERT_TRUE(m_testFbxSkinBuilder->BuildSkin());
                CheckBuildSkinData();
            }
        }
    }
}