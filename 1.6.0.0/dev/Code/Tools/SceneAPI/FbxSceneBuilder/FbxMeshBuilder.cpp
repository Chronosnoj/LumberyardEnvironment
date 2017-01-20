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

#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/std/string/conversions.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>
#include <SceneAPI/FbxSceneBuilder/FbxMeshBuilder.h>
#include <SceneAPI/FbxSDKWrapper/FbxNodeWrapper.h>
#include <SceneAPI/SceneData/GraphData/MeshData.h>
#include <SceneAPI/SceneData/GraphData/MeshVertexColorData.h>
#include <SceneAPI/SceneData/GraphData/MeshVertexUVData.h>
#include <SceneAPI/FbxSceneBuilder/FbxSceneSystem.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace FbxSceneImporter
        {
            FbxMeshBuilder::FbxMeshBuilder(const std::shared_ptr<FbxSDKWrapper::FbxMeshWrapper>& fbxMesh, Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex target, const std::shared_ptr<FbxSceneSystem>& sceneSystem)
                : m_fbxMesh(fbxMesh)
                , m_graph(graph)
                , m_targetNode(target)
                , m_vertexCount(0)
                , m_sceneSystem(sceneSystem)
            {
            }

            void FbxMeshBuilder::Reset(const std::shared_ptr<FbxSDKWrapper::FbxMeshWrapper>& fbxMesh, Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex target, const std::shared_ptr<FbxSceneSystem>& sceneSystem)
            {
                m_fbxMesh = fbxMesh;
                m_graph = graph;
                m_targetNode = target;
                m_vertexCount = 0;
                m_sceneSystem = sceneSystem;
            }

            bool FbxMeshBuilder::BuildMesh()
            {
                if (!m_fbxMesh)
                {
                    return false;
                }
                AZStd::shared_ptr<SceneData::GraphData::MeshData> mesh = AZStd::make_shared<SceneData::GraphData::MeshData>();
                return BuildMeshInternal(mesh);
            }

            bool FbxMeshBuilder::BuildMeshInternal(const AZStd::shared_ptr<SceneData::GraphData::MeshData>& mesh)
            {
                // Get mesh subset count by scanning material IDs in meshes.
                // For negative material ids we will add an additional
                // subset at the end, see "++maxMaterialIndex".

                // These defines material index range for all polygons in the mesh
                // Each polygon has a material index
                int minMeshMaterialIndex = INT_MAX;
                int maxMeshMaterialIndex = INT_MIN;

                {
                    FbxLayerElementArrayTemplate<int>* fbxMaterialIndices;
                    m_fbxMesh->GetMaterialIndices(&fbxMaterialIndices); // per polygon

                    const int fbxPolygonCount = m_fbxMesh->GetPolygonCount();
                    for (int fbxPolygonIndex = 0; fbxPolygonIndex < fbxPolygonCount; ++fbxPolygonIndex)
                    {
                        // if the polygon has less than 3 vertices, it's not a valid polygon and is skipped
                        const int fbxPolygonVertexCount = m_fbxMesh->GetPolygonSize(fbxPolygonIndex);
                        if (fbxPolygonVertexCount <= 2)
                        {
                            continue;
                        }

                        // Get the material index of each polygon
                        const int meshMaterialIndex = fbxMaterialIndices ? (*fbxMaterialIndices)[fbxPolygonIndex] : -1;
                        minMeshMaterialIndex = AZ::GetMin<int>(minMeshMaterialIndex, meshMaterialIndex);
                        maxMeshMaterialIndex = AZ::GetMax<int>(maxMeshMaterialIndex, meshMaterialIndex);
                    }

                    if (minMeshMaterialIndex > maxMeshMaterialIndex)
                    {
                        return false;
                    }

                    if (maxMeshMaterialIndex < 0)
                    {
                        minMeshMaterialIndex = maxMeshMaterialIndex = 0;
                    }
                    else if (minMeshMaterialIndex < 0)
                    {
                        minMeshMaterialIndex = 0;
                        ++maxMeshMaterialIndex;
                    }
                }

                // Fill geometry

                {
                    // Control points contain positions of vertices
                    AZStd::vector<Vector3> fbxControlPoints = m_fbxMesh->GetControlPoints();
                    const int* const fbxPolygonVertices = m_fbxMesh->GetPolygonVertices();

                    FbxLayerElementArrayTemplate<int>* fbxMaterialIndices = nullptr;
                    m_fbxMesh->GetMaterialIndices(&fbxMaterialIndices); // per polygon

                    // Iterate through each polygon in the mesh and convert data
                    const int fbxPolygonCount = m_fbxMesh->GetPolygonCount();
                    for (int fbxPolygonIndex = 0; fbxPolygonIndex < fbxPolygonCount; ++fbxPolygonIndex)
                    {
                        const int fbxPolygonVertexCount = m_fbxMesh->GetPolygonSize(fbxPolygonIndex);
                        if (fbxPolygonVertexCount <= 2)
                        {
                            continue;
                        }

                        // Ensure the validity of the material index for the polygon
                        int fbxMaterialIndex = fbxMaterialIndices ? (*fbxMaterialIndices)[fbxPolygonIndex] : -1;
                        if (fbxMaterialIndex < minMeshMaterialIndex || fbxMaterialIndex > maxMeshMaterialIndex)
                        {
                            fbxMaterialIndex = maxMeshMaterialIndex;
                        }

                        const int fbxVertexStartIndex = m_fbxMesh->GetPolygonVertexIndex(fbxPolygonIndex);

                        // Triangulate polygon as a fan and remember resulting vertices and faces

                        int firstMeshVertexIndex = -1;
                        int previousMeshVertexIndex = -1;

                        AZ::SceneAPI::DataTypes::IMeshData::Face meshFace;
                        int verticesInMeshFace = 0;

                        // Iterate through each vertex in the polygon
                        for (int i = 0; i < fbxPolygonVertexCount; ++i)
                        {
                            const int meshVertexIndex = static_cast<int>(mesh->GetVertexCount());

                            const int fbxPolygonVertexIndex = fbxVertexStartIndex + i;
                            const int fbxControlPointIndex = fbxPolygonVertices[fbxPolygonVertexIndex];

                            Vector3 meshPosition = fbxControlPoints[fbxControlPointIndex];

                            Vector3 meshVertexNormal;
                            m_fbxMesh->GetPolygonVertexNormal(fbxPolygonIndex, i, meshVertexNormal);

                            m_sceneSystem->SwapVec3ForUpAxis(meshPosition);
                            m_sceneSystem->ConvertUnit(meshPosition);
                            // Add position
                            mesh->AddPosition(meshPosition);

                            // Add normal
                            m_sceneSystem->SwapVec3ForUpAxis(meshVertexNormal);
                            meshVertexNormal.Normalize();
                            mesh->AddNormal(meshVertexNormal);

                            // Add face
                            {
                                if (i == 0)
                                {
                                    firstMeshVertexIndex = meshVertexIndex;
                                }

                                int meshVertices[3];
                                int meshVertexCount = 0;

                                meshVertices[meshVertexCount++] = meshVertexIndex;

                                // If we already have generated one triangle before, make a new triangle at a time as we encounter a new vertex.
                                // The new triangle is composed with the first vertex of the polygon, the last vertex, and the current vertex.
                                if (i >= 3)
                                {
                                    meshVertices[meshVertexCount++] = firstMeshVertexIndex;
                                    meshVertices[meshVertexCount++] = previousMeshVertexIndex;
                                }

                                for (int j = 0; j < meshVertexCount; ++j)
                                {
                                    meshFace.vertexIndex[verticesInMeshFace++] = meshVertices[j];
                                    if (verticesInMeshFace == 3)
                                    {
                                        verticesInMeshFace = 0;
                                        mesh->AddFace(meshFace, fbxMaterialIndex);
                                    }
                                }
                            }

                            previousMeshVertexIndex = meshVertexIndex;
                        }

                        // Report problem if there are vertices that left for forming a polygon
                        if (verticesInMeshFace != 0)
                        {
                            AZ_TracePrintf(Utilities::ErrorWindow, "Internal error in mesh filler");
                            return false;
                        }
                    }

                    // Report problem if no vertex or face converted to MeshData
                    if (mesh->GetVertexCount() <= 0 || mesh->GetFaceCount() <= 0)
                    {
                        AZ_TracePrintf(Utilities::ErrorWindow, "Missing geometry data in mesh node");
                        return false;
                    }
                }

                m_vertexCount = mesh->GetVertexCount();
                m_graph.SetContent(m_targetNode, AZStd::move(mesh));
                
                // Append vertex color streams if found.
                ProcessVertexColors();

                // Append uv/texcoord information if found.
                ProcessVertexUVs();
                
                return true;
            }

            void FbxMeshBuilder::ProcessVertexColors()
            {
                Containers::SceneGraph::NodeIndex currentNode = m_targetNode;
                for (int index = 0; index < m_fbxMesh->GetElementVertexColorCount(); ++index)
                {
                    FbxSDKWrapper::FbxVertexColorWrapper fbxVertexColors = m_fbxMesh->GetElementVertexColor(index);

                    if (!fbxVertexColors.IsValid())
                    {
                        return;
                    }

                    

                    AZStd::shared_ptr<DataTypes::IGraphObject> vertexColors = FbxMeshBuilder::BuildVertexColorData(index);
                    if (!vertexColors)
                    {
                        continue;
                    }

                    if (Containers::SceneGraph::IsValidName(fbxVertexColors.GetName()))
                    {
                        currentNode = m_graph.AddChild(m_targetNode, fbxVertexColors.GetName(), AZStd::move(vertexColors));
                    }
                    else
                    {
                        AZStd::string name("colorStream_");
                        name += AZStd::to_string(index);
                        currentNode = m_graph.AddChild(m_targetNode, name.c_str(), AZStd::move(vertexColors));
                    }

                    m_graph.MakeEndPoint(currentNode);
                }
            }

            AZStd::shared_ptr<DataTypes::IGraphObject> FbxMeshBuilder::BuildVertexColorData(int index)
            {
                FbxSDKWrapper::FbxVertexColorWrapper colors = m_fbxMesh->GetElementVertexColor(index);
                AZ_Assert(colors.IsValid(), "BuildVertexColorData was called for index %i which doesn't contain a color stream.", index);
                if (!colors.IsValid())
                {
                    return nullptr;
                }

                AZStd::shared_ptr<SceneData::GraphData::MeshVertexColorData> colorData = AZStd::make_shared<AZ::SceneData::GraphData::MeshVertexColorData>();
                colorData->ReserveContainerSpace(m_vertexCount);

                const int fbxPolygonCount = m_fbxMesh->GetPolygonCount();
                const int* const fbxPolygonVertices = m_fbxMesh->GetPolygonVertices();
                for (int fbxPolygonIndex = 0; fbxPolygonIndex < fbxPolygonCount; ++fbxPolygonIndex)
                {
                    const int fbxPolygonVertexCount = m_fbxMesh->GetPolygonSize(fbxPolygonIndex);
                    if (fbxPolygonVertexCount <= 2)
                    {
                        continue;
                    }

                    const int fbxVertexStartIndex = m_fbxMesh->GetPolygonVertexIndex(fbxPolygonIndex);

                    for (int i = 0; i < fbxPolygonVertexCount; ++i)
                    {
                        const int fbxPolygonVertexIndex = fbxVertexStartIndex + i;
                        const int fbxControlPointIndex = fbxPolygonVertices[fbxPolygonVertexIndex];

                        FbxSDKWrapper::FbxColorWrapper color = colors.GetElementAt(fbxPolygonIndex, fbxPolygonVertexIndex, fbxControlPointIndex);

                        colorData->AppendColor({color.GetR(), color.GetG(), color.GetB(), color.GetAlpha()});
                    }
                }

                if (colorData->GetCount() != m_vertexCount)
                {
                    AZ_TracePrintf(Utilities::ErrorWindow, "Vertex count (%i) doesn't match the number of entries for the vertex color stream %s (%i)",
                        m_vertexCount, colors.GetName(), colorData->GetCount());
                    return nullptr;
                }

                return colorData;
            }

            void FbxMeshBuilder::ProcessVertexUVs()
            {
                Containers::SceneGraph::NodeIndex currentNode = m_targetNode;
                int uv_count = m_fbxMesh->GetElementUVCount();
                for (int index = 0; index < uv_count; ++index)
                {
                    FbxSDKWrapper::FbxUVWrapper fbxVertexUVs = m_fbxMesh->GetElementUV(index);
                    if (!fbxVertexUVs.IsValid())
                    {
                        return;
                    }
                    if (!Containers::SceneGraph::IsValidName(fbxVertexUVs.GetName()))
                    {
                        AZ_TracePrintf(Utilities::WarningWindow, "Invalid name '%s' for uv set, set ignored.", fbxVertexUVs.GetName());
                        continue;
                    }

                    AZStd::shared_ptr<DataTypes::IGraphObject> vertexUVs = FbxMeshBuilder::BuildVertexUVData(index);
                    if (!vertexUVs)
                    {
                        continue;
                    }

                    currentNode = m_graph.AddChild(m_targetNode, fbxVertexUVs.GetName(), AZStd::move(vertexUVs));
                    m_graph.MakeEndPoint(currentNode);
                }
            }

            AZStd::shared_ptr<DataTypes::IGraphObject> FbxMeshBuilder::BuildVertexUVData(int index)
            {
                FbxSDKWrapper::FbxUVWrapper uvs = m_fbxMesh->GetElementUV(index);
                AZ_Assert(uvs.IsValid(), "BuildVertexUVData was called for index %i which doesn't contain a uv set.", index);

                if (!uvs.IsValid())
                {
                    return nullptr;
                }
                AZStd::shared_ptr<SceneData::GraphData::MeshVertexUVData> uvData = AZStd::make_shared<AZ::SceneData::GraphData::MeshVertexUVData>();
                uvData->ReserveContainerSpace(m_vertexCount);

                const int fbxPolygonCount = m_fbxMesh->GetPolygonCount();
                const int* const fbxPolygonVertices = m_fbxMesh->GetPolygonVertices();
                for (int fbxPolygonIndex = 0; fbxPolygonIndex < fbxPolygonCount; ++fbxPolygonIndex)
                {
                    const int fbxPolygonVertexCount = m_fbxMesh->GetPolygonSize(fbxPolygonIndex);
                    if (fbxPolygonVertexCount <= 2)
                    {
                        continue;
                    }

                    const int fbxVertexStartIndex = m_fbxMesh->GetPolygonVertexIndex(fbxPolygonIndex);

                    for (int i = 0; i < fbxPolygonVertexCount; ++i)
                    {
                        const int fbxPolygonVertexIndex = fbxVertexStartIndex + i;
                        const int fbxControlPointIndex = fbxPolygonVertices[fbxPolygonVertexIndex];

                        Vector2 uv = uvs.GetElementAt(fbxPolygonIndex, fbxPolygonVertexIndex, fbxControlPointIndex);
                        uv.SetY(1.0f - uv.GetY());
                        uvData->AppendUV(uv);
                    }
                }

                if (uvData->GetCount() != m_vertexCount)
                {
                    AZ_TracePrintf(Utilities::ErrorWindow, "Vertex count (%i) doesn't match the number of entries for the uv set %s (%i)",
                        m_vertexCount, uvs.GetName(), uvData->GetCount());
                    return nullptr;
                }

                return uvData;
            }

        }
    }
}