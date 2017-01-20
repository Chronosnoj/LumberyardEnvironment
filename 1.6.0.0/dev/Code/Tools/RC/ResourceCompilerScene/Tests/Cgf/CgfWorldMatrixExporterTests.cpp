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

#include <Cry_Geo.h> // Needed for Cry_Matrix34.h
#include <Cry_Matrix34.h>
#include <RC/ResourceCompilerScene/Cgf/CgfExportContexts.h>
#include <RC/ResourceCompilerScene/Cgf/CgfWorldMatrixExporter.h>
#include <RC/ResourceCompilerScene/Tests/Cgf/CgfExportContextTestBase.h>
#include <SceneAPI/SceneCore/Mocks/DataTypes/GraphData/MockITransform.h>
#include <SceneAPI/SceneCore/Mocks/DataTypes/GraphData/MockIMeshData.h>
#include <SceneAPI/SceneCore/Mocks/DataTypes/ManifestBase/MockISceneNodeSelectionList.h>

namespace AZ
{
    namespace RC
    {
        using ::testing::Const;
        using ::testing::Return;
        using ::testing::ReturnRef;

        class CgfWorldMatrixExporterContextTestBase
            : public CgfExporterContextTestBase
        {
        public:
            CgfWorldMatrixExporterContextTestBase()
                : m_stubTransformData(new SceneAPI::DataTypes::MockITransform())
                , m_stubMeshData(new SceneAPI::DataTypes::MockIMeshData())
                , m_stubTransform(AZ::Transform::CreateTranslation(AZ::Vector3(0.f, 0.f, 1.f)))
            {
            }
            ~CgfWorldMatrixExporterContextTestBase() override = default;

        protected:
            void SetUp()
            {
                CgfExporterContextTestBase::SetUp();

                m_outNode.bIdentityMatrix = true;
                SceneAPI::Containers::SceneGraph& sceneGraph = m_stubScene.GetGraph();
                SceneAPI::Containers::SceneGraph::NodeIndex rootIndex = sceneGraph.GetRoot();
                SceneAPI::Containers::SceneGraph::NodeIndex transformIndex = sceneGraph.AddChild(rootIndex, "SampleTransformData", m_stubTransformData);
                SceneAPI::Containers::SceneGraph::NodeIndex meshIndex = sceneGraph.AddChild(transformIndex, "SampleMeshData", m_stubMeshData);

                UpdateNodeIndex(meshIndex);

                EXPECT_CALL(m_stubMeshGroup, GetRuleCount())
                    .WillRepeatedly(Return(0));

                EXPECT_CALL(Const(*m_stubTransformData), GetMatrix())
                    .WillRepeatedly(ReturnRef(m_stubTransform));
            }

            bool TestChangedData()
            {
                return !m_outNode.bIdentityMatrix;
            }

            AZStd::shared_ptr<SceneAPI::DataTypes::MockITransform> m_stubTransformData;
            AZStd::shared_ptr<SceneAPI::DataTypes::MockIMeshData> m_stubMeshData;
            AZ::Transform m_stubTransform;
            CgfWorldMatrixExporter m_testExporter;
        };

        class CgfWorldMatrixExporterNoOpTestFramework
            : public CgfWorldMatrixExporterContextTestBase
        {
        public:

            ~CgfWorldMatrixExporterNoOpTestFramework() override = default;
        };

        TEST_P(CgfWorldMatrixExporterNoOpTestFramework, Process_UnsupportedContext_OutNodeAtIndentity)
        {
            m_testExporter.Process(m_stubContext);
            EXPECT_FALSE(TestChangedData());
        }

        ContextPhaseTuple g_unsupportedContextPhaseTuples[] =
        {
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

        INSTANTIATE_TEST_CASE_P(WorldMatrixExporter,
            CgfWorldMatrixExporterNoOpTestFramework,
            ::testing::ValuesIn(g_unsupportedContextPhaseTuples));

        class CgfWorldMatrixExporterSimpleTests
            : public CgfWorldMatrixExporterContextTestBase
        {
        public:
            CgfWorldMatrixExporterSimpleTests()
                : m_cacheGenerationContext(m_stubScene, m_sampleOutputDirectory, m_sampleGroupName, m_stubMeshGroup, Phase::Construction)
            {
            }
            ~CgfWorldMatrixExporterSimpleTests() override = default;

        protected:
            void SetUp() override
            {
                CgfWorldMatrixExporterContextTestBase::SetUp();
                m_testExporter.Process(&m_cacheGenerationContext);
            }

            CgfMeshGroupExportContext m_cacheGenerationContext;
        };


        TEST_P(CgfWorldMatrixExporterSimpleTests, Process_SupportedContext_OutNodeNotAtIndentity)
        {
            m_testExporter.Process(m_stubContext);
            EXPECT_TRUE(TestChangedData());
        }

        ContextPhaseTuple g_supportedContextPhaseTuples[] =
        {
            { TestContextNode, Phase::Filling }
        };

        INSTANTIATE_TEST_CASE_P(WorldMatrixExporter,
            CgfWorldMatrixExporterSimpleTests,
            ::testing::ValuesIn(g_supportedContextPhaseTuples));
    } // namespace RC
} // namespace AZ