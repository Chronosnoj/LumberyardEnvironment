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

#include <AzCore/IO/SystemFile.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <SceneAPI/SceneCore/Containers/SceneManifest.h>
#include <SceneAPI/SceneCore/Events/AssetImportRequest.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Events
        {
            //
            // Loading Result Combiner
            //

            LoadingResultCombiner::LoadingResultCombiner()
                : m_manifestResult(ProcessingResult::Ignored)
                , m_assetResult(ProcessingResult::Ignored)
            {
            }

            void LoadingResultCombiner::operator=(LoadingResult rhs)
            {
                switch (rhs)
                {
                case LoadingResult::Ignored:
                    return;
                case LoadingResult::AssetLoaded:
                    m_assetResult = m_assetResult != ProcessingResult::Failure ? ProcessingResult::Success : ProcessingResult::Failure;
                    return;
                case LoadingResult::ManifestLoaded:
                    m_manifestResult = m_manifestResult != ProcessingResult::Failure ? ProcessingResult::Success : ProcessingResult::Failure;
                    return;
                case LoadingResult::AssetFailure:
                    m_assetResult = ProcessingResult::Failure;
                    return;
                case LoadingResult::ManifestFailure:
                    m_manifestResult = ProcessingResult::Failure;
                    return;
                }
            }
            
            ProcessingResult LoadingResultCombiner::GetManifestResult() const
            {
                return m_manifestResult;
            }

            ProcessingResult LoadingResultCombiner::GetAssetResult() const
            {
                return m_assetResult;
            }

            //
            // Asset Importer Request
            //

            void AssetImportRequest::GetManifestExtension(AZStd::string& /*result*/)
            {
            }

            void AssetImportRequest::GetSupportedFileExtensions(AZStd::unordered_set<AZStd::string>& /*extensions*/)
            {
            }

            ProcessingResult AssetImportRequest::PrepareForAssetLoading(Containers::Scene& /*scene*/)
            {
                return ProcessingResult::Ignored;
            }

            LoadingResult AssetImportRequest::LoadAsset(Containers::Scene& /*scene*/, const AZStd::string& /*path*/, RequestingApplication /*requester*/)
            {
                return LoadingResult::Ignored;
            }

            void AssetImportRequest::FinalizeAssetLoading(Containers::Scene& /*scene*/)
            {
            }

            ProcessingResult AssetImportRequest::UpdateManifest(Containers::Scene& /*scene*/, ManifestAction /*action*/, RequestingApplication /*requester*/)
            {
                return ProcessingResult::Ignored;
            }

            AZStd::shared_ptr<Containers::Scene> AssetImportRequest::LoadScene(const AZStd::string& assetFilePath, RequestingApplication requester)
            {
                AZStd::string manifestExtension;
                EBUS_EVENT(AssetImportRequestBus, GetManifestExtension, manifestExtension);
                AZ_Assert(!manifestExtension.empty(), "Manifest extension was not declared.");

                AZStd::string path = assetFilePath;
                if (AzFramework::StringFunc::Path::IsExtension(assetFilePath.c_str(), manifestExtension.c_str()))
                {
                    AzFramework::StringFunc::Path::ReplaceExtension(path, "");
                }

                AZStd::unordered_set<AZStd::string> extensions;
                EBUS_EVENT(AssetImportRequestBus, GetSupportedFileExtensions, extensions);
                AZ_Assert(!extensions.empty(), "No extensions found.");

                // Check if the extension is valid.
                bool isValidType = false;
                for (AZStd::string& it : extensions)
                {
                    if (AzFramework::StringFunc::Path::IsExtension(path.c_str(), it.c_str()))
                    {
                        isValidType = true;
                        break;
                    }
                }

                if (!isValidType)
                {
                    AZ_TracePrintf(Utilities::ErrorWindow, "Path '%s' doesn't point to a supported type.\n", path.c_str());
                    return nullptr;
                }

                // Check if the file exists.
                if (!IO::SystemFile::Exists(path.c_str()))
                {
                    AZ_TracePrintf(Utilities::ErrorWindow, "Failed to get valid asset path.\n");
                    return nullptr;
                }

                return LoadSceneFromVerifiedPath(path, requester);
            }

            AZStd::shared_ptr<Containers::Scene> AssetImportRequest::LoadSceneFromVerifiedPath(const AZStd::string& assetFilePath, 
                RequestingApplication requester)
            {
                AZStd::string sceneName;
                AzFramework::StringFunc::Path::GetFileName(assetFilePath.c_str(), sceneName);
                AZStd::shared_ptr<Containers::Scene> scene = AZStd::make_shared<Containers::Scene>(AZStd::move(sceneName));
                AZ_Assert(scene, "Unable to create new scene for asset importing.");

                ProcessingResultCombiner areAllPrepared;
                EBUS_EVENT_RESULT(areAllPrepared, AssetImportRequestBus, PrepareForAssetLoading, *scene);
                if (areAllPrepared.GetResult() == ProcessingResult::Failure)
                {
                    AZ_TracePrintf(Utilities::ErrorWindow, "Not all asset loaders could initialize.\n");
                    return nullptr;
                }

                LoadingResultCombiner filesLoaded;
                EBUS_EVENT_RESULT(filesLoaded, AssetImportRequestBus, LoadAsset, *scene, assetFilePath, requester);
                EBUS_EVENT(AssetImportRequestBus, FinalizeAssetLoading, *scene);
                
                if (filesLoaded.GetAssetResult() != ProcessingResult::Success)
                {
                    AZ_TracePrintf(Utilities::ErrorWindow, "Failed to load requested asset.\n");
                    return nullptr;
                }

                ManifestAction action = ManifestAction::Update;
                if (filesLoaded.GetManifestResult() == ProcessingResult::Failure || scene->GetManifest().IsEmpty())
                {
                    scene->GetManifest().Clear();
                    action = ManifestAction::ConstructDefault;
                }
                ProcessingResultCombiner manifestUpdate;
                EBUS_EVENT_RESULT(manifestUpdate, AssetImportRequestBus, UpdateManifest, *scene, action, requester);
                if (manifestUpdate.GetResult() == ProcessingResult::Failure)
                {
                    AZ_TracePrintf(Utilities::ErrorWindow, "Unable to %s manifest.\n", action == ManifestAction::ConstructDefault ? "create new" : "update");
                    return nullptr;
                }

                return scene;
            }
        } // Events
    } // SceneAPI
} // AZ