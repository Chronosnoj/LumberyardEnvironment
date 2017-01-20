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

#include <vector>
#include <SceneAPI/SceneCore/Export/IExportersList.h>
#include <SceneAPI/SceneCore/SceneCoreConfiguration.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Export
        {
            class ExportersList
                : public IExportersList
            {
            public:
                SCENE_CORE_API void RegisterExporter(std::shared_ptr<IExporter> exporter) override;

                SCENE_CORE_API bool ExportScene(const char* outputDirectory, const AZ::SceneAPI::Containers::Scene& scene) override;
                SCENE_CORE_API bool ExportScene(const std::string& outputDirectory, const AZ::SceneAPI::Containers::Scene& scene) override;

            private:
                std::vector<std::shared_ptr<IExporter> > m_exporters;
            };
        } // Export
    } // SceneAPI
} // AZ