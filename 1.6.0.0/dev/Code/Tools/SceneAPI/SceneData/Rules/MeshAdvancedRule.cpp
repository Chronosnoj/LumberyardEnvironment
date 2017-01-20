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
#include <SceneAPI/SceneData/Rules/MeshAdvancedRule.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshVertexUVData.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshVertexColorData.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace SceneData
        {
            AZ_CLASS_ALLOCATOR_IMPL(MeshAdvancedRule, SystemAllocator, 0)

            MeshAdvancedRule::MeshAdvancedRule()
                : m_use32bitVertices(false)
                , m_mergeMeshes(true)
            {
            }

            void MeshAdvancedRule::SetUse32bitVertices(bool value)
            {
                m_use32bitVertices = value;
            }

            bool MeshAdvancedRule::Use32bitVertices() const
            {
                return m_use32bitVertices;
            }

            void MeshAdvancedRule::SetMergeMeshes(bool value)
            {
                m_mergeMeshes = value;
            }

            bool MeshAdvancedRule::MergeMeshes() const
            {
                return m_mergeMeshes;
            }

            const AZStd::string& MeshAdvancedRule::GetDisabledName()
            {
                static AZStd::string disabledString = "Disabled";
                return disabledString;
            }

            void MeshAdvancedRule::SetVertexColorStreamName(const AZStd::string& name)
            {
                m_vertexColorStreamName = name;
            }

            void MeshAdvancedRule::SetVertexColorStreamName(AZStd::string&& name)
            {
                m_vertexColorStreamName = AZStd::move(name);
            }

            const AZStd::string& MeshAdvancedRule::GetVertexColorStreamName() const
            {
                return m_vertexColorStreamName;
            }

            bool MeshAdvancedRule::IsVertexColorStreamDisabled() const
            {
                return m_vertexColorStreamName == GetDisabledName();
            }

            void MeshAdvancedRule::SetUVStreamName(const AZStd::string& name)
            {
                m_uvStreamName = name;
            }

            void MeshAdvancedRule::SetUVStreamName(AZStd::string&& name)
            {
                m_uvStreamName = AZStd::move(name);
            }

            const AZStd::string& MeshAdvancedRule::GetUVStreamName() const
            {
                return m_uvStreamName;
            }

            bool MeshAdvancedRule::IsUVStreamDisabled() const
            {
                return m_uvStreamName == GetDisabledName();
            }

            void MeshAdvancedRule::Reflect(ReflectContext* context)
            {
                SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context);
                if (!serializeContext || serializeContext->FindClassData(MeshAdvancedRule::TYPEINFO_Uuid()))
                {
                    return;
                }

                serializeContext->Class<MeshAdvancedRule, DataTypes::IMeshAdvancedRule>()->Version(3)
                    ->Field("use32bitVertices", &MeshAdvancedRule::m_use32bitVertices)
                    ->Field("mergeMeshes", &MeshAdvancedRule::m_mergeMeshes)
                    ->Field("vertexColorStreamName", &MeshAdvancedRule::m_vertexColorStreamName)
                    ->Field("uvStreamName", &MeshAdvancedRule::m_uvStreamName);

                EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Class<MeshAdvancedRule>("Mesh (Advanced)", "Advanced export options for meshes")
                        ->ClassElement(Edit::ClassElements::EditorData, "")
                            ->Attribute("AutoExpand", true)
                            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, "")
                        ->DataElement(Edit::UIHandlers::Default, &MeshAdvancedRule::m_use32bitVertices, "32-bit Vertex Precision",
                            "Activating will use 32-bits of precision for the position of each vertex, increasing accuracy when the mesh is located far from its pivot.\n\n"
                            "Note that Sony Playstation platforms only supports 16-bit precision. For more details please see documentation.")
                        ->DataElement(Edit::UIHandlers::Default, &MeshAdvancedRule::m_mergeMeshes, "Merge Meshes", "Merge all meshes into one single mesh.")
                        ->DataElement("NodeListSelection", &MeshAdvancedRule::m_vertexColorStreamName, "Vertex Color Stream",
                            "Select a vertex color stream to enable Vertex Coloring or 'Disable' to turn Vertex Coloring off.\n\n"
                            "Vertex Coloring works in conjunction with materials. If a material was previously generated,\n"
                            "changing vertex coloring will require the material to be reset or the material editor to be used\n"
                            "to enable 'Vertex Coloring'.")
                            ->Attribute("ClassTypeIdFilter", DataTypes::IMeshVertexColorData::TYPEINFO_Uuid())
                            ->Attribute("DisabledOption", GetDisabledName())
                            ->Attribute("DefaultToDisabled", true)
                            ->Attribute("UseShortNames", true)
                        ->DataElement("NodeListSelection", &MeshAdvancedRule::m_uvStreamName, "UV Stream",
                            "Select a UV stream to enable Texturing or 'Disable' to turn Texturing off.")
                            ->Attribute("ClassTypeIdFilter", DataTypes::IMeshVertexUVData::TYPEINFO_Uuid())
                            ->Attribute("DisabledOption", GetDisabledName())
                            ->Attribute("DefaultToDisabled", true)
                            ->Attribute("UseShortNames", true);
                            
                }
            }
        } // SceneData
    } // SceneAPI
} // AZ
