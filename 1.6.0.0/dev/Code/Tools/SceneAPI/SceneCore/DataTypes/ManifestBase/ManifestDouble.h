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
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Memory/Memory.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <SceneAPI/SceneCore/DataTypes/IManifestObject.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace DataTypes
        {
            class ManifestDouble
                : public IManifestObject
            {
            public:
                AZ_RTTI(ManifestDouble, "{459262A9-E8A5-4585-A226-D0FAB3ADE21F}", IManifestObject);
                AZ_CLASS_ALLOCATOR(ManifestDouble, AZ::SystemAllocator, 0)

                inline ManifestDouble();
                inline explicit ManifestDouble(double value);
                virtual ~ManifestDouble() override = default;

                inline double GetValue() const;
                inline void SetValue(double value);

                inline static void Reflect(AZ::ReflectContext* context);

            protected:
                double m_value;
            };
        }  // DataTypes
    }  // SceneAPI
}  // AZ

#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ManifestDouble.inl>
