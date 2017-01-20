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
#include <AzCore/Math/Transform.h>
#include <SceneAPI/SceneCore/Containers/Views/SceneGraphUpwardsIterator.h>
#include <SceneAPI/SceneData/GraphData/BoneData.h>
#include <SceneAPI/SceneData/GraphData/RootBoneData.h>
#include <SceneAPI/FbxSDKWrapper/FbxNodeWrapper.h>
#include <SceneAPI/FbxSceneBuilder/FbxBoneBuilder.h>
#include <SceneAPI/FbxSceneBuilder/FbxSceneSystem.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace FbxSceneImporter
        {
            FbxBoneBuilder::FbxBoneBuilder(FbxSDKWrapper::FbxNodeWrapper& fbxNode, Containers::SceneGraph& graph,
                Containers::SceneGraph::NodeIndex target, const std::shared_ptr<FbxSceneSystem>& sceneSystem)
                : m_fbxNode(fbxNode)
                , m_graph(graph)
                , m_targetNode(target)
                , m_sceneSystem(sceneSystem)
            {       
            }

            void FbxBoneBuilder::Reset(FbxSDKWrapper::FbxNodeWrapper& fbxNode, Containers::SceneGraph& graph,
                Containers::SceneGraph::NodeIndex target, const std::shared_ptr<FbxSceneSystem>& sceneSystem)
            {
                m_fbxNode = fbxNode;
                m_graph = graph;
                m_targetNode = target;
                m_sceneSystem = sceneSystem;
            }

            bool FbxBoneBuilder::BuildBone()
            {
                if (!m_fbxNode.IsBone())
                {
                    return false;
                }

                Transform globalTransform = m_fbxNode.EvaluateGlobalTransform();
                m_sceneSystem->SwapTransformForUpAxis(globalTransform);
                m_sceneSystem->ConvertBoneUnit(globalTransform);

                bool isRootBone = true;
                auto nodeIterator = m_graph.ConvertToHierarchyIterator(m_targetNode);
                auto view = Containers::Views::MakeSceneGraphUpwardsView(m_graph, nodeIterator, m_graph.GetContentStorage().begin(), true);
                for (auto it = ++view.begin(); it != view.end(); ++it)
                {
                    if ((*it) && (*it)->RTTI_IsTypeOf(AZ::SceneAPI::DataTypes::IBoneData::TYPEINFO_Uuid()))
                    {
                        isRootBone = false;
                        break;
                    }
                }

                if (isRootBone)
                {
                    AZStd::shared_ptr<SceneData::GraphData::RootBoneData> bone = AZStd::make_shared<SceneData::GraphData::RootBoneData>();
                    bone->SetWorldTransform(globalTransform);
                    m_graph.SetContent(m_targetNode, AZStd::move(bone));
                }
                else
                {
                    AZStd::shared_ptr<SceneData::GraphData::BoneData> bone = AZStd::make_shared<SceneData::GraphData::BoneData>();
                    bone->SetWorldTransform(globalTransform);
                    m_graph.SetContent(m_targetNode, AZStd::move(bone));
                }

                return true;
            }
        }
    }
}