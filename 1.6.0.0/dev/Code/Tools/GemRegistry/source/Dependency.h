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
#pragma once

#include "GemRegistry/IDependency.h"
#include "GemRegistry/IGemRegistry.h"

#include <AzCore/std/string/regex.h>

namespace Gems
{
    class Dependency
        : public IDependency
    {
    public:
        Dependency();
        Dependency(const Dependency& dep);

        // IDependency
        const AZ::Uuid& GetID() const override { return m_id; }
        const AZStd::vector<Bound>& GetBounds() const override { return m_bounds; }
        bool IsFullfilledBy(const Specifier& spec) const override;
        AZ::Outcome<void, AZStd::string> ParseVersions(AZStd::vector<AZStd::string> deps) override;
        // ~IDependency

        // Internal methods
        void SetID(const AZ::Uuid& id) { m_id = id; }

        AZ::Uuid m_id;
        AZStd::vector<Bound> m_bounds;

    private:
        /**
         * Parses a version from a string.
         *
         * \param[in]  str      The string to parse.
         * \param[out] ver      The version parsed.
         *
         * \returns             The depth of version provided (number of decimals) (1-3). Returns -1 if the string is invalid.
         */
        AZ::Outcome<AZ::u8> ParseVersion(const AZStd::string& str, GemVersion& ver);

        // Regex caches
        AZStd::regex m_dependencyRegex;
        AZStd::regex m_versionRegex;
    };
} // namespace Gems
