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

#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <SceneAPI/FbxSceneBuilder/FbxSceneBuilderConfiguration.h>

namespace AZ
{
    namespace SceneAPI
    {
        class FbxSceneSystem;

        namespace FbxSceneImporter
        {
            // FbxBoneBuilder builds the SceneGraph data BoneData from FbxNodeWrapper data
            //      It converts name, world transform for a bone node
            class FbxBoneBuilder
            {
            public:
                FBX_SCENE_BUILDER_API FbxBoneBuilder(FbxSDKWrapper::FbxNodeWrapper& fbxNode, Containers::SceneGraph& graph,
                    Containers::SceneGraph::NodeIndex target, const std::shared_ptr<FbxSceneSystem>& sceneSystem);
                FBX_SCENE_BUILDER_API void Reset(FbxSDKWrapper::FbxNodeWrapper& fbxNode, Containers::SceneGraph& graph,
                    Containers::SceneGraph::NodeIndex target, const std::shared_ptr<FbxSceneSystem>& sceneSystem);

                FBX_SCENE_BUILDER_API bool BuildBone();

            protected:
                FbxSDKWrapper::FbxNodeWrapper& m_fbxNode;
                Containers::SceneGraph& m_graph;
                Containers::SceneGraph::NodeIndex m_targetNode;
                std::shared_ptr<FbxSceneSystem> m_sceneSystem;
            };
        }
    }
}