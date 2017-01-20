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

#include <queue>
#include <AzCore/Math/Transform.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <SceneAPI/FbxSceneBuilder/FbxImporter.h>
#include <SceneAPI/FbxSDKWrapper/FbxSceneWrapper.h>
#include <SceneAPI/FbxSDKWrapper/FbxMeshWrapper.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <SceneAPI/SceneData/GraphData/TransformData.h>
#include <SceneAPI/FbxSceneBuilder/FbxMeshBuilder.h>
#include <SceneAPI/FbxSceneBuilder/FbxMaterialBuilder.h>
#include <SceneAPI/FbxSceneBuilder/FbxBoneBuilder.h>
#include <SceneAPI/FbxSceneBuilder/FbxSkinBuilder.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace FbxSceneImporter
        {
            struct QueueNode
            {
                std::shared_ptr<FbxSDKWrapper::FbxNodeWrapper> m_node;
                Containers::SceneGraph::NodeIndex m_parent;

                QueueNode() = default;
                QueueNode(std::shared_ptr<FbxSDKWrapper::FbxNodeWrapper>&& node, Containers::SceneGraph::NodeIndex parent)
                    : m_node(std::move(node))
                    , m_parent(parent)
                {
                }
            };

            const char* FbxImporter::s_transformNodeName = "transform";

            FbxImporter::FbxImporter()
                : m_sceneWrapper(new FbxSDKWrapper::FbxSceneWrapper())
                , m_sceneSystem(new FbxSceneSystem())
            {
            }

            FbxImporter::FbxImporter(std::unique_ptr<FbxSDKWrapper::FbxSceneWrapper>&& specifiedScene)
                : m_sceneWrapper(std::move(specifiedScene))
                , m_sceneSystem(new FbxSceneSystem())
            {
                if (!m_sceneWrapper)
                {
                    m_sceneWrapper.reset(new FbxSDKWrapper::FbxSceneWrapper);
                }
            }

            FbxImporter::~FbxImporter() = default;

            const char* FbxImporter::GetDefaultFileExtension()
            {
                return "fbx";
            }

            bool FbxImporter::PopulateFromFile(const char* path, Containers::Scene& scene) const
            {
                m_sceneWrapper->Clear();

                if (!m_sceneWrapper->LoadSceneFromFile(path))
                {
                    return false;
                }
                m_sceneSystem->Set(*m_sceneWrapper);
                return ConvertFbxScene(scene);
            }

            bool FbxImporter::PopulateFromFile(const std::string& path, Containers::Scene& scene) const
            {
                return PopulateFromFile(path.c_str(), scene);
            }

            bool FbxImporter::ConvertFbxScene(Containers::Scene& scene) const
            {
                std::shared_ptr<FbxSDKWrapper::FbxNodeWrapper> fbxRoot = m_sceneWrapper->GetRootNode();
                if (!fbxRoot)
                {
                    return false;
                }

                std::queue<QueueNode> nodes;
                nodes.emplace(std::move(fbxRoot), scene.GetGraph().GetRoot());

                while (!nodes.empty())
                {
                    QueueNode& node = nodes.front();

                    AZ_Assert(node.m_node, "Empty fbx node queued");
                    Containers::SceneGraph::NodeIndex currentNodeIndex = AppendNodeToScene(scene.GetGraph(), node.m_parent, *node.m_node);

                    int childCount = node.m_node->GetChildCount();
                    for (int i = 0; i < childCount; ++i)
                    {
                        std::shared_ptr<FbxSDKWrapper::FbxNodeWrapper> child = node.m_node->GetChild(i);
                        if (child)
                        {
                            nodes.emplace(std::move(child), currentNodeIndex);
                        }
                    }
                    nodes.pop();
                }

                return true;
            }

            Containers::SceneGraph::NodeIndex FbxImporter::AppendNodeToScene(Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex parent, FbxSDKWrapper::FbxNodeWrapper& node) const
            {
                // Always create a node, even if it remains empty.
                parent = graph.AddChild(parent, node.GetName());

                // Build Transform
                bool nodeUsed = BuildSkinNode(graph, parent, node);
                if (!nodeUsed)
                {
                    nodeUsed = BuildMeshNode(graph, parent, node);
                }
                if (nodeUsed)
                {
                    BuildMaterialNode(graph, parent, node);
                }

                if (!nodeUsed)
                {
                    nodeUsed = BuildBoneNode(graph, parent, node);
                }
                BuildTransformNode(graph, parent, node, nodeUsed);

                return parent;
            }

            AZStd::shared_ptr<DataTypes::IGraphObject> FbxImporter::BuildTransform(FbxSDKWrapper::FbxNodeWrapper& node) const
            {
                Transform localTransform = node.EvaluateLocalTransform();
                Transform geoTransform = node.GetGeometricTransform();

                localTransform *= geoTransform;
                if (localTransform == Transform::Identity())
                {
                    return nullptr;
                }

                m_sceneSystem->SwapTransformForUpAxis(localTransform);
                m_sceneSystem->ConvertUnit(localTransform);

                return AZStd::make_shared<SceneData::GraphData::TransformData>(localTransform);
            }

            bool FbxImporter::BuildMeshNode(Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex parent, FbxSDKWrapper::FbxNodeWrapper& node) const
            {
                AZ_Assert(!graph.HasNodeContent(parent), "BuildMeshNode was failed as the node was already used");
                FbxMeshBuilder meshBuilder(node.GetMesh(), graph, parent, m_sceneSystem);
                meshBuilder.BuildMesh();
                return graph.HasNodeContent(parent);
            }

            bool FbxImporter::BuildSkinNode(Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex parent, FbxSDKWrapper::FbxNodeWrapper& node) const
            {
                AZ_Assert(!graph.HasNodeContent(parent), "BuildMeshNode was failed as the node was already used");
                FbxSkinBuilder skinBuilder(node.GetMesh(), graph, parent, m_sceneSystem);
                skinBuilder.BuildSkin();
                return graph.HasNodeContent(parent);
            }

            bool FbxImporter::BuildBoneNode(Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex parent, FbxSDKWrapper::FbxNodeWrapper& node) const
            {
                AZ_Assert(!graph.HasNodeContent(parent), "BuildMeshNode was failed as the node was already used");
                FbxBoneBuilder boneBuilder(node, graph, parent, m_sceneSystem);
                boneBuilder.BuildBone();
                return graph.HasNodeContent(parent);
            }

            bool FbxImporter::BuildMaterialNode(Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex parent, FbxSDKWrapper::FbxNodeWrapper& node) const
            {
                // Materials are always added as children
                int materialCount = node.GetMaterialCount();
                if (materialCount == 0)
                {
                    return false;
                }
                FbxMaterialBuilder materialBuilder;
                Containers::SceneGraph::NodeIndex currentMaterialNode = graph.AddChild(parent,
                    node.GetMaterialName(0), materialBuilder.BuildMaterial(node, 0));
                graph.MakeEndPoint(currentMaterialNode);
                for (int i = 1; i < materialCount; ++i)
                {
                    currentMaterialNode = graph.AddSibling(currentMaterialNode, node.GetMaterialName(i),
                        materialBuilder.BuildMaterial(node, i));
                    graph.MakeEndPoint(currentMaterialNode);
                }
                return true;
            }

            bool FbxImporter::BuildTransformNode(Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex parent, FbxSDKWrapper::FbxNodeWrapper& node, bool parentNodeUsed) const
            {
                AZStd::shared_ptr<DataTypes::IGraphObject> transform = BuildTransform(node);
                if (!transform)
                {
                    return false;
                }
                if (parentNodeUsed)
                {
                    Containers::SceneGraph::NodeIndex node = graph.AddChild(parent, s_transformNodeName, AZStd::move(transform));
                    graph.MakeEndPoint(node);
                }
                else
                {
                    graph.SetContent(parent, std::move(transform));
                }
                return true;
            }

        } // FbxSceneImporter
    } // SceneAPI
} // AZ