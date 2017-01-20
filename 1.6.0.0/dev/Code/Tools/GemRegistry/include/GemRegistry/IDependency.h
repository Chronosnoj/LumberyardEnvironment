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

#include <AzCore/Math/Uuid.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/containers/vector.h>

#include "GemRegistry/GemVersion.h"

namespace Gems
{
    /**
     * Specifies a particular Gem instance (by ID and version).
     */
    struct Specifier
    {
        AZ::Uuid m_id;
        GemVersion m_version;

        Specifier(const AZ::Uuid& id, const GemVersion& version)
            : m_id(id)
            , m_version(version)
        {}

        virtual ~Specifier() = default;
    };

    /**
     * Defines a Gem's dependency upon another Gem.
     */
    class IDependency
    {
    public:
        struct Bound
        {
            enum class Comparison : AZ::u8
            {
                // Don't compare against this version
                None            = 0,
                // Traditional operators
                GreaterThan     = 1 << 0,
                LessThan        = 1 << 1,
                EqualTo         = 1 << 2,
                // Special operators
                TwiddleWakka    = 1 << 3
            };

            GemVersion m_version = { 0, 0, 0 };
            Comparison m_comparison = Comparison::None;
            AZ::u8 m_parseDepth;

            virtual AZStd::string ToString() const;
            virtual bool MatchesVersion(GemVersion version) const;
        };

        virtual ~IDependency() = default;

        /**
         * Gets the ID of the Gem depended on.
         *
         * \returns             The ID of the Gem depended on.
         */
        virtual const AZ::Uuid& GetID() const = 0;

        /**
         * Gets the bounds that the dependee's version must fulfill.
         *
         * \returns             The list of bounds.
         */
        virtual const AZStd::vector<Bound>& GetBounds() const = 0;

        /**
         * Checks if a specifier matches a dependency.
         *
         * Checks that the specifier's ID is the one depended on,
         * and that the version matches the bounds (which can be retrieved by GetBounds()).
         *
         * \params[in] spec     The specifier to test.
         *
         * \returns             Whether or not the Specifier fits the dependency.
         */
        virtual bool IsFullfilledBy(const Specifier& spec) const = 0;

        /**
         * Parses version bounds from a list of strings.
         *
         * Each string should fit the pattern [OPERATOR][VERSION],
         * where [OPERATOR] is >, >=, <, <=, ==, or ~>,
         * and [VERSION] is a valid version string, parsable by Gems::GemVersion.
         *
         * \params[in] deps     The list of bound strings to parse.
         *
         * \returns             True on success, false on failure.
         */
        virtual AZ::Outcome<void, AZStd::string> ParseVersions(AZStd::vector<AZStd::string> deps) = 0;
    };

    #define BITMASK_OPS(Ty)                                                                             \
    inline Ty&operator&=(Ty & left, Ty right)   { left = (Ty)((int)left & (int)right); return (left); } \
    inline Ty& operator|=(Ty& left, Ty right)   { left = (Ty)((int)left | (int)right); return (left); } \
    inline Ty& operator^=(Ty& left, Ty right)   { left = (Ty)((int)left ^ (int)right); return (left); } \
    inline Ty operator&(Ty left, Ty right)      { return ((Ty)((int)left & (int)right)); }              \
    inline Ty operator|(Ty left, Ty right)      { return ((Ty)((int)left | (int)right)); }              \
    inline Ty operator^(Ty left, Ty right)      { return ((Ty)((int)left ^ (int)right)); }              \
    inline Ty operator~(Ty left)                { return ((Ty) ~(int)left); }

    BITMASK_OPS(IDependency::Bound::Comparison)

    #undef BITMASK_OPS
} // namespace Gems
