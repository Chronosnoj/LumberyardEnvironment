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

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzToolsFramework/Debug/TraceContext.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>
#include <SceneAPI/FbxSDKWrapper/FbxMeshWrapper.h>
#include <SceneAPI/FbxSDKWrapper/FbxSkinWrapper.h>
#include <SceneAPI/FbxSDKWrapper/FbxNodeWrapper.h>
#include <SceneAPI/SceneData/GraphData/SkinMeshData.h>
#include <SceneAPI/SceneData/GraphData/SkinWeightData.h>
#include <SceneAPI/FbxSceneBuilder/FbxSkinBuilder.h>


namespace AZ
{
    namespace SceneAPI
    {
        namespace FbxSceneImporter
        {
            FbxSkinBuilder::FbxSkinBuilder(const std::shared_ptr<FbxSDKWrapper::FbxMeshWrapper>& fbxMesh, Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex target, const std::shared_ptr<FbxSceneSystem>& sceneSystem)
                : FbxMeshBuilder(fbxMesh, graph, target, sceneSystem)
            {
            }

            void FbxSkinBuilder::Reset(const std::shared_ptr<FbxSDKWrapper::FbxMeshWrapper>& fbxMesh, Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex target, const std::shared_ptr<FbxSceneSystem>& sceneSystem)
            {
                FbxMeshBuilder::Reset(fbxMesh, graph, target, sceneSystem);
            }

            bool FbxSkinBuilder::BuildSkin()
            {
                if (!m_fbxMesh)
                {
                    return false;
                }
                
                if (!ProcessSkinDeformers())
                {
                    return false;
                }

                AZStd::shared_ptr<SceneData::GraphData::SkinMeshData> skinMesh = AZStd::make_shared<SceneData::GraphData::SkinMeshData>();
                return BuildMeshInternal(skinMesh);
            }

            bool FbxSkinBuilder::ProcessSkinDeformers()
            {
                Containers::SceneGraph::NodeIndex currentNode = m_targetNode;
                for (int deformerIndex = 0; deformerIndex < m_fbxMesh->GetDeformerCount(); ++deformerIndex)
                {
                    AZStd::shared_ptr<const FbxSDKWrapper::FbxSkinWrapper> fbxSkin = m_fbxMesh->GetSkin(deformerIndex);
                    if (!fbxSkin)
                    {
                        return false;
                    }
                    AZ_TraceContext("Skin name", fbxSkin->GetName());
                    if (!Containers::SceneGraph::IsValidName(fbxSkin->GetName()))
                    {
                        AZ_TracePrintf(Utilities::WarningWindow, "Invalid name for skin, skin ignored.");
                        continue;
                    }

                    AZStd::shared_ptr<DataTypes::IGraphObject> skinDeformer = BuildSkinWeightData(deformerIndex);
                    if (!skinDeformer)
                    {
                        continue;
                    }

                    currentNode = m_graph.AddChild(m_targetNode, fbxSkin->GetName(), AZStd::move(skinDeformer));
                    m_graph.MakeEndPoint(currentNode);
                }

                bool builtSkinDeformer = currentNode != m_targetNode;
                return builtSkinDeformer;
            }

            AZStd::shared_ptr<DataTypes::IGraphObject> FbxSkinBuilder::BuildSkinWeightData(int index)
            {
                AZStd::shared_ptr<const FbxSDKWrapper::FbxSkinWrapper> fbxSkin = m_fbxMesh->GetSkin(index);
                AZ_Assert(fbxSkin, "BuildSkinWeightData was called for index %i which doesn't contain a skin deformer.", index);

                if (!fbxSkin)
                {
                    return nullptr;
                }
                AZStd::shared_ptr<SceneData::GraphData::SkinWeightData> skinWeightData = AZStd::make_shared<SceneData::GraphData::SkinWeightData>();

                int controlPointCount = m_fbxMesh->GetControlPointsCount();
                skinWeightData->ResizeContainerSpace(controlPointCount);

                int clusterCount = fbxSkin->GetClusterCount();

                for (int clusterIndex = 0; clusterIndex < clusterCount; ++clusterIndex)
                {
                    int controlPointCount = fbxSkin->GetClusterControlPointIndicesCount(clusterIndex);
                    AZStd::shared_ptr<const FbxSDKWrapper::FbxNodeWrapper> fbxLink = fbxSkin->GetClusterLink(clusterIndex);
                    AZStd::string boneName = fbxLink->GetName();
                    int boneId = skinWeightData->GetBoneId(boneName);

                    for (int pointIndex = 0; pointIndex < controlPointCount; ++pointIndex)
                    {
                        SceneAPI::DataTypes::ISkinWeightData::Link link;
                        link.boneId = boneId;
                        link.weight = aznumeric_caster(fbxSkin->GetClusterControlPointWeight(clusterIndex, pointIndex));
                        skinWeightData->AppendLink(fbxSkin->GetClusterControlPointIndex(clusterIndex, pointIndex), link);
                    }
                }

                return skinWeightData;
            }
        }
    }
}