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
#include <Cry_Geo.h>
#include <ConvertContext.h>
#include <IIndexedMesh.h>
#include <CGFContent.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <GFxFramework/MaterialIO/Material.h>
#include <AzToolsFramework/Debug/TraceContext.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <SceneAPI/SceneCore/DataTypes/Groups/IMeshGroup.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMaterialData.h>
#include <SceneAPI/SceneCore/Utilities/FileUtilities.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>
#include <RC/ResourceCompilerScene/Cgf/CgfExportContexts.h>
#include <RC/ResourceCompilerScene/Cgf/CgfMaterialExporter.h>

namespace AZ
{
    namespace RC
    {
        namespace SceneEvents = AZ::SceneAPI::Events;
        namespace SceneDataTypes = AZ::SceneAPI::DataTypes;
        namespace SceneContainers = AZ::SceneAPI::Containers;

        CgfMaterialExporter::CgfMaterialExporter(IConvertContext* convertContext)
            : CallProcessorConnector()
            , m_cachedMeshGroup(nullptr)
            , m_convertContext(convertContext)
        {
            m_physMaterialNames[PHYS_GEOM_TYPE_DEFAULT_PROXY] = GFxFramework::MaterialExport::g_stringPhysicsNoDraw;
        }

        SceneEvents::ProcessingResult CgfMaterialExporter::Process(SceneEvents::ICallContext* context)
        {
            if (context->RTTI_GetType() == CgfMeshGroupExportContext::TYPEINFO_Uuid())
            {
                CgfMeshGroupExportContext* castContext = azrtti_cast<CgfMeshGroupExportContext*>(context);
                AZ_Assert(castContext, "Unable to cast to to CgfMeshGroupExportContext");
                switch (castContext->m_phase)
                {
                case Phase::Construction:
                    m_cachedMeshGroup = &castContext->m_group;
                    return SceneEvents::ProcessingResult::Success;
                case Phase::Finalizing:
                    Reset();
                    return SceneEvents::ProcessingResult::Success;
                default:
                    return SceneEvents::ProcessingResult::Ignored;
                }
            }
            else if (context->RTTI_GetType() == CgfContainerExportContext::TYPEINFO_Uuid())
            {
                CgfContainerExportContext* castContext = azrtti_cast<CgfContainerExportContext*>(context);
                AZ_Assert(castContext, "Unable to cast to to CgfContainerExportContext");
                
                AZStd::string fileName;
                bool fileRead = false;
                AZStd::string rootPath(static_cast<ConvertContext*>(m_convertContext)->GetSourcePath().c_str());
                AzFramework::StringFunc::Path::StripFullName(rootPath);

                switch (castContext->m_phase)
                {
                case Phase::Construction:
                {
                    m_materialGroup = AZStd::make_shared<GFxFramework::MaterialGroup>();
                    fileName =
                        AZ::SceneAPI::Utilities::FileUtilities::CreateOutputFileName(castContext->m_groupName,
                            rootPath, GFxFramework::MaterialExport::g_mtlExtension);
                    AZ_TraceContext("MTL File Name", fileName);

                    fileRead = m_materialGroup->ReadMtlFile(fileName.c_str());

                    if (!fileRead)
                    {
                        AZ_TracePrintf(AZ::SceneAPI::Utilities::ErrorWindow, "Unable to read MTL file for processing meshes.");
                        return SceneEvents::ProcessingResult::Failure;
                    }

                    SetupGlobalMaterial(*castContext);
                    return SceneEvents::ProcessingResult::Success;
                }
                case Phase::Finalizing:
                    PatchSubmeshes(*castContext);
                    CreateSubMaterials(*castContext);
                    return SceneEvents::ProcessingResult::Success;
                default:
                    return SceneEvents::ProcessingResult::Ignored;
                }
            }
            else if (context->RTTI_GetType() == CgfNodeExportContext::TYPEINFO_Uuid())
            {
                CgfNodeExportContext* castContext = azrtti_cast<CgfNodeExportContext*>(context);
                AZ_Assert(castContext, "Unable to cast to to CgfNodeExportContext");
                if (castContext->m_phase == Phase::Filling)
                {
                    AssignCommonMaterial(*castContext);
                    return SceneEvents::ProcessingResult::Success;
                }
                else
                {
                    return SceneEvents::ProcessingResult::Ignored;
                }
            }
            else if (context->RTTI_GetType() == CgfMeshNodeExportContext::TYPEINFO_Uuid())
            {
                CgfMeshNodeExportContext* castContext = azrtti_cast<CgfMeshNodeExportContext*>(context);
                AZ_Assert(castContext, "Unable to cast to to CgfMeshNodeExportContext");
                if (castContext->m_phase == Phase::Filling)
                {
                    PatchMaterials(*castContext);
                    return SceneEvents::ProcessingResult::Success;
                }
                else
                {
                    return SceneEvents::ProcessingResult::Ignored;
                }
            }
            return SceneEvents::ProcessingResult::Ignored;
        }

        void CgfMaterialExporter::SetupGlobalMaterial(CgfContainerExportContext& context)
        {
            AZ_Assert(m_cachedMeshGroup == &context.m_group, "CgfContainerExportContext doesn't belong to chain of previously called CgfMeshGroupExportContext.");

            CMaterialCGF* rootMaterial = context.m_container.GetCommonMaterial();
            if (!rootMaterial)
            {
                rootMaterial = new CMaterialCGF();
                rootMaterial->nPhysicalizeType = PHYS_GEOM_TYPE_NONE;
                azstrcpy(rootMaterial->name, sizeof(rootMaterial->name), context.m_groupName.c_str());
                context.m_container.SetCommonMaterial(rootMaterial);
            }
        }

        void CgfMaterialExporter::AssignCommonMaterial(CgfNodeExportContext& context)
        {
            AZ_Assert(m_cachedMeshGroup == &context.m_group, "CgfMeshNodeExportContext doesn't belong to chain of previously called CgfMeshGroupExportContext.");

            CMaterialCGF* rootMaterial = context.m_container.GetCommonMaterial();
            AZ_Assert(rootMaterial, "Previously assigned root material has been deleted.");
            context.m_node.pMaterial = rootMaterial;
        }

        void CgfMaterialExporter::PatchMaterials(CgfMeshNodeExportContext& context)
        {
            AZ_Assert(m_cachedMeshGroup == &context.m_group, "CgfMeshNodeExportContext doesn't belong to chain of previously called CgfMeshGroupExportContext.");

            AZStd::vector<size_t> relocationTable;
            BuildRelocationTable(relocationTable, context);

            if (relocationTable.empty())
            {
                // If the relocationTable is empty no materials were assigned to any of the
                //      selected meshes. In this case simply leave the subsets as assigned
                //      so users can later manually add materials if needed.
                return;
            }

            if (context.m_container.GetExportInfo()->bMergeAllNodes)
            {
                // Due to a bug which cases subsets to not merge correctly (see PatchSubmeshes for more details) use the global
                //      table so far to patch the subset index in the face info instead. This way they will be assigned to the
                //      eventual global subset stored in the first mesh.
                int faceCount = context.m_mesh.GetFaceCount();
                for (int i = 0; i < faceCount; ++i)
                {
                    context.m_mesh.m_pFaces[i].nSubset = relocationTable[context.m_mesh.m_pFaces[i].nSubset];
                }
            }
            else
            {
                for (SMeshSubset& subset : context.m_mesh.m_subsets)
                {
                    subset.nMatID = relocationTable[subset.nMatID];
                }
            }
        }

        void CgfMaterialExporter::PatchSubmeshes(CgfContainerExportContext& context)
        {
            // Due to a bug in the merging process of the CgfCompiler it will always take the number of subsets of the first mesh
            //      it finds. This causes files with more materials than the first model to not merge properly and ultimately cause
            //      the entire export to fail. (See CGFNodeMerger::MergeNodes for more details.) The work-around for now is to fill
            //      the first mesh up with placeholder subsets and adjust the subset indices in the face info.
            AZ_Assert(m_cachedMeshGroup == &context.m_group, "CgfContainerExportContext doesn't belong to chain of previously called CgfMeshGroupExportContext.");

            if (context.m_container.GetExportInfo()->bMergeAllNodes)
            {
                CMesh* firstMesh = nullptr;
                int nodeCount = context.m_container.GetNodeCount();
                for (int i = 0; i < nodeCount; ++i)
                {
                    CNodeCGF* node = context.m_container.GetNode(i);
                    if (node->pMesh && !node->bPhysicsProxy && node->type == CNodeCGF::NODE_MESH)
                    {
                        firstMesh = node->pMesh;
                        break;
                    }
                }

                if (firstMesh)
                {
                    int subsetCount = firstMesh->GetSubSetCount();
                    size_t materialCount = m_materialGroup->GetMaterialCount();

                    for (int i = 0; i < subsetCount; ++i)
                    {
                        AZ_Assert(firstMesh->m_subsets[i].nMatID == i, "Materials addition order broken. (%i vs. %i)", firstMesh->m_subsets[i].nMatID, i);
                    }

                    for (size_t i = subsetCount; i < materialCount; ++i)
                    {
                        SMeshSubset meshSubset;
                        meshSubset.nMatID = i;
                        firstMesh->m_subsets.push_back(meshSubset);
                    }
                }
            }
        }

        void CgfMaterialExporter::BuildRelocationTable(AZStd::vector<size_t>& table, CgfMeshNodeExportContext& context)
        {
            auto physicalizeType = context.m_physicalizeType;
            if (physicalizeType == PHYS_GEOM_TYPE_DEFAULT_PROXY)
            {
                table.push_back(m_materialGroup->FindMaterialIndex(GFxFramework::MaterialExport::g_stringPhysicsNoDraw));
            }
            else
            {
                const SceneContainers::SceneGraph& graph = context.m_scene.GetGraph();

                SceneContainers::SceneGraph::NodeIndex index = graph.GetNodeChild(context.m_nodeIndex);
                while (index.IsValid())
                {
                    AZStd::shared_ptr<const SceneDataTypes::IGraphObject> object = graph.GetNodeContent(index);
                    if (object && object->RTTI_IsTypeOf(SceneDataTypes::IMaterialData::TYPEINFO_Uuid()))
                    {
                        const std::string& nodeFullName = graph.GetNodeName(index);
                        AZStd::string nodeName = SceneContainers::SceneGraph::GetShortName(nodeFullName).c_str();

                        table.push_back(m_materialGroup->FindMaterialIndex(nodeName));
                    }
                    index = graph.GetNodeSibling(index);
                }
            }
        }

        void CgfMaterialExporter::CreateSubMaterials(CgfContainerExportContext& context)
        {
            AZ_Assert(m_cachedMeshGroup == &context.m_group, "CgfMeshNodeExportContext doesn't belong to chain of previously called CgfMeshGroupExportContext.");

            CMaterialCGF* rootMaterial = context.m_container.GetCommonMaterial();
            if (!rootMaterial)
            {
                AZ_Assert(rootMaterial, "Previously assigned root material has been deleted.");
                return;
            }

            // Create sub-materials stored in root material. Sub-materials will be used to assign physical types
            // to subsets stored in meshes when mesh gets compiled later on.
            rootMaterial->subMaterials.resize(m_materialGroup->GetMaterialCount(), nullptr);

            for (size_t i = 0; i < m_materialGroup->GetMaterialCount(); ++i)
            {
                CMaterialCGF* materialCGF = new CMaterialCGF();
                AZStd::shared_ptr<const GFxFramework::IMaterial> material = m_materialGroup->GetMaterial(i);
                if (material)
                {
                    azstrncpy(materialCGF->name, material->GetName().c_str(), sizeof(materialCGF->name));
                    materialCGF->nPhysicalizeType = material->IsPhysicalMaterial() ? PHYS_GEOM_TYPE_DEFAULT_PROXY : PHYS_GEOM_TYPE_NONE;
                    rootMaterial->subMaterials[i] = materialCGF;
                }
            }
        }

        void CgfMaterialExporter::Reset()
        {
            m_materialGroup = nullptr;
        }
    } // RC
} // AZ
