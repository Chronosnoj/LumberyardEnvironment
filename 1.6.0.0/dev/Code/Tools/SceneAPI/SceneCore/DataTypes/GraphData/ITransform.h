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

#include <AzCore/RTTI/RTTI.h>
#include <SceneAPI/SceneCore/DataTypes/IGraphObject.h>

namespace AZ
{
    class Transform;
    namespace SceneAPI
    {
        namespace DataTypes
        {
            class ITransform
                : public IGraphObject
            {
            public:
                AZ_RTTI(ITransform, "{C1A24A14-EDD4-422F-AB62-9566D744AF1B}", IGraphObject);

                virtual ~ITransform() override = default;

                virtual AZ::Transform& GetMatrix() = 0;
                virtual const AZ::Transform& GetMatrix() const = 0;
            };
        }  // DataTypes
    }  // SceneAPI
}  // AZ