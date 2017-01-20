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
#include <RC/ResourceCompilerScene/Cgf/CgfMeshGroupExporter.h>
#include <RC/ResourceCompilerScene/Tests/Cgf/CgfExportContextTestBase.h>
#include <SceneAPI/SceneCore/Mocks/DataTypes/ManifestBase/MockISceneNodeSelectionList.h>

namespace AZ
{
    namespace RC
    {
        using ::testing::Return;
        using ::testing::ReturnRef;
        using ::testing::Const;

        class CgfMeshGroupExporterContextTestBase
            : public CgfExporterContextTestBase
        {
        public:
            CgfMeshGroupExporterContextTestBase()
                : m_testExporter(&m_mockAssetWriter)
            {
            }
            ~CgfMeshGroupExporterContextTestBase() override = default;

        protected:
            StrictMock<MockIAssetWriter> m_mockAssetWriter;
            CgfMeshGroupExporter m_testExporter;
        };

        class CgfMeshGroupExporterNoOpTests
            : public CgfMeshGroupExporterContextTestBase
        {
        public:
            ~CgfMeshGroupExporterNoOpTests() override = default;
        };

        TEST_P(CgfMeshGroupExporterNoOpTests, Process_UnsupportedContext_WriterNotUsed)
        {
            m_testExporter.Process(m_stubContext);
        }

        static const ContextPhaseTuple g_unsupportedContextPhaseTuples[] =
        {
            { TestContextMeshGroup, Phase::Construction },
            { TestContextMeshGroup, Phase::Finalizing },
            { TestContextContainer, Phase::Construction },
            { TestContextContainer, Phase::Filling },
            { TestContextContainer, Phase::Finalizing },
            { TestContextNode, Phase::Construction },
            { TestContextNode, Phase::Filling },
            { TestContextNode, Phase::Finalizing },
            { TestContextMeshNode, Phase::Construction },
            { TestContextMeshNode, Phase::Filling },
            { TestContextMeshNode, Phase::Finalizing }
        };

        INSTANTIATE_TEST_CASE_P(MeshGroupExporter,
            CgfMeshGroupExporterNoOpTests,
            ::testing::ValuesIn(g_unsupportedContextPhaseTuples));

        class CgfMeshExporterSimpleTestFramework
            : public CgfMeshGroupExporterContextTestBase
        {
        public:
            ~CgfMeshExporterSimpleTestFramework() override = default;

        protected:
            void SetUp() override
            {
            }

            void TearDown() override
            {
            }
            
            SceneAPI::DataTypes::MockISceneNodeSelectionList m_stubSceneNodeSelectionList;
        };

        TEST_P(CgfMeshExporterSimpleTestFramework, Process_SupportedContextNoNodesSelected_WriterNotUsed)
        {
            EXPECT_CALL(m_stubSceneNodeSelectionList, GetSelectedNodeCount())
                .WillRepeatedly(Return(0));
            EXPECT_CALL(m_stubSceneNodeSelectionList, GetUnselectedNodeCount())
                .WillRepeatedly(Return(0));
            EXPECT_CALL(Const(m_stubMeshGroup), GetSceneNodeSelectionList())
                .WillRepeatedly(ReturnRef(m_stubSceneNodeSelectionList));
            EXPECT_CALL(m_stubMeshGroup, GetRuleCount())
                .WillRepeatedly(Return(0));
            m_testExporter.Process(&m_stubMeshGroupExportContext);
        }

        static const ContextPhaseTuple g_supportedContextPhaseTuples[] =
        {
            { TestContextMeshGroup, Phase::Filling }
        };

        INSTANTIATE_TEST_CASE_P(MeshGroupExporter,
            CgfMeshExporterSimpleTestFramework,
            ::testing::ValuesIn(g_supportedContextPhaseTuples));
    } // namespace RC
} // namespace AZ