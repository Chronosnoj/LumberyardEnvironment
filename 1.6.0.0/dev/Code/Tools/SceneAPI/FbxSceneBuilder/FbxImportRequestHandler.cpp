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
#include <AzCore/IO/GenericStreams.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <SceneAPI/SceneCore/Containers/SceneManifest.h>
#include <SceneAPI/SceneCore/Containers/Views/PairIterator.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshData.h>
#include <SceneAPI/SceneCore/DataTypes/Groups/IMeshGroup.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ISceneNodeSelectionList.h>
#include <SceneAPI/SceneCore/Events/ManifestMetaInfoBus.h>
#include <SceneAPI/SceneData/Groups/MeshGroup.h>
#include <SceneAPI/FbxSceneBuilder/FbxImporter.h>
#include <SceneAPI/FbxSceneBuilder/FbxImportRequestHandler.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace FbxSceneImporter
        {
            const char* FbxImportRequestHandler::s_extension = ".fbx";

            AZ_CLASS_ALLOCATOR_IMPL(FbxImportRequestHandler, AZ::SystemAllocator, 0)

            FbxImportRequestHandler::FbxImportRequestHandler()
            {
                BusConnect();
            }

            FbxImportRequestHandler::~FbxImportRequestHandler()
            {
                BusDisconnect();
            }

            void FbxImportRequestHandler::GetSupportedFileExtensions(AZStd::unordered_set<AZStd::string>& extensions)
            {
                extensions.insert(s_extension);
            }

            Events::LoadingResult FbxImportRequestHandler::LoadAsset(Containers::Scene& scene, const AZStd::string& path, RequestingApplication requester)
            {
                if (!AzFramework::StringFunc::Path::IsExtension(path.c_str(), s_extension))
                {
                    return Events::LoadingResult::Ignored;
                }

                scene.SetSourceFilename(path);

                FbxImporter importer;
                return importer.PopulateFromFile(path.c_str(), scene) ? Events::LoadingResult::AssetLoaded : Events::LoadingResult::AssetFailure;
            }

            Events::ProcessingResult FbxImportRequestHandler::UpdateManifest(Containers::Scene& scene, ManifestAction action, RequestingApplication requester)
            {
                if (action == ManifestAction::ConstructDefault && requester != RequestingApplication::AssetProcessor)
                {
                    // Does manifest already have a mesh group?
                    Containers::SceneManifest& manifest = scene.GetManifest();
                    for (auto& it : manifest.GetValueStorage())
                    {
                        if (it->RTTI_IsTypeOf(DataTypes::IMeshGroup::TYPEINFO_Uuid()))
                        {
                            return Events::ProcessingResult::Ignored;
                        }
                    }

                    // Check if there's any meshes to include in the group to begin with.
                    Containers::SceneGraph& graph = scene.GetGraph();
                    bool hasMeshes = false;
                    for (auto& it : graph.GetContentStorage())
                    {
                        if (it && it->RTTI_IsTypeOf(DataTypes::IMeshData::TYPEINFO_Uuid()))
                        {
                            hasMeshes = true;
                            break;
                        }
                    }
                    if (!hasMeshes)
                    {
                        AZ_TracePrintf(Utilities::WarningWindow, "No meshes found for exporting.");
                        return Events::ProcessingResult::Failure;
                    }

                    // There are meshes but no mesh group, so add a default mesh group to the manifest.
                    AZStd::shared_ptr<SceneData::MeshGroup> group = AZStd::make_shared<SceneData::MeshGroup>();
                    EBUS_EVENT(Events::ManifestMetaInfoBus, InitializeObject, scene, group.get());
                    manifest.AddEntry(scene.GetName(), AZStd::move(group));

                    return Events::ProcessingResult::Success;
                }
                else
                {
                    return Events::ProcessingResult::Ignored;
                }
            }
        } // Import
    } // SceneAPI
} // AZ