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

#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IRule.h>
#include <SceneAPI/SceneData/Groups/SkeletonGroup.h>
#include <SceneAPI/SceneData/GraphData/RootBoneData.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace SceneData
        {
            AZ_CLASS_ALLOCATOR_IMPL(SkeletonGroup, SystemAllocator, 0)

            size_t SkeletonGroup::GetRuleCount() const
            {
                return m_rules.size();
            }

            AZStd::shared_ptr<DataTypes::IRule> SkeletonGroup::GetRule(size_t index) const
            {
                AZ_Assert(index < m_rules.size(), "Invalid index %i for rule in skeleton group.", index);
                return m_rules[index];
            }

            void SkeletonGroup::AddRule(const AZStd::shared_ptr<DataTypes::IRule>& rule)
            {
                if (AZStd::find(m_rules.begin(), m_rules.end(), rule) == m_rules.end())
                {
                    m_rules.push_back(rule);
                }
            }

            void SkeletonGroup::AddRule(AZStd::shared_ptr<DataTypes::IRule>&& rule)
            {
                if (AZStd::find(m_rules.begin(), m_rules.end(), rule) == m_rules.end())
                {
                    m_rules.push_back(AZStd::move(rule));
                }
            }

            void SkeletonGroup::RemoveRule(size_t index)
            {
                if (index < m_rules.size())
                {
                    m_rules.erase(m_rules.begin() + index);
                }
            }

            void SkeletonGroup::RemoveRule(const AZStd::shared_ptr<DataTypes::IRule>& rule)
            {
                auto it = AZStd::find(m_rules.begin(), m_rules.end(), rule);
                if (it != m_rules.end())
                {
                    m_rules.erase(it);
                }
            }

            const AZStd::string& SkeletonGroup::GetSelectedRootBone() const
            {
                return m_selectedRootBone;
            }

            void SkeletonGroup::SetSelectedRootBone(const AZStd::string& selectedRootBone)
            {
                m_selectedRootBone = selectedRootBone;
            }

            void SkeletonGroup::Reflect(ReflectContext* context)
            {
                SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context);
                if (!serializeContext || serializeContext->FindClassData(SkeletonGroup::TYPEINFO_Uuid()))
                {
                    return;
                }

                serializeContext->Class<SkeletonGroup, DataTypes::ISkeletonGroup>()->Version(1)
                    ->Field("selectedRootBone", &SkeletonGroup::m_selectedRootBone)
                    ->Field("rules", &SkeletonGroup::m_rules);

                EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Class<SkeletonGroup>("Skeleton Group", "Configure skeleton data exporting.")
                        ->ClassElement(Edit::ClassElements::EditorData, "")
                            ->Attribute("AutoExpand", true)
                            ->Attribute(Edit::Attributes::NameLabelOverride, "")
                        ->DataElement("NodeListSelection", &SkeletonGroup::m_selectedRootBone, "Root Bone", "The root bone of the skeleton that will be exported")
                            ->Attribute("ClassTypeIdFilter", AZ::SceneData::GraphData::RootBoneData::TYPEINFO_Uuid())
                            ->Attribute("UseShortNames", true)
                        ->DataElement(AZ_CRC("ManifestVector", 0x895aa9aa), &SkeletonGroup::m_rules, "", "Add or remove rules to fine-tune the export process.")
                            ->ElementAttribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_Hide"))
                            ->Attribute(AZ::Edit::Attributes::ContainerCanBeModified, false)
                            ->Attribute(AZ_CRC("ObjectTypeName", 0x6559e0c0), "Rule");
                }
            }
        }
    }
}