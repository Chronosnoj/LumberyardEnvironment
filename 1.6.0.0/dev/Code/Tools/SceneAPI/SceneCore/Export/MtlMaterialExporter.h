#pragma once

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

#include <AzCore/std/string/string.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Export/IExporter.h>

namespace AZ
{
    namespace rapidxml
    {
        template<class Ch>
        class xml_node;
        template<class Ch>
        class xml_attribute;
        template<class Ch>
        class xml_document;
    }

    namespace SceneAPI
    {
        namespace DataTypes
        {
            class IMaterialData;
            class IMeshGroup;
            class IMeshAdvancedRule;
        }

        namespace Export
        {
            class MtlMaterialExporter
                : public IExporter
            {
            public:
                virtual ~MtlMaterialExporter() = default;

                SCENE_CORE_API bool SaveMaterialGroup(const AZStd::string& fileName, const AZ::SceneAPI::DataTypes::IMeshGroup& meshGroup,
                    const SceneAPI::Containers::Scene& scene, const AZStd::string& textureRootPath);
                SCENE_CORE_API bool WriteToFile(const char* outputDirectory, const AZ::SceneAPI::Containers::Scene& scene) override;
                SCENE_CORE_API bool WriteToFile(const std::string& outputDirectory, const AZ::SceneAPI::Containers::Scene& scene) override;

            protected:
                struct MaterialInfo
                {
                    AZStd::shared_ptr<const AZ::SceneAPI::DataTypes::IMaterialData> m_materialData { nullptr };
                    bool m_usesVertexColoring { false };
                    bool m_physicalize { false };
                    AZStd::string m_name;
                };

                struct MaterialGroup
                {
                    AZStd::vector<MaterialInfo> m_materials;
                    bool m_enableMaterials;
                    bool m_removeMaterials;
                    bool m_updateMaterials;
                };

                SCENE_CORE_API bool BuildMaterialGroup(const AZ::SceneAPI::DataTypes::IMeshGroup& meshGroup, const SceneAPI::Containers::Scene& scene,
                    MaterialGroup& materialGroup) const;

                SCENE_CORE_API bool WriteMaterialFile(const char* fileName, MaterialGroup& materialGroup) const;

                SCENE_CORE_API bool UsesVertexColoring(const AZ::SceneAPI::DataTypes::IMeshGroup& meshGroup, const SceneAPI::Containers::Scene& scene,
                    SceneAPI::Containers::SceneGraph::HierarchyStorageConstIterator materialNode) const;

                SCENE_CORE_API const SceneAPI::DataTypes::IMeshAdvancedRule* FindMeshAdvancedRule(const SceneAPI::DataTypes::IMeshGroup* group) const;
                SCENE_CORE_API bool DoesMeshNodeHaveColorStreamChild(const SceneAPI::Containers::Scene& scene,
                    SceneAPI::Containers::SceneGraph::NodeIndex meshNode) const;

                static const AZStd::string m_extension;

                SceneAPI::Containers::SceneGraph::NodeIndex* m_root;
                AZStd::string m_filename;
                AZStd::string m_textureRootPath;
                MaterialGroup m_materialGroup;

                bool m_physicalize;
            };
        }
    }
}
