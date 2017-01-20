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
#include <RC/ResourceCompilerScene/Cgf/CgfContainerSettingsExporter.h>
#include <RC/ResourceCompilerScene/Tests/Cgf/CgfExportContextTestBase.h>
#include <SceneAPI/SceneCore/Mocks/DataTypes/ManifestBase/MockISceneNodeSelectionList.h>
#include <SceneAPI/SceneCore/Mocks/DataTypes/Rules/MockIMeshAdvancedRule.h>

namespace AZ
{
    namespace RC
    {
        using ::testing::Return;
        using ::testing::_;

        class CgfContainerSettingsExporterContextTestBase
            : public CgfExporterContextTestBase
        {
        public:
            CgfContainerSettingsExporterContextTestBase()
                : m_stubMeshAdvancedRule(new SceneAPI::DataTypes::MockIMeshAdvancedRule())
            {
            }

            ~CgfContainerSettingsExporterContextTestBase() override = default;

        protected:
            // Minimal subset for check
            // - Group has advanced rule
            // - Advanced rule defines 32 bit vertex precision to be true
            void SetUp() override
            {
                CgfExporterContextTestBase::SetUp();

                EXPECT_CALL(*m_stubMeshAdvancedRule, Use32bitVertices())
                    .WillRepeatedly(Return(true));
                EXPECT_CALL(m_stubMeshGroup, GetRuleCount())
                    .WillRepeatedly(Return(1));
                EXPECT_CALL(m_stubMeshGroup, GetRule(_))
                    .WillRepeatedly(Return(m_stubMeshAdvancedRule));
                EXPECT_CALL(*m_stubMeshAdvancedRule, MergeMeshes())
                    .WillRepeatedly(Return(false));
            }

            bool TestDataChanged()
            {
                return m_outContent.GetExportInfo()->bWantF32Vertices;
            }

            AZStd::shared_ptr<SceneAPI::DataTypes::MockIMeshAdvancedRule> m_stubMeshAdvancedRule;
            CgfContainerSettingsExporter m_testExporter;
        };

        class CgfContainerSettingsExporterNoOpTests
            : public CgfContainerSettingsExporterContextTestBase
        {
        public:
            ~CgfContainerSettingsExporterNoOpTests() override = default;
        };

        TEST_P(CgfContainerSettingsExporterNoOpTests, Process_UnsupportedContext_ExportInfoNotChanged)
        {
            m_testExporter.Process(m_stubContext);
            EXPECT_FALSE(TestDataChanged());
        }

        static const ContextPhaseTuple g_unsupportedContextPhaseTuples[] =
        {
            { TestContextMeshGroup, Phase::Construction },
            { TestContextMeshGroup, Phase::Filling },
            { TestContextMeshGroup, Phase::Finalizing },
            { TestContextContainer, Phase::Filling },
            { TestContextContainer, Phase::Finalizing },
            { TestContextNode, Phase::Construction },
            { TestContextNode, Phase::Filling },
            { TestContextNode, Phase::Finalizing },
            { TestContextMeshNode, Phase::Construction },
            { TestContextMeshNode, Phase::Filling },
            { TestContextMeshNode, Phase::Finalizing }
        };

        INSTANTIATE_TEST_CASE_P(ContainerSettingsExporter,
            CgfContainerSettingsExporterNoOpTests,
            ::testing::ValuesIn(g_unsupportedContextPhaseTuples));

        class CgfContainerSettingsExporterSimpleTests
            : public CgfContainerSettingsExporterContextTestBase
        {
        public:
            ~CgfContainerSettingsExporterSimpleTests() override = default;
        };

        TEST_P(CgfContainerSettingsExporterSimpleTests, Process_SupportedContext_ExportInfoChanged)
        {
            m_testExporter.Process(m_stubContext);
            EXPECT_TRUE(TestDataChanged());
        }

        static const ContextPhaseTuple g_supportedContextPhaseTuples[] =
        {
            {TestContextContainer, Phase::Construction}
        };

        INSTANTIATE_TEST_CASE_P(ContainerSettingsExporter,
            CgfContainerSettingsExporterSimpleTests,
            ::testing::ValuesIn(g_supportedContextPhaseTuples));
    } // namespace RC
} // namespace AZ