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
#include <CryCrc32.h>
#include <CGFContent.h>
#include <AzToolsFramework/Debug/TraceContext.h>
#include <RC/ResourceCompilerScene/Chr/ChrExportContexts.h>
#include <RC/ResourceCompilerScene/Chr/ChrSkeletonExporter.h>
#include <RC/ResourceCompilerScene/Common/AssetExportUtilities.h>
#include <SceneAPI/SceneCore/Containers/Views/PairIterator.h>
#include <SceneAPI/SceneCore/Containers/Views/SceneGraphDownwardsIterator.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IBoneData.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>

namespace AZ
{
    namespace RC
    {
        namespace SceneEvents = AZ::SceneAPI::Events;
        namespace SceneUtil = AZ::SceneAPI::Utilities;
        namespace SceneContainer = AZ::SceneAPI::Containers;
        namespace SceneDataTypes = AZ::SceneAPI::DataTypes;

        ChrSkeletonExporter::ChrSkeletonExporter()
            : CallProcessorBinder()
        {
            BindToCall(&ChrSkeletonExporter::CreateBoneDescData);
            BindToCall(&ChrSkeletonExporter::CreateBoneEntityData);
        }

        SceneEvents::ProcessingResult ChrSkeletonExporter::CreateBoneDescData(SkeletonExportContext& context)
        {
            if (context.m_phase != Phase::Filling)
            {
                return SceneEvents::ProcessingResult::Ignored;
            }

            const char* rootBoneName = context.m_rootBoneName.c_str();
            const SceneContainer::SceneGraph& graph = context.m_scene.GetGraph();
            SceneContainer::SceneGraph::NodeIndex nodeIndex = graph.Find(rootBoneName);
            if (!nodeIndex.IsValid())
            {
                AZ_TracePrintf(SceneUtil::ErrorWindow, "Root bone '%s' cannot be found.\n", rootBoneName);
                return SceneEvents::ProcessingResult::Failure;
            }
            return CreateBoneDescDataRecursion(context, nodeIndex, graph);
        }

        SceneAPI::Events::ProcessingResult ChrSkeletonExporter::CreateBoneDescDataRecursion(SkeletonExportContext& context, SceneContainer::SceneGraph::NodeIndex index, 
            const SceneContainer::SceneGraph& graph)
        {
            if (index.IsValid())
            {
                AZStd::shared_ptr<const SceneDataTypes::IBoneData> boneData = azrtti_cast<const SceneDataTypes::IBoneData*>(graph.GetNodeContent(index));
                if (boneData)
                {
                    AZStd::string fullName = graph.GetNodeName(index).c_str();
                    AZStd::string shortName = graph.GetShortName(fullName.c_str()).c_str();

                    CryBoneDescData boneDesc;
                    boneDesc.m_DefaultB2W = AssetExportUtilities::ConvertToCryMatrix34(boneData->GetWorldTransform());
                    boneDesc.m_DefaultW2B = boneDesc.m_DefaultB2W.GetInverted();

                    SetBoneName(shortName, boneDesc);
                    boneDesc.m_nControllerID = CCrc32::ComputeLowercase(fullName.c_str());

                    int boneIndex = aznumeric_caster(context.m_skinningInfo.m_arrBonesDesc.size());
                    context.m_skinningInfo.m_arrBonesDesc.push_back(boneDesc);

                    m_nodeNameBoneIndexMap[fullName] = boneIndex;

                    auto childIndex = graph.GetNodeChild(index);
                    while (childIndex.IsValid())
                    {
                        CreateBoneDescDataRecursion(context, childIndex, graph);
                        childIndex = graph.GetNodeSibling(childIndex);
                    }
                }
            }
            return SceneEvents::ProcessingResult::Success;
        }

        SceneEvents::ProcessingResult ChrSkeletonExporter::CreateBoneEntityData(SkeletonExportContext& context)
        {
            if (context.m_phase != Phase::Filling)
            {
                return SceneEvents::ProcessingResult::Ignored;
            }

            const AZStd::string& rootBoneName = context.m_rootBoneName;
            AZ_TraceContext("Root bone", rootBoneName.c_str());
            const SceneContainer::SceneGraph& graph = context.m_scene.GetGraph();
            SceneContainer::SceneGraph::NodeIndex nodeIndex = graph.Find(rootBoneName.c_str());
            if (!nodeIndex.IsValid())
            {
                AZ_TracePrintf(SceneUtil::ErrorWindow, "Root bone '%s' cannot be found.\n", rootBoneName.c_str());
                return SceneEvents::ProcessingResult::Failure;
            }
            return CreateBoneEntityDataRecursion(context, nodeIndex, graph, rootBoneName);
        }

        SceneEvents::ProcessingResult ChrSkeletonExporter::CreateBoneEntityDataRecursion(SkeletonExportContext& context, AZ::SceneAPI::Containers::SceneGraph::NodeIndex index,
            const AZ::SceneAPI::Containers::SceneGraph& graph, const AZStd::string& rootBoneName)
        {
            if (index.IsValid())
            {
                AZStd::shared_ptr<const SceneDataTypes::IBoneData> boneData = azrtti_cast<const SceneDataTypes::IBoneData*>(graph.GetNodeContent(index));
                if (boneData)
                {
                    AZStd::string fullName = graph.GetNodeName(index).c_str();

                    BONE_ENTITY boneEntity;
                    memset(&boneEntity, 0, sizeof(boneEntity));

                    auto it = m_nodeNameBoneIndexMap.find(fullName);
                    if (it != m_nodeNameBoneIndexMap.end())
                    {
                        boneEntity.BoneID = it->second;
                        boneEntity.ParentID = -1;
                        if (rootBoneName != fullName)
                        {
                            SceneContainer::SceneGraph::NodeIndex parentIndex = graph.GetNodeParent(index);
                            AZStd::string parentName = graph.GetNodeName(parentIndex).c_str();
                            auto parentIt = m_nodeNameBoneIndexMap.find(parentName);
                            if (parentIt != m_nodeNameBoneIndexMap.end())
                            {
                                boneEntity.ParentID = parentIt->second;
                            }
                            else
                            {
                                AZ_TracePrintf(SceneUtil::ErrorWindow, "Detected bone '%s' direct parent is not bone type.\n", fullName.c_str());
                                return SceneEvents::ProcessingResult::Failure;
                            }
                        }
                    }
                    boneEntity.ControllerID = CCrc32::ComputeLowercase(fullName.c_str());
                    boneEntity.phys.nPhysGeom = -1;

                    auto childIndex = graph.GetNodeChild(index);
                    while (childIndex.IsValid())
                    {
                        if (m_nodeNameBoneIndexMap.find(graph.GetNodeName(childIndex).c_str()) != m_nodeNameBoneIndexMap.end())
                        {
                            boneEntity.nChildren++;
                        }
                        childIndex = graph.GetNodeSibling(childIndex);
                    }

                    context.m_skinningInfo.m_arrBoneEntities.push_back(boneEntity);

                    childIndex = graph.GetNodeChild(index);
                    while (childIndex.IsValid())
                    {
                        CreateBoneEntityDataRecursion(context, childIndex, graph, rootBoneName);
                        childIndex = graph.GetNodeSibling(childIndex);
                    }
                }
            }
            return SceneEvents::ProcessingResult::Success;
        }

        void ChrSkeletonExporter::SetBoneName(const AZStd::string& name, CryBoneDescData& boneDesc) const
        {
            static const size_t nodeNameCount = sizeof(boneDesc.m_arrBoneName) / sizeof(boneDesc.m_arrBoneName[0]);
            size_t offset = (name.length() < nodeNameCount) ? 0 : (name.length() - nodeNameCount + 1);
            azstrcpy(boneDesc.m_arrBoneName, nodeNameCount, name.c_str() + offset);
        }
    }
}
