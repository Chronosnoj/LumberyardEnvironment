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

#include <SceneAPI/SceneCore/Events/ManifestMetaInfoBus.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Events
        {
            void ManifestMetaInfo::GetCategoryAssignments(CategoryMap& /*categories*/)
            {
            }

            void ManifestMetaInfo::GetIconPath(AZStd::string& /*iconPath*/, const DataTypes::IManifestObject* /*target*/)
            {
            }

            void ManifestMetaInfo::GetAvailableModifiers(ModifiersList& /*modifiers*/,
                const AZStd::shared_ptr<const Containers::Scene>& /*scene*/, const DataTypes::IManifestObject* /*target*/)
            {
            }

            void ManifestMetaInfo::InitializeObject(const Containers::Scene& /*scene*/,
                DataTypes::IManifestObject* /*target*/)
            {
            }
        } // Events
    } // SceneAPI
} // AZ
