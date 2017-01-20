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
#include <AzCore/IO/SystemFile.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzToolsFramework/Debug/TraceContext.h>
#include <RC/ResourceCompilerScene/Cgf/CgfExportContexts.h>
#include <RC/ResourceCompilerScene/Cgf/CgfMeshGroupExporter.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Utilities/SceneGraphSelector.h>
#include <SceneAPI/SceneCore/Utilities/FileUtilities.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshData.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ISceneNodeSelectionList.h>
#include <SceneAPI/SceneCore/DataTypes/Groups/IMeshGroup.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IPhysicsRule.h>
#include <SceneAPI/SceneCore/DataTypes/DataTypeUtilities.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>

namespace AZ
{
    namespace RC
    {
        namespace SceneEvents = AZ::SceneAPI::Events;
        namespace SceneUtil = AZ::SceneAPI::Utilities;
        namespace SceneDataTypes = AZ::SceneAPI::DataTypes;
        namespace SceneContainers = AZ::SceneAPI::Containers;

        const AZStd::string CgfMeshGroupExporter::fileExtension = "cgf";

        CgfMeshGroupExporter::CgfMeshGroupExporter(IAssetWriter* writer)
            : CallProcessorBinder()
            , m_assetWriter(writer)
        {
            BindToCall(&CgfMeshGroupExporter::ProcessContext);
        }

        SceneEvents::ProcessingResult CgfMeshGroupExporter::ProcessContext(CgfMeshGroupExportContext& context) const
        {
            if (context.m_phase != Phase::Filling)
            {
                return SceneEvents::ProcessingResult::Ignored;
            }

            AZStd::string filename = SceneUtil::FileUtilities::CreateOutputFileName(context.m_groupName, context.m_outputDirectory, fileExtension);
            AZ_TraceContext("CGF File Name", filename);
            if (filename.empty() || !SceneUtil::FileUtilities::EnsureTargetFolderExists(filename))
            {
                AZ_TracePrintf(AZ::SceneAPI::Utilities::ErrorWindow, "Unable to write CGF file. Filename is empty or target folder does not exist.");
                return SceneEvents::ProcessingResult::Failure;
            }

            SceneEvents::ProcessingResultCombiner result;
            
            CContentCGF cgfContent(filename.c_str());
            ConfigureCgfContent(cgfContent);
            result += SceneEvents::Process<CgfContainerExportContext>(context, cgfContent, Phase::Construction);
            
            CgfContainerExportContext containerContextFilling(context, cgfContent, Phase::Filling);
            ProcessMeshes(containerContextFilling, cgfContent);
            result += SceneEvents::Process(containerContextFilling);

            result += SceneEvents::Process<CgfContainerExportContext>(context, cgfContent, Phase::Finalizing);
            
            if (m_assetWriter && cgfContent.GetNodeCount() > 0)
            {
                if (!m_assetWriter->WriteCGF(&cgfContent))
                {
                    AZ_TracePrintf(AZ::SceneAPI::Utilities::ErrorWindow, "Unable to write CGF file.");
                    result += SceneEvents::ProcessingResult::Failure;
                }
            }
            return result.GetResult();
        }

        void CgfMeshGroupExporter::ConfigureCgfContent(CContentCGF& content) const
        {
            CExportInfoCGF* exportInfo = content.GetExportInfo();

            exportInfo->bMergeAllNodes = true;
            exportInfo->bUseCustomNormals = false;
            exportInfo->bCompiledCGF = false;
            exportInfo->bHavePhysicsProxy = false;
            exportInfo->bHaveAutoLods = false;
            exportInfo->bNoMesh = true;
            exportInfo->b8WeightsPerVertex = false;
            exportInfo->bWantF32Vertices = false;
            exportInfo->authorToolVersion = 1;
        }

        void CgfMeshGroupExporter::ProcessMeshes(CgfContainerExportContext& context, CContentCGF& content) const
        {
            const SceneContainers::SceneGraph& graph = context.m_scene.GetGraph();

            AZStd::vector<AZStd::string> physTargetNodes;
            AZStd::shared_ptr<const SceneDataTypes::IPhysicsRule> physRule = SceneAPI::DataTypes::Utilities::FindRule<SceneDataTypes::IPhysicsRule>(context.m_group);
            if (physRule)
            {
                physTargetNodes = SceneUtil::SceneGraphSelector::GenerateTargetNodes(graph, physRule->GetSceneNodeSelectionList(), SceneUtil::SceneGraphSelector::IsMesh);
            }
            ProcessMeshType(context, content, physTargetNodes, PHYS_GEOM_TYPE_DEFAULT_PROXY);
            AZStd::vector<AZStd::string> targetNodes = SceneUtil::SceneGraphSelector::GenerateTargetNodes(graph,
                    context.m_group.GetSceneNodeSelectionList(), SceneUtil::SceneGraphSelector::IsMesh);
            ProcessMeshType(context, content, targetNodes, PHYS_GEOM_TYPE_NONE);
        }

        void CgfMeshGroupExporter::ProcessMeshType(CgfContainerExportContext& context, CContentCGF& content, AZStd::vector<AZStd::string>& targetNodes, EPhysicsGeomType physicalizeType) const
        {
            const SceneContainers::SceneGraph& graph = context.m_scene.GetGraph();
            for (const AZStd::string& nodeName : targetNodes)
            {
                SceneContainers::SceneGraph::NodeIndex index = graph.Find(nodeName.c_str());
                if (index.IsValid())
                {
                    CNodeCGF* node = new CNodeCGF(); // will be auto deleted by CContentCGF cgf
                    SetNodeName(nodeName, *node);
                    SceneEvents::Process<CgfNodeExportContext>(context, *node, nodeName, index, physicalizeType, Phase::Construction);
                    SceneEvents::Process<CgfNodeExportContext>(context, *node, nodeName, index, physicalizeType, Phase::Filling);
                    content.AddNode(node);
                    SceneEvents::Process<CgfNodeExportContext>(context, *node, nodeName, index, physicalizeType, Phase::Finalizing);
                }
            }
        }

        void CgfMeshGroupExporter::SetNodeName(const AZStd::string& name, CNodeCGF& node) const
        {
            static const size_t nodeNameCount = sizeof(node.name) / sizeof(node.name[0]);

            size_t offset = name.length() < nodeNameCount ? 0 : name.length() - nodeNameCount + 1;
            azstrcpy(node.name, nodeNameCount, name.c_str() + offset);
        }
    } // RC
} // AZ
