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
#include <AzCore/Math/Vector2.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <SceneAPI/SceneCore/DataTypes/DataTypeUtilities.h>
#include <SceneAPI/SceneCore/DataTypes/Groups/IMeshGroup.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IMeshAdvancedRule.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshVertexUVData.h>
#include <RC/ResourceCompilerScene/Cgf/CgfExportContexts.h>
#include <RC/ResourceCompilerScene/Cgf/CgfUVStreamExporter.h>

namespace AZ
{
    namespace RC
    {
        namespace SceneEvents = AZ::SceneAPI::Events;
        namespace SceneDataTypes = AZ::SceneAPI::DataTypes;
        namespace SceneContainers = AZ::SceneAPI::Containers;

        CgfUVStreamExporter::CgfUVStreamExporter()
            : CallProcessorBinder()
        {
            BindToCall(&CgfUVStreamExporter::CopyUVStream);
        }

        SceneEvents::ProcessingResult CgfUVStreamExporter::CopyUVStream(CgfMeshNodeExportContext& context) const
        {
            if (context.m_phase != Phase::Filling)
            {
                return SceneEvents::ProcessingResult::Ignored;
            }

            const SceneContainers::SceneGraph& graph = context.m_scene.GetGraph();
            const SceneDataTypes::IMeshGroup& group = context.m_group;

            AZStd::shared_ptr<const SceneDataTypes::IMeshVertexUVData> uvs = nullptr;
            AZStd::string streamName;
            bool searchForDefaultUVs = true;

            AZStd::shared_ptr<const SceneDataTypes::IMeshAdvancedRule> rule = SceneDataTypes::Utilities::FindRule<SceneDataTypes::IMeshAdvancedRule>(group);
            if (rule)
            {
                if (rule->IsUVStreamDisabled())
                {
                    searchForDefaultUVs = false;
                }
                else if (!rule->GetUVStreamName().empty())
                {
                    SceneContainers::SceneGraph::NodeIndex index =
                        graph.Find(context.m_nodeIndex, rule->GetUVStreamName().c_str());
                    uvs = azrtti_cast<const SceneDataTypes::IMeshVertexUVData*>(graph.GetNodeContent(index));
                    if (uvs)
                    {
                        streamName = rule->GetUVStreamName();
                        searchForDefaultUVs = false;
                    }
                }
            }
            
            if (searchForDefaultUVs)
            {
                SceneContainers::SceneGraph::NodeIndex index = graph.GetNodeChild(context.m_nodeIndex);
                while (index.IsValid())
                {
                    uvs = azrtti_cast<const SceneDataTypes::IMeshVertexUVData*>(graph.GetNodeContent(index));
                    if (uvs)
                    {
                        streamName = graph.GetNodeName(index).substr(graph.GetNodeName(context.m_nodeIndex).size() + 1).c_str();
                        break;
                    }
                    index = graph.GetNodeSibling(index);
                }
            }

            context.m_mesh.ReallocStream(CMesh::TEXCOORDS, context.m_mesh.GetVertexCount());
            
            if (uvs)
            {
                bool matchCount = context.m_mesh.GetVertexCount() == uvs->GetCount();
                AZ_Assert(matchCount,
                    "Number of vertices in the mesh (%i) doesn't match with the number of stored UVs (%i) in stream '%s'",
                    context.m_mesh.GetVertexCount(), uvs->GetCount(), streamName.c_str());
                if (!matchCount)
                {
                    return SceneEvents::ProcessingResult::Failure;
                }

                for (int i = 0; i < context.m_mesh.GetVertexCount(); ++i)
                {
                    const AZ::Vector2& uv = uvs->GetUV(i);
                    context.m_mesh.m_pTexCoord[i] = SMeshTexCoord(uv.GetX(), uv.GetY());
                }
            }
            else //CGF needs something in the texture channel so dummy coords if we don't have any uv's supplied
            {
                static const SMeshTexCoord defaultTextureCoordinate(0.0f, 0.0f);
                for (int i = 0; i < context.m_mesh.GetVertexCount(); ++i)
                {
                    context.m_mesh.m_pTexCoord[i] = defaultTextureCoordinate;
                }
            }
            return SceneEvents::ProcessingResult::Success;
        }
    } // RC
} // AZ