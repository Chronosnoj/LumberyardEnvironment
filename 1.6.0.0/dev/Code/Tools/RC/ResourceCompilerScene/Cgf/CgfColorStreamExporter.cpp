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

#include <Cry_Geo.h> // Needed for IIndexedMesh.h
#include <IIndexedMesh.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <SceneAPI/SceneCore/DataTypes/DataTypeUtilities.h>
#include <SceneAPI/SceneCore/DataTypes/Groups/IMeshGroup.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IMeshAdvancedRule.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshVertexColorData.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>
#include <RC/ResourceCompilerScene/Cgf/CgfExportContexts.h>
#include <RC/ResourceCompilerScene/Cgf/CgfColorStreamExporter.h>

namespace AZ
{
    namespace RC
    {
        namespace SceneEvents = AZ::SceneAPI::Events;
        namespace SceneDataTypes = AZ::SceneAPI::DataTypes;
        namespace SceneContainers = AZ::SceneAPI::Containers;

        CgfColorStreamExporter::CgfColorStreamExporter()
            : CallProcessorBinder()
        {
            BindToCall(&CgfColorStreamExporter::CopyVertexColorStream);
        }

        SceneEvents::ProcessingResult CgfColorStreamExporter::CopyVertexColorStream(CgfMeshNodeExportContext& context) const
        {
            if (context.m_phase != Phase::Filling)
            {
                return SceneEvents::ProcessingResult::Ignored;
            }

            const SceneContainers::SceneGraph& graph = context.m_scene.GetGraph();
            const SceneDataTypes::IMeshGroup& group = context.m_group;

            AZStd::shared_ptr<const SceneDataTypes::IMeshVertexColorData> colors = nullptr;
            AZStd::string streamName;

            AZStd::shared_ptr<const SceneDataTypes::IMeshAdvancedRule> rule = SceneDataTypes::Utilities::FindRule<SceneDataTypes::IMeshAdvancedRule>(group);
            if (rule)
            {
                if (rule->IsVertexColorStreamDisabled())
                {
                    return SceneEvents::ProcessingResult::Ignored;
                }
                if (!rule->GetVertexColorStreamName().empty())
                {
                    SceneContainers::SceneGraph::NodeIndex index =
                        graph.Find(context.m_nodeIndex, rule->GetVertexColorStreamName().c_str());
                    colors = azrtti_cast<const SceneDataTypes::IMeshVertexColorData*>(graph.GetNodeContent(index));
                    if (colors)
                    {
                        streamName = rule->GetVertexColorStreamName();
                    }
                }
            }
            
            // No rule set or the rule was set to an option that's not supported, so default to the first vertex color stream found.
            if (!colors)
            {
                AZ_TracePrintf(AZ::SceneAPI::Utilities::LogWindow, "No vertex color stream specified. Defaulting to first available stream.");
                SceneContainers::SceneGraph::NodeIndex index = graph.GetNodeChild(context.m_nodeIndex);
                while (index.IsValid())
                {
                    colors = azrtti_cast<const SceneDataTypes::IMeshVertexColorData*>(graph.GetNodeContent(index));
                    if (colors)
                    {
                        streamName = graph.GetNodeName(index).substr(graph.GetNodeName(context.m_nodeIndex).size() + 1).c_str();
                        AZ_TracePrintf(AZ::SceneAPI::Utilities::LogWindow, "Default color stream found.");
                        break;
                    }
                    index = graph.GetNodeSibling(index);
                }
                AZ_TracePrintf(AZ::SceneAPI::Utilities::LogWindow, "No default color stream found.");

            }

            if (colors)
            {
                bool countMatch = context.m_mesh.GetVertexCount() == colors->GetCount();
                AZ_Assert(countMatch,
                    "Number of vertices in the mesh (%i) don't match with the number of stored vertex colors (%i) in stream '%s'",
                    context.m_mesh.GetVertexCount(), colors->GetCount(), streamName.c_str());
                if (!countMatch)
                {
                    return SceneEvents::ProcessingResult::Failure;
                }

                // Vertex coloring always uses the first vertex color stream.
                context.m_mesh.ReallocStream(CMesh::COLORS_0, context.m_mesh.GetVertexCount());
                for (int i = 0; i < context.m_mesh.GetVertexCount(); ++i)
                {
                    const SceneDataTypes::Color& color = colors->GetColor(i);
                    context.m_mesh.m_pColor0[i] = SMeshColor(
                            static_cast<uint8_t>(GetClamp<float>(color.red, 0.0f, 1.0f) * 255.0f),
                            static_cast<uint8_t>(GetClamp<float>(color.green, 0.0f, 1.0f) * 255.0f),
                            static_cast<uint8_t>(GetClamp<float>(color.blue, 0.0f, 1.0f) * 255.0f),
                            static_cast<uint8_t>(GetClamp<float>(color.alpha, 0.0f, 1.0f) * 255.0f));
                }
                return SceneEvents::ProcessingResult::Success;
            }
            else
            {
                return SceneEvents::ProcessingResult::Ignored;
            }
        }
    } // RC
} // AZ