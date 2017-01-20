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

#include <SceneAPI/SceneCore/Export/IExporter.h>
#include <SceneAPI/SceneCore/Export/ExportersList.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Export
        {
            void ExportersList::RegisterExporter(std::shared_ptr<IExporter> exporter)
            {
                if (exporter != nullptr)
                {
                    m_exporters.push_back(exporter);
                }
            }

            bool ExportersList::ExportScene(const char* outputDirectory, const AZ::SceneAPI::Containers::Scene& scene)
            {
                bool result = true;
                for (auto it : m_exporters)
                {
                    result &= it->WriteToFile(outputDirectory, scene);
                }
                return result;
            }

            bool ExportersList::ExportScene(const std::string& outputDirectory, const AZ::SceneAPI::Containers::Scene& scene)
            {
                bool result = true;
                for (auto it : m_exporters)
                {
                    result &= it->WriteToFile(outputDirectory, scene);
                }
                return result;
            }
        } // Export
    } // SceneAPI
} // AZ