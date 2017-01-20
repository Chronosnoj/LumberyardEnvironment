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
#include <SceneAPI/SceneCore/Events/AssetImportRequest.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Events
        {
            using ::testing::Invoke;
            using ::testing::Return;
            using ::testing::Matcher;
            using ::testing::_;

            class MockAssetImportRequestHandler
                : public AssetImportRequestBus::Handler
            {
            public:
                MockAssetImportRequestHandler()
                {
                    BusConnect();
                }

                ~MockAssetImportRequestHandler() override
                {
                    BusDisconnect();
                }

                MOCK_METHOD1(GetSupportedFileExtensions,
                    void(AZStd::unordered_set<AZStd::string>& extensions));
                MOCK_METHOD1(GetManifestExtension,
                    void(AZStd::string& result));

                MOCK_METHOD1(PrepareForAssetLoading,
                    ProcessingResult(Containers::Scene& scene));
                MOCK_METHOD3(LoadAsset,
                    LoadingResult(Containers::Scene& scene, const AZStd::string& path, RequestingApplication requester));
                MOCK_METHOD1(FinalizeAssetLoading,
                    void(Containers::Scene& scene));
                MOCK_METHOD3(UpdateManifest,
                    ProcessingResult(Containers::Scene& scene, ManifestAction action, RequestingApplication requester));

                void DefaultGetSupportedFileExtensions(AZStd::unordered_set<AZStd::string>& extensions)
                {
                    extensions.insert(".asset");
                }

                void DefaultGetManifestExtension(AZStd::string& result)
                {
                    result = ".manifest";
                }

                void SetDefaultExtensions()
                {
                    ON_CALL(*this, GetSupportedFileExtensions(Matcher<AZStd::unordered_set<AZStd::string>&>(_)))
                        .WillByDefault(Invoke(this, &MockAssetImportRequestHandler::DefaultGetSupportedFileExtensions));
                    ON_CALL(*this, GetManifestExtension(Matcher<AZStd::string&>(_)))
                        .WillByDefault(Invoke(this, &MockAssetImportRequestHandler::DefaultGetManifestExtension));
                }

                void SetDefaultProcessingResults(bool forManifest)
                {
                    ON_CALL(*this, PrepareForAssetLoading(Matcher<Containers::Scene&>(_)))
                        .WillByDefault(Return(ProcessingResult::Ignored));
                    if (forManifest)
                    {
                        ON_CALL(*this, LoadAsset(Matcher<Containers::Scene&>(_), Matcher<const AZStd::string&>(_), Matcher<RequestingApplication>(_)))
                            .WillByDefault(Return(LoadingResult::ManifestLoaded));
                    }
                    else
                    {
                        ON_CALL(*this, LoadAsset(Matcher<Containers::Scene&>(_), Matcher<const AZStd::string&>(_), Matcher<RequestingApplication>(_)))
                            .WillByDefault(Return(LoadingResult::AssetLoaded));
                    }
                    ON_CALL(*this, UpdateManifest(Matcher<Containers::Scene&>(_), Matcher<ManifestAction>(_), Matcher<RequestingApplication>(_)))
                        .WillByDefault(Return(ProcessingResult::Ignored));
                }
            };
        } // Events
    } // SceneAPI
} // AZ