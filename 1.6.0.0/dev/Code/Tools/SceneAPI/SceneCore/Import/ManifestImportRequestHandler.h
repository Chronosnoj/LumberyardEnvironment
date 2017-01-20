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
#include <SceneAPI/SceneCore/SceneCoreConfiguration.h>
#include <SceneAPI/SceneCore/Events/AssetImportRequest.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Import
        {
            class ManifestImportRequestHandler
                : public Events::AssetImportRequestBus::Handler
            {
            public:
                AZ_CLASS_ALLOCATOR_DECL

                SCENE_CORE_API ManifestImportRequestHandler();
                SCENE_CORE_API ~ManifestImportRequestHandler() override;

                SCENE_CORE_API void GetManifestExtension(AZStd::string& result) override;
                SCENE_CORE_API Events::LoadingResult LoadAsset(Containers::Scene& scene, const AZStd::string& path, 
                    RequestingApplication requester) override;
                
            private:
                static const char* s_extension;
            };
        } // Import
    } // SceneAPI
} // AZ