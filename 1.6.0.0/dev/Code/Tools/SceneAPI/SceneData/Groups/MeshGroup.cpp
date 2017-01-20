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

#include <algorithm>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <SceneAPI/SceneData/Groups/MeshGroup.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshData.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IRule.h>
#include <SceneAPI/SceneUI/RowWidgets/ManifestVectorHandler.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace SceneData
        {
            size_t MeshGroup::GetRuleCount() const
            {
                return m_rules.size();
            }

            AZStd::shared_ptr<DataTypes::IRule> MeshGroup::GetRule(size_t index) const
            {
                AZ_Assert(index < m_rules.size(), "Invalid index %i for rule in mesh group.", index);
                return m_rules[index];
            }

            void MeshGroup::AddRule(const AZStd::shared_ptr<DataTypes::IRule>& rule)
            {
                if (AZStd::find(m_rules.begin(), m_rules.end(), rule) == m_rules.end())
                {
                    m_rules.push_back(rule);
                }
            }

            void MeshGroup::AddRule(AZStd::shared_ptr<DataTypes::IRule>&& rule)
            {
                if (AZStd::find(m_rules.begin(), m_rules.end(), rule) == m_rules.end())
                {
                    m_rules.push_back(AZStd::move(rule));
                }
            }

            void MeshGroup::RemoveRule(size_t index)
            {
                if (index < m_rules.size())
                {
                    m_rules.erase(m_rules.begin() + index);
                }
            }

            void MeshGroup::RemoveRule(const AZStd::shared_ptr<DataTypes::IRule>& rule)
            {
                auto it = AZStd::find(m_rules.begin(), m_rules.end(), rule);
                if (it != m_rules.end())
                {
                    m_rules.erase(it);
                }
            }

            DataTypes::ISceneNodeSelectionList& MeshGroup::GetSceneNodeSelectionList()
            {
                return m_nodeSelectionList;
            }

            const DataTypes::ISceneNodeSelectionList& MeshGroup::GetSceneNodeSelectionList() const
            {
                return m_nodeSelectionList;
            }

            void MeshGroup::Reflect(ReflectContext* context)
            {
                SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context);
                if (!serializeContext || serializeContext->FindClassData(MeshGroup::TYPEINFO_Uuid()))
                {
                    return;
                }

                serializeContext->Class<MeshGroup, DataTypes::IMeshGroup>()->Version(1)
                    ->Field("nodeSelectionList", &MeshGroup::m_nodeSelectionList)
                    ->Field("rules", &MeshGroup::m_rules);

                EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Class<MeshGroup>("Mesh Group", "Configure mesh data exporting.")
                        ->ClassElement(Edit::ClassElements::EditorData, "")
                            ->Attribute("AutoExpand", true)
                            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, "")
                        ->DataElement(Edit::UIHandlers::Default, &MeshGroup::m_nodeSelectionList, "Mesh selection", "Manage the meshes that are selected for export.")
                            ->Attribute("FilterName", "meshes")
                            ->Attribute("FilterType", DataTypes::IMeshData::TYPEINFO_Uuid())
                        ->DataElement(AZ_CRC("ManifestVector", 0x895aa9aa), &MeshGroup::m_rules, "", "Add or remove rules to fine-tune the export process.")
                            ->ElementAttribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_Hide", 0x32ab90f7))
                            ->Attribute(AZ::Edit::Attributes::ContainerCanBeModified, false)
                            ->Attribute(AZ_CRC("ObjectTypeName", 0x6559e0c0), "Rule");
                }
            }
        } // SceneData
    } // SceneAPI
} // AZ