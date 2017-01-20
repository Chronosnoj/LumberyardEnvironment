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
#include <AzCore/std/containers/unordered_map.h>
#include <GFxFramework/MaterialIO/IMaterial.h>
#include <SceneAPI/SceneCore/Events/CallProcessorConnector.h>

struct IConvertContext;

namespace AZ
{
    namespace SceneAPI
    {
        namespace DataTypes
        {
            class IMeshGroup;
        }
    }
    namespace RC
    {
        struct CgfMeshGroupExportContext;
        struct CgfContainerExportContext;
        struct CgfMeshNodeExportContext;

        class CgfMaterialExporter
            : public SceneAPI::Events::CallProcessorConnector
        {
        public:
            CgfMaterialExporter(IConvertContext* convertContext);
            ~CgfMaterialExporter() override = default;

            SceneAPI::Events::ProcessingResult Process(AZ::SceneAPI::Events::ICallContext* context) override;

        protected:
            void SetupGlobalMaterial(CgfContainerExportContext& context);
            void CreateSubMaterials(CgfContainerExportContext& context);
            void PatchSubmeshes(CgfContainerExportContext& context);
            void AssignCommonMaterial(CgfNodeExportContext& context);
            void PatchMaterials(CgfMeshNodeExportContext& context);
            void BuildRelocationTable(AZStd::vector<size_t>& table, CgfMeshNodeExportContext& context);
            void Reset();

            AZStd::shared_ptr<AZ::GFxFramework::IMaterialGroup> m_materialGroup;
            AZStd::unordered_map<int, AZStd::string> m_physMaterialNames;
            const SceneAPI::DataTypes::IMeshGroup* m_cachedMeshGroup;
            IConvertContext* m_convertContext;
        };
    } // RC
} // AZ
