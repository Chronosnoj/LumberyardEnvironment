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
#include <AzTest/AzTest.h>
#include "Dependency.h"

using namespace Gems;
using Comp = IDependency::Bound::Comparison;

class DependencyTest
    : public ::testing::Test
{
protected:
    enum class Bound
    {
        Upper,
        Lower
    };

    void SetUp() override
    {
        m_errCopyCtorFailed         = "Failed to copy Dependency instance.";
        m_errParseInvalidSucceeded  = "Failed to report invalid version.";
        m_errParseFailed            = "Failed to parse valid version string.";
        m_errIncorrectBoundCount    = "Improper number of bounds generated.";
        m_errComparisonMismatch     = "Comparison generated does not match ";
        m_errSpecFulfillsFailed     = "Spec that fulfills dependency was reported as invalid.";
        m_errInvalidSpecFulfills    = "Spec that does not fulfill dependency was reported as valid.";
        m_errToStringIncorrect      = "ToString result is incorrect.";
    }

    const char* ErrComparisonMismatch(const char* op)
    {
        return AZStd::string::format("Comparison generated does not match %s.", op).c_str();
    }

    const char* ErrInvalidBound(Bound b)
    {
        return AZStd::string::format("Version generated does not match %s bound.", b == Bound::Upper ? "upper" : "lower").c_str();
    }

    const char* m_errCopyCtorFailed;
    const char* m_errParseInvalidSucceeded;
    const char* m_errParseFailed;
    const char* m_errIncorrectBoundCount;
    const char* m_errComparisonMismatch;
    const char* m_errSpecFulfillsFailed;
    const char* m_errInvalidSpecFulfills;
    const char* m_errToStringIncorrect;
};

TEST_F(DependencyTest, MiscTests)
{
    Dependency dep1;
    dep1.m_id = AZ::Uuid::CreateRandom();
    dep1.m_bounds.push_back(IDependency::Bound {});

    Dependency dep2 {
        dep1
    };
    EXPECT_EQ(dep2.m_id, dep1.m_id)                                     << m_errCopyCtorFailed;
    EXPECT_EQ(dep2.m_bounds.size(), dep1.m_bounds.size())               << m_errCopyCtorFailed;
}

TEST_F(DependencyTest, FailureTest)
{
    Dependency dep;

    EXPECT_FALSE(dep.ParseVersions({ "Not a version requirement!" }).IsSuccess())   << m_errParseInvalidSucceeded;
    EXPECT_FALSE(dep.ParseVersions({ "~>1" }).IsSuccess())                          << m_errParseInvalidSucceeded;
    EXPECT_FALSE(dep.ParseVersions({ "~>1.invalid" }).IsSuccess())                  << m_errParseInvalidSucceeded;
}

TEST_F(DependencyTest, TwiddleWakkaTest)
{
    Dependency dep;

    ASSERT_TRUE(dep.ParseVersions({ "~>1.0" }).IsSuccess())             << m_errParseFailed;
    ASSERT_EQ(dep.GetBounds().size(), 1)                                << m_errIncorrectBoundCount;
    EXPECT_EQ(dep.GetBounds()[0].m_comparison, Comp::TwiddleWakka)      << ErrComparisonMismatch("Twiddle Wakka");
    EXPECT_EQ(dep.GetBounds()[0].m_version, GemVersion(1, 0, 0))        << ErrInvalidBound(Bound::Lower);

    dep.m_bounds.clear();

    ASSERT_TRUE(dep.ParseVersions({ "~>1.0.1" }).IsSuccess())           << m_errParseFailed;
    ASSERT_EQ(dep.GetBounds().size(), 1)                                << m_errIncorrectBoundCount;
    EXPECT_EQ(dep.GetBounds()[0].m_comparison, Comp::TwiddleWakka)      << ErrComparisonMismatch("Twiddle Wakka");
    EXPECT_EQ(dep.GetBounds()[0].m_version, GemVersion(1, 0, 1))        << ErrInvalidBound(Bound::Lower);
}

TEST_F(DependencyTest, SingleVersionTest)
{
    Dependency dep;

    ASSERT_TRUE(dep.ParseVersions({ ">=1.0.0" }).IsSuccess())           << m_errParseFailed;
    ASSERT_EQ(dep.GetBounds().size(), 1)                                << m_errIncorrectBoundCount;
    EXPECT_EQ(dep.GetBounds()[0].m_comparison, Comp::GreaterThan | Comp::EqualTo) << ErrComparisonMismatch(">=");
    EXPECT_EQ(dep.GetBounds()[0].m_version, GemVersion(1, 0, 0))        << ErrInvalidBound(Bound::Lower);

    dep.m_bounds.clear();

    ASSERT_TRUE(dep.ParseVersions({ ">2.20.0" }).IsSuccess())           << m_errParseFailed;
    ASSERT_EQ(dep.GetBounds().size(), 1)                                << m_errIncorrectBoundCount;
    EXPECT_EQ(dep.GetBounds()[0].m_comparison, Comp::GreaterThan)       << ErrComparisonMismatch(">");
    EXPECT_EQ(dep.GetBounds()[0].m_version, GemVersion(2, 20, 0))       << ErrInvalidBound(Bound::Lower);

    dep.m_bounds.clear();

    ASSERT_TRUE(dep.ParseVersions({ "==3.4.0" }).IsSuccess())           << m_errParseFailed;
    ASSERT_EQ(dep.GetBounds().size(), 1)                                << m_errIncorrectBoundCount;
    EXPECT_EQ(dep.GetBounds()[0].m_comparison, Comp::EqualTo)           << ErrComparisonMismatch("==");
    EXPECT_EQ(dep.GetBounds()[0].m_version, GemVersion(3, 4, 0))        << ErrInvalidBound(Bound::Lower);
}

TEST_F(DependencyTest, DoubleVersionTest)
{
    Dependency dep;

    ASSERT_TRUE(dep.ParseVersions({ ">=1.0.0", "<2.0.0" }).IsSuccess()) << m_errParseFailed;
    ASSERT_EQ(dep.GetBounds().size(), 2)                                << m_errIncorrectBoundCount;
    EXPECT_EQ(dep.GetBounds()[0].m_comparison, Comp::GreaterThan | Comp::EqualTo) << ErrComparisonMismatch(">=");
    EXPECT_EQ(dep.GetBounds()[1].m_comparison, Comp::LessThan)          << ErrComparisonMismatch("<");
    EXPECT_EQ(dep.GetBounds()[0].m_version, GemVersion(1, 0, 0))        << ErrInvalidBound(Bound::Lower);
    EXPECT_EQ(dep.GetBounds()[1].m_version, GemVersion(2, 0, 0))        << ErrInvalidBound(Bound::Upper);

    dep.m_bounds.clear();

    ASSERT_TRUE(dep.ParseVersions({ ">2.20.0", "<=3" }).IsSuccess())    << m_errParseFailed;
    ASSERT_EQ(dep.GetBounds().size(), 2)                                << m_errIncorrectBoundCount;
    EXPECT_EQ(dep.GetBounds()[0].m_comparison, Comp::GreaterThan)       << ErrComparisonMismatch(">");
    EXPECT_EQ(dep.GetBounds()[1].m_comparison, Comp::LessThan | Comp::EqualTo) << ErrComparisonMismatch("<=");
    EXPECT_EQ(dep.GetBounds()[0].m_version, GemVersion(2, 20, 0))       << ErrInvalidBound(Bound::Lower);
    EXPECT_EQ(dep.GetBounds()[1].m_version, GemVersion(3, 0, 0))        << ErrInvalidBound(Bound::Upper);

    dep.m_bounds.clear();

    ASSERT_TRUE(dep.ParseVersions({ "<3.4.0", ">=20.1" }).IsSuccess())  << m_errParseFailed;
    ASSERT_EQ(dep.GetBounds().size(), 2)                                << m_errIncorrectBoundCount;
    EXPECT_EQ(dep.GetBounds()[0].m_comparison, Comp::LessThan)          << ErrComparisonMismatch("<");
    EXPECT_EQ(dep.GetBounds()[1].m_comparison, Comp::GreaterThan | Comp::EqualTo) << ErrComparisonMismatch(">=");
    EXPECT_EQ(dep.GetBounds()[0].m_version, GemVersion(3, 4, 0))        << ErrInvalidBound(Bound::Lower);
    EXPECT_EQ(dep.GetBounds()[1].m_version, GemVersion(20, 1, 0))       << ErrInvalidBound(Bound::Upper);
}

TEST_F(DependencyTest, FullfillmentTest)
{
    AZ::Uuid gemId = AZ::Uuid::CreateRandom();

    Dependency dep;
    dep.SetID(gemId);

    Specifier spec1 = { gemId, GemVersion(1, 0, 0) };
    Specifier specR = { AZ::Uuid::CreateRandom(), GemVersion(0, 0, 0) };

    ASSERT_TRUE(dep.ParseVersions(AZStd::vector<AZStd::string>()).IsSuccess())<< m_errParseFailed;
    ASSERT_EQ(dep.GetBounds().size(), 0)                                << m_errIncorrectBoundCount;
    EXPECT_TRUE(dep.IsFullfilledBy(spec1))                              << m_errSpecFulfillsFailed;

    dep.m_bounds.clear();

    ASSERT_TRUE(dep.ParseVersions({ ">=1" }).IsSuccess())               << m_errParseFailed;
    ASSERT_EQ(dep.GetBounds().size(), 1)                                << m_errIncorrectBoundCount;
    EXPECT_TRUE(dep.IsFullfilledBy(spec1))                              << m_errSpecFulfillsFailed;
    EXPECT_FALSE(dep.IsFullfilledBy(specR))                             << m_errInvalidSpecFulfills;

    dep.m_bounds.clear();

    ASSERT_TRUE(dep.ParseVersions({ ">0", "<1.1" }).IsSuccess())        << m_errParseFailed;
    ASSERT_EQ(dep.GetBounds().size(), 2)                                << m_errIncorrectBoundCount;
    EXPECT_TRUE(dep.IsFullfilledBy(spec1))                              << m_errSpecFulfillsFailed;
    EXPECT_FALSE(dep.IsFullfilledBy(specR))                             << m_errInvalidSpecFulfills;

    dep.m_bounds.clear();

    ASSERT_TRUE(dep.ParseVersions({ ">1", "<2", "==1.2" }).IsSuccess()) << m_errParseFailed;
    ASSERT_EQ(dep.GetBounds().size(), 3)                                << m_errIncorrectBoundCount;
    EXPECT_FALSE(dep.IsFullfilledBy(spec1))                             << m_errInvalidSpecFulfills;
    EXPECT_FALSE(dep.IsFullfilledBy(specR))                             << m_errInvalidSpecFulfills;

    dep.m_bounds.clear();

    ASSERT_TRUE(dep.ParseVersions({ "~>1.0" }).IsSuccess())             << m_errParseFailed;
    ASSERT_EQ(dep.GetBounds().size(), 1)                                << m_errIncorrectBoundCount;
    EXPECT_TRUE(dep.IsFullfilledBy({ gemId, GemVersion(1, 0, 0) }))     << m_errSpecFulfillsFailed;
    EXPECT_TRUE(dep.IsFullfilledBy({ gemId, GemVersion(1, 1, 0) }))     << m_errSpecFulfillsFailed;
    EXPECT_FALSE(dep.IsFullfilledBy({ gemId, GemVersion(2, 0, 0) }))    << m_errSpecFulfillsFailed;
    EXPECT_FALSE(dep.IsFullfilledBy({ gemId, GemVersion(0, 0, 1) }))    << m_errSpecFulfillsFailed;

    dep.m_bounds.clear();

    ASSERT_TRUE(dep.ParseVersions({ "~>1.0.1" }).IsSuccess())           << m_errParseFailed;
    ASSERT_EQ(dep.GetBounds().size(), 1)                                << m_errIncorrectBoundCount;
    EXPECT_TRUE(dep.IsFullfilledBy({ gemId, GemVersion(1, 0, 1) }))     << m_errSpecFulfillsFailed;
    EXPECT_FALSE(dep.IsFullfilledBy({ gemId, GemVersion(1, 1, 0) }))    << m_errSpecFulfillsFailed;
    EXPECT_FALSE(dep.IsFullfilledBy({ gemId, GemVersion(1, 0, 0) }))    << m_errSpecFulfillsFailed;
}

TEST_F(DependencyTest, BoundToStringTest)
{
    GemVersion v1 {
        1, 0, 0
    };

    IDependency::Bound bnd;
    bnd.m_version = v1;

    bnd.m_comparison = Comp::EqualTo;
    EXPECT_STREQ(bnd.ToString().c_str(), "==1.0.0")                     << m_errToStringIncorrect;

    bnd.m_comparison = Comp::GreaterThan;
    EXPECT_STREQ(bnd.ToString().c_str(), ">1.0.0")                      << m_errToStringIncorrect;

    bnd.m_comparison = Comp::LessThan;
    EXPECT_STREQ(bnd.ToString().c_str(), "<1.0.0")                      << m_errToStringIncorrect;

    bnd.m_comparison = Comp::GreaterThan | Comp::EqualTo;
    EXPECT_STREQ(bnd.ToString().c_str(), ">=1.0.0")                     << m_errToStringIncorrect;

    bnd.m_comparison = Comp::LessThan | Comp::EqualTo;
    EXPECT_STREQ(bnd.ToString().c_str(), "<=1.0.0")                     << m_errToStringIncorrect;
}
