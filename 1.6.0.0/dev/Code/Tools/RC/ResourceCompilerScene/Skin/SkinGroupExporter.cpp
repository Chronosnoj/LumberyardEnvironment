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

#include <Cry_Geo.h> // Needed for CGFContent.h
#include <CGFContent.h>
#include <RC/ResourceCompilerScene/Chr/ChrExportContexts.h>
#include <RC/ResourceCompilerScene/Skin/SkinExportContexts.h>
#include <RC/ResourceCompilerScene/Skin/SkinGroupExporter.h>
#include <SceneAPI/SceneCore/Utilities/FileUtilities.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>

namespace AZ
{
    namespace RC
    {
        namespace SceneEvents = AZ::SceneAPI::Events;
        namespace SceneUtil = AZ::SceneAPI::Utilities;
        namespace SceneContainer = AZ::SceneAPI::Containers;
        namespace SceneDataTypes = AZ::SceneAPI::DataTypes;

        const AZStd::string SkinGroupExporter::fileExtension = "skin";

        SkinGroupExporter::SkinGroupExporter(IAssetWriter* writer, IConvertContext* convertContext)
            : CallProcessorBinder()
            , m_assetWriter(writer)
            , m_convertContext(convertContext)
        {
            BindToCall(&SkinGroupExporter::ProcessContext);
        }

        SceneEvents::ProcessingResult SkinGroupExporter::ProcessContext(SkinGroupExportContext& context) const
        {
            if (context.m_phase != Phase::Filling)
            {
                return SceneEvents::ProcessingResult::Ignored;
            }

            AZStd::string filename = SceneUtil::FileUtilities::CreateOutputFileName(context.m_groupName, context.m_outputDirectory, fileExtension);
            if (filename.empty() || !SceneUtil::FileUtilities::EnsureTargetFolderExists(filename))
            {
                return SceneEvents::ProcessingResult::Failure;
            }

            SceneEvents::ProcessingResultCombiner result;

            CContentCGF cgfContent(filename.c_str());
            ConfigureSkinContent(cgfContent);

            // TODO: detectedRootBoneName will be determined by looking at the bones that are skinned to the skin meshes
            AZStd::string detectedRootBoneName = "RootNode.joint1";
            result += SceneEvents::Process<SkeletonExportContext>(context.m_scene, detectedRootBoneName, *cgfContent.GetSkinningInfo(), Phase::Construction);
            result += SceneEvents::Process<SkeletonExportContext>(context.m_scene, detectedRootBoneName, *cgfContent.GetSkinningInfo(), Phase::Filling);
            result += SceneEvents::Process<SkeletonExportContext>(context.m_scene, detectedRootBoneName, *cgfContent.GetSkinningInfo(), Phase::Finalizing);

            if (m_assetWriter)
            {
                if (!m_assetWriter->WriteSKIN(&cgfContent, m_convertContext))
                {
                    result += SceneEvents::ProcessingResult::Failure;
                }
            }
            return result.GetResult();
        }

        void SkinGroupExporter::ConfigureSkinContent(CContentCGF& content) const
        {
            CExportInfoCGF* exportInfo = content.GetExportInfo();

            exportInfo->bMergeAllNodes = true;
            exportInfo->bUseCustomNormals = false;
            exportInfo->bCompiledCGF = false;
            exportInfo->bHavePhysicsProxy = false;
            exportInfo->bHaveAutoLods = false;
            exportInfo->bNoMesh = true;
            exportInfo->b8WeightsPerVertex = false;
            exportInfo->bWantF32Vertices = false;
            exportInfo->authorToolVersion = 1;
        }
    }
}