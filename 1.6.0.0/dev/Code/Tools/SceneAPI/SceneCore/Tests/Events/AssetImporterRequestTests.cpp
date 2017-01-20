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
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Events/AssetImportRequest.h>
#include <SceneAPI/SceneCore/Mocks/Events/MockAssetImportRequest.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Events
        {
            using ::testing::StrictMock;
            using ::testing::NiceMock;
            using ::testing::Invoke;
            using ::testing::Return;
            using ::testing::Matcher;
            using ::testing::_;

            /*
            * ProcessingResultCombinerTests
            */

            TEST(ProcessingResultCombinerTests, GetResult_GetStoredValue_ReturnsTheDefaultValue)
            {
                ProcessingResultCombiner combiner;
                EXPECT_EQ(ProcessingResult::Ignored, combiner.GetResult());
            }

            TEST(ProcessingResultCombinerTests, OperatorEquals_SuccessIsStored_ResultIsSuccess)
            {
                ProcessingResultCombiner combiner;
                combiner = ProcessingResult::Success;
                EXPECT_EQ(ProcessingResult::Success, combiner.GetResult());
            }

            TEST(ProcessingResultCombinerTests, OperatorEquals_FailureIsStored_ResultIsFailure)
            {
                ProcessingResultCombiner combiner;
                combiner = ProcessingResult::Failure;
                EXPECT_EQ(ProcessingResult::Failure, combiner.GetResult());
            }

            TEST(ProcessingResultCombinerTests, OperatorEquals_SuccessDoesNotOverwriteFailure_ResultIsFailure)
            {
                ProcessingResultCombiner combiner;
                combiner = ProcessingResult::Failure;
                combiner = ProcessingResult::Success;
                EXPECT_EQ(ProcessingResult::Failure, combiner.GetResult());
            }

            TEST(ProcessingResultCombinerTests, OperatorEquals_IgnoreDoesNotChangeTheStoredValue_ResultIsSuccess)
            {
                ProcessingResultCombiner combiner;
                combiner = ProcessingResult::Success;
                combiner = ProcessingResult::Ignored;
                EXPECT_EQ(ProcessingResult::Success, combiner.GetResult());
            }


            /*
            * LoadingResultCombinerTests
            */

            TEST(LoadingResultCombinerTests, GetResult_GetStoredValues_ReturnsTheDefaultValues)
            {
                LoadingResultCombiner combiner;
                EXPECT_EQ(ProcessingResult::Ignored, combiner.GetAssetResult());
                EXPECT_EQ(ProcessingResult::Ignored, combiner.GetManifestResult());
            }

            TEST(LoadingResultCombinerTests, OperatorEquals_AssetLoadedIsStored_ResultIsSuccess)
            {
                LoadingResultCombiner combiner;
                combiner = LoadingResult::AssetLoaded;
                EXPECT_EQ(ProcessingResult::Success, combiner.GetAssetResult());
                EXPECT_EQ(ProcessingResult::Ignored, combiner.GetManifestResult());
            }

            TEST(LoadingResultCombinerTests, OperatorEquals_ManifestLoadedIsStored_ResultIsSuccess)
            {
                LoadingResultCombiner combiner;
                combiner = LoadingResult::ManifestLoaded;
                EXPECT_EQ(ProcessingResult::Ignored, combiner.GetAssetResult());
                EXPECT_EQ(ProcessingResult::Success, combiner.GetManifestResult());
            }

            TEST(LoadingResultCombinerTests, OperatorEquals_AssetFailureIsStored_ResultIsFailure)
            {
                LoadingResultCombiner combiner;
                combiner = LoadingResult::AssetFailure;
                EXPECT_EQ(ProcessingResult::Failure, combiner.GetAssetResult());
                EXPECT_EQ(ProcessingResult::Ignored, combiner.GetManifestResult());
            }

            TEST(LoadingResultCombinerTests, OperatorEquals_ManifestFailureIsStored_ResultIsFailure)
            {
                LoadingResultCombiner combiner;
                combiner = LoadingResult::ManifestFailure;
                EXPECT_EQ(ProcessingResult::Ignored, combiner.GetAssetResult());
                EXPECT_EQ(ProcessingResult::Failure, combiner.GetManifestResult());
            }

            TEST(LoadingResultCombinerTests, OperatorEquals_LoadedDoesNotOverwriteFailure_ResultIsFailure)
            {
                LoadingResultCombiner combiner;
                combiner = LoadingResult::AssetFailure;
                combiner = LoadingResult::ManifestFailure;

                combiner = LoadingResult::AssetLoaded;
                combiner = LoadingResult::ManifestLoaded;
                
                EXPECT_EQ(ProcessingResult::Failure, combiner.GetAssetResult());
                EXPECT_EQ(ProcessingResult::Failure, combiner.GetManifestResult());
            }

            TEST(LoadingResultCombinerTests, OperatorEquals_IgnoreDoesNotChangeTheStoredValue_ResultIsSuccess)
            {
                LoadingResultCombiner combiner;
                combiner = LoadingResult::AssetLoaded;
                combiner = LoadingResult::ManifestLoaded;

                combiner = LoadingResult::Ignored;
                combiner = LoadingResult::Ignored;

                EXPECT_EQ(ProcessingResult::Success, combiner.GetAssetResult());
                EXPECT_EQ(ProcessingResult::Success, combiner.GetManifestResult());
            }


            /*
            * AssetImporterRequestTests
            */
            
            TEST(AssetImporterRequestTests, LoadScene_NoManifestExtension_AssertTriggered)
            {
                NiceMock<MockAssetImportRequestHandler> handler;
                ON_CALL(handler, GetSupportedFileExtensions(Matcher<AZStd::unordered_set<AZStd::string>&>(_)))
                    .WillByDefault(Invoke(&handler, &MockAssetImportRequestHandler::DefaultGetSupportedFileExtensions));
                
                EXPECT_DEATH(AssetImportRequest::LoadScene("test.asset", Events::AssetImportRequest::RequestingApplication::Generic), ".*");
            }

            TEST(AssetImporterRequestTests, LoadScene_NoAssetExtensions_AssertTriggered)
            {
                NiceMock<MockAssetImportRequestHandler> handler;
                ON_CALL(handler, GetManifestExtension(Matcher<AZStd::string&>(_)))
                    .WillByDefault(Invoke(&handler, &MockAssetImportRequestHandler::DefaultGetManifestExtension));

                EXPECT_DEATH(AssetImportRequest::LoadScene("test.asset", Events::AssetImportRequest::RequestingApplication::Generic), ".*");
            }

            TEST(AssetImporterRequestTests, LoadScene_InvalidExtension_NoProcessingFunctionsCalledAndReturnsNullptr)
            {
                StrictMock<MockAssetImportRequestHandler> handler;
                handler.SetDefaultExtensions();
                
                EXPECT_CALL(handler, GetManifestExtension(_));
                EXPECT_CALL(handler, GetSupportedFileExtensions(_));

                EXPECT_CALL(handler, PrepareForAssetLoading(_)).Times(0);
                EXPECT_CALL(handler, LoadAsset(_, _, _)).Times(0);
                EXPECT_CALL(handler, FinalizeAssetLoading(_)).Times(0);
                EXPECT_CALL(handler, UpdateManifest(_, _, _)).Times(0);

                AZStd::shared_ptr<Containers::Scene> result = 
                    AssetImportRequest::LoadScene("test.invalid", Events::AssetImportRequest::RequestingApplication::Generic);
                EXPECT_EQ(nullptr, result);
            }

            TEST(AssetImporterRequestTests, LoadScene_InvalidExtensionWithValidManifestExtension_NoProcessingFunctionsCalledAndReturnsNullptr)
            {
                StrictMock<MockAssetImportRequestHandler> handler;
                handler.SetDefaultExtensions();

                EXPECT_CALL(handler, GetManifestExtension(_));
                EXPECT_CALL(handler, GetSupportedFileExtensions(_));

                EXPECT_CALL(handler, PrepareForAssetLoading(_)).Times(0);
                EXPECT_CALL(handler, LoadAsset(_, _, _)).Times(0);
                EXPECT_CALL(handler, FinalizeAssetLoading(_)).Times(0);
                EXPECT_CALL(handler, UpdateManifest(_, _, _)).Times(0);

                AZStd::shared_ptr<Containers::Scene> result =
                    AssetImportRequest::LoadScene("test.invalid.manifest", Events::AssetImportRequest::RequestingApplication::Generic);
                EXPECT_EQ(nullptr, result);
            }

            TEST(AssetImporterRequestTests, LoadSceneFromVerifiedPath_FailureToPrepare_LoadAndFollowingStepsNotCalledAndReturnsNullptr)
            {
                StrictMock<MockAssetImportRequestHandler> handler;
                handler.SetDefaultExtensions();
                handler.SetDefaultProcessingResults(false);

                ON_CALL(handler, PrepareForAssetLoading(Matcher<Containers::Scene&>(_)))
                    .WillByDefault(Return(ProcessingResult::Failure));

                EXPECT_CALL(handler, GetManifestExtension(_)).Times(0);
                EXPECT_CALL(handler, GetSupportedFileExtensions(_)).Times(0);

                EXPECT_CALL(handler, PrepareForAssetLoading(_)).Times(1);
                EXPECT_CALL(handler, LoadAsset(_, _, _)).Times(0);
                EXPECT_CALL(handler, FinalizeAssetLoading(_)).Times(0);
                EXPECT_CALL(handler, UpdateManifest(_, _, _)).Times(0);

                AZStd::shared_ptr<Containers::Scene> result = 
                    AssetImportRequest::LoadSceneFromVerifiedPath("test.asset", Events::AssetImportRequest::RequestingApplication::Generic);
                EXPECT_EQ(nullptr, result);
            }

            TEST(AssetImporterRequestTests, LoadSceneFromVerifiedPath_AssetFailureToLoad_UpdateManifestNotCalledAndReturnsNullptr)
            {
                StrictMock<MockAssetImportRequestHandler> handler;
                handler.SetDefaultExtensions();
                handler.SetDefaultProcessingResults(false);

                ON_CALL(handler, LoadAsset(
                    Matcher<Containers::Scene&>(_), Matcher<const AZStd::string&>(_), Matcher<AssetImportRequest::RequestingApplication>(_)))
                    .WillByDefault(Return(LoadingResult::AssetFailure));

                EXPECT_CALL(handler, GetManifestExtension(_)).Times(0);
                EXPECT_CALL(handler, GetSupportedFileExtensions(_)).Times(0);

                EXPECT_CALL(handler, PrepareForAssetLoading(_)).Times(1);
                EXPECT_CALL(handler, LoadAsset(_, _, _)).Times(1);
                EXPECT_CALL(handler, FinalizeAssetLoading(_)).Times(1);
                EXPECT_CALL(handler, UpdateManifest(_, _, _)).Times(0);

                AZStd::shared_ptr<Containers::Scene> result = 
                    AssetImportRequest::LoadSceneFromVerifiedPath("test.asset", Events::AssetImportRequest::RequestingApplication::Generic);
                EXPECT_EQ(nullptr, result);
            }

            TEST(AssetImporterRequestTests, LoadSceneFromVerifiedPath_ManifestFailureToLoad_UpdateManifestNotCalledAndReturnsNullptr)
            {
                StrictMock<MockAssetImportRequestHandler> handler;
                handler.SetDefaultExtensions();
                handler.SetDefaultProcessingResults(true);

                ON_CALL(handler, LoadAsset(
                    Matcher<Containers::Scene&>(_), Matcher<const AZStd::string&>(_), Matcher<AssetImportRequest::RequestingApplication>(_)))
                    .WillByDefault(Return(LoadingResult::ManifestFailure));

                EXPECT_CALL(handler, GetManifestExtension(_)).Times(0);
                EXPECT_CALL(handler, GetSupportedFileExtensions(_)).Times(0);

                EXPECT_CALL(handler, PrepareForAssetLoading(_)).Times(1);
                EXPECT_CALL(handler, LoadAsset(_, _, _)).Times(1);
                EXPECT_CALL(handler, FinalizeAssetLoading(_)).Times(1);
                EXPECT_CALL(handler, UpdateManifest(_, _, _)).Times(0);

                AZStd::shared_ptr<Containers::Scene> result =
                    AssetImportRequest::LoadSceneFromVerifiedPath("test.asset", Events::AssetImportRequest::RequestingApplication::Generic);
                EXPECT_EQ(nullptr, result);
            }

            TEST(AssetImporterRequestTests, LoadSceneFromVerifiedPath_NothingLoaded_UpdateManifestNotCalledAndReturnsNullptr)
            {
                StrictMock<MockAssetImportRequestHandler> handler;
                handler.SetDefaultExtensions();
                handler.SetDefaultProcessingResults(false);

                ON_CALL(handler, LoadAsset(
                    Matcher<Containers::Scene&>(_), Matcher<const AZStd::string&>(_), Matcher<AssetImportRequest::RequestingApplication>(_)))
                    .WillByDefault(Return(LoadingResult::Ignored));

                EXPECT_CALL(handler, GetManifestExtension(_)).Times(0);
                EXPECT_CALL(handler, GetSupportedFileExtensions(_)).Times(0);

                EXPECT_CALL(handler, PrepareForAssetLoading(_)).Times(1);
                EXPECT_CALL(handler, LoadAsset(_, _, _)).Times(1);
                EXPECT_CALL(handler, FinalizeAssetLoading(_)).Times(1);
                EXPECT_CALL(handler, UpdateManifest(_, _, _)).Times(0);

                AZStd::shared_ptr<Containers::Scene> result =
                    AssetImportRequest::LoadSceneFromVerifiedPath("test.asset", Events::AssetImportRequest::RequestingApplication::Generic);
                EXPECT_EQ(nullptr, result);
            }

            TEST(AssetImporterRequestTests, LoadSceneFromVerifiedPath_ManifestUpdateFailed_ReturnsNullptr)
            {
                StrictMock<MockAssetImportRequestHandler> assetHandler;
                assetHandler.SetDefaultExtensions();
                assetHandler.SetDefaultProcessingResults(false);
                StrictMock<MockAssetImportRequestHandler> manifestHandler;
                manifestHandler.SetDefaultExtensions();
                manifestHandler.SetDefaultProcessingResults(true);

                ON_CALL(assetHandler, UpdateManifest(Matcher<Containers::Scene&>(_), Matcher<AssetImportRequest::ManifestAction>(_), 
                    Matcher<AssetImportRequest::RequestingApplication>(_)))
                    .WillByDefault(Return(ProcessingResult::Failure));

                EXPECT_CALL(assetHandler, GetManifestExtension(_)).Times(0);
                EXPECT_CALL(assetHandler, GetSupportedFileExtensions(_)).Times(0);
                EXPECT_CALL(manifestHandler, GetManifestExtension(_)).Times(0);
                EXPECT_CALL(manifestHandler, GetSupportedFileExtensions(_)).Times(0);

                EXPECT_CALL(assetHandler, PrepareForAssetLoading(_)).Times(1);
                EXPECT_CALL(assetHandler, LoadAsset(_, _, _)).Times(1);
                EXPECT_CALL(assetHandler, FinalizeAssetLoading(_)).Times(1);
                EXPECT_CALL(assetHandler, UpdateManifest(_, _, _)).Times(1);
                EXPECT_CALL(manifestHandler, PrepareForAssetLoading(_)).Times(1);
                EXPECT_CALL(manifestHandler, LoadAsset(_, _, _)).Times(1);
                EXPECT_CALL(manifestHandler, FinalizeAssetLoading(_)).Times(1);
                EXPECT_CALL(manifestHandler, UpdateManifest(_, _, _)).Times(1);

                AZStd::shared_ptr<Containers::Scene> result =
                    AssetImportRequest::LoadSceneFromVerifiedPath("test.asset", Events::AssetImportRequest::RequestingApplication::Generic);
                EXPECT_EQ(nullptr, result);
            }

            TEST(AssetImporterRequestTests, LoadSceneFromVerifiedPath_FullLoad_ReturnsValidScenePointer)
            {
                StrictMock<MockAssetImportRequestHandler> assetHandler;
                assetHandler.SetDefaultExtensions();
                assetHandler.SetDefaultProcessingResults(false);
                StrictMock<MockAssetImportRequestHandler> manifestHandler;
                manifestHandler.SetDefaultExtensions();
                manifestHandler.SetDefaultProcessingResults(true);

                EXPECT_CALL(assetHandler, GetManifestExtension(_)).Times(0);
                EXPECT_CALL(assetHandler, GetSupportedFileExtensions(_)).Times(0);
                EXPECT_CALL(manifestHandler, GetManifestExtension(_)).Times(0);
                EXPECT_CALL(manifestHandler, GetSupportedFileExtensions(_)).Times(0);

                EXPECT_CALL(assetHandler, PrepareForAssetLoading(_)).Times(1);
                EXPECT_CALL(assetHandler, LoadAsset(_, _, _)).Times(1);
                EXPECT_CALL(assetHandler, FinalizeAssetLoading(_)).Times(1);
                EXPECT_CALL(assetHandler, UpdateManifest(_, _, _)).Times(1);
                EXPECT_CALL(manifestHandler, PrepareForAssetLoading(_)).Times(1);
                EXPECT_CALL(manifestHandler, LoadAsset(_, _, _)).Times(1);
                EXPECT_CALL(manifestHandler, FinalizeAssetLoading(_)).Times(1);
                EXPECT_CALL(manifestHandler, UpdateManifest(_, _, _)).Times(1);

                AZStd::shared_ptr<Containers::Scene> result =
                    AssetImportRequest::LoadSceneFromVerifiedPath("test.asset", Events::AssetImportRequest::RequestingApplication::Generic);
                EXPECT_NE(nullptr, result);
            }
            
        } // Events
    } // SceneAPI
} // AZ