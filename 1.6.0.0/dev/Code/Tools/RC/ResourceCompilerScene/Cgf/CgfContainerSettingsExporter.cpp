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

#include <Cry_Geo.h> // Needed for CGFContent.h
#include <CGFContent.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/DataTypes/DataTypeUtilities.h>
#include <SceneAPI/SceneCore/DataTypes/Groups/IMeshGroup.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IMeshAdvancedRule.h>
#include <RC/ResourceCompilerScene/Cgf/CgfExportContexts.h>
#include <RC/ResourceCompilerScene/Cgf/CgfContainerSettingsExporter.h>

namespace AZ
{
    namespace RC
    {
        namespace SceneEvents = AZ::SceneAPI::Events;
        namespace SceneDataTypes = AZ::SceneAPI::DataTypes;

        CgfContainerSettingsExporter::CgfContainerSettingsExporter()
            : CallProcessorBinder()
        {
            BindToCall(&CgfContainerSettingsExporter::ProcessContext);
        }

        SceneEvents::ProcessingResult CgfContainerSettingsExporter::ProcessContext(CgfContainerExportContext& context) const
            {
            if (context.m_phase != Phase::Construction)
                {
                return SceneEvents::ProcessingResult::Ignored;
                }

            AZStd::shared_ptr<const SceneDataTypes::IMeshAdvancedRule> advancedRule =
                SceneDataTypes::Utilities::FindRule<SceneDataTypes::IMeshAdvancedRule>(context.m_group);
            if (advancedRule)
            {
                context.m_container.GetExportInfo()->bWantF32Vertices = advancedRule->Use32bitVertices();
                context.m_container.GetExportInfo()->bMergeAllNodes = advancedRule->MergeMeshes();
                return SceneEvents::ProcessingResult::Success;
            }
            else
            {
                return SceneEvents::ProcessingResult::Ignored;
            }
        }
    } // RC
} // AZ