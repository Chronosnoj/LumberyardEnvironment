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

#include <CgfExporter.h>
#include <Cry_Geo.h>
#include <ConvertContext.h>
#include <CGFContent.h>

#include <AzToolsFramework/Debug/TraceContext.h>

#include <SceneAPI/SceneCore/DataTypes/Groups/IMeshGroup.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneManifest.h>
#include <SceneAPI/SceneCore/Containers/Views/FilterIterator.h>
#include <SceneAPI/SceneCore/Containers/Views/PairIterator.h>

#include <RC/ResourceCompilerScene/Cgf/CgfMeshExporter.h>
#include <RC/ResourceCompilerScene/Cgf/CgfExportContexts.h>
#include <RC/ResourceCompilerScene/Cgf/CgfMaterialExporter.h>
#include <RC/ResourceCompilerScene/Cgf/CgfMeshGroupExporter.h>
#include <RC/ResourceCompilerScene/Cgf/CgfContainerSettingsExporter.h>
#include <RC/ResourceCompilerScene/Cgf/CgfWorldMatrixExporter.h>
#include <RC/ResourceCompilerScene/Cgf/CgfColorStreamExporter.h>
#include <RC/ResourceCompilerScene/Cgf/CgfUVStreamExporter.h>

namespace AZ
{
    namespace RC
    {
        namespace SceneViews = AZ::SceneAPI::Containers::Views;
        namespace SceneDataTypes = AZ::SceneAPI::DataTypes;
        namespace SceneEvents = AZ::SceneAPI::Events;

        CgfExporter::CgfExporter(IAssetWriter* writer, IConvertContext* convertContext)
            : CallProcessorConnector()
            , m_assetWriter(writer)
            , m_convertContext(convertContext)
        {
            AZ_Assert(m_assetWriter != nullptr, "Invalid IAssetWriter initialization.");
        }

        CgfExporter::~CgfExporter()
        {
            m_assetWriter = nullptr;
        }

        SceneEvents::ProcessingResult CgfExporter::Process(SceneEvents::ICallContext* context)
        {
            SceneEvents::ExportEventContext* exportContext = azrtti_cast<SceneEvents::ExportEventContext*>(context);
            if (exportContext)
            {
                AZ_TraceContext("CGF Scene Name", exportContext->GetScene().GetName());
                AZ_TraceContext("Source File", exportContext->GetScene().GetSourceFilename());
                AZ_TraceContext("Output Path", exportContext->GetOutputDirectory());

                // Register additional processors. Will be automatically unregistered when leaving scope.
                CgfMeshGroupExporter meshGroupExporter(m_assetWriter);
                CgfContainerSettingsExporter meshAdvancedExporter;
                CgfMaterialExporter materialExporter(m_convertContext);
                CgfWorldMatrixExporter worldMatrixExporter;
                CgfMeshExporter meshExporter;
                CgfColorStreamExporter colorStreamExporter;
                CgfUVStreamExporter uvStreamExporter;

                const AZ::SceneAPI::Containers::SceneManifest& manifest = exportContext->GetScene().GetManifest();
                auto values = SceneViews::MakePairView(manifest.GetNameStorage(), manifest.GetValueStorage());
                auto view = SceneViews::MakeFilterView(values,
                        [](decltype(*values.begin())& instance) -> bool
                        {
                            return instance.second && instance.second->RTTI_IsTypeOf(SceneDataTypes::IMeshGroup::TYPEINFO_Uuid());
                        }
                        );

                SceneEvents::ProcessingResultCombiner result;
                for (auto& it : view)
                {
                    const SceneDataTypes::IMeshGroup* meshGroup = azrtti_cast<const SceneDataTypes::IMeshGroup*>(it.second.get());
                    AZ_Assert(meshGroup != nullptr, "Failed to cast manifest object to mesh group.");

                    result += SceneEvents::Process<CgfMeshGroupExportContext>(*exportContext, it.first, *meshGroup, Phase::Construction);
                    result += SceneEvents::Process<CgfMeshGroupExportContext>(*exportContext, it.first, *meshGroup, Phase::Filling);
                    result += SceneEvents::Process<CgfMeshGroupExportContext>(*exportContext, it.first, *meshGroup, Phase::Finalizing);
                }
                return result.GetResult();
            }
            else
            {
                return SceneEvents::ProcessingResult::Ignored;
            }
        }
    } // RC
} // AZ