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

#include "stdafx.h"
#include <IRCLog.h>
#include <ISceneConfig.h>
#include <ISystem.h>
#include <SceneCompiler.h>
#include <CgfExporter.h>
#include <ChrExporter.h>
#include <SkinExporter.h>

#include <AzCore/IO/SystemFile.h>
#include <AzCore/std/string/string.h>
#include <AzCore/Memory/MemoryComponent.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzToolsFramework/Debug/TraceContext.h>

#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Events/AssetImportRequest.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>

namespace AZ
{
    namespace RC
    {
        namespace SceneEvents = AZ::SceneAPI::Events;
        namespace SceneContainers = AZ::SceneAPI::Containers;

        void RCToolApplication::AddSystemComponents(AZ::Entity* systemEntity)
        {
            AZ::ComponentApplication::AddSystemComponents(systemEntity);
            EnsureComponentAdded<AZ::MemoryComponent>(systemEntity);
        }

        SceneCompiler::SceneCompiler(const AZStd::shared_ptr<ISceneConfig>& config)
            : m_config(config)
        {
        }

        void SceneCompiler::Release()
        {
            delete this;
        }

        void SceneCompiler::BeginProcessing(const IConfig* /*config*/)
        {
        }

        bool SceneCompiler::Process()
        {
            const char* gameFolder = m_context.pRC->GetSystemEnvironment()->pFileIO->GetAlias("@devassets@");
            AZ_TraceContext("Game folder", gameFolder);
            AZStd::string configPath;
            if (!AzFramework::StringFunc::Path::Join(gameFolder, "config/RCScene.xml", configPath))
            {
                AZ_TracePrintf(SceneAPI::Utilities::ErrorWindow, "Unable to composite path for system configuration file.\n");
                return false;
            }
            AZ_TraceContext("Config file", configPath);
            m_application.Start(configPath.c_str());

            AZ::SerializeContext* context;
            EBUS_EVENT_RESULT(context, AZ::ComponentApplicationBus, GetSerializeContext);
            if (!context)
            {
                AZ_TracePrintf(SceneAPI::Utilities::ErrorWindow, "No serialization context was created for the tools application.\n");
                return false;
            }
            m_config->ReflectModules(context);

            AZStd::string sourcePath = m_context.GetSourcePath().c_str();
            AZ_TraceContext("Source", sourcePath);
            AZStd::shared_ptr<SceneContainers::Scene> scene = 
                SceneEvents::AssetImportRequest::LoadScene(sourcePath, SceneEvents::AssetImportRequest::RequestingApplication::AssetProcessor);
            if (!scene)
            {
                AZ_TracePrintf(SceneAPI::Utilities::ErrorWindow, "Failed to load asset.\n");
                return false;
            }

            AZ_TraceContext("Manifest", scene->GetManifestFilename());
            if (scene->GetManifest().IsEmpty())
            {
                AZ_TracePrintf(SceneAPI::Utilities::LogWindow, "No manifest loaded and not enough information to create a default manifest.\n");
                return true;
            }

            bool result = ExportScene(*scene);
            if (!result)
            {
                AZ_TracePrintf(SceneAPI::Utilities::ErrorWindow, "Failed to convert and export scene\n");
            }

            m_application.Stop();

            return result;
        }

        void SceneCompiler::EndProcessing()
        {
        }

        IConvertContext* SceneCompiler::GetConvertContext()
        {
            return &m_context;
        }

        bool SceneCompiler::ExportScene(const AZ::SceneAPI::Containers::Scene& scene)
        {
            AZ_TraceContext("Output folder", m_context.GetOutputFolder().c_str());

            CgfExporter cgfProcessor(m_context.pRC->GetAssetWriter(), &m_context);
            ChrExporter chrProcessor(m_context.pRC->GetAssetWriter(), &m_context);
            SkinExporter skinProcessor(m_context.pRC->GetAssetWriter(), &m_context);

            SceneEvents::ProcessingResultCombiner result;
            result += SceneEvents::Process<SceneEvents::PreExportEventContext>(scene);
            result += SceneEvents::Process<SceneEvents::ExportEventContext>(m_context.GetOutputFolder().c_str(), scene);
            result += SceneEvents::Process<SceneEvents::PostExportEventContext>(m_context.GetOutputFolder().c_str());

            switch (result.GetResult())
            {
            case SceneEvents::ProcessingResult::Success:
                return true;
            case SceneEvents::ProcessingResult::Ignored:
                AZ_TracePrintf(SceneAPI::Utilities::WarningWindow, "Nothing found to convert and export.\n");
                return true;
            case SceneEvents::ProcessingResult::Failure:
                AZ_TracePrintf(SceneAPI::Utilities::ErrorWindow, "Failure during conversion and exporting.\n");
                return false;
            default:
                AZ_TracePrintf(SceneAPI::Utilities::ErrorWindow, 
                    "Unexpected result from conversion and exporting (%i).\n", result.GetResult());
                return false;
            }
        }
    }
}
