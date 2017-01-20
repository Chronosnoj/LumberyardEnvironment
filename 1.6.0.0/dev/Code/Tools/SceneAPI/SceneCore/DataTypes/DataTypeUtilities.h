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

#include <AzCore/std/smart_ptr/shared_ptr.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace DataTypes
        {
            class IRule;
            class IGroup;

            namespace Utilities
            {
                // Finds the first rule of the template type attached to the given group,
                //      or nullptr if not found.
                template<typename T>
                AZStd::shared_ptr<T> FindRule(IGroup& group);
                template<typename T>
                AZStd::shared_ptr<const T> FindRule(const IGroup& group);
            } // Utilities
        } // DataTypes
    } // SceneAPI
} // AZ

#include <SceneAPI/SceneCore/DataTypes/DataTypeUtilities.inl>