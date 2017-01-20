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
#include "Dependency.h"
#include <AzFramework/StringFunc/StringFunc.h>

namespace Gems
{
    using Comp = IDependency::Bound::Comparison;

    bool IDependency::Bound::MatchesVersion(GemVersion version) const
    {
        bool satisfies = false;

        #define CHECK_OPERATOR(comp, OP)                        \
    if (!satisfies && (m_comparison& Comp::comp) != Comp::None) \
    {                                                           \
        satisfies = version OP m_version;                       \
    }

        CHECK_OPERATOR(EqualTo, ==);
        CHECK_OPERATOR(GreaterThan, >);
        CHECK_OPERATOR(LessThan, <);
        #undef CHECK_OPERATOR

        return satisfies;
    }

    AZStd::string IDependency::Bound::ToString() const
    {
        if ((m_comparison& Comp::TwiddleWakka) != Comp::None)
        {
            AZStd::string version = AZStd::string::format("~>%llu", m_version.m_major);
            for (AZ::u8 i = 1; i < m_parseDepth; ++i)
            {
                version += AZStd::string::format(".%llu", m_version.m_parts[i]);
            }
            return version;
        }

        AZStd::string op = "";

        if ((m_comparison& Comp::GreaterThan) != Comp::None)
        {
            op = ">";
        }
        else if ((m_comparison& Comp::LessThan) != Comp::None)
        {
            op = "<";
        }

        if ((m_comparison& Comp::EqualTo) != Comp::None)
        {
            op += op.length() == 0 ? "==" : "=";
        }

        return op + m_version.ToString();
    }

    Dependency::Dependency()
        : m_id(AZ::Uuid::CreateNull())
        , m_bounds()
        // Used by Dependency::ParseVersions. Looks for "<operator> <version>".
        , m_dependencyRegex("(?:(~>|[>=<]{1,2}) *([0-9]+(?:\\.[0-9]+){0,2}))")
        // Used by Dependency::ParseVersion. Looks for a semantic version of a variable depth ("num[.num[.num]]").
        , m_versionRegex("([0-9]+)(?:\\.([0-9]+)(?:\\.([0-9]+))?)?")
    { }

    Dependency::Dependency(const Dependency& dep)
        : m_id(dep.m_id)
        , m_bounds(dep.m_bounds)
        , m_dependencyRegex(dep.m_dependencyRegex)
        , m_versionRegex(dep.m_versionRegex)
    { }

    bool Dependency::IsFullfilledBy(const Specifier& spec) const
    {
        if (spec.m_id != m_id)
        {
            return false;
        }

        for (auto && bound : m_bounds)
        {
            bool satisfies = false;

            if (bound.m_comparison == Comp::TwiddleWakka)
            {
                // Lower bound
                Bound lower;
                lower.m_comparison = Comp::EqualTo | Comp::GreaterThan;
                lower.m_version = bound.m_version;
                lower.m_parseDepth = bound.m_parseDepth;

                // Upper bound
                Bound upper;
                upper.m_comparison = Comp::LessThan;
                upper.m_version = lower.m_version;
                upper.m_parseDepth = 3;
                // ~>1.0 becomes >=1.0.0 <2.0.0
                upper.m_version.m_parts[lower.m_parseDepth - 1] = 0;
                upper.m_version.m_parts[lower.m_parseDepth - 2]++;

                if (!lower.MatchesVersion(spec.m_version)
                    || !upper.MatchesVersion(spec.m_version))
                {
                    return false;
                }
            }
            else
            {
                if (!bound.MatchesVersion(spec.m_version))
                {
                    return false;
                }
            }
        }

        return true;
    }

    AZ::Outcome<void, AZStd::string> Dependency::ParseVersions(AZStd::vector<AZStd::string> deps)
    {
        using Comp = IDependency::Bound::Comparison;

        AZStd::smatch match;
        AZStd::string depStr;

        for (const auto& depStrRaw : deps)
        {
            depStr = depStrRaw;
            AzFramework::StringFunc::Strip(depStr, " \t");

            if (depStr == "*")
            {
                // If * is in the constraints, allow ANY version of the dependency
                m_bounds.clear();
                return AZ::Success();
            }
            else if (AZStd::regex_match(depStr, match, m_dependencyRegex) && match.size() >= 3)
            {
                // Check for twiddle wakka, it's a special case
                if (match[1].str() == "~>")
                {
                    // Lower bound
                    Bound bound;
                    bound.m_comparison = Comp::TwiddleWakka;
                    auto parseOutcome = ParseVersion(match[2].str(), bound.m_version);
                    if (!parseOutcome.IsSuccess() || parseOutcome.GetValue() < 2)
                    {
                        // ~>1 not allowed, must specifiy ~>1.0
                        goto invalid_version_str;
                    }
                    bound.m_parseDepth = parseOutcome.GetValue();
                    m_bounds.push_back(bound);
                }
                else
                {
                    AZStd::string op = match[1].str();
                    Bound current;

                    auto findOperator = [&op, &current](Comp comp, const char* str) -> void
                        {
                            if (op.find(str) != AZStd::string::npos)
                            {
                                current.m_comparison |= comp;
                            }
                        };

                    findOperator(Comp::EqualTo, "=");
                    findOperator(Comp::LessThan, "<");
                    findOperator(Comp::GreaterThan, ">");

                    auto parseOutcome = ParseVersion(match[2].str(), current.m_version);
                    AZ_Assert(parseOutcome.IsSuccess(), "Internal Error: Invalid version should have been caught by dependencyRegex.");
                    current.m_parseDepth = parseOutcome.GetValue();

                    m_bounds.push_back(current);
                }
            }
            else
            {
                goto invalid_version_str;
            }
        }

        return AZ::Success();

invalid_version_str:
        // Error message
        m_bounds.clear();
        return AZ::Failure(AZStd::string::format(
                "Failed to parse dependency version string \"%s\": invalid format.",
                depStr.c_str()));
    }

    AZ::Outcome<AZ::u8> Dependency::ParseVersion(const AZStd::string& str, GemVersion& ver)
    {
        AZStd::smatch match;
        AZ::u8 depth = 0;

        if (AZStd::regex_match(str, match, m_versionRegex))
        {
            for (size_t i = 1; i < match.size(); ++i)
            {
                if (!match[i].str().empty())
                {
                    ver.m_parts[depth++] = static_cast<GemVersion::ComponentType>(AZStd::stoull(match[i].str()));
                }
                else
                {
                    break;
                }
            }

            return AZ::Success(depth);
        }
        else
        {
            return AZ::Failure();
        }
    }
} // namespace Gems
