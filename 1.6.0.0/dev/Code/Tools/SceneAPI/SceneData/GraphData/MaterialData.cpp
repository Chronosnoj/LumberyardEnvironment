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

#include <SceneAPI/SceneData/GraphData/MaterialData.h>

namespace AZ
{
    namespace SceneData
    {
        namespace GraphData
        {
            namespace DataTypes = AZ::SceneAPI::DataTypes;

            const AZStd::string MaterialData::s_DiffuseMapName = "Diffuse";
            const AZStd::string MaterialData::s_SpecularMapName = "Specular";
            const AZStd::string MaterialData::s_BumpMapName = "Bump";
            const AZStd::string MaterialData::s_emptyString = "";

            void MaterialData::SetTexture(TextureMapType mapType, const char* textureFileName)
            {
                if (textureFileName)
                {
                    SetTexture(mapType, AZStd::string(textureFileName));
                }
            }

            void MaterialData::SetTexture(TextureMapType mapType, const AZStd::string& textureFileName)
            {
                SetTexture(mapType, AZStd::string(textureFileName));
            }

            void MaterialData::SetTexture(TextureMapType mapType, AZStd::string&& textureFileName)
            {
                if (!textureFileName.empty())
                {
                    m_textureMap[mapType] = AZStd::move(textureFileName);
                }
            }

            const AZStd::string& MaterialData::GetTexture(TextureMapType mapType) const
            {
                auto result = m_textureMap.find(mapType);
                if (result != m_textureMap.end())
                {
                    return result->second;
                }

                return s_emptyString;
            }

            void MaterialData::SetNoDraw(bool isNoDraw)
            {
                m_isNoDraw = isNoDraw;
            }

            bool MaterialData::IsNoDraw() const
            {
                return m_isNoDraw;
            }
        }
    }
}
