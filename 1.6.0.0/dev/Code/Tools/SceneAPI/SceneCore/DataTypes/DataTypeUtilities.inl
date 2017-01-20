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

#include <SceneAPI/SceneCore/DataTypes/Groups/IGroup.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace DataTypes
        {
            namespace Utilities
            {
                template<typename T>
                AZStd::shared_ptr<T> FindRule(IGroup& group)
                {
                    size_t ruleCount = group.GetRuleCount();
                    for (size_t i = 0; i < ruleCount; ++i)
                    {
                        if (group.GetRule(i)->RTTI_IsTypeOf(T::TYPEINFO_Uuid()))
                        {
                            return AZStd::static_pointer_cast<T>(group.GetRule(i));
                        }
                    }
                    return AZStd::shared_ptr<T>();
                }

                template<typename T>
                AZStd::shared_ptr<const T> FindRule(const IGroup& group)
                {
                    size_t ruleCount = group.GetRuleCount();
                    for (size_t i = 0; i < ruleCount; ++i)
                    {
                        if (group.GetRule(i)->RTTI_IsTypeOf(T::TYPEINFO_Uuid()))
                        {
                            return AZStd::static_pointer_cast<const T>(group.GetRule(i));
                        }
                    }
                    return AZStd::shared_ptr<const T>();
                }
            } // Utilities
        } // DataTypes
    } // SceneAPI
} // AZ