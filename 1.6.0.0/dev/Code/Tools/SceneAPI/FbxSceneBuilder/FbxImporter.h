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
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <SceneAPI/FbxSceneBuilder/FbxSceneBuilderConfiguration.h>
#include <SceneAPI/SceneCore/Import/IImporter.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <SceneAPI/FbxSceneBuilder/FbxSceneSystem.h>

namespace AZ
{
    class Transform;

    namespace FbxSDKWrapper
    {
        class FbxNodeWrapper;
        class FbxSceneWrapper;
    }

    namespace SceneAPI
    {
        namespace Containers
        {
            class DataObject;
        }

        namespace FbxSceneImporter
        {
            class FbxImporter
                : public Import::IImporter
            {
            public:
                static const char* s_transformNodeName;
                FBX_SCENE_BUILDER_API FbxImporter();
                FBX_SCENE_BUILDER_API explicit FbxImporter(std::unique_ptr<FbxSDKWrapper::FbxSceneWrapper>&& specifiedScene);
                FBX_SCENE_BUILDER_API virtual ~FbxImporter();

                FBX_SCENE_BUILDER_API static const char* GetDefaultFileExtension();

                FBX_SCENE_BUILDER_API bool PopulateFromFile(const char* path, Containers::Scene& scene) const override;
                FBX_SCENE_BUILDER_API bool PopulateFromFile(const std::string& path, Containers::Scene& scene) const override;

            protected:
                FBX_SCENE_BUILDER_API bool ConvertFbxScene(Containers::Scene& scene) const;
                FBX_SCENE_BUILDER_API Containers::SceneGraph::NodeIndex AppendNodeToScene(
                    Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex parent, FbxSDKWrapper::FbxNodeWrapper& node) const;

                FBX_SCENE_BUILDER_API AZStd::shared_ptr<DataTypes::IGraphObject> BuildTransform(FbxSDKWrapper::FbxNodeWrapper& node) const;

                FBX_SCENE_BUILDER_API bool BuildMeshNode(Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex parent, FbxSDKWrapper::FbxNodeWrapper& node) const;
                FBX_SCENE_BUILDER_API bool BuildSkinNode(Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex parent, FbxSDKWrapper::FbxNodeWrapper& node) const;
                FBX_SCENE_BUILDER_API bool BuildBoneNode(Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex parent, FbxSDKWrapper::FbxNodeWrapper& node) const;
                FBX_SCENE_BUILDER_API bool BuildTransformNode(Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex parent, FbxSDKWrapper::FbxNodeWrapper& node, bool parentNodeUsed) const;
                FBX_SCENE_BUILDER_API bool BuildMaterialNode(Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex parent, FbxSDKWrapper::FbxNodeWrapper& node) const;

                std::unique_ptr<FbxSDKWrapper::FbxSceneWrapper> m_sceneWrapper;
                std::shared_ptr<FbxSceneSystem> m_sceneSystem;
            };
        } // FbxSceneImporter
    } // SceneAPI
} // AZ