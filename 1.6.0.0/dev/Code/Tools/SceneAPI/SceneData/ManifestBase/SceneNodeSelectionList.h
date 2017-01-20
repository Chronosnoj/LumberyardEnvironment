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


#include <AzCore/JSON/document.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ISceneNodeSelectionList.h>
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
            class IManifestObject;
        }
        namespace SceneData
        {
            class SceneNodeSelectionList
                : public DataTypes::ISceneNodeSelectionList
            {
            public:
                AZ_RTTI(SceneNodeSelectionList, "{D0CE66CE-1BAD-42F5-86ED-3923573B3A02}", ISceneNodeSelectionList);
                ~SceneNodeSelectionList() = default;

                SCENE_DATA_API size_t GetSelectedNodeCount() const override;
                SCENE_DATA_API const AZStd::string& GetSelectedNode(size_t index) const override;
                SCENE_DATA_API size_t AddSelectedNode(const AZStd::string& name) override;
                SCENE_DATA_API size_t AddSelectedNode(AZStd::string&& name) override;
                SCENE_DATA_API void RemoveSelectedNode(size_t index) override;
                SCENE_DATA_API void RemoveSelectedNode(const AZStd::string& name) override;
                SCENE_DATA_API void ClearSelectedNodes() override;

                SCENE_DATA_API size_t GetUnselectedNodeCount() const override;
                SCENE_DATA_API const AZStd::string& GetUnselectedNode(size_t index) const override;
                SCENE_DATA_API void ClearUnselectedNodes() override;

                SCENE_DATA_API AZStd::unique_ptr<DataTypes::ISceneNodeSelectionList> Copy() const override;
                SCENE_DATA_API void CopyTo(ISceneNodeSelectionList& other) const override;

                static void Reflect(AZ::ReflectContext* context);
                
            protected:
                AZStd::vector<AZStd::string> m_selectedNodes;
                AZStd::vector<AZStd::string> m_unselectedNodes;
            };
        } // SceneData
    } // SceneAPI
} // AZ
