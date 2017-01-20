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

#include <AzCore/Memory/Memory.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <SceneAPI/SceneCore/DataTypes/Groups/IMeshGroup.h>
#include <SceneAPI/SceneData/ManifestBase/SceneNodeSelectionList.h>
#include <SceneAPI/SceneData/SceneDataConfiguration.h>

namespace AZ
{
    class ReflectContext;

    namespace SceneAPI
    {
        namespace Containers
        {
            class Scene;
        }
        namespace DataTypes
        {
            IManifestObject;
            IRule;
        }
        namespace SceneData
        {
            class MeshGroup
                : public DataTypes::IMeshGroup
            {
            public:
                AZ_RTTI(MeshGroup, "{07B356B7-3635-40B5-878A-FAC4EFD5AD86}", DataTypes::IMeshGroup);
                AZ_CLASS_ALLOCATOR(MeshGroup, SystemAllocator, 0)

                SCENE_DATA_API ~MeshGroup() override = default;

                SCENE_DATA_API size_t GetRuleCount() const override;
                SCENE_DATA_API AZStd::shared_ptr<DataTypes::IRule> GetRule(size_t index) const override;
                SCENE_DATA_API void AddRule(const AZStd::shared_ptr<DataTypes::IRule>& rule) override;
                SCENE_DATA_API void AddRule(AZStd::shared_ptr<DataTypes::IRule>&& rule) override;
                SCENE_DATA_API void RemoveRule(size_t index) override;
                SCENE_DATA_API void RemoveRule(const AZStd::shared_ptr<DataTypes::IRule>& rule) override;

                SCENE_DATA_API DataTypes::ISceneNodeSelectionList& GetSceneNodeSelectionList() override;
                SCENE_DATA_API const DataTypes::ISceneNodeSelectionList& GetSceneNodeSelectionList() const override;

                static void Reflect(AZ::ReflectContext* context);

            protected:
                AZStd::vector<AZStd::shared_ptr<DataTypes::IRule> > m_rules;
                SceneNodeSelectionList m_nodeSelectionList;
            };
        } // SceneData
    } // SceneAPI
} // AZ
