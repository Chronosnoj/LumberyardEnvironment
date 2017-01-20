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
            ManifestDouble::ManifestDouble()
                : m_value(0.0)
            {
            }

            ManifestDouble::ManifestDouble(double value)
                : m_value(value)
            {
            }

            double ManifestDouble::GetValue() const
            {
                return m_value;
            }

            void ManifestDouble::SetValue(double value)
            {
                m_value = value;
            }

            void ManifestDouble::Reflect(AZ::ReflectContext* context)
            {
                AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
                if (serializeContext && !serializeContext->FindClassData(ManifestDouble::TYPEINFO_Uuid()))
                {
                    serializeContext->
                        Class<ManifestDouble, IManifestObject>()->
                        Version(1)->
                        Field("value", &ManifestDouble::m_value);
                }
            }
        }
    }
}