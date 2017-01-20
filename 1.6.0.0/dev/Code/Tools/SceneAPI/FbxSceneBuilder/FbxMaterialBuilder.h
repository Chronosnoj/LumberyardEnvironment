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

#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <SceneAPI/FbxSceneBuilder/FbxSceneBuilderConfiguration.h>

namespace AZ
{
    namespace FbxSDKWrapper
    {
        class FbxNodeWrapper;
        class FbxMaterialWrapper;
    }

    namespace SceneAPI
    {
        namespace DataTypes
        {
            class IGraphObject;
        }

        namespace FbxSceneImporter
        {
            // FbxMaterialBuilder builds the SceneGraph data MaterialData from FbxMaterialWrapper data
            //      It converts name, texture mappings and NoDraw flag for a single material
            class FbxMaterialBuilder
            {
            public:
                FBX_SCENE_BUILDER_API AZStd::shared_ptr<DataTypes::IGraphObject> BuildMaterial(FbxSDKWrapper::FbxNodeWrapper& node, int materialIndex) const;
            };
        }
    }
}
