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

#include <ChrExporter.h>
#include <Cry_Geo.h>
#include <ConvertContext.h>
#include <CGFContent.h>

#include <SceneAPI/SceneCore/DataTypes/Groups/ISkeletonGroup.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneManifest.h>
#include <SceneAPI/SceneCore/Containers/Views/FilterIterator.h>
#include <SceneAPI/SceneCore/Containers/Views/PairIterator.h>

#include <RC/ResourceCompilerScene/Chr/ChrExportContexts.h>
#include <RC/ResourceCompilerScene/Chr/ChrSkeletonExporter.h>
#include <RC/ResourceCompilerScene/Chr/ChrSkeletonGroupExporter.h>

namespace AZ
{
    namespace RC
    {
        namespace SceneViews = AZ::SceneAPI::Containers::Views;
        namespace SceneDataTypes = AZ::SceneAPI::DataTypes;
        namespace SceneEvents = AZ::SceneAPI::Events;

        ChrExporter::ChrExporter(IAssetWriter* writer, IConvertContext* convertContext)
            : CallProcessorConnector()
            , m_assetWriter(writer)
            , m_convertContext(convertContext)
        {
            AZ_Assert(m_assetWriter != nullptr, "Invalid IAssetWriter initialization.");
        }

        ChrExporter::~ChrExporter()
        {
            m_assetWriter = nullptr;
        }

        SceneEvents::ProcessingResult ChrExporter::Process(SceneEvents::ICallContext* context)
        {
            SceneEvents::ExportEventContext* exportContext = azrtti_cast<SceneEvents::ExportEventContext*>(context);
            if (exportContext)
            {
                ChrSkeletonGroupExporter skeletonGroupExporter(m_assetWriter, m_convertContext);
                ChrSkeletonExporter skeletonExporter;

                const AZ::SceneAPI::Containers::SceneManifest& manifest = exportContext->GetScene().GetManifest();
                auto values = SceneViews::MakePairView(manifest.GetNameStorage(), manifest.GetValueStorage());
                auto view = SceneViews::MakeFilterView(values,
                    [](decltype(*values.begin())& instance) -> bool
                    {
                        return instance.second && instance.second->RTTI_IsTypeOf(SceneDataTypes::ISkeletonGroup::TYPEINFO_Uuid());
                    }
                );

                SceneEvents::ProcessingResultCombiner result;
                for (auto& it : view)
                {
                    const SceneDataTypes::ISkeletonGroup* skeletonGroup = azrtti_cast<const SceneDataTypes::ISkeletonGroup*>(it.second.get());
                    AZ_Assert(skeletonGroup != nullptr, "Failed to cast manifest object to skeleton group.");

                    result += SceneEvents::Process<ChrSkeletonGroupExportContext>(*exportContext, it.first, *skeletonGroup, Phase::Construction);
                    result += SceneEvents::Process<ChrSkeletonGroupExportContext>(*exportContext, it.first, *skeletonGroup, Phase::Filling);
                    result += SceneEvents::Process<ChrSkeletonGroupExportContext>(*exportContext, it.first, *skeletonGroup, Phase::Finalizing);
                }
                return result.GetResult();
            }
            else
            {
                return SceneEvents::ProcessingResult::Ignored;
            }
        }
    }
}