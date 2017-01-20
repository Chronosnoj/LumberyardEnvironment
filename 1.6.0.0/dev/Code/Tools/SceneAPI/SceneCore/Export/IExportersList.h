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

#include <memory>
#include <string>
#include <SceneAPI/SceneCore/Export/IExportersList.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            class Scene;
        }

        namespace Export
        {
            class IExporter;

            // Interface for creating exporter lists. The main purpose is to be able to
            //      register multiple exporters and make a single call to export all
            //      needed information from a scene.
            class IExportersList
            {
            public:
                virtual ~IExportersList() = 0;

                virtual void RegisterExporter(std::shared_ptr<IExporter> exporter) = 0;

                virtual bool ExportScene(const char* outputDirectory, const AZ::SceneAPI::Containers::Scene& scene) = 0;
                virtual bool ExportScene(const std::string& outputDirectory, const AZ::SceneAPI::Containers::Scene& scene) = 0;
            };

            inline IExportersList::~IExportersList()
            {
            }
        } // Export
    } // SceneAPI
} // AZ