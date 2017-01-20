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
#include <SceneAPI/SceneCore/Mocks/Export/MockIExporter.h>
#include <SceneAPI/SceneCore/Mocks/Containers/MockScene.h>
#include <SceneAPI/SceneCore/Export/ExportersList.h>
#include <string>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            class Scene;
        }

        namespace Export
        {
            using ::testing::NiceMock;
            using ::testing::Return;
            using ::testing::_;
            using ::testing::A;

            namespace SceneExport = AZ::SceneAPI::Export;


            //ExportScene char *
            TEST(ExportersListTests, ExportScene_CharPtrDirSceneGoodExporter_ReturnTrue)
            {
                ExportersList testExportersList;

                std::shared_ptr<NiceMock<SceneExport::MockIExporter> > testExporter
                    = std::make_shared<NiceMock<SceneExport::MockIExporter> >();

                ON_CALL(*testExporter, WriteToFile(A<const char*>(), _))
                    .WillByDefault(Return(true));

                NiceMock<SceneAPI::Containers::MockScene> testScene("test");

                testExportersList.RegisterExporter(testExporter);

                bool result = testExportersList.ExportScene("", testScene);
                EXPECT_TRUE(result);
            }

            TEST(ExportersListTests, ExportScene_CharPtrDirScene2GoodExporters_ReturnTrue)
            {
                ExportersList testExportersList;

                std::shared_ptr<NiceMock<SceneExport::MockIExporter> > testExporter
                    = std::make_shared<NiceMock<SceneExport::MockIExporter> >();

                std::shared_ptr<NiceMock<SceneExport::MockIExporter> > testExporter2
                    = std::make_shared<NiceMock<SceneExport::MockIExporter> >();

                ON_CALL(*testExporter, WriteToFile(A<const char*>(), _))
                    .WillByDefault(Return(true));

                ON_CALL(*testExporter2, WriteToFile(A<const char*>(), _))
                    .WillByDefault(Return(true));

                NiceMock<SceneAPI::Containers::MockScene> testScene("test");

                testExportersList.RegisterExporter(testExporter);
                testExportersList.RegisterExporter(testExporter2);

                bool result = testExportersList.ExportScene("", testScene);
                EXPECT_TRUE(result);
            }

            TEST(ExportersListTests, ExportScene_CharPtrDirScene1stGood2ndBadExporters_ReturnFalse)
            {
                ExportersList testExportersList;

                std::shared_ptr<NiceMock<SceneExport::MockIExporter> > testExporter
                    = std::make_shared<NiceMock<SceneExport::MockIExporter> >();

                std::shared_ptr<NiceMock<SceneExport::MockIExporter> > testExporter2
                    = std::make_shared<NiceMock<SceneExport::MockIExporter> >();

                ON_CALL(*testExporter, WriteToFile(A<const char*>(), _))
                    .WillByDefault(Return(true));

                ON_CALL(*testExporter2, WriteToFile(A<const char*>(), _))
                    .WillByDefault(Return(false));

                NiceMock<SceneAPI::Containers::MockScene> testScene("test");

                testExportersList.RegisterExporter(testExporter);
                testExportersList.RegisterExporter(testExporter2);

                bool result = testExportersList.ExportScene("", testScene);
                EXPECT_FALSE(result);
            }

            TEST(ExportersListTests, ExportScene_CharPtrDirScene1stBad2ndGoodExporters_ReturnFalse)
            {
                ExportersList testExportersList;

                std::shared_ptr<NiceMock<SceneExport::MockIExporter> > testExporter
                    = std::make_shared<NiceMock<SceneExport::MockIExporter> >();

                std::shared_ptr<NiceMock<SceneExport::MockIExporter> > testExporter2
                    = std::make_shared<NiceMock<SceneExport::MockIExporter> >();

                ON_CALL(*testExporter, WriteToFile(A<const char*>(), _))
                    .WillByDefault(Return(false));

                ON_CALL(*testExporter2, WriteToFile(A<const char*>(), _))
                    .WillByDefault(Return(true));

                NiceMock<SceneAPI::Containers::MockScene> testScene("test");

                testExportersList.RegisterExporter(testExporter);
                testExportersList.RegisterExporter(testExporter2);

                bool result = testExportersList.ExportScene("", testScene);
                EXPECT_FALSE(result);
            }


            //ExportScene String
            TEST(ExportersListTests, ExportScene_StringDirSceneGoodExporter_ReturnTrue)
            {
                ExportersList testExportersList;

                std::shared_ptr<NiceMock<SceneExport::MockIExporter> > testExporter
                    = std::make_shared<NiceMock<SceneExport::MockIExporter> >();

                ON_CALL(*testExporter, WriteToFile(A<const std::string&>(), _))
                    .WillByDefault(Return(true));

                NiceMock<SceneAPI::Containers::MockScene> testScene("test");

                testExportersList.RegisterExporter(testExporter);

                bool result = testExportersList.ExportScene(std::string(""), testScene);
                EXPECT_TRUE(result);
            }

            TEST(ExportersListTests, ExportScene_StringDirScene2GoodExporters_ReturnTrue)
            {
                ExportersList testExportersList;

                std::shared_ptr<NiceMock<SceneExport::MockIExporter> > testExporter
                    = std::make_shared<NiceMock<SceneExport::MockIExporter> >();

                std::shared_ptr<NiceMock<SceneExport::MockIExporter> > testExporter2
                    = std::make_shared<NiceMock<SceneExport::MockIExporter> >();

                ON_CALL(*testExporter, WriteToFile(A<const std::string&>(), _))
                    .WillByDefault(Return(true));

                ON_CALL(*testExporter2, WriteToFile(A<const std::string&>(), _))
                    .WillByDefault(Return(true));

                NiceMock<SceneAPI::Containers::MockScene> testScene("test");

                testExportersList.RegisterExporter(testExporter);
                testExportersList.RegisterExporter(testExporter2);

                bool result = testExportersList.ExportScene(std::string(""), testScene);
                EXPECT_TRUE(result);
            }

            TEST(ExportersListTests, ExportScene_StringDirScene1stGood2ndBadExporters_ReturnFalse)
            {
                ExportersList testExportersList;

                std::shared_ptr<NiceMock<SceneExport::MockIExporter> > testExporter
                    = std::make_shared<NiceMock<SceneExport::MockIExporter> >();

                std::shared_ptr<NiceMock<SceneExport::MockIExporter> > testExporter2
                    = std::make_shared<NiceMock<SceneExport::MockIExporter> >();

                ON_CALL(*testExporter, WriteToFile(A<const std::string&>(), _))
                    .WillByDefault(Return(true));

                ON_CALL(*testExporter2, WriteToFile(A<const std::string&>(), _))
                    .WillByDefault(Return(false));

                NiceMock<SceneAPI::Containers::MockScene> testScene("test");

                testExportersList.RegisterExporter(testExporter);
                testExportersList.RegisterExporter(testExporter2);

                bool result = testExportersList.ExportScene(std::string(""), testScene);
                EXPECT_FALSE(result);
            }

            TEST(ExportersListTests, ExportScene_StringDirScene1stBad2ndGoodExporters_ReturnFalse)
            {
                ExportersList testExportersList;

                std::shared_ptr<NiceMock<SceneExport::MockIExporter> > testExporter
                    = std::make_shared<NiceMock<SceneExport::MockIExporter> >();

                std::shared_ptr<NiceMock<SceneExport::MockIExporter> > testExporter2
                    = std::make_shared<NiceMock<SceneExport::MockIExporter> >();

                ON_CALL(*testExporter, WriteToFile(A<const std::string&>(), _))
                    .WillByDefault(Return(false));

                ON_CALL(*testExporter2, WriteToFile(A<const std::string&>(), _))
                    .WillByDefault(Return(true));

                NiceMock<SceneAPI::Containers::MockScene> testScene("test");

                testExportersList.RegisterExporter(testExporter);
                testExportersList.RegisterExporter(testExporter2);

                bool result = testExportersList.ExportScene(std::string(""), testScene);
                EXPECT_FALSE(result);
            }
        }
    }
}

