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

#include <string>
#include <SceneAPI/SceneCore/Export/IExporter.h>
#include <SceneAPI/SceneCore/SceneCoreConfiguration.h>

namespace AZ
{
    namespace IO
    {
        class SystemFile;
    }
    namespace SceneAPI
    {
        namespace Export
        {
            namespace Utilities
            {
                // The DotExporter exports a graph diagram in Graphviz's DOT format.
                //      This exporter aims to be a debugging tool to aid inspecting
                //      the SceneGraph, not as a storage format or fully supported
                //      graph representation of the SceneGraph.
                //      Online viewers for the DOT format can be found here:
                //          - http://webgraphviz.com/
                //          - http://sandbox.kidstrythisathome.com/erdos/
                //      Gray nodes contain data and some visualization will show the
                //      type of data as a tooltip when hovering over them.
                class DotExporter
                    : public IExporter
                {
                public:
                    SCENE_CORE_API bool WriteToFile(const char* outputDirectory, const AZ::SceneAPI::Containers::Scene& scene) override;
                    SCENE_CORE_API bool WriteToFile(const std::string& outputDirectory, const AZ::SceneAPI::Containers::Scene& scene) override;

                private:
                    std::string ExtractName(const std::string& name, char prefix) const;

                    void Write(AZ::IO::SystemFile& file, char character) const;
                    void Write(AZ::IO::SystemFile& file, const char* string) const;

                    void WriteProlog(AZ::IO::SystemFile& file, const char* name) const;
                    void WriteEpilog(AZ::IO::SystemFile& file) const;

                    void WriteNodeName(AZ::IO::SystemFile& file, const char* name, const char* label, const char* dataTypeName) const;
                    void WriteNodeParent(AZ::IO::SystemFile& file, const char* from, const char* to, bool bidirectional) const;
                    void WriteNodeSibling(AZ::IO::SystemFile& file, const char* from, const char* to) const;

                    void WriteScene(AZ::IO::SystemFile& file, const AZ::SceneAPI::Containers::Scene& scene) const;
                };
            } // Utilities
        } // Export
    } // SceneAPI
} // AZ