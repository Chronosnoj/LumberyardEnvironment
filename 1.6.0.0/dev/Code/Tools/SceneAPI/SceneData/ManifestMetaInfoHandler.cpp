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

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/set.h>
#include <AzToolsFramework/Debug/TraceContext.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>
#include <SceneAPI/SceneCore/Utilities/SceneGraphSelector.h>
#include <SceneAPI/SceneData/ManifestMetaInfoHandler.h>
#include <SceneAPI/SceneData/Groups/MeshGroup.h>
#include <SceneAPI/SceneData/Groups/SkeletonGroup.h>
#include <SceneAPI/SceneData/Groups/SkinGroup.h>
#include <SceneAPI/SceneData/Rules/CommentRule.h>
#include <SceneAPI/SceneData/Rules/MaterialRule.h>
#include <SceneAPI/SceneData/Rules/MeshAdvancedRule.h>
#include <SceneAPI/SceneData/Rules/OriginRule.h>
#include <SceneAPI/SceneData/Rules/PhysicsRule.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace SceneData
        {
            AZ_CLASS_ALLOCATOR_IMPL(ManifestMetaInfoHandler, SystemAllocator, 0)

            ManifestMetaInfoHandler::ManifestMetaInfoHandler()
            {
                BusConnect();
            }

            ManifestMetaInfoHandler::~ManifestMetaInfoHandler()
            {
                BusDisconnect();
            }

            void ManifestMetaInfoHandler::GetCategoryAssignments(CategoryMap& categories)
            {
                categories.insert(CategoryMap::value_type("Meshes", MeshGroup::TYPEINFO_Uuid()));
               // categories.insert(CategoryMap::value_type("Rigs", SkeletonGroup::TYPEINFO_Uuid()));
               // categories.insert(CategoryMap::value_type("Rigs", SkinGroup::TYPEINFO_Uuid()));
            }

            void ManifestMetaInfoHandler::GetAvailableModifiers(ModifiersList& modifiers,
                const AZStd::shared_ptr<const Containers::Scene>& /*scene*/, const DataTypes::IManifestObject* target)
            {
                AZ_TraceContext("Object Type", target->RTTI_GetTypeName());
                // Temporary until there is a manager than maintains and dynamically
                //  updates modifier mappings (allowing user customization)
                if (target->RTTI_IsTypeOf(DataTypes::IMeshGroup::TYPEINFO_Uuid()))
                {
                    const DataTypes::IMeshGroup* group = azrtti_cast<const DataTypes::IMeshGroup*>(target);
                    modifiers.push_back(SceneData::CommentRule::TYPEINFO_Uuid());

                    AZStd::set<Uuid> existingRules;
                    for (size_t i = 0; i < group->GetRuleCount(); ++i)
                    {
                        existingRules.insert(group->GetRule(i)->RTTI_GetType());
                    }

                    if (existingRules.find(SceneData::MaterialRule::TYPEINFO_Uuid()) == existingRules.end())
                    {
                        modifiers.push_back(SceneData::MaterialRule::TYPEINFO_Uuid());
                    }
                    if (existingRules.find(SceneData::MeshAdvancedRule::TYPEINFO_Uuid()) == existingRules.end())
                    {
                        modifiers.push_back(SceneData::MeshAdvancedRule::TYPEINFO_Uuid());
                    }
                    if (existingRules.find(SceneData::OriginRule::TYPEINFO_Uuid()) == existingRules.end())
                    {
                        modifiers.push_back(SceneData::OriginRule::TYPEINFO_Uuid());
                    }
                    if (existingRules.find(SceneData::PhysicsRule::TYPEINFO_Uuid()) == existingRules.end())
                    {
                        modifiers.push_back(SceneData::PhysicsRule::TYPEINFO_Uuid());
                    }
                }
                else if (target->RTTI_IsTypeOf(DataTypes::ISkinGroup::TYPEINFO_Uuid()))
                {
                    const DataTypes::ISkinGroup* group = azrtti_cast<const DataTypes::ISkinGroup*>(target);
                    modifiers.push_back(SceneData::CommentRule::TYPEINFO_Uuid());

                    AZStd::set<AZ::Uuid> existingRules;
                    for (size_t i = 0; i < group->GetRuleCount(); ++i)
                    {
                        existingRules.insert(group->GetRule(i)->RTTI_GetType());
                    }

                    if (existingRules.find(SceneData::MaterialRule::TYPEINFO_Uuid()) == existingRules.end())
                    {
                        modifiers.push_back(SceneData::MaterialRule::TYPEINFO_Uuid());
                    }
                    if (existingRules.find(SceneData::MeshAdvancedRule::TYPEINFO_Uuid()) == existingRules.end())
                    {
                        modifiers.push_back(SceneData::MeshAdvancedRule::TYPEINFO_Uuid());
                    }
                }
                else if (target->RTTI_IsTypeOf(DataTypes::ISkeletonGroup::TYPEINFO_Uuid()))
                {
                    modifiers.push_back(SceneData::CommentRule::TYPEINFO_Uuid());
                }
                else
                {
                    AZ_TracePrintf(Utilities::LogWindow, "No modifiers setup for type");
                }
            }

            void ManifestMetaInfoHandler::InitializeObject(const Containers::Scene& scene, DataTypes::IManifestObject* target)
            {
                AZ_Assert(target, "Received a null manifest object.");
                if (!target)
                {
                    return;
                }

                AZ_TraceContext("Object Type", target->RTTI_GetTypeName());

                if (target->RTTI_IsTypeOf(DataTypes::IMeshGroup::TYPEINFO_Uuid()))
                {
                    DataTypes::IMeshGroup* group = azrtti_cast<DataTypes::IMeshGroup*>(target);
                    Utilities::SceneGraphSelector::SelectAll(scene.GetGraph(), group->GetSceneNodeSelectionList());
                }
                else if (target->RTTI_IsTypeOf(DataTypes::IPhysicsRule::TYPEINFO_Uuid()))
                {
                    DataTypes::IPhysicsRule* rule = azrtti_cast<DataTypes::IPhysicsRule*>(target);
                    Utilities::SceneGraphSelector::UnselectAll(scene.GetGraph(), rule->GetSceneNodeSelectionList());
                }
            }
        } // SceneData
    } // SceneAPI
} // AZ