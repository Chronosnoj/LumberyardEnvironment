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
            ManifestBool::ManifestBool()
                : m_value(false)
            {
            }

            ManifestBool::ManifestBool(bool value)
                : m_value(value)
            {
            }

            bool ManifestBool::GetValue() const
            {
                return m_value;
            }

            void ManifestBool::SetValue(bool value)
            {
                m_value = value;
            }

            void ManifestBool::Reflect(AZ::ReflectContext* context)
            {
                AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
                if (serializeContext && !serializeContext->FindClassData(ManifestBool::TYPEINFO_Uuid()))
                {
                    serializeContext->
                        Class<ManifestBool, IManifestObject>()->
                        Version(1)->
                        Field("value", &ManifestBool::m_value);
                }
            }
        }
    }
}