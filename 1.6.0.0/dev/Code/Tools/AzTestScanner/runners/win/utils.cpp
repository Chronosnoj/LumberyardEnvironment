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
#include "utils.h"
#include <algorithm>

namespace AzTest
{
    namespace Utils
    {
        bool endsWith(const std::string& s, const std::string& ending)
        {
            if (ending.length() > s.length())
            {
                return false;
            }
            return std::equal(ending.rbegin(), ending.rend(), s.rbegin());
        }

        void removeParameters(int& argc, char** argv, int startIndex, int endIndex)
        {
            // protect against invalid order of parameters
            if (startIndex > endIndex)
            {
                return;
            }

            // constraint to valid range
            endIndex = std::min(endIndex, argc - 1);
            startIndex = std::max(startIndex, 0);

            int numRemoved = 0;
            int i = startIndex;
            int j = endIndex + 1;

            // copy all existing paramters
            while (j < argc)
            {
                argv[i++] = argv[j++];
            }

            // null out all the remaining parameters and count how many
            // were removed simultaneously
            while (i < argc)
            {
                argv[i++] = nullptr;
                ++numRemoved;
            }

            argc -= numRemoved;
        }
    }
}
