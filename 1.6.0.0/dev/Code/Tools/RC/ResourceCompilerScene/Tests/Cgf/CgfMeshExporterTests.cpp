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

#include <RC/ResourceCompilerScene/Cgf/CgfExportContexts.h>
#include <RC/ResourceCompilerScene/Cgf/CgfMeshExporter.h>
#include <RC/ResourceCompilerScene/Tests/Cgf/CgfExportContextTestBase.h>
#include <SceneAPI/SceneCore/Mocks/DataTypes/ManifestBase/MockISceneNodeSelectionList.h>
#include <SceneAPI/SceneCore/Mocks/DataTypes/GraphData/MockIMeshData.h>

namespace AZ
{
    namespace RC
    {
        using ::testing::Return;

        class CgfMeshExporterContextTestBase
            : public CgfExporterContextTestBase
        {
        public:
            CgfMeshExporterContextTestBase()
                : m_stubMeshData(new SceneAPI::DataTypes::MockIMeshData())
            {
            }
            ~CgfMeshExporterContextTestBase() override = default;

        protected:
            // Minimal subset for a valid Processing pass
            void SetUp() override
            {
                CgfExporterContextTestBase::SetUp();

                SceneAPI::Containers::SceneGraph& graph = m_stubScene.GetGraph();
                SceneAPI::Containers::SceneGraph::NodeIndex rootIndex = graph.GetRoot();
                SceneAPI::Containers::SceneGraph::NodeIndex meshIndex = graph.AddChild(rootIndex, "sampleMeshData", m_stubMeshData);

                UpdateNodeIndex(meshIndex);

                m_outMesh.SetVertexCount(3);

                m_outContent.GetExportInfo()->bNoMesh = true;
            }

            bool TestChangedData()
            {
                return !m_outContent.GetExportInfo()->bNoMesh;
            }

            AZStd::shared_ptr<SceneAPI::DataTypes::MockIMeshData> m_stubMeshData;
            CgfMeshExporter m_testExporter;
        };

        class CgfMeshExporterNoOpTests
            : public CgfMeshExporterContextTestBase
        {
        public:
            ~CgfMeshExporterNoOpTests() override = default;
        };

        TEST_P(CgfMeshExporterNoOpTests, Process_UnsupportedContext_ContentNotChanged)
        {
            m_testExporter.Process(m_stubContext);
            EXPECT_FALSE(TestChangedData());
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
            { TestContextNode, Phase::Finalizing },
            { TestContextMeshNode, Phase::Construction },
            { TestContextMeshNode, Phase::Filling },
            { TestContextMeshNode, Phase::Finalizing }
        };

        INSTANTIATE_TEST_CASE_P(MeshExporter,
            CgfMeshExporterNoOpTests,
            ::testing::ValuesIn(g_unsupportedContextPhaseTuples));

        class CgfMeshExporterSimpleTests
            : public CgfMeshExporterContextTestBase
        {
        public:
            ~CgfMeshExporterSimpleTests() override = default;
        };

        TEST_P(CgfMeshExporterSimpleTests, Process_SupportedContext_ContentChanged)
        {
            EXPECT_CALL(*m_stubMeshData, GetVertexCount())
                .WillRepeatedly(Return(0));
            EXPECT_CALL(*m_stubMeshData, GetFaceCount())
                .WillRepeatedly(Return(0));
            EXPECT_CALL(*m_stubMeshData, HasNormalData())
                .WillRepeatedly(Return(false));

            m_testExporter.Process(&m_stubNodeExportContext);
            EXPECT_TRUE(TestChangedData());
        }

        static const ContextPhaseTuple g_supportedContextPhaseTuples[] =
        {
            {TestContextNode, Phase::Filling}
        };

        INSTANTIATE_TEST_CASE_P(MeshExporter,
            CgfMeshExporterSimpleTests,
            ::testing::ValuesIn(g_supportedContextPhaseTuples));
    } // namespace RC
} // namespace AZ