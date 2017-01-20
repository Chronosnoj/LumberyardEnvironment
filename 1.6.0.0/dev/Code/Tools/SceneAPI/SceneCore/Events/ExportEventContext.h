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
#include <AzCore/std/string/string.h>
#include <SceneAPI/SceneCore/Events/CallProcessorBus.h>
#include <SceneAPI/SceneCore/SceneCoreConfiguration.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            class Scene;
        }

        namespace Events
        {
            // Signals an export of the contained scene is about to happen.
            class PreExportEventContext
                : public ICallContext
            {
            public:
                AZ_RTTI(PreExportEventContext, "{6B303E35-8BF0-43DD-9AD7-7D7F24F18F37}", ICallContext);
                ~PreExportEventContext() override = default;
                SCENE_CORE_API PreExportEventContext(const Containers::Scene& scene);
                SCENE_CORE_API const Containers::Scene& GetScene() const;

            private:
                const Containers::Scene& m_scene;
            };

            // Signals the scene that the contained scene needs to be exported to the specified directory.
            class ExportEventContext
                : public ICallContext
            {
            public:
                AZ_RTTI(ExportEventContext, "{ECE4A3BD-CE48-4B17-9609-6D97F8A887D3}", ICallContext);
                ~ExportEventContext() override = default;

                SCENE_CORE_API ExportEventContext(const char* outputDirectory, const Containers::Scene& scene);
                SCENE_CORE_API ExportEventContext(const AZStd::string& outputDirectory, const Containers::Scene& scene);
                SCENE_CORE_API ExportEventContext(AZStd::string&& outputDirectory, const Containers::Scene& scene);

                SCENE_CORE_API const AZStd::string& GetOutputDirectory() const;
                SCENE_CORE_API const Containers::Scene& GetScene() const;

            private:
                AZStd::string m_outputDirectory;
                const Containers::Scene& m_scene;
            };

            // Signals that an export has completed and written (if successful) to the specified directory.
            class PostExportEventContext
                : public ICallContext
            {
            public:
                AZ_RTTI(PostExportEventContext, "{92E0AD59-62CA-45E3-BB73-5659D10FF0DE}", ICallContext);
                ~PostExportEventContext() override = default;

                SCENE_CORE_API PostExportEventContext(const char* outputDirectory);
                SCENE_CORE_API PostExportEventContext(const AZStd::string& outputDirectory);
                SCENE_CORE_API PostExportEventContext(AZStd::string&& outputDirectory);

                SCENE_CORE_API const AZStd::string GetOutputDirectory() const;

            private:
                AZStd::string m_outputDirectory;
            };
        } // Events
    } // SceneAPI
} // AZ