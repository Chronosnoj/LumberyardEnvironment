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

#include <memory>
#include <string>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            class Scene;
        }

        namespace Import
        {
            // Interface for classes that import (parts of) a scene.
            class IImporter
            {
            public:
                virtual ~IImporter() = 0;

                virtual bool PopulateFromFile(const char* path, AZ::SceneAPI::Containers::Scene& scene) const = 0;
                virtual bool PopulateFromFile(const std::string& path, AZ::SceneAPI::Containers::Scene& scene) const = 0;
            };

            inline IImporter::~IImporter()
            {
            }
        } // Import
    } // SceneAPI
} // AZ