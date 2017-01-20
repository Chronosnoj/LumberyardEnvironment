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
#include <AzCore/std/string/string.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <SceneAPI/FbxSceneBuilder/FbxSceneBuilderConfiguration.h>

namespace AZ
{
    namespace FbxSDKWrapper
    {
        class FbxNodeWrapper;
        class FbxMeshWrapper;
    }

    namespace SceneData
    {
        namespace GraphData
        {
            class MeshData;
        }
    }

    namespace SceneAPI
    {
        namespace DataTypes
        {
            class IGraphObject;
        }

        class FbxSceneSystem;

        namespace FbxSceneImporter
        {
            // FbxMeshBuilder builds the SceneGraph data MeshData from FbxMeshWrapper data
            //      It converts positions, normals, texture coordinates & vertex colors and faces
            //      on a per-vertex basis
            class FbxMeshBuilder
            {
            public:
                FBX_SCENE_BUILDER_API FbxMeshBuilder(const std::shared_ptr<FbxSDKWrapper::FbxMeshWrapper>& fbxMesh, Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex target, const std::shared_ptr<FbxSceneSystem>& sceneSystem);
                FBX_SCENE_BUILDER_API void Reset(const std::shared_ptr<FbxSDKWrapper::FbxMeshWrapper>& fbxMesh, Containers::SceneGraph& graph, Containers::SceneGraph::NodeIndex target, const std::shared_ptr<FbxSceneSystem>& sceneSystem);

                // Builds and adds a mesh to the SceneGraph. The result indicates if data was added, but even if true doesn't guarantee 
                //      that all data from the source FBX file was used.
                FBX_SCENE_BUILDER_API bool BuildMesh();

            protected:
                bool BuildMeshInternal(const AZStd::shared_ptr<SceneData::GraphData::MeshData>& mesh);

                void ProcessVertexColors();
                AZStd::shared_ptr<DataTypes::IGraphObject> BuildVertexColorData(int index);

                void ProcessVertexUVs();
                AZStd::shared_ptr<DataTypes::IGraphObject> BuildVertexUVData(int index);

                std::shared_ptr<FbxSDKWrapper::FbxMeshWrapper> m_fbxMesh;
                Containers::SceneGraph& m_graph;
                Containers::SceneGraph::NodeIndex m_targetNode;
                size_t m_vertexCount;
                std::shared_ptr<FbxSceneSystem> m_sceneSystem;
            };
        }
    }
}