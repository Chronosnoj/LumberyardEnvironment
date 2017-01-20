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
#include <SceneAPI/SceneData/Rules/MaterialRule.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace SceneData
        {
            AZ_CLASS_ALLOCATOR_IMPL(MaterialRule, SystemAllocator, 0)

            MaterialRule::MaterialRule()
                : m_enableMaterials(true)
                , m_removeMaterials(false)
                , m_updateMaterials(false)
            {
            }

            bool MaterialRule::EnableMaterials() const
            {
                return m_enableMaterials;
            }

            bool MaterialRule::RemoveUnusedMaterials() const
            {
                return m_removeMaterials;
            }

            bool MaterialRule::UpdateMaterials() const
            {
                return m_updateMaterials;
            }

            void MaterialRule::Reflect(ReflectContext* context)
            {
                SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context);
                if (!serializeContext || serializeContext->FindClassData(MaterialRule::TYPEINFO_Uuid()))
                {
                    return;
                }

                serializeContext->Class<MaterialRule, DataTypes::IMaterialRule>()->Version(1)
                    ->Field("enableMaterials", &MaterialRule::m_enableMaterials)
                    ->Field("updateMaterials", &MaterialRule::m_updateMaterials)
                    ->Field("removeMaterials", &MaterialRule::m_removeMaterials);

                EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Class<MaterialRule>("Material", "Control materials for exporting from the group")
                        ->ClassElement(Edit::ClassElements::EditorData, "")
                            ->Attribute("AutoExpand", true)
                            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, "")
                        ->DataElement(Edit::UIHandlers::Default, &MaterialRule::m_enableMaterials, "Enable Materials", "Enable or disabling linking the group to the materials. If an existing material is found, any materials present in the FBX, but not the MTL file will be added.")
                        ->DataElement(Edit::UIHandlers::Default, &MaterialRule::m_updateMaterials, "Update Materials", "Will update any existing MTL file to use the same settings as the FBX file. Will not affect settings not controlled by FBX")
                        ->DataElement(Edit::UIHandlers::Default, &MaterialRule::m_removeMaterials, "Remove Unused Materials","Will remove any material present in an existing MTL file not present in the FBX file.");
                }
            }
        } // SceneData
    } // SceneAPI
} // AZ
