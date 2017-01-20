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
            class ManifestInt
                : public IManifestObject
            {
            public:
                AZ_RTTI(ManifestInt, "{D6F96B49-4E6F-4EE8-A5A3-959B76F90DA8}", IManifestObject);
                AZ_CLASS_ALLOCATOR(ManifestInt, AZ::SystemAllocator, 0)

                inline ManifestInt();
                inline explicit ManifestInt(int64_t value);
                virtual ~ManifestInt() override = default;

                inline int64_t GetValue() const;
                inline void SetValue(int64_t value);

                inline static void Reflect(AZ::ReflectContext* context);

            protected:
                int64_t m_value;
            };
        }  // DataTypes
    }  // SceneAPI
}  // AZ

#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ManifestInt.inl>
