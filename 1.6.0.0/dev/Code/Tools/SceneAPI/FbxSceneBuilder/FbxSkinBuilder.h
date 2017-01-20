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

#include <SceneAPI/FbxSceneBuilder/FbxMeshBuilder.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace FbxSceneImporter
        {
            class FbxSkinBuilder
                : public FbxMeshBuilder
            {
            public:
                FBX_SCENE_BUILDER_API FbxSkinBuilder(const std::shared_ptr<FbxSDKWrapper::FbxMeshWrapper>& fbxMesh, Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex target, const std::shared_ptr<FbxSceneSystem>& sceneSystem);
                FBX_SCENE_BUILDER_API void Reset(const std::shared_ptr<FbxSDKWrapper::FbxMeshWrapper>& fbxMesh, Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex target, const std::shared_ptr<FbxSceneSystem>& sceneSystem);

                FBX_SCENE_BUILDER_API bool BuildSkin();

            protected:
                bool ProcessSkinDeformers();
                AZStd::shared_ptr<DataTypes::IGraphObject> FbxSkinBuilder::BuildSkinWeightData(int index);
            };
        }
    }
}