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

#include <AzCore/RTTI/RTTI.h>
#include <SceneAPI/SceneCore/Events/CallProcessorBus.h>
#include <SceneAPI/SceneCore/Events/ExportEventContext.h>
#include <ExportContextGlobal.h>

struct CSkinningInfo;

namespace AZ
{
    namespace SceneAPI
    {
        namespace DataTypes
        {
            class ISkeletonGroup;
        }
    }

    namespace RC
    {
        // Called to export a specific Skeleton Group
        struct ChrSkeletonGroupExportContext
            : public SceneAPI::Events::ICallContext
        {
            AZ_RTTI(ChrSkeletonGroupExportContext, "{294BB98B-DE9D-4B03-B219-A8A94657E81E}", SceneAPI::Events::ICallContext);

            ChrSkeletonGroupExportContext(SceneAPI::Events::ExportEventContext& parent, const AZStd::string& groupName,
                const SceneAPI::DataTypes::ISkeletonGroup& group, Phase phase);
            ChrSkeletonGroupExportContext(const SceneAPI::Containers::Scene& scene, const AZStd::string& outputDirectory,
                const AZStd::string& groupName, const SceneAPI::DataTypes::ISkeletonGroup& group, Phase phase);
            ChrSkeletonGroupExportContext(const ChrSkeletonGroupExportContext& copyContent, Phase phase);
            ChrSkeletonGroupExportContext(const ChrSkeletonGroupExportContext& copyContent) = delete;
            ~ChrSkeletonGroupExportContext() override = default;

            ChrSkeletonGroupExportContext& operator=(const ChrSkeletonGroupExportContext& other) = delete;

            const SceneAPI::Containers::Scene& m_scene;
            const AZStd::string& m_outputDirectory;
            const AZStd::string& m_groupName;
            const SceneAPI::DataTypes::ISkeletonGroup& m_group;
            const Phase m_phase;
        };

        struct SkeletonExportContext
            : public SceneAPI::Events::ICallContext
        {
            AZ_RTTI(SkeletonExportContext, "{40512752-150F-4BAF-BC4E-01016DAE5088}", SceneAPI::Events::ICallContext);

            SkeletonExportContext(const SceneAPI::Containers::Scene& scene, const AZStd::string& rootBoneName, CSkinningInfo& skinningInfo,
                Phase phase);
            SkeletonExportContext(const SkeletonExportContext& copyContext, Phase phase);
            SkeletonExportContext(const SkeletonExportContext& copyContext) = delete;
            ~SkeletonExportContext() override = default;

            SkeletonExportContext& operator=(const SkeletonExportContext& other) = delete;

            const SceneAPI::Containers::Scene& m_scene;
            const AZStd::string& m_rootBoneName;
            CSkinningInfo& m_skinningInfo;
            const Phase m_phase;
        };
    }
}