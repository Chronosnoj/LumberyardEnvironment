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

#include <SceneAPI/SceneCore/Events/ExportEventContext.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Events
        {
            /////////////
            // PreExportEventContext
            /////////////

            PreExportEventContext::PreExportEventContext(const Containers::Scene& scene)
                : m_scene(scene)
            {
            }

            const Containers::Scene& PreExportEventContext::GetScene() const
            {
                return m_scene;
            }

            /////////////
            // ExportEventContext
            /////////////

            ExportEventContext::ExportEventContext(const char* outputDirectory, const Containers::Scene& scene)
                : m_outputDirectory(outputDirectory)
                , m_scene(scene)
            {
            }

            ExportEventContext::ExportEventContext(const AZStd::string& outputDirectory, const Containers::Scene& scene)
                : m_outputDirectory(outputDirectory)
                , m_scene(scene)
            {
            }

            ExportEventContext::ExportEventContext(AZStd::string&& outputDirectory, const Containers::Scene& scene)
                : m_outputDirectory(AZStd::move(outputDirectory))
                , m_scene(scene)
            {
            }

            const AZStd::string& ExportEventContext::GetOutputDirectory() const
            {
                return m_outputDirectory;
            }

            const Containers::Scene& ExportEventContext::GetScene() const
            {
                return m_scene;
            }

            /////////////
            // PostExportEventContext
            /////////////

            PostExportEventContext::PostExportEventContext(const char* outputDirectory)
                : m_outputDirectory(outputDirectory)
            {
            }

            PostExportEventContext::PostExportEventContext(const AZStd::string& outputDirectory)
                : m_outputDirectory(outputDirectory)
            {
            }

            PostExportEventContext::PostExportEventContext(AZStd::string&& outputDirectory)
                : m_outputDirectory(AZStd::move(outputDirectory))
            {
            }

            const AZStd::string PostExportEventContext::GetOutputDirectory() const
            {
                return m_outputDirectory;
            }
        } // Events
    } // SceneAPI
} // AZ