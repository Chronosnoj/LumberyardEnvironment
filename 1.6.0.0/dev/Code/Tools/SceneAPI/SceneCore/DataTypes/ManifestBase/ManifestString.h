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
#include <AzCore/std/string/string.h>
#include <AzCore/Memory/Memory.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <SceneAPI/SceneCore/DataTypes/IManifestObject.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace DataTypes
        {
            class ManifestString
                : public IManifestObject
            {
            public:
                AZ_RTTI(ManifestString, "{ABF7FBDE-60AD-4441-BD86-29CD4840E075}", IManifestObject);
                AZ_CLASS_ALLOCATOR(ManifestString, AZ::SystemAllocator, 0)

                inline ManifestString() = default;
                inline explicit ManifestString(const AZStd::string& value);
                inline explicit ManifestString(AZStd::string&& value);
                virtual ~ManifestString() override = default;

                inline const AZStd::string& GetValue() const;
                inline void SetValue(const AZStd::string& value);
                inline void SetValue(AZStd::string&& value);

                inline static void Reflect(AZ::ReflectContext* context);

            protected:
                AZStd::string m_value;
            };
        }  // DataTypes
    }  // SceneAPI
}  // AZ

#include <SceneAPI/SceneCore/DataTypes/ManifestBase/ManifestString.inl>
