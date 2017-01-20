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
#include <SceneAPI/SceneCore/Events/CallProcessorBinder.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>

struct CryBoneDescData;

namespace AZ
{
    class Transform;

    namespace RC
    {
        struct SkeletonExportContext;

        class ChrSkeletonExporter
            : public SceneAPI::Events::CallProcessorBinder
        {
        public:
            ChrSkeletonExporter();
            ~ChrSkeletonExporter() override = default;

            SceneAPI::Events::ProcessingResult CreateBoneDescData(SkeletonExportContext& context);
            SceneAPI::Events::ProcessingResult CreateBoneEntityData(SkeletonExportContext& context);

        protected:
            SceneAPI::Events::ProcessingResult CreateBoneDescDataRecursion(SkeletonExportContext& context, AZ::SceneAPI::Containers::SceneGraph::NodeIndex index,
                const AZ::SceneAPI::Containers::SceneGraph& graph);
            SceneAPI::Events::ProcessingResult CreateBoneEntityDataRecursion(SkeletonExportContext& context, AZ::SceneAPI::Containers::SceneGraph::NodeIndex index,
                const AZ::SceneAPI::Containers::SceneGraph& graph, const AZStd::string& rootBoneName);
            void SetBoneName(const AZStd::string& name, CryBoneDescData& boneDesc) const;

            AZStd::unordered_map<AZStd::string, int> m_nodeNameBoneIndexMap;
        };
    }
}