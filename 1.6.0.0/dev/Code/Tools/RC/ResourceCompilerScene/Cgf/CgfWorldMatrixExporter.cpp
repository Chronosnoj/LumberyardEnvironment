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

#include <Cry_Geo.h> // Needed for CGFContent.h
#include <CGFContent.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/Views/SceneGraphUpwardsIterator.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IOriginRule.h>
#include <SceneAPI/SceneCore/DataTypes/Groups/IMeshGroup.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/ITransform.h>
#include <SceneAPI/SceneCore/DataTypes/DataTypeUtilities.h>
#include <RC/ResourceCompilerScene/Cgf/CgfExportContexts.h>
#include <RC/ResourceCompilerScene/Cgf/CgfWorldMatrixExporter.h>

namespace AZ
{
    namespace RC
    {
        namespace SceneEvents = AZ::SceneAPI::Events;
        namespace SceneDataTypes = AZ::SceneAPI::DataTypes;
        namespace SceneContainers = AZ::SceneAPI::Containers;
        namespace SceneViews = AZ::SceneAPI::Containers::Views;

        CgfWorldMatrixExporter::CgfWorldMatrixExporter()
            : CallProcessorBinder()
            , m_cachedRootMatrix(Transform::CreateIdentity())
            , m_cachedMeshGroup(nullptr)
            , m_cachedRootMatrixIsSet(false)
        {
            BindToCall(&CgfWorldMatrixExporter::ProcessMeshGroup);
            BindToCall(&CgfWorldMatrixExporter::ProcessNode);
        }

        SceneEvents::ProcessingResult CgfWorldMatrixExporter::ProcessMeshGroup(CgfMeshGroupExportContext& context)
        {
            if (context.m_phase != Phase::Construction)
            {
                return SceneEvents::ProcessingResult::Ignored;
            }

            m_cachedMeshGroup = &context.m_group;

            m_cachedRootMatrix = Transform::CreateIdentity();
            m_cachedRootMatrixIsSet = false;

            AZStd::shared_ptr<const SceneDataTypes::IOriginRule> rule = SceneDataTypes::Utilities::FindRule<SceneDataTypes::IOriginRule>(context.m_group);
            if (rule)
            {
                if (rule->GetTranslation() != Vector3(0.0f, 0.0f, 0.0f) || !rule->GetRotation().IsIdentity())
                {
                    m_cachedRootMatrix = Transform::CreateFromQuaternionAndTranslation(rule->GetRotation(), rule->GetTranslation());
                    m_cachedRootMatrixIsSet = true;
                }

                if (rule->GetScale() != 1.0f)
                {
                    float scale = rule->GetScale();
                    m_cachedRootMatrix.MultiplyByScale(Vector3(scale, scale, scale));
                    m_cachedRootMatrixIsSet = true;
                }

                if (!rule->GetOriginNodeName().empty() && !rule->UseRootAsOrigin())
                {
                    const SceneContainers::SceneGraph& graph = context.m_scene.GetGraph();
                    SceneContainers::SceneGraph::NodeIndex index = graph.Find(rule->GetOriginNodeName().c_str());
                    if (index.IsValid())
                    {
                        Transform worldMatrix = Transform::CreateIdentity();
                        if (ConcatenateMatricesUpwards(worldMatrix, graph.ConvertToHierarchyIterator(index), graph))
                        {
                            worldMatrix.InvertFull();
                            m_cachedRootMatrix *= worldMatrix;
                            m_cachedRootMatrixIsSet = true;
                        }
                    }
                }
                return m_cachedRootMatrixIsSet ? SceneEvents::ProcessingResult::Success : SceneEvents::ProcessingResult::Ignored;
            }
            else
            {
                return SceneEvents::ProcessingResult::Ignored;
            }
        }

        SceneEvents::ProcessingResult CgfWorldMatrixExporter::ProcessNode(CgfNodeExportContext& context)
        {
            if (context.m_phase != Phase::Filling)
            {
                return SceneEvents::ProcessingResult::Ignored;
            }

            Transform worldMatrix = Transform::CreateIdentity();

            const SceneContainers::SceneGraph& graph = context.m_scene.GetGraph();
            HierarchyStorageIterator nodeIterator = graph.ConvertToHierarchyIterator(context.m_nodeIndex);
            bool translated = ConcatenateMatricesUpwards(worldMatrix, nodeIterator, graph);

            AZ_Assert(m_cachedMeshGroup == &context.m_group, "CgfNodeExportContext doesn't belong to chain of previously called CgfMeshGroupExportContext.");
            if (m_cachedRootMatrixIsSet)
            {
                worldMatrix = m_cachedRootMatrix * worldMatrix;
                translated = true;
            }

 

            //If we aren't merging nodes we need to put the transforms into the localTM
            //due to how the CGFSaver works inside the ResourceCompilerPC code. 
            if (!context.m_container.GetExportInfo()->bMergeAllNodes)
            {
                TransformToMatrix34(context.m_node.localTM, worldMatrix);
            }
            else
            {
                TransformToMatrix34(context.m_node.worldTM, worldMatrix);
            }
            context.m_node.bIdentityMatrix = !translated;

            return SceneEvents::ProcessingResult::Success;
        }

        bool CgfWorldMatrixExporter::ConcatenateMatricesUpwards(Transform& transform, const HierarchyStorageIterator& nodeIterator, const SceneContainers::SceneGraph& graph) const
        {
            bool translated = false;

            auto view = SceneViews::MakeSceneGraphUpwardsView(graph, nodeIterator, graph.GetContentStorage().cbegin(), true);
            for (auto& it = view.begin(); it != view.end(); ++it)
            {
                if (!(*it))
                {
                    continue;
                }

                const SceneDataTypes::ITransform* nodeTransform = azrtti_cast<const SceneDataTypes::ITransform*>(it->get());
                if (nodeTransform)
                {
                    transform = nodeTransform->GetMatrix() * transform;
                    translated = true;
                }
                else
                {
                    bool endPointTransform = MultiplyEndPointTransforms(transform, it.GetHierarchyIterator(), graph);
                    translated = translated || endPointTransform;
                }
            }
            return translated;
        }

        bool CgfWorldMatrixExporter::MultiplyEndPointTransforms(Transform& transform, const HierarchyStorageIterator& nodeIterator, const SceneContainers::SceneGraph& graph) const
        {
            // TODO: Create a specialized child iterator so this switch is not needed.
            SceneContainers::SceneGraph::NodeIndex index = graph.ConvertToNodeIndex(nodeIterator);
            index = graph.GetNodeChild(index);
            while (index.IsValid())
            {
                // If the translation is not an end point it means it's its own group as opposed to being
                //      a component of the parent.
                if (graph.IsNodeEndPoint(index))
                {
                    AZStd::shared_ptr<const SceneDataTypes::ITransform> nodeTransform = azrtti_cast<const SceneDataTypes::ITransform*>(graph.GetNodeContent(index));
                    if (nodeTransform)
                    {
                        transform = nodeTransform->GetMatrix() * transform;
                        return true;
                    }
                }

                index = graph.GetNodeSibling(index);
            }

            return false;
        }

        void CgfWorldMatrixExporter::TransformToMatrix34(Matrix34& out, const Transform& in) const
        {
            // Setting column instead of row because as of writing Matrix34 doesn't support adding
            //      full rows, as the translation has to be done separately.
            for (int column = 0; column < 4; ++column)
            {
                Vector3 data = in.GetColumn(column);
                out.SetColumn(column, Vec3(data.GetX(), data.GetY(), data.GetZ()));
            }
        }
    } // RC
} // AZ