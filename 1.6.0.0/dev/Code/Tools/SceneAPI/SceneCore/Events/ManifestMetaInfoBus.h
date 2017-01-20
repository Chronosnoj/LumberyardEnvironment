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

#include <AzCore/ebus/ebus.h>
#include <AzCore/Math/Uuid.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/unordered_map.h>
#include <SceneAPI/SceneCore/SceneCoreConfiguration.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            class Scene;
        }
        namespace DataTypes
        {
            class IManifestObject;
        }
        namespace Events
        {
            class ManifestMetaInfo
                : public AZ::EBusTraits
            {
            public:
                static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;

                using CategoryMap = AZStd::unordered_multimap<AZStd::string, AZ::Uuid>;
                using ModifiersList = AZStd::vector<AZ::Uuid>;
                
                virtual ~ManifestMetaInfo() = 0;

                // Gets a list of all the categories and the class identifiers that are listed for that category.
                SCENE_CORE_API virtual void GetCategoryAssignments(CategoryMap& categories);

                // Gets the path to the icon associated with the given object.
                SCENE_CORE_API virtual void GetIconPath(AZStd::string& iconPath, const DataTypes::IManifestObject* target);

                // Gets a list of a the modifiers (such as rules for  groups) that the target accepts.
                //      Note that updates to the target may change what modifiers can be accepted. For instance
                //      if a group only accepts a single rule of a particular type, call this functions a second time
                //      will not include the uuid of that rule.
                SCENE_CORE_API virtual void GetAvailableModifiers(ModifiersList& modifiers,
                    const AZStd::shared_ptr<const Containers::Scene>& scene, const DataTypes::IManifestObject* target);

                // Initialized the given manifest object based on the scene. Depending on what other entries have been added
                //      to the manifest, an implementation of this function may decided that certain values should or shouldn't
                //      be added, such as not adding meshes to a group that already belong to another group.
                SCENE_CORE_API virtual void InitializeObject(const Containers::Scene& scene, DataTypes::IManifestObject* target);
            };

            inline ManifestMetaInfo::~ManifestMetaInfo() = default;

            using ManifestMetaInfoBus = AZ::EBus<ManifestMetaInfo>;
        } // Events
    } // SceneAPI
} // AZ
