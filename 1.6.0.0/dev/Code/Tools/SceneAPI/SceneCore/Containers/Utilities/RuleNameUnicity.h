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

#include <regex>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/conversions.h>

namespace AZ
{
    namespace SceneAPI
    {
        /*
        Generates a string with a {baseNameRaw}-[0-9]+ pattern. The number used for the return value will
        be one bigger than the largest number parsed out of existing names. The result will also follow
        the same pattern.
        */
        template<typename TStringIteratable>
        AZStd::string GenerateUniqueRuleKey(TStringIteratable& existingNames, const AZStd::string& baseNameRaw)
        {
            AZStd::string baseName = baseNameRaw;
            if (!baseNameRaw.empty())
            {
                baseName += "-";
            }

            std::string regexTarget = baseName.c_str();
            std::regex testExpression(regexTarget + "[0-9]+");

            uint64_t max = 0;
            bool nameExists = false;
            for (const auto& name : existingNames)
            {
                if (std::regex_match(name.c_str(), testExpression))
                {
                    // At this point, regex has just guaranteed us that a real number (at least a single digit in size)
                    //  is waiting for us on the other side of the hyphen; so we just need to substring to get it.
                    AZStd::string numericIndicatorString = name.substr(baseName.size());

                    uint64_t value = AZStd::stoul(numericIndicatorString);
                    max = AZStd::max(value, max);
                }
                else if (name == baseNameRaw)
                {
                    nameExists = true;
                }
            }

            if (max == 0 && !nameExists)
            {
                if (!baseNameRaw.empty())
                {
                    return baseNameRaw;
                }
                else
                {
                    return "1";
                }
            }
            else
            {
                return baseName + AZStd::to_string(max + 1);
            }
        }
    }
}