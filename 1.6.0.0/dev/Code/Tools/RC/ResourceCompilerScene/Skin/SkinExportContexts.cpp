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

#include <RC/ResourceCompilerScene/Skin/SkinExportContexts.h>

namespace AZ
{
    namespace RC
    {
        SkinGroupExportContext::SkinGroupExportContext(SceneAPI::Events::ExportEventContext& parent, const AZStd::string& groupName,
            const SceneAPI::DataTypes::ISkinGroup& group, Phase phase)
            : m_scene(parent.GetScene())
            , m_outputDirectory(parent.GetOutputDirectory())
            , m_groupName(groupName)
            , m_group(group)
            , m_phase(phase)
        {
        }

        SkinGroupExportContext::SkinGroupExportContext(const SceneAPI::Containers::Scene& scene, const AZStd::string& outputDirectory,
            const AZStd::string& groupName, const SceneAPI::DataTypes::ISkinGroup& group, Phase phase)
            : m_scene(scene)
            , m_outputDirectory(outputDirectory)
            , m_groupName(groupName)
            , m_group(group)
            , m_phase(phase)
        {
        }

        SkinGroupExportContext::SkinGroupExportContext(const SkinGroupExportContext& copyContext, Phase phase)
            : m_scene(copyContext.m_scene)
            , m_outputDirectory(copyContext.m_outputDirectory)
            , m_groupName(copyContext.m_groupName)
            , m_group(copyContext.m_group)
            , m_phase(phase)
        {
        }
    }
}