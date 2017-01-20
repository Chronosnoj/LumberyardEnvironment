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

#include <AzCore/Serialization/SerializeContext.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace DataTypes
        {
            ManifestString::ManifestString(const AZStd::string& value)
                : m_value(value)
            {
            }

            ManifestString::ManifestString(AZStd::string&& value)
                : m_value(AZStd::move(value))
            {
            }

            const AZStd::string& ManifestString::GetValue() const
            {
                return m_value;
            }

            void ManifestString::SetValue(const AZStd::string& value)
            {
                m_value = value;
            }

            void ManifestString::SetValue(AZStd::string&& value)
            {
                m_value = AZStd::move(value);
            }

            void ManifestString::Reflect(AZ::ReflectContext* context)
            {
                AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
                if (serializeContext && !serializeContext->FindClassData(ManifestString::TYPEINFO_Uuid()))
                {
                    serializeContext->
                        Class<ManifestString, IManifestObject>()->
                        Version(1)->
                        Field("value", &ManifestString::m_value);
                }
            }
        }
    }
}