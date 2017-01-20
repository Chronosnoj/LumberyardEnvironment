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

#ifndef AZINCLUDE_TOOLS_SCENEAPI_SCENEDATA_GRAPHDATA_MATERIALDATA_H_
#define AZINCLUDE_TOOLS_SCENEAPI_SCENEDATA_GRAPHDATA_MATERIALDATA_H_

#include <SceneAPI/SceneData/SceneDataConfiguration.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMaterialData.h>
#include <AzCore/std/containers/unordered_map.h>

namespace AZ
{
    namespace SceneData
    {
        namespace GraphData
        {
            class MaterialData
                : public AZ::SceneAPI::DataTypes::IMaterialData
            {
            public:
                AZ_RTTI(MaterialData, "{F2EE1768-183B-483E-9778-CB3D3D0DA68A}", AZ::SceneAPI::DataTypes::IMaterialData)

                SCENE_DATA_API virtual ~MaterialData() = default;

                SCENE_DATA_API virtual void SetTexture(TextureMapType mapType, const char* textureFileName);
                SCENE_DATA_API virtual void SetTexture(TextureMapType mapType, const AZStd::string& textureFileName);
                SCENE_DATA_API virtual void SetTexture(TextureMapType mapType, AZStd::string&& textureFileName);
                SCENE_DATA_API virtual void SetNoDraw(bool isNoDraw);

                SCENE_DATA_API const AZStd::string& GetTexture(TextureMapType mapType) const override;
                SCENE_DATA_API bool IsNoDraw() const override;

            protected:
                AZStd::unordered_map<TextureMapType, AZStd::string> m_textureMap;
                bool m_isNoDraw;

                const static AZStd::string s_DiffuseMapName;
                const static AZStd::string s_SpecularMapName;
                const static AZStd::string s_BumpMapName;
                const static AZStd::string s_emptyString;
            };
        }
    }
}

#endif // AZINCLUDE_TOOLS_SCENEAPI_SCENEDATA_GRAPHDATA_MATERIALDATA_H_