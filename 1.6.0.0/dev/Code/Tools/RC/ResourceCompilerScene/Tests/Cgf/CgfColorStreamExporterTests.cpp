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

#include <RC/ResourceCompilerScene/Cgf/CgfColorStreamExporter.h>
#include <RC/ResourceCompilerScene/Tests/Cgf/CgfExportContextTestBase.h>
#include <SceneAPI/SceneCore/Mocks/DataTypes/GraphData/MockIMeshData.h>
#include <SceneAPI/SceneCore/Mocks/DataTypes/GraphData/MockIMeshVertexColorData.h>
#include <SceneAPI/SceneCore/Mocks/DataTypes/ManifestBase/MockISceneNodeSelectionList.h>

namespace AZ
{
    namespace RC
    {
        using ::testing::Return;
        using ::testing::ReturnRef;
        using ::testing::_;

        class CgfColorStreamExporterContextTestBase
            : public CgfExporterContextTestBase
        {
        public:
            CgfColorStreamExporterContextTestBase()
                : m_stubMeshData(new SceneAPI::DataTypes::MockIMeshData())
                , m_stubMeshVertexColorData(new SceneAPI::DataTypes::MockIMeshVertexColorData())
                , m_sampleColor({1.f, 0.f, 0.f, 1.f})
            {
            }
            ~CgfColorStreamExporterContextTestBase() override = default;

        protected:
            // Minimal data subset:
            // - Graph contains a single MeshData node
            // - MeshData node has a single MeshDataVertexColor child
            void SetUp() override
            {
                CgfExporterContextTestBase::SetUp();

                SceneAPI::Containers::SceneGraph& graph = m_stubScene.GetGraph();
                SceneAPI::Containers::SceneGraph::NodeIndex rootIndex = graph.GetRoot();

                SceneAPI::Containers::SceneGraph::NodeIndex meshIndex = graph.AddChild(rootIndex, "sampleMeshData", m_stubMeshData);
                UpdateNodeIndex(meshIndex);
                graph.AddChild(m_sampleNodeIndex, "sampleMeshVertexColorData", m_stubMeshVertexColorData);

                m_outMesh.SetVertexCount(3);

                EXPECT_CALL(m_stubMeshGroup, GetRuleCount())
                    .WillRepeatedly(Return(0));
                EXPECT_CALL(*m_stubMeshData, GetVertexCount())
                    .WillRepeatedly(Return(3));
                EXPECT_CALL(*m_stubMeshVertexColorData, GetCount())
                    .WillRepeatedly(Return(3));
                EXPECT_CALL(*m_stubMeshVertexColorData, GetColor(_))
                    .WillRepeatedly(ReturnRef(m_sampleColor));
            }

            bool TestCausedNoChanges()
            {
                CMesh emptyMesh;
                emptyMesh.SetVertexCount(3);
                return emptyMesh.CompareStreams(m_outMesh);
            }

            AZStd::shared_ptr<SceneAPI::DataTypes::MockIMeshData> m_stubMeshData;
            AZStd::shared_ptr<SceneAPI::DataTypes::MockIMeshVertexColorData> m_stubMeshVertexColorData;
            CgfColorStreamExporter m_testExporter;
            AZ::SceneAPI::DataTypes::Color m_sampleColor;
        };

        class CgfColorStreamExporterNoOpTests
            : public CgfColorStreamExporterContextTestBase
        {
        public:
            ~CgfColorStreamExporterNoOpTests() override = default;
        };

        TEST_P(CgfColorStreamExporterNoOpTests, Process_UnsupportedContext_MeshRemainsEmpty)
        {
            m_testExporter.Process(m_stubContext);
            EXPECT_TRUE(TestCausedNoChanges());
        }

        static const ContextPhaseTuple g_unsupportedContextPhaseTuples[] =
        {
            { TestContextMeshGroup, Phase::Construction },
            { TestContextMeshGroup, Phase::Filling },
            { TestContextMeshGroup, Phase::Finalizing },
            { TestContextContainer, Phase::Construction },
            { TestContextContainer, Phase::Filling },
            { TestContextContainer, Phase::Finalizing },
            { TestContextNode, Phase::Construction },
            { TestContextNode, Phase::Filling },
            { TestContextNode, Phase::Finalizing },
            { TestContextMeshNode, Phase::Construction },
            { TestContextMeshNode, Phase::Finalizing }
        };

        INSTANTIATE_TEST_CASE_P(ColorStreamExporter,
            CgfColorStreamExporterNoOpTests,
            ::testing::ValuesIn(g_unsupportedContextPhaseTuples));

        class CgfColorStreamExporterSimpleTests
            : public CgfColorStreamExporterContextTestBase
        {
        public:
            ~CgfColorStreamExporterSimpleTests() override = default;
        };

        TEST_P(CgfColorStreamExporterSimpleTests, Process_SupportedContext_MeshIsNotEmpty)
        {
            m_testExporter.Process(m_stubContext);
            EXPECT_TRUE(!TestCausedNoChanges());
        }

        static const ContextPhaseTuple g_supportedContextPhaseTuples[] =
        {
            { TestContextMeshNode, Phase::Filling }
        };

        INSTANTIATE_TEST_CASE_P(ColorStreamExporter,
            CgfColorStreamExporterSimpleTests,
            ::testing::ValuesIn(g_supportedContextPhaseTuples));
    } // namespace RC
} // namespace AZ