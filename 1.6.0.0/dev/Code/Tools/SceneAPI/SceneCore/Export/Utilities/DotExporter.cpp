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

#include <algorithm>
#include <AzCore/IO/SystemFile.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <SceneAPI/SceneCore/DataTypes/IGraphObject.h>
#include <SceneAPI/SceneCore/Containers/Views/PairIterator.h>
#include <SceneAPI/SceneCore/Export/Utilities/DotExporter.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Export
        {
            namespace Utilities
            {
                bool DotExporter::WriteToFile(const char* outputDirectory, const AZ::SceneAPI::Containers::Scene& scene)
                {
                    AZStd::string targetFilename;
                    if (!AzFramework::StringFunc::Path::ConstructFull(outputDirectory, scene.GetName().c_str(), ".dot", targetFilename, true))
                    {
                        return false;
                    }
                    if (AZ::IO::SystemFile::Exists(targetFilename.c_str()) && !AZ::IO::SystemFile::IsWritable(targetFilename.c_str()))
                    {
                        return false;
                    }

                    AZ::IO::SystemFile output;
                    if (!output.Open(targetFilename.c_str(),
                            AZ::IO::SystemFile::SF_OPEN_CREATE | AZ::IO::SystemFile::SF_OPEN_CREATE_PATH | AZ::IO::SystemFile::SF_OPEN_WRITE_ONLY))
                    {
                        return false;
                    }

                    WriteProlog(output, scene.GetName().c_str());
                    WriteScene(output, scene);
                    WriteEpilog(output);
                    output.Close();

                    return true;
                }

                bool DotExporter::WriteToFile(const std::string& outputDirectory, const AZ::SceneAPI::Containers::Scene& scene)
                {
                    return WriteToFile(outputDirectory.c_str(), scene);
                }

                std::string DotExporter::ExtractName(const std::string& name, char prefix) const
                {
                    std::string result;
                    result += prefix;
                    result += name;
                    result.erase(std::remove(result.begin(), result.end(), '.'), result.end());
                    return result;
                }

                void DotExporter::Write(AZ::IO::SystemFile& file, char character) const
                {
                    file.Write(&character, 1);
                }

                void DotExporter::Write(AZ::IO::SystemFile& file, const char* string) const
                {
                    file.Write(string, strlen(string));
                }

                void DotExporter::WriteProlog(AZ::IO::SystemFile& file, const char* name) const
                {
                    Write(file, "digraph ");
                    Write(file, name);
                    Write(file,
                        "\n"
                        "{\n"
                        "    rankdir=\"BT\"");
                }

                void DotExporter::WriteEpilog(AZ::IO::SystemFile& file) const
                {
                    Write(file,
                        "\n"
                        "}");
                }

                void DotExporter::WriteNodeName(AZ::IO::SystemFile& file, const char* name, const char* label, const char* dataTypeName) const
                {
                    Write(file,
                        "\n"
                        "    ");
                    Write(file, name);
                    Write(file, " [label=\"");
                    Write(file, label);
                    Write(file, '\"');
                    if (dataTypeName != nullptr)
                    {
                        Write(file, ", style=filled, fillcolor=lightgray, tooltip=\"");
                        Write(file, dataTypeName);
                        Write(file, '"');
                    }
                    Write(file, ']');
                }

                void DotExporter::WriteNodeParent(AZ::IO::SystemFile& file, const char* from, const char* to, bool bidirectional) const
                {
                    Write(file,
                        "\n"
                        "    ");
                    Write(file, from);
                    Write(file, " -> ");
                    Write(file, to);
                    if (bidirectional)
                    {
                        Write(file, " [dir=both, color=blue]");
                    }
                }
                void DotExporter::WriteNodeSibling(AZ::IO::SystemFile& file, const char* from, const char* to) const
                {
                    Write(file,
                        "\n"
                        "    ");
                    Write(file, from);
                    Write(file, " -> ");
                    Write(file, to);
                    Write(file,
                        " [color=purple]\n"
                        "    {rank=same; ");
                    Write(file, to);
                    Write(file, ' ');
                    Write(file, from);
                    Write(file, '}');
                }

                void DotExporter::WriteScene(AZ::IO::SystemFile& file, const AZ::SceneAPI::Containers::Scene& scene) const
                {
                    const Containers::SceneGraph& graph = scene.GetGraph();
                    auto view = Containers::Views::MakePairView(graph.GetNameStorage(), graph.GetHierarchyStorage());
                    for (auto it = view.begin(); it != view.end(); ++it)
                    {
                        const char* typeName = nullptr;
                        Containers::SceneGraph::NodeIndex index = graph.ConvertToNodeIndex(it.GetSecondIterator());
                        if (graph.HasNodeContent(index))
                        {
                            typeName = graph.GetNodeContent(index)->RTTI_GetTypeName();
                        }

                        std::string label = AZ::SceneAPI::Containers::SceneGraph::GetShortName(it->first);
                        if (it->second.IsEndPoint())
                        {
                            label += " *";
                        }

                        WriteNodeName(file, ExtractName(it->first, 'n').c_str(), label.c_str(), typeName);
                        if (it->second.HasParent())
                        {
                            Containers::SceneGraph::NodeIndex parentIndex = graph.GetNodeParent(it->second);
                            bool isBidirectional = (graph.ConvertToNodeIndex(it.GetSecondIterator()) == graph.GetNodeChild(parentIndex));
                            WriteNodeParent(file, ExtractName(it->first, 'n').c_str(), ExtractName(graph.GetNodeName(parentIndex), 'n').c_str(), isBidirectional);
                        }

                        if (it->second.HasSibling())
                        {
                            Containers::SceneGraph::NodeIndex siblingIndex = graph.GetNodeSibling(it->second);
                            WriteNodeSibling(file, ExtractName(it->first, 'n').c_str(), ExtractName(graph.GetNodeName(siblingIndex), 'n').c_str());
                        }
                    }
                }
            } // Utilities
        } // Export
    } // SceneAPI
} // AZ