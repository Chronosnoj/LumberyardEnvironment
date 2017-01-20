#pragma once

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
#include <AzCore/Math/Transform.h>
#include <SceneAPI/FbxSceneBuilder/FbxMeshBuilder.h>
#include <SceneAPI/FbxSDKWrapper/Mocks/MockFbxNodeWrapper.h>
#include <SceneAPI/FbxSDKWrapper/Mocks/MockFbxMeshWrapper.h>
#include <SceneAPI/FbxSceneBuilder/Tests/TestFbxNode.h>
#include <SceneAPI/SceneData/GraphData/MeshData.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshData.h>
#include <SceneAPI/FbxSceneBuilder/FbxSceneSystem.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace FbxSceneImporter
        {
            class FbxMeshBuilderIntegrationTests
                : public ::testing::Test
            {
            public:
                FbxMeshBuilderIntegrationTests()
                    : m_stubFbxMesh(new FbxSDKWrapper::TestFbxMesh())
                    , m_stubFbxSceneSystem(new FbxSceneSystem())
                {
                    m_testFbxMeshBuilder = std::make_unique<FbxMeshBuilder>(m_stubFbxMesh, m_graph, m_graph.GetRoot(), m_stubFbxSceneSystem);
                }

                virtual ~FbxMeshBuilderIntegrationTests() = default;

            protected:
                virtual void SetUp()
                {
                }

                virtual void TearDown()
                {
                }

                // Build testing primitives
                //      Squares, Cubes
                void BuildSquare()
                {
                    // Create test fbx mesh data
                    std::vector<AZ::Vector3> positions;
                    positions.reserve(4);
                    positions.emplace_back(1.0f, 0.0f, 1.0f);
                    positions.emplace_back(1.0f, 0.0f, -1.0f);
                    positions.emplace_back(-1.0f, 0.0f, -1.0f);
                    positions.emplace_back(-1.0f, 0.0f, 1.0f);
                    std::vector<std::vector<int> > polygonVertexIndices =
                    {
                        { 0, 1, 2, 3 }
                    };
                    m_stubFbxMesh->CreateMesh(positions, polygonVertexIndices);

                    // Create expected output MeshData
                    std::vector<std::vector<int> > expectedOutputFaceVertexIndices =
                    {
                        { 0, 1, 2 }, { 3, 0, 2 }
                    };
                    m_stubFbxMesh->CreateExpectMeshInfo(expectedOutputFaceVertexIndices);
                }

                void BuildCube()
                {
                    std::vector<AZ::Vector3> positions;
                    positions.reserve(8);
                    positions.emplace_back(1.0f, 1.0f, 1.0f);
                    positions.emplace_back(-1.0f, 1.0f, 1.0f);
                    positions.emplace_back(-1.0f, -1.0f, 1.0f);
                    positions.emplace_back(1.0f, -1.0f, 1.0f);
                    positions.emplace_back(1.0f, 1.0f, -1.0f);
                    positions.emplace_back(-1.0f, 1.0f, -1.0f);
                    positions.emplace_back(-1.0f, -1.0f, -1.0f);
                    positions.emplace_back(1.0f, -1.0f, -1.0f);
                    std::vector<std::vector<int> > polygonVertexIndices =
                    {
                        { 0, 1, 2, 3 }, { 0, 3, 7, 4 }, { 4, 7, 6, 5 },
                        { 1, 5, 6, 2 }, { 0, 4, 5, 1 }, { 2, 6, 7, 3 }
                    };
                    m_stubFbxMesh->CreateMesh(positions, polygonVertexIndices);

                    std::vector<std::vector<int> > expectedOutputFaceVertexIndices =
                    {
                        { 0, 1, 2 }, { 3, 0, 2 }, { 0, 3, 7 },
                        { 4, 0, 7 }, { 4, 7, 6 }, { 5, 4, 6 },
                        { 1, 5, 6 }, { 2, 1, 6 }, { 0, 4, 5 },
                        { 1, 0, 5 }, { 2, 6, 7 }, { 3, 2, 7 }
                    };
                    m_stubFbxMesh->CreateExpectMeshInfo(expectedOutputFaceVertexIndices);
                }

                void CheckBuildMeshData()
                {
                    AZStd::shared_ptr<DataTypes::IGraphObject> buildObject = m_graph.GetNodeContent(m_graph.GetRoot());
                    EXPECT_NE(nullptr, buildObject);

                    AZStd::shared_ptr<const DataTypes::IMeshData> buildMesh = azrtti_cast<const DataTypes::IMeshData*>(buildObject);
                    EXPECT_NE(nullptr, buildMesh);

                    EXPECT_EQ(buildMesh->GetVertexCount(), m_stubFbxMesh->GetExpectedVetexCount());
                    EXPECT_EQ(buildMesh->GetFaceCount(), m_stubFbxMesh->GetExpectedFaceCount());

                    // check if face matches expected wiring
                    for (unsigned int i = 0; i < buildMesh->GetFaceCount(); ++i)
                    {
                        for (unsigned int j = 0; j < 3; ++j)
                        {
                            EXPECT_EQ(buildMesh->GetPosition(buildMesh->GetFaceInfo(i).vertexIndex[j]),
                                m_stubFbxMesh->GetExpectedFaceVertexPosition(i, j));
                        }
                    }
                }

                std::shared_ptr<FbxSDKWrapper::TestFbxMesh> m_stubFbxMesh;
                std::unique_ptr<FbxMeshBuilder> m_testFbxMeshBuilder;
                AZ::SceneAPI::Containers::SceneGraph m_graph;
                std::shared_ptr<FbxSceneSystem> m_stubFbxSceneSystem;
            };
        }
    }
}