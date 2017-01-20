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

#include "stdafx.h"
#include <IConfig.h>
#include <SceneConfig.h>
#include <AzCore/Module/DynamicModuleHandle.h>
#include <SceneAPI/SceneCore/Import/Utilities/FileFinder.h>
#include <SceneAPI/FbxSceneBuilder/FbxImporter.h>

namespace AZ
{
    namespace RC
    {
        namespace FbxSceneImporter = AZ::SceneAPI::FbxSceneImporter;

        SceneConfig::SceneConfig()
        {
            LoadSceneLibrary("SceneCore");
            LoadSceneLibrary("SceneData");
            LoadSceneLibrary("FbxSceneBuilder");
            
            m_importersList.RegisterImporter(FbxSceneImporter::FbxImporter::GetDefaultFileExtension(), std::make_shared<FbxSceneImporter::FbxImporter>());
        }

        const char* SceneConfig::GetManifestFileExtension() const
        {
            return AZ::SceneAPI::Import::Utilities::FileFinder::GetManifestExtension();
        }

        const SceneAPI::Import::IImportersList& SceneConfig::GetImporters() const
        {
            return m_importersList;
        }

        void SceneConfig::ReflectModules(SerializeContext* context)
        {
            for (AZStd::unique_ptr<AZ::DynamicModuleHandle>& module : m_modules)
            {
                using ReflectFunc = void(*)(SerializeContext*);

                ReflectFunc reflect = module->GetFunction<ReflectFunc>("Reflect");
                if (reflect)
                {
                    (*reflect)(context);
                }
            }
        }

        void SceneConfig::LoadSceneLibrary(const char* name)
        {
            AZStd::unique_ptr<DynamicModuleHandle> module = AZ::DynamicModuleHandle::Create(name);
            AZ_Assert(module, "Failed to initialize library '%s'", name);
            module->Load(true);
            m_modules.push_back(AZStd::move(module));
        }
    } // RC
} // AZ