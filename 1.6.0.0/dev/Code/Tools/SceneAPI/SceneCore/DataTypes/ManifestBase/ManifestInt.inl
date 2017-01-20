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
            ManifestInt::ManifestInt()
                : m_value(0)
            {
            }

            ManifestInt::ManifestInt(int64_t value)
                : m_value(value)
            {
            }

            int64_t ManifestInt::GetValue() const
            {
                return m_value;
            }

            void ManifestInt::SetValue(int64_t value)
            {
                m_value = value;
            }

            void ManifestInt::Reflect(AZ::ReflectContext* context)
            {
                AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
                if (serializeContext && !serializeContext->FindClassData(ManifestInt::TYPEINFO_Uuid()))
                {
                    serializeContext->
                        Class<ManifestInt, IManifestObject>()->
                        Version(1)->
                        Field("value", &ManifestInt::m_value);
                }
            }
        }
    }
}