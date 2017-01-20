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
            class ManifestBool
                : public IManifestObject
            {
            public:
                AZ_RTTI(ManifestBool, "{62CDB8B0-E920-48E2-B673-0A3F58F690A4}", IManifestObject);
                AZ_CLASS_ALLOCATOR(ManifestBool, AZ::SystemAllocator, 0)

                inline ManifestBool();
                inline explicit ManifestBool(bool value);
                virtual ~ManifestBool() override = default;

                inline bool GetValue() const;
                inline void SetValue(bool value);

                inline static void Reflect(AZ::ReflectContext* context);

            protected:
                bool m_value;
            };
        }  // DataTypes
    }  // SceneAPI
}  // AZ

#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ManifestBool.inl>
