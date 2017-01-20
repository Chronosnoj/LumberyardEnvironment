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

#ifndef AZINCLUDE_TOOLS_SCENECORE_DATATYPES_IMATERIALDATA_H_
#define AZINCLUDE_TOOLS_SCENECORE_DATATYPES_IMATERIALDATA_H_

#include <AzCore/std/string/string.h>
#include <AzCore/RTTI/RTTI.h>
#include <SceneAPI/SceneCore/DataTypes/IGraphObject.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace DataTypes
        {
            class IMaterialData
                : public IGraphObject
            {
            public:
                AZ_RTTI(IMaterialData, "{4C0E818F-CEE8-48A0-AC3D-AC926811BFE4}", IGraphObject);

                enum class TextureMapType
                {
                    Diffuse,
                    Specular,
                    Bump
                };

                virtual ~IMaterialData() override = default;

                virtual const AZStd::string& GetTexture(TextureMapType mapType) const = 0;
                virtual bool IsNoDraw() const = 0;
            };
        }  //namespace DataTypes
    }  //namespace SceneAPI
}  //namespace AZ

#endif // AZINCLUDE_TOOLS_SCENECORE_DATATYPES_IMATERIALDATA_H_