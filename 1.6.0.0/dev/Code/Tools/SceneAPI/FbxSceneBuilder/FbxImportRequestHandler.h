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

#include <AzCore/Memory/Memory.h>
#include <SceneAPI/SceneCore/Events/AssetImportRequest.h>
#include <SceneAPI/FbxSceneBuilder/FbxSceneBuilderConfiguration.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace FbxSceneImporter
        {
            class FbxImportRequestHandler
                : public Events::AssetImportRequestBus::Handler
            {
            public:
                AZ_CLASS_ALLOCATOR_DECL

                FBX_SCENE_BUILDER_API FbxImportRequestHandler();
                FBX_SCENE_BUILDER_API ~FbxImportRequestHandler() override;

                FBX_SCENE_BUILDER_API void GetSupportedFileExtensions(AZStd::unordered_set<AZStd::string>& extensions) override;
                FBX_SCENE_BUILDER_API Events::LoadingResult LoadAsset(Containers::Scene& scene, const AZStd::string& path, 
                    RequestingApplication requester) override;
                FBX_SCENE_BUILDER_API Events::ProcessingResult UpdateManifest(Containers::Scene& scene, ManifestAction action,
                    RequestingApplication requester) override;

            private:
                static const char* s_extension;
            };
        } // FbxSceneImporter
    } // SceneAPI
} // AZ