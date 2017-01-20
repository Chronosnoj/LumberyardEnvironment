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

#include <SkinExporter.h>
#include <Cry_Geo.h>
#include <ConvertContext.h>
#include <CGFContent.h>

#include <SceneAPI/SceneCore/DataTypes/Groups/ISkinGroup.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneManifest.h>
#include <SceneAPI/SceneCore/Containers/Views/FilterIterator.h>
#include <SceneAPI/SceneCore/Containers/Views/PairIterator.h>

#include <RC/ResourceCompilerScene/Skin/SkinExportContexts.h>
#include <RC/ResourceCompilerScene/Skin/SkinGroupExporter.h>
#include <RC/ResourceCompilerScene/Chr/ChrSkeletonExporter.h>

namespace AZ
{
    namespace RC
    {
        namespace SceneViews = AZ::SceneAPI::Containers::Views;
        namespace SceneDataTypes = AZ::SceneAPI::DataTypes;
        namespace SceneEvents = AZ::SceneAPI::Events;

        SkinExporter::SkinExporter(IAssetWriter* writer, IConvertContext* convertContext)
            : CallProcessorConnector()
            , m_assetWriter(writer)
            , m_convertContext(convertContext)
        {
            AZ_Assert(m_assetWriter != nullptr, "Invalid IAssetWriter initialization.");
        }

        SkinExporter::~SkinExporter()
        {
            m_assetWriter = nullptr;
        }

        SceneEvents::ProcessingResult SkinExporter::Process(SceneEvents::ICallContext* context)
        {
            SceneEvents::ExportEventContext* exportContext = azrtti_cast<SceneEvents::ExportEventContext*>(context);
            if (exportContext)
            {
                SkinGroupExporter skinGroupExporter(m_assetWriter, m_convertContext);
                ChrSkeletonExporter skeletonExporter;

                const AZ::SceneAPI::Containers::SceneManifest& manifest = exportContext->GetScene().GetManifest();
                auto values = SceneViews::MakePairView(manifest.GetNameStorage(), manifest.GetValueStorage());
                auto view = SceneViews::MakeFilterView(values,
                    [](decltype(*values.begin())& instance) -> bool
                    {
                        return instance.second && instance.second->RTTI_IsTypeOf(SceneDataTypes::ISkinGroup::TYPEINFO_Uuid());
                    }
                );

                SceneEvents::ProcessingResultCombiner result;
                for (auto& it : view)
                {
                    const SceneDataTypes::ISkinGroup* skinGroup = azrtti_cast<const SceneDataTypes::ISkinGroup*>(it.second.get());
                    AZ_Assert(skinGroup != nullptr, "Failed to cast manifest object to skin group.");

                    result += SceneEvents::Process<SkinGroupExportContext>(*exportContext, it.first, *skinGroup, Phase::Construction);
                    result += SceneEvents::Process<SkinGroupExportContext>(*exportContext, it.first, *skinGroup, Phase::Filling);
                    result += SceneEvents::Process<SkinGroupExportContext>(*exportContext, it.first, *skinGroup, Phase::Finalizing);
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