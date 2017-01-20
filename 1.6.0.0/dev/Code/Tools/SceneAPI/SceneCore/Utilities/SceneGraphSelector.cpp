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

#include <SceneAPI/SceneCore/Containers/Views/SceneGraphDownwardsIterator.h>
#include <SceneAPI/SceneCore/Containers/Views/PairIterator.h>
#include <SceneAPI/SceneCore/Utilities/SceneGraphSelector.h>
#include <AzCore/std/containers/set.h>
#include <AzCore/Math/Transform.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ISceneNodeSelectionList.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshData.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Utilities
        {
            bool SceneGraphSelector::IsTreeViewType(const Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex& index)
            {
                if (index == graph.GetRoot())
                {
                    return false;
                }
                return !graph.IsNodeEndPoint(index);
            }

            bool SceneGraphSelector::IsMesh(const Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex& index)
            {
                AZStd::shared_ptr<const DataTypes::IGraphObject> object = graph.GetNodeContent(index);
                return SceneGraphSelector::IsMeshObject(object);
            }

            bool SceneGraphSelector::IsMeshObject(const AZStd::shared_ptr<const DataTypes::IGraphObject>& object)
            {
                return object && object->RTTI_IsTypeOf(DataTypes::IMeshData::TYPEINFO_Uuid());
            }

            AZStd::vector<AZStd::string> SceneGraphSelector::GenerateTargetNodes(const Containers::SceneGraph& graph, const DataTypes::ISceneNodeSelectionList& list, NodeFilterFunction nodeFilter)
            {
                AZStd::vector<AZStd::string> targetNodes;
                AZStd::set<AZStd::string> selectedNodesSet;
                AZStd::set<AZStd::string> unselectedNodesSet;
                CopySelectionToSet(selectedNodesSet, unselectedNodesSet, list);
                CorrectRootNode(graph, selectedNodesSet, unselectedNodesSet);

                auto nodeIterator = graph.ConvertToHierarchyIterator(graph.GetRoot());
                auto view = Containers::Views::MakeSceneGraphDownwardsView<Containers::Views::BreadthFirst>(graph, nodeIterator, graph.GetContentStorage().cbegin(), true);
                if (view.begin() == view.end())
                {
                    return targetNodes;
                }
                for (auto it = ++view.begin(); it != view.end(); ++it)
                {
                    Containers::SceneGraph::NodeIndex index = graph.ConvertToNodeIndex(it.GetHierarchyIterator());
                    AZStd::string currentNodeName = graph.GetNodeName(index).c_str();
                    if (unselectedNodesSet.find(currentNodeName) != unselectedNodesSet.end())
                    {
                        continue;
                    }
                    if (selectedNodesSet.find(currentNodeName) != selectedNodesSet.end())
                    {
                        if (nodeFilter(graph, index))
                        {
                            targetNodes.push_back(currentNodeName);
                        }
                        continue;
                    }

                    Containers::SceneGraph::NodeIndex parentIndex = graph.GetNodeParent(index);
                    if (parentIndex.IsValid())
                    {
                        AZStd::string parentNodeName = graph.GetNodeName(parentIndex).c_str();
                        if (unselectedNodesSet.find(parentNodeName) != unselectedNodesSet.end())
                        {
                            unselectedNodesSet.insert(currentNodeName);
                        }
                        else if (selectedNodesSet.find(parentNodeName) != selectedNodesSet.end())
                        {
                            selectedNodesSet.insert(currentNodeName);
                            if (nodeFilter(graph, index))
                            {
                                targetNodes.push_back(currentNodeName);
                            }
                        }
                        else
                        {
                            AZ_Assert(false, "SceneGraphSelector does a breadth first iteration so parent should be processed, "
                                "but '%s' wasn't found in either selected or unselected list.", currentNodeName.c_str());
                        }
                    }
                }
                return targetNodes;
            }

            void SceneGraphSelector::SelectAll(const Containers::SceneGraph& graph, DataTypes::ISceneNodeSelectionList& list)
            {
                list.ClearSelectedNodes();
                list.ClearUnselectedNodes();
                
                auto range = Containers::Views::MakePairView(graph.GetHierarchyStorage(), graph.GetNameStorage());
                for (auto& it : range)
                {
                    if (!it.first.IsEndPoint())
                    {
                        list.AddSelectedNode(it.second.c_str());
                    }
                }
            }

            void SceneGraphSelector::UnselectAll(const Containers::SceneGraph& graph, DataTypes::ISceneNodeSelectionList& list)
            {
                list.ClearSelectedNodes();
                list.ClearUnselectedNodes();
                
                auto range = Containers::Views::MakePairView(graph.GetHierarchyStorage(), graph.GetNameStorage());
                for (auto& it : range)
                {
                    if (!it.first.IsEndPoint())
                    {
                        list.RemoveSelectedNode(it.second.c_str());
                    }
                }
            }

            void SceneGraphSelector::UpdateNodeSelection(const Containers::SceneGraph& graph, DataTypes::ISceneNodeSelectionList& list)
            {
                AZStd::set<AZStd::string> selectedNodesSet;
                AZStd::set<AZStd::string> unselectedNodesSet;
                CopySelectionToSet(selectedNodesSet, unselectedNodesSet, list);
                CorrectRootNode(graph, selectedNodesSet, unselectedNodesSet);

                list.ClearSelectedNodes();
                list.ClearUnselectedNodes();

                auto keyValueView = Containers::Views::MakePairView(graph.GetNameStorage(), graph.GetContentStorage());
                auto nodeIterator = graph.ConvertToHierarchyIterator(graph.GetRoot());
                auto view = Containers::Views::MakeSceneGraphDownwardsView<Containers::Views::BreadthFirst>(graph, nodeIterator, keyValueView.cbegin(), true);
                if (view.begin() == view.end())
                {
                    return;
                }
                for (auto it = ++view.begin(); it != view.end(); ++it)
                {
                    Containers::SceneGraph::NodeIndex index = graph.ConvertToNodeIndex(it.GetHierarchyIterator());
                    if (graph.IsNodeEndPoint(index))
                    {
                        // Filter out end points because they can never add anything to the hierarchy.
                        continue;
                    }

                    AZStd::string currentNodeName = it->first.c_str();
                    if (unselectedNodesSet.find(currentNodeName) != unselectedNodesSet.end())
                    {
                        list.RemoveSelectedNode(AZStd::move(currentNodeName));
                        continue;
                    }
                    if (selectedNodesSet.find(currentNodeName) != selectedNodesSet.end())
                    {
                        list.AddSelectedNode(AZStd::move(currentNodeName));
                        continue;
                    }

                    Containers::SceneGraph::NodeIndex parentIndex = graph.GetNodeParent(index);
                    if (parentIndex.IsValid())
                    {
                        AZStd::string parentNodeName = graph.GetNodeName(parentIndex).c_str();
                        if (unselectedNodesSet.find(parentNodeName) != unselectedNodesSet.end())
                        {
                            unselectedNodesSet.insert(currentNodeName);
                            list.RemoveSelectedNode(AZStd::move(currentNodeName));
                        }
                        else
                        {
                            selectedNodesSet.insert(currentNodeName);
                            list.AddSelectedNode(AZStd::move(currentNodeName));
                        }
                    }
                }
            }

            void SceneGraphSelector::UpdateTargetNodes(const Containers::SceneGraph& graph, DataTypes::ISceneNodeSelectionList& list, const AZStd::unordered_set<AZStd::string>& targetNodes,
                NodeFilterFunction nodeFilter)
            {
                list.ClearSelectedNodes();
                list.ClearUnselectedNodes();
                auto view = Containers::Views::MakePairView(graph.GetNameStorage(), graph.GetContentStorage());
                if (view.begin() == view.end())
                {
                    return;
                }
                for (auto it = ++view.begin(); it != view.end(); ++it)
                {
                    Containers::SceneGraph::NodeIndex index = graph.ConvertToNodeIndex(it.GetSecondIterator());
                    AZStd::string currentNodeName = it->first.c_str();
                    if (nodeFilter(graph, index))
                    {
                        if (targetNodes.find(currentNodeName) != targetNodes.end())
                        {
                            list.AddSelectedNode(currentNodeName);
                        }
                        else
                        {
                            list.RemoveSelectedNode(currentNodeName);
                        }
                    }
                }
            }

            void SceneGraphSelector::CopySelectionToSet(AZStd::set<AZStd::string>& selected, AZStd::set<AZStd::string>& unselected, const DataTypes::ISceneNodeSelectionList& list)
            {
                for (size_t i = 0; i < list.GetSelectedNodeCount(); ++i)
                {
                    selected.insert(list.GetSelectedNode(i));
                }
                for (size_t i = 0; i < list.GetUnselectedNodeCount(); ++i)
                {
                    unselected.insert(list.GetUnselectedNode(i));
                }
            }

            void SceneGraphSelector::CorrectRootNode(const Containers::SceneGraph& graph,
                AZStd::set<AZStd::string>& selected, AZStd::set<AZStd::string>& unselected)
            {
                AZStd::string rootNodeName = graph.GetNodeName(graph.GetRoot()).c_str();
                if (selected.find(rootNodeName) == selected.end())
                {
                    selected.insert(rootNodeName);
                }
                auto root = unselected.find(rootNodeName);
                if (root != unselected.end())
                {
                    unselected.erase(root);
                }
            }
        }
    }
}