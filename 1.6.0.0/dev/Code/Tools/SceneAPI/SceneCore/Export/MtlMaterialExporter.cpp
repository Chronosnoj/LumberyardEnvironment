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

#include <memory>
#include <SceneAPI/SceneCore/Export/MtlMaterialExporter.h>
#include <AzCore/std/containers/queue.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/XML/rapidxml.h>
#include <AzCore/XML/rapidxml_print.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/std/string/conversions.h>
#include <AzCore/Casting/numeric_cast.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzFramework/API/ApplicationAPI.h>
#include <AzToolsFramework/Debug/TraceContext.h>
#include <GFxFramework/MaterialIO/Material.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMaterialData.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshVertexColorData.h>
#include <SceneAPI/SceneCore/DataTypes/Groups/IMeshGroup.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IMeshAdvancedRule.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IPhysicsRule.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IMaterialRule.h>
#include <SceneAPI/SceneCore/DataTypes/DataTypeUtilities.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ISceneNodeSelectionList.h>
#include <SceneAPI/SceneCore/Containers/Views/SceneGraphDownwardsIterator.h>
#include <SceneAPI/SceneCore/Containers/Views/PairIterator.h>
#include <SceneAPI/SceneCore/Containers/Views/FilterIterator.h>
#include <SceneAPI/SceneCore/Utilities/SceneGraphSelector.h>
#include <SceneAPI/SceneCore/Utilities/FileUtilities.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Export
        {
            bool MtlMaterialExporter::SaveMaterialGroup(const AZStd::string& fileName, const DataTypes::IMeshGroup& meshGroup,
                const Containers::Scene& scene, const AZStd::string& textureRootPath)
            {
                m_filename = fileName;
                m_textureRootPath = textureRootPath;
                EBUS_EVENT(AzFramework::ApplicationRequests::Bus, NormalizePath, m_textureRootPath);
                return BuildMaterialGroup(meshGroup, scene, m_materialGroup);
            }

            bool MtlMaterialExporter::WriteToFile(const char* outputDirectory, const Containers::Scene& scene)
            {
                AZStd::string targetFilename = Utilities::FileUtilities::CreateOutputFileName(m_filename, outputDirectory, 
                    GFxFramework::MaterialExport::g_mtlExtension);
                if (targetFilename.empty())
                {
                    return false;
                }
                if (AZ::IO::SystemFile::Exists(targetFilename.c_str()) && !AZ::IO::SystemFile::IsWritable(targetFilename.c_str()))
                {
                    return false;
                }
                return WriteMaterialFile(targetFilename.c_str(), m_materialGroup);
            }

            bool MtlMaterialExporter::WriteToFile(const std::string& outputDirectory, const Containers::Scene& scene)
            {
                return WriteToFile(outputDirectory.c_str(), scene);
            }

            bool MtlMaterialExporter::BuildMaterialGroup(const DataTypes::IMeshGroup& meshGroup, const Containers::Scene& scene,
                MaterialGroup& materialGroup) const
            {
                //Default rule settings for materials.
                materialGroup.m_materials.clear();
                materialGroup.m_enableMaterials = true;
                materialGroup.m_removeMaterials = false;
                materialGroup.m_updateMaterials = false;

                //Check for a material rule
                size_t ruleCount = meshGroup.GetRuleCount();
                for (size_t i = 0; i < ruleCount; ++i)
                {
                    const AZ::SceneAPI::DataTypes::IMaterialRule* rule = azrtti_cast<const AZ::SceneAPI::DataTypes::IMaterialRule*>(meshGroup.GetRule(i).get());
                    if (rule)
                    {
                        materialGroup.m_enableMaterials = rule->EnableMaterials();
                        materialGroup.m_removeMaterials = rule->RemoveUnusedMaterials();
                        materialGroup.m_updateMaterials = rule->UpdateMaterials();
                        break;
                    }
                }

                AZStd::unordered_set<AZStd::string> usedMaterials;
                const Containers::SceneGraph& sceneGraph = scene.GetGraph();

                AZStd::vector<AZStd::string> physTargetNodes;
                AZStd::shared_ptr<const DataTypes::IPhysicsRule> physRule = DataTypes::Utilities::FindRule<DataTypes::IPhysicsRule>(meshGroup);
                if (physRule)
                {
                    physTargetNodes = Utilities::SceneGraphSelector::GenerateTargetNodes(sceneGraph, physRule->GetSceneNodeSelectionList(), Utilities::SceneGraphSelector::IsMesh);
                }
                bool hasValidPhysTargetNodes = false;
                for (auto& nodeName : physTargetNodes)
                {
                    auto index = sceneGraph.Find(nodeName.c_str());
                    if (index.IsValid())
                    {
                        hasValidPhysTargetNodes = true;
                        break;
                    }
                }

                if (hasValidPhysTargetNodes)
                {
                    MaterialInfo info;
                    info.m_name = GFxFramework::MaterialExport::g_stringPhysicsNoDraw;
                    info.m_materialData = nullptr;
                    info.m_usesVertexColoring = false;
                    info.m_physicalize = true;
                    materialGroup.m_materials.push_back(info);
                }

                // create materials for render nodes
                AZStd::vector<AZStd::string> renderTargetNodes = Utilities::SceneGraphSelector::GenerateTargetNodes(sceneGraph, meshGroup.GetSceneNodeSelectionList(), Utilities::SceneGraphSelector::IsMesh);
                for (auto& nodeName : renderTargetNodes)
                {
                    auto index = sceneGraph.Find(nodeName.c_str());
                    if (index.IsValid())
                    {
                        auto childIndex = sceneGraph.GetNodeChild(index);
                        while (childIndex.IsValid())
                        {
                            auto object = sceneGraph.GetNodeContent(childIndex);
                            if (object && object->RTTI_IsTypeOf(DataTypes::IMaterialData::TYPEINFO_Uuid()))
                            {
                                AZStd::string nodeName = Containers::SceneGraph::GetShortName(sceneGraph.GetNodeName(childIndex)).c_str();
                                if (usedMaterials.find(nodeName) == usedMaterials.end())
                                {
                                    MaterialInfo info;
                                    info.m_name = nodeName;
                                    info.m_materialData = azrtti_cast<const DataTypes::IMaterialData*>(object);
                                    info.m_usesVertexColoring = UsesVertexColoring(meshGroup, scene, sceneGraph.ConvertToHierarchyIterator(childIndex));
                                    info.m_physicalize = false;
                                    materialGroup.m_materials.push_back(info);
                                    usedMaterials.insert(nodeName);
                                }
                            }
                            childIndex = sceneGraph.GetNodeSibling(childIndex);
                        }
                    }
                }

                return materialGroup.m_materials.size() > 0;
            }

         
            // Check if there's a mesh advanced rule of the given mesh group that
            //      specifically controls vertex coloring. If no rule exists for group, check if there are any
            //      vertex color streams, which would automatically enable the vertex coloring feature.
            bool MtlMaterialExporter::UsesVertexColoring(const DataTypes::IMeshGroup& meshGroup, const Containers::Scene& scene,
                Containers::SceneGraph::HierarchyStorageConstIterator materialNode) const
            {
                const Containers::SceneGraph& graph = scene.GetGraph();
                Containers::SceneGraph::NodeIndex meshNodeIndex = graph.GetNodeParent(graph.ConvertToNodeIndex(materialNode));

                AZStd::shared_ptr<const DataTypes::IMeshAdvancedRule> rule = DataTypes::Utilities::FindRule<DataTypes::IMeshAdvancedRule>(meshGroup);
                if (rule)
                {
                    return !rule->IsVertexColorStreamDisabled() && !rule->GetVertexColorStreamName().empty();
                }
                else
                {
                    if (DoesMeshNodeHaveColorStreamChild(scene, meshNodeIndex))
                    {
                        return true;
                    }
                }

                return false;
            }

            bool MtlMaterialExporter::WriteMaterialFile(const char* fileName, MaterialGroup& materialGroup) const
            {
                if (materialGroup.m_materials.size() == 0)
                {
                    return false;
                }

                AZ::GFxFramework::MaterialGroup matGroup;
                AZ::GFxFramework::MaterialGroup doNotRemoveGroup;

                //Open the MTL file for read if it exists 
                bool fileRead = matGroup.ReadMtlFile(fileName);
                AZ_TraceContext("MTL File Name", fileName);
                if (fileRead)
                {
                    AZ_TracePrintf(Utilities::LogWindow, "MTL File found and will be updated as needed");
                }
                else
                {
                    AZ_TracePrintf(Utilities::LogWindow, "No existing MTL file found. A new one will be generated.");
                }

                bool hasPhysicalMaterial = false;

                for (const auto& material : materialGroup.m_materials)
                {
                    AZStd::shared_ptr<AZ::GFxFramework::IMaterial> mat = AZStd::make_shared<AZ::GFxFramework::Material>();
                    mat->EnableUseVertexColor(material.m_usesVertexColoring);
                    mat->EnablePhysicalMaterial(material.m_physicalize);
                    hasPhysicalMaterial |= material.m_physicalize;
                    mat->SetName(material.m_name);
                    if (material.m_materialData)
                    {
                        mat->SetTexture(AZ::GFxFramework::TextureMapType::Diffuse, 
                            Utilities::FileUtilities::GetRelativePath(
                                material.m_materialData->GetTexture(DataTypes::IMaterialData::TextureMapType::Diffuse)
                                ,m_textureRootPath));

                        mat->SetTexture(AZ::GFxFramework::TextureMapType::Specular,
                            Utilities::FileUtilities::GetRelativePath(
                                material.m_materialData->GetTexture(DataTypes::IMaterialData::TextureMapType::Specular)
                                ,m_textureRootPath));

                        mat->SetTexture(AZ::GFxFramework::TextureMapType::Bump, 
                            Utilities::FileUtilities::GetRelativePath(
                                material.m_materialData->GetTexture(DataTypes::IMaterialData::TextureMapType::Bump)
                                ,m_textureRootPath));
                    }
                    size_t matIndex = matGroup.FindMaterialIndex(material.m_name);
                    if (materialGroup.m_updateMaterials && matIndex != GFxFramework::MaterialExport::g_materialNotFound)
                    {
                        AZStd::shared_ptr<GFxFramework::IMaterial> origMat = matGroup.GetMaterial(matIndex);
                        origMat->SetName(mat->GetName());
                        origMat->EnableUseVertexColor(mat->UseVertexColor());
                        origMat->EnablePhysicalMaterial(mat->IsPhysicalMaterial());
                        origMat->SetTexture(GFxFramework::TextureMapType::Diffuse, mat->GetTexture(GFxFramework::TextureMapType::Diffuse));
                        origMat->SetTexture(GFxFramework::TextureMapType::Specular, mat->GetTexture(GFxFramework::TextureMapType::Specular));
                        origMat->SetTexture(GFxFramework::TextureMapType::Bump, mat->GetTexture(GFxFramework::TextureMapType::Bump));
                    }
                    //This rule could change independently of an update material flag as it is set in the advanced rule.
                    else if (matIndex != GFxFramework::MaterialExport::g_materialNotFound)
                    {
                        AZStd::shared_ptr<GFxFramework::IMaterial> origMat = matGroup.GetMaterial(matIndex);
                        origMat->EnableUseVertexColor(mat->UseVertexColor());
                    }
                    else
                    {
                        matGroup.AddMaterial(mat);
                    }

                    if (materialGroup.m_removeMaterials)
                    {
                        doNotRemoveGroup.AddMaterial(mat);
                    }
                }

                //Remove a physical material if one had been added previously. 
                if (!hasPhysicalMaterial)
                {
                    matGroup.RemoveMaterial(GFxFramework::MaterialExport::g_stringPhysicsNoDraw);
                }

                //Remove any unused materials
                if (materialGroup.m_removeMaterials)
                {
                    AZStd::vector<AZStd::string> removeNames;
                    for (size_t i = 0; i < matGroup.GetMaterialCount(); ++i)
                    {
                        if (doNotRemoveGroup.FindMaterialIndex(matGroup.GetMaterial(i)->GetName()) == GFxFramework::MaterialExport::g_materialNotFound)
                        {
                            removeNames.push_back(matGroup.GetMaterial(i)->GetName());
                        }
                    }

                    for (const auto& name : removeNames)
                    {
                        matGroup.RemoveMaterial(name);
                    }
                }

                matGroup.WriteMtlFile(fileName);

                return true;
            }

            bool MtlMaterialExporter::DoesMeshNodeHaveColorStreamChild(const Containers::Scene& scene, Containers::SceneGraph::NodeIndex meshNode) const
            {
                const Containers::SceneGraph& graph = scene.GetGraph();
                Containers::SceneGraph::NodeIndex index = graph.GetNodeChild(meshNode);
                while (index.IsValid())
                {
                    AZStd::shared_ptr<const DataTypes::IGraphObject> object = graph.GetNodeContent(index);
                    if (object && object->RTTI_IsTypeOf(DataTypes::IMeshVertexColorData::TYPEINFO_Uuid()))
                    {
                        return true;
                    }
                    index = graph.GetNodeSibling(index);
                }

                return false;
            }
        }
    }
}
