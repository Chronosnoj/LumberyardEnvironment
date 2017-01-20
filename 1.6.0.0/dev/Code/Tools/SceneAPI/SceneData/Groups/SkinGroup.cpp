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

#include <SceneAPI/SceneData/Groups/SkinGroup.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IRule.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace SceneData
        {
            AZ_CLASS_ALLOCATOR_IMPL(SkinGroup, AZ::SystemAllocator, 0)

            DataTypes::ISceneNodeSelectionList& SkinGroup::GetSceneNodeSelectionList() 
            {
                return m_nodeSelectionList; 
            }

            const DataTypes::ISceneNodeSelectionList& SkinGroup::GetSceneNodeSelectionList() const 
            {
                return m_nodeSelectionList; 
            }

            size_t SkinGroup::GetRuleCount() const 
            {
                return m_rules.size(); 
            }

            AZStd::shared_ptr<AZ::SceneAPI::DataTypes::IRule> SkinGroup::GetRule(size_t index) const 
            {
                return m_rules[index];
            }

            void SkinGroup::AddRule(const AZStd::shared_ptr<AZ::SceneAPI::DataTypes::IRule>& rule) 
            {
                if (AZStd::find(m_rules.begin(), m_rules.end(), rule) == m_rules.end())
                {
                    m_rules.push_back(rule);
                }

            }

            void SkinGroup::AddRule(AZStd::shared_ptr<AZ::SceneAPI::DataTypes::IRule>&& rule) 
            {
                if (AZStd::find(m_rules.begin(), m_rules.end(), rule) == m_rules.end())
                {
                    m_rules.push_back(rule);
                }
            }

            void SkinGroup::RemoveRule(size_t index)
            {
                m_rules.erase(m_rules.begin() + index);
            }

            void SkinGroup::RemoveRule(const AZStd::shared_ptr<AZ::SceneAPI::DataTypes::IRule>& rule) 
            {
                for (auto iter = m_rules.begin(); iter != m_rules.end(); ++iter)
                {
                    if (*iter == rule)
                    {
                        m_rules.erase(iter);
                        return;
                    }
                }
            }

            void SkinGroup::Reflect(AZ::ReflectContext* context)
            {
                AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
                if (serializeContext == nullptr)
                {
                    return;
                }

                if (serializeContext->FindClassData(SkinGroup::TYPEINFO_Uuid()))
                {
                    return;
                }

                serializeContext->Class<SkinGroup, DataTypes::ISkinGroup>()->Version(1)
                    ->Field("nodeSelectionList", &SkinGroup::m_nodeSelectionList)
                    ->Field("rules", &SkinGroup::m_rules);

                EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Class<SkinGroup>("Skin Group", "Configure meshes for skinning.")
                        ->ClassElement(Edit::ClassElements::EditorData, "")
                            ->Attribute("AutoExpand", true)
                            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, "")
                        ->DataElement(Edit::UIHandlers::Default, &SkinGroup::m_nodeSelectionList, "Skin selection", "Manage the skins that are selected for export.")
                            ->Attribute("FilterName", "skins")
                        ->DataElement(AZ_CRC("ManifestVector", 0x895aa9aa), &SkinGroup::m_rules, "", "Add or remove rules to fine-tune the export process.")
                        ->ElementAttribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_Hide"))
                            ->Attribute(AZ::Edit::Attributes::ContainerCanBeModified, false)
                            ->Attribute(AZ_CRC("ObjectTypeName", 0x6559e0c0), "Rule");
                }
            }
        }
    }
}
