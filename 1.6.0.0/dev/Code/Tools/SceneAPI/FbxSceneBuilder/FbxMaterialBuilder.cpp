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

#include <AzCore/std/smart_ptr/make_shared.h>
#include <SceneAPI/FbxSceneBuilder/FbxMaterialBuilder.h>
#include <SceneAPI/FbxSDKWrapper/FbxMaterialWrapper.h>
#include <SceneAPI/FbxSDKWrapper/FbxNodeWrapper.h>
#include <SceneAPI/SceneData/GraphData/MaterialData.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace FbxSceneImporter
        {
            AZStd::shared_ptr<DataTypes::IGraphObject> FbxMaterialBuilder::BuildMaterial(FbxSDKWrapper::FbxNodeWrapper& node, int materialIndex) const
            {
                AZ_Assert(materialIndex < node.GetMaterialCount(), "Invalid material index (%i)", materialIndex);
                const std::shared_ptr<FbxSDKWrapper::FbxMaterialWrapper> fbxMaterial = node.GetMaterial(materialIndex);
                if (!fbxMaterial)
                {
                    return nullptr;
                }

                AZStd::shared_ptr<SceneData::GraphData::MaterialData> material = AZStd::make_shared<SceneData::GraphData::MaterialData>();

                material->SetTexture(DataTypes::IMaterialData::TextureMapType::Diffuse,
                    fbxMaterial->GetTextureFileName(FbxSDKWrapper::FbxMaterialWrapper::MaterialMapType::Diffuse).c_str());
                material->SetTexture(DataTypes::IMaterialData::TextureMapType::Specular,
                    fbxMaterial->GetTextureFileName(FbxSDKWrapper::FbxMaterialWrapper::MaterialMapType::Specular).c_str());
                material->SetTexture(DataTypes::IMaterialData::TextureMapType::Bump,
                    fbxMaterial->GetTextureFileName(FbxSDKWrapper::FbxMaterialWrapper::MaterialMapType::Bump).c_str());
                material->SetNoDraw(fbxMaterial->IsNoDraw());

                return material;
            }
        }
    }
}