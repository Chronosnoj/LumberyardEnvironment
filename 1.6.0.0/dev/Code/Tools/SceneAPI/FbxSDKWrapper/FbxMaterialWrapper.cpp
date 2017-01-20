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

#include "FbxMaterialWrapper.h"
#include "FbxPropertyWrapper.h"

namespace
{
    const char* s_physicalisedAttributeName = "physicalize";
    const char* s_proxyNoDraw = "ProxyNoDraw";
}
namespace AZ
{
    namespace FbxSDKWrapper
    {
        FbxMaterialWrapper::FbxMaterialWrapper(FbxSurfaceMaterial* fbxMaterial)
            : m_fbxMaterial(fbxMaterial)
        {
            assert(fbxMaterial);
        }

        FbxMaterialWrapper::~FbxMaterialWrapper()
        {
            m_fbxMaterial = nullptr;
        }

        std::string FbxMaterialWrapper::GetName() const
        {
            return m_fbxMaterial->GetInitialName();
        }

        std::string FbxMaterialWrapper::GetTextureFileName(const char* textureType) const
        {
            FbxFileTexture* fileTexture = nullptr;
            FbxProperty property = m_fbxMaterial->FindProperty(textureType);
            FbxDataType propertyType = property.GetPropertyDataType();

            /// Engine currently doesn't support multiple textures. Right now we only use first texture of first layer.
            int layeredTextureCount = property.GetSrcObjectCount<FbxLayeredTexture>();
            if (layeredTextureCount > 0)
            {
                FbxLayeredTexture* layeredTexture = FbxCast<FbxLayeredTexture>(property.GetSrcObject<FbxLayeredTexture>(0));
                int textureCount = layeredTexture->GetSrcObjectCount<FbxTexture>();
                if (textureCount > 0)
                {
                    fileTexture = FbxCast<FbxFileTexture>(layeredTexture->GetSrcObject<FbxTexture>(0));
                }
            }
            else
            {
                int textureCount = property.GetSrcObjectCount<FbxTexture>();
                if (textureCount > 0)
                {
                    fileTexture = FbxCast<FbxFileTexture>(property.GetSrcObject<FbxTexture>(0));
                }
            }

            return fileTexture ? fileTexture->GetFileName() : std::string();
        }

        std::string FbxMaterialWrapper::GetTextureFileName(const std::string& textureType) const
        {
            return GetTextureFileName(textureType.c_str());
        }

        std::string FbxMaterialWrapper::GetTextureFileName(MaterialMapType textureType) const
        {
            switch (textureType)
            {
            case MaterialMapType::Diffuse:
                return GetTextureFileName(FbxSurfaceMaterial::sDiffuse);
            case MaterialMapType::Specular:
                return GetTextureFileName(FbxSurfaceMaterial::sSpecular);
            case MaterialMapType::Bump:
                return GetTextureFileName(FbxSurfaceMaterial::sBump);
            }

            return std::string();
        }

        bool FbxMaterialWrapper::IsNoDraw() const
        {
            FbxProperty fbxProperty = m_fbxMaterial->FindProperty(s_physicalisedAttributeName);
            FbxPropertyWrapper property(&fbxProperty);
            if (property.IsValid())
            {
                return property.GetFbxString() == s_proxyNoDraw;
            }

            return false;
        }
    }
}