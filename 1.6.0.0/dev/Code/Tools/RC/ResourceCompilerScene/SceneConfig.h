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

#include <ISceneConfig.h>
#include <TraceDrillerHook.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <SceneAPI/SceneCore/Import/ImportersList.h>

namespace AZ
{
    class DynamicModuleHandle;
    namespace RC
    {
        class SceneConfig
            : public ISceneConfig
        {
        public:
            SceneConfig();

            const char* GetManifestFileExtension() const override;
            const SceneAPI::Import::IImportersList& GetImporters() const override;

            void ReflectModules(SerializeContext* context) override;

        protected:
            virtual void SceneConfig::LoadSceneLibrary(const char* name);
            
            AZStd::vector<AZStd::unique_ptr<AZ::DynamicModuleHandle>> m_modules;
            AZ::SceneAPI::Import::ImportersList m_importersList;

            TraceDrillerHook traceHook;
        };
    } // RC
} // AZ