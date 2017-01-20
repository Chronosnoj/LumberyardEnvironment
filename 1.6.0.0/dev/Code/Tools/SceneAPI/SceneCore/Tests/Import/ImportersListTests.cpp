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
#include <SceneAPI/SceneCore/Mocks/Import/MockIImporter.h>
#include <SceneAPI/SceneCore/Import/ImportersList.h>
#include <string>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Import
        {
            using ::testing::NiceMock;

            namespace SceneImport = AZ::SceneAPI::Import;

            //RegisterImporter with const char *
            TEST(ImportersListTests, RegisterImporter_CharPtrValidExtensionAndImporter_ReturnTrue)
            {
                ImportersList testImportersList;
                std::shared_ptr<NiceMock<SceneImport::MockIImporter> > testImporter
                    = std::make_shared<NiceMock<SceneImport::MockIImporter> >();

                bool result = testImportersList.RegisterImporter("ext", testImporter);
                EXPECT_TRUE(result);
            }

            //RegisterImporter with string
            TEST(ImportersListTests, RegisterImporter_ValidExtensionAndImporter_ReturnTrue)
            {
                ImportersList testImportersList;
                std::shared_ptr<NiceMock<SceneImport::MockIImporter> > testImporter
                    = std::make_shared<NiceMock<SceneImport::MockIImporter> >();

                bool result = testImportersList.RegisterImporter(std::string("ext"), testImporter);
                EXPECT_TRUE(result);
            }

            TEST(ImportersListTests, RegisterImporter_ValidExtensionNoImporter_ReturnFalse)
            {
                ImportersList testImportersList;
                bool result = testImportersList.RegisterImporter(std::string("ext"), nullptr);
                EXPECT_FALSE(result);
            }

            TEST(ImportersListTests, RegisterImporter_InvalidExtensionValidImporter_ReturnFalse)
            {
                ImportersList testImportersList;
                std::shared_ptr<NiceMock<SceneImport::MockIImporter> > testImporter
                    = std::make_shared<NiceMock<SceneImport::MockIImporter> >();

                bool result = testImportersList.RegisterImporter(std::string(""), testImporter);
                EXPECT_FALSE(result);
            }

            //RegisterImporter with string &&
            TEST(ImportersListTests, RegisterImporter_ValidMoveExtensionValidImporter_ReturnTrue)
            {
                ImportersList testImportersList;
                std::shared_ptr<NiceMock<SceneImport::MockIImporter> > testImporter
                    = std::make_shared<NiceMock<SceneImport::MockIImporter> >();
                std::string extensionString("ext");
                bool result = testImportersList.RegisterImporter(std::move(extensionString), testImporter);
                EXPECT_TRUE(result);
            }

            //RegisterImporter Multiple
            TEST(ImportersListTests, RegisterImporter_MultipleValidExtensionAndImporter_ReturnTrue)
            {
                ImportersList testImportersList;
                std::shared_ptr<NiceMock<SceneImport::MockIImporter> > testImporter
                    = std::make_shared<NiceMock<SceneImport::MockIImporter> >();

                testImportersList.RegisterImporter(std::string("ext"), testImporter);

                testImporter = std::make_shared<NiceMock<SceneImport::MockIImporter> >();

                bool result = testImportersList.RegisterImporter(std::string("woot2"), testImporter);
                EXPECT_TRUE(result);
            }

            TEST(ImportersListTests, RegisterImporter_MultipleDuplicateExtension_ReturnFalse)
            {
                ImportersList testImportersList;
                std::shared_ptr<NiceMock<SceneImport::MockIImporter> > testImporter
                    = std::make_shared<NiceMock<SceneImport::MockIImporter> >();

                testImportersList.RegisterImporter(std::string("ext"), testImporter);

                testImporter = std::make_shared<NiceMock<SceneImport::MockIImporter> >();

                bool result = testImportersList.RegisterImporter(std::string("ext"), testImporter);
                EXPECT_FALSE(result);
            }

            //GetExtensionCount
            TEST(ImportersListTests, GetExtensionCount_SingleImporter_Return1)
            {
                ImportersList testImportersList;
                std::shared_ptr<NiceMock<SceneImport::MockIImporter> > testImporter
                    = std::make_shared<NiceMock<SceneImport::MockIImporter> >();
                std::string extensionString("ext");
                testImportersList.RegisterImporter(std::move(extensionString), testImporter);

                size_t count = testImportersList.GetExtensionCount();
                EXPECT_EQ(1, count);
            }

            TEST(ImportersListTests, GetExtensionCount_NoImporter_Return0)
            {
                ImportersList testImportersList;

                size_t count = testImportersList.GetExtensionCount();
                EXPECT_EQ(0, count);
            }

            TEST(ImportersListTests, GetExtensionCount_2Importer_Return2)
            {
                ImportersList testImportersList;
                std::shared_ptr<NiceMock<SceneImport::MockIImporter> > testImporter
                    = std::make_shared<NiceMock<SceneImport::MockIImporter> >();
                std::string extensionString("ext");
                testImportersList.RegisterImporter(std::move(extensionString), testImporter);

                testImporter = std::make_shared<NiceMock<SceneImport::MockIImporter> >();
                extensionString = "ext2";
                testImportersList.RegisterImporter(std::move(extensionString), testImporter);

                size_t count = testImportersList.GetExtensionCount();
                EXPECT_EQ(2, count);
            }

            //GetExtension
            TEST(ImportersListTests, GetExtension_SingleImporter_ReturnExtension)
            {
                ImportersList testImportersList;
                std::shared_ptr<NiceMock<SceneImport::MockIImporter> > testImporter
                    = std::make_shared<NiceMock<SceneImport::MockIImporter> >();
                std::string extensionString("ext");

                testImportersList.RegisterImporter(extensionString, testImporter);

                const std::string& savedExtension = testImportersList.GetExtension(0);
                EXPECT_EQ(extensionString, savedExtension);
            }

#if 0
            //TODO  enable this when our test environment can handle the assert appropriately
            TEST(ImportersListTests, GetExtension_NoImporter_ReturnNullPtr)
            {
                ImportersList testImportersList;

                const std::string savedExtension = testImportersList.GetExtension(0);
                should assert (does)
            }
#endif

            TEST(ImportersListTests, GetExtension_MultipleImporter_ReturnFirstDiffReturnSecond)
            {
                ImportersList testImportersList;
                std::shared_ptr<NiceMock<SceneImport::MockIImporter> > testImporter
                    = std::make_shared<NiceMock<SceneImport::MockIImporter> >();

                testImportersList.RegisterImporter("ext", testImporter);

                testImporter = std::make_shared<NiceMock<SceneImport::MockIImporter> >();

                testImportersList.RegisterImporter("ext2", testImporter);

                std::string savedExtension0 = testImportersList.GetExtension(0);
                std::string savedExtension1 = testImportersList.GetExtension(1);
                EXPECT_NE(savedExtension0, savedExtension1);
            }

#if 0
            //TODO  enable this when our test environment can handle the assert appropriately
            TEST(ImportersListTests, GetExtension_SingleImporterLookFor3_ReturnNullptr)
            {
                ImportersList testImportersList;
                std::shared_ptr<NiceMock<SceneImport::MockIImporter> > testImporter
                    = std::make_shared<NiceMock<SceneImport::MockIImporter> >();
                std::string extensionString("ext");

                testImportersList.RegisterImporter(extensionString, testImporter);

                const std::string& savedExtension = testImportersList.GetExtension(3);
                //should assert (does)
            }
#endif

            //FindImporter

            TEST(ImportersListTests, FindImporter_CharPtrFileExtension_ReturnImporter)
            {
                ImportersList testImportersList;
                std::shared_ptr<NiceMock<SceneImport::MockIImporter> > testImporter
                    = std::make_shared<NiceMock<SceneImport::MockIImporter> >();
                std::string extensionString("ext");

                testImportersList.RegisterImporter(extensionString, testImporter);

                std::shared_ptr<IImporter> savedImporter = testImportersList.FindImporter("ext");
                EXPECT_EQ(testImporter, savedImporter);
            }

            TEST(ImportersListTests, FindImporter_FileExtension_ReturnImporter)
            {
                ImportersList testImportersList;
                std::shared_ptr<NiceMock<SceneImport::MockIImporter> > testImporter
                    = std::make_shared<NiceMock<SceneImport::MockIImporter> >();
                std::string extensionString("ext");

                testImportersList.RegisterImporter(extensionString, testImporter);

                std::shared_ptr<IImporter> savedImporter = testImportersList.FindImporter(extensionString);
                EXPECT_EQ(testImporter, savedImporter);
            }

            TEST(ImportersListTests, FindImporter_MulitpleInListFileExtension1_ReturnImporter)
            {
                ImportersList testImportersList;
                std::shared_ptr<NiceMock<SceneImport::MockIImporter> > testImporter
                    = std::make_shared<NiceMock<SceneImport::MockIImporter> >();
                std::string extensionString("ext");

                testImportersList.RegisterImporter(extensionString, testImporter);

                std::shared_ptr<IImporter> testImporter2 = std::make_shared<NiceMock<SceneImport::MockIImporter> >();
                std::string extensionString2("ext2");

                testImportersList.RegisterImporter(extensionString2, testImporter);

                std::shared_ptr<IImporter> savedImporter = testImportersList.FindImporter(extensionString);
                EXPECT_EQ(testImporter, savedImporter);
            }

            TEST(ImportersListTests, FindImporter_MulitpleInListFileExtension2_ReturnImporter)
            {
                ImportersList testImportersList;
                std::shared_ptr<NiceMock<SceneImport::MockIImporter> > testImporter
                    = std::make_shared<NiceMock<SceneImport::MockIImporter> >();
                std::string extensionString("ext");

                testImportersList.RegisterImporter(extensionString, testImporter);

                testImporter = std::make_shared<NiceMock<SceneImport::MockIImporter> >();
                std::string extensionString2("ext2");

                testImportersList.RegisterImporter(extensionString2, testImporter);

                std::shared_ptr<IImporter> savedImporter = testImportersList.FindImporter(extensionString2);
                EXPECT_EQ(testImporter, savedImporter);
            }

            //FindImporter Invalid accesses
            TEST(ImportersListTests, FindImporter_NoImportersFileExtension_ReturnNullPtr)
            {
                ImportersList testImportersList;

                std::shared_ptr<IImporter> savedImporter = testImportersList.FindImporter("ext");
                EXPECT_EQ(nullptr, savedImporter);
            }

            TEST(ImportersListTests, FindImporter_UsingDifferentExtension_ReturnNullPtr)
            {
                ImportersList testImportersList;
                std::shared_ptr<NiceMock<SceneImport::MockIImporter> > testImporter
                    = std::make_shared<NiceMock<SceneImport::MockIImporter> >();
                std::string extensionString("ext");

                testImportersList.RegisterImporter(extensionString, testImporter);

                std::shared_ptr<IImporter> savedImporter = testImportersList.FindImporter("ext2");
                EXPECT_EQ(nullptr, savedImporter);
            }
        }
    }
}

