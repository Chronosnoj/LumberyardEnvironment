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

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/hash.h>

namespace Gems
{
    /**
     * Represents a version conforming to the Semantic Versioning standard (http://semver.org/)
     */
    struct GemVersion
    {
        /// The type each component is stored as.
        using ComponentType = AZ::u64;

        GemVersion()
            : m_major(0)
            , m_minor(0)
            , m_patch(0) { }
        GemVersion(const GemVersion& other)
            : m_major(other.m_major)
            , m_minor(other.m_minor)
            , m_patch(other.m_patch) { }
        GemVersion(ComponentType major, ComponentType minor, ComponentType patch)
            : m_major(major)
            , m_minor(minor)
            , m_patch(patch) { }

        /**
        * Parses a version from a string in the format "[major].[minor].[patch]".
        *
        * \param[in] versionStr    The string to parse for a version.
        * \returns                 On success, the parsed GemVersion; on failure, a message describing the error.
        */
        static AZ::Outcome<GemVersion, AZStd::string> ParseFromString(const AZStd::string& versionStr)
        {
            ComponentType major, minor, patch;
            // Verify that all 3 components are successfully matched
            if (3 == azsscanf(versionStr.c_str(), "%llu.%llu.%llu", &major, &minor, &patch))
            {
                return AZ::Success(GemVersion(major, minor, patch));
            }
            else
            {
                return AZ::Failure(AZStd::string::format("Failed to parse invalid version string \"%s\".", versionStr.c_str()));
            }
        }

        /**
         * Returns the version in string form.
         *
         * \returns                 The version as a string formatted as "[major].[minor].[patch]".
         */
        AZStd::string ToString() const
        {
            return AZStd::string::format("%llu.%llu.%llu", m_major, m_minor, m_patch);
        }

        union
        {
            struct
            {
                ComponentType m_major;
                ComponentType m_minor;
                ComponentType m_patch;
            };

            ComponentType m_parts[3];
        };

        /**
         * Compare two versions.
         *
         * Returns: 0 if a == b, <0 if a < b, and >0 if a > b.
         */
        static int Compare(const GemVersion& a, const GemVersion& b)
        {
            for (int i = 0; i < 3; ++i)
            {
                if (a.m_parts[i] != b.m_parts[i])
                {
                    return static_cast<int>(a.m_parts[i]) - static_cast<int>(b.m_parts[i]);
                }
            }

            return 0;
        }

        /**
         * Check if current version is 0.0.0
         *
         * Returns: true if the current version is 0.0.0, else false
         */
        bool IsZero() const
        {
            return (m_major == 0) && (m_minor == 0) && (m_patch == 0);
        }
    };

    inline bool operator< (const GemVersion& a, const GemVersion& b) { return GemVersion::Compare(a, b) <  0; }
    inline bool operator> (const GemVersion& a, const GemVersion& b) { return GemVersion::Compare(a, b) >  0; }
    inline bool operator<=(const GemVersion& a, const GemVersion& b) { return GemVersion::Compare(a, b) <= 0; }
    inline bool operator>=(const GemVersion& a, const GemVersion& b) { return GemVersion::Compare(a, b) >= 0; }
    inline bool operator==(const GemVersion& a, const GemVersion& b) { return GemVersion::Compare(a, b) == 0; }
    inline bool operator!=(const GemVersion& a, const GemVersion& b) { return GemVersion::Compare(a, b) != 0; }
} // namespace Gems

namespace AZStd
{
    template<>
    struct hash<Gems::GemVersion>
    {
        size_t operator()(const Gems::GemVersion& ver) const
        {
            hash<decltype(ver.m_parts)> hasher;
            return hasher(ver.m_parts);
        }
    };
} // namespace AZStd
