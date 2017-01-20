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
#include <GemRegistry/GemVersion.h>

using Gems::GemVersion;

class VersionTest
    : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_errParseFailed            = "Failed to parse valid version string.";
        m_errParseInvalidSucceeded  = "Parsing invalid version string succeeded.";
        m_errParseInvalid           = "ParseFromString resulted in incorrect version.";
        m_errCompareIncorrect       = "Result of Compare is incorrect.";
        m_errToStringIncorrect      = "ToString result is incorrect.";
    }

    const char* m_errParseFailed;
    const char* m_errParseInvalidSucceeded;
    const char* m_errParseInvalid;
    const char* m_errCompareIncorrect;
    const char* m_errToStringIncorrect;
};

TEST_F(VersionTest, ParsingTest)
{
    auto versionOutcome = GemVersion::ParseFromString("1.2.3");
    ASSERT_TRUE(versionOutcome.IsSuccess())         << m_errParseFailed;

    GemVersion v0 = versionOutcome.GetValue();

    ASSERT_EQ(v0.m_major, 1)                        << m_errParseInvalid;
    ASSERT_EQ(v0.m_minor, 2)                        << m_errParseInvalid;
    ASSERT_EQ(v0.m_patch, 3)                        << m_errParseInvalid;

    ASSERT_FALSE(GemVersion::ParseFromString("NotAVersion").IsSuccess()) << m_errParseInvalidSucceeded;
}

TEST_F(VersionTest, CompareTest)
{
    GemVersion v1 = { 1, 0, 0 };
    GemVersion v2 = { 2, 0, 0 };
    GemVersion v3 = { 1, 1, 0 };
    GemVersion v4 = { 1, 1, 0 };

    ASSERT_LT(GemVersion::Compare(v1, v2), 0)       << m_errCompareIncorrect;
    ASSERT_LT(GemVersion::Compare(v1, v3), 0)       << m_errCompareIncorrect;
    ASSERT_GT(GemVersion::Compare(v2, v3), 0)       << m_errCompareIncorrect;
    ASSERT_EQ(GemVersion::Compare(v3, v4), 0)       << m_errCompareIncorrect;

    ASSERT_TRUE(v1 < v2)                            << m_errCompareIncorrect;
    ASSERT_TRUE(v1 <= v2)                           << m_errCompareIncorrect;
    ASSERT_TRUE(v1 < v3)                            << m_errCompareIncorrect;
    ASSERT_TRUE(v1 <= v3)                           << m_errCompareIncorrect;
    ASSERT_TRUE(v2 > v3)                            << m_errCompareIncorrect;
    ASSERT_TRUE(v2 >= v3)                           << m_errCompareIncorrect;
    ASSERT_TRUE(v3 == v4)                           << m_errCompareIncorrect;
    ASSERT_TRUE(v1 != v2)                           << m_errCompareIncorrect;
}

TEST_F(VersionTest, ToStringTest)
{
    const char* versionString = "1.2.3";
    auto versionOutcome = GemVersion::ParseFromString(versionString);
    ASSERT_TRUE(versionOutcome.IsSuccess())             << m_errParseFailed;
    GemVersion v0 = versionOutcome.GetValue();
    GemVersion v1 = { 1, 0, 0 };
    GemVersion v2 = { 2, 0, 0 };
    GemVersion v3 = { 1, 1, 0 };
    GemVersion v4 = { 1, 1, 0 };

    ASSERT_STREQ(v0.ToString().c_str(), versionString)  << m_errToStringIncorrect;
    ASSERT_STREQ(v1.ToString().c_str(), "1.0.0")        << m_errToStringIncorrect;
    ASSERT_STREQ(v2.ToString().c_str(), "2.0.0")        << m_errToStringIncorrect;
    ASSERT_STREQ(v3.ToString().c_str(), "1.1.0")        << m_errToStringIncorrect;
    ASSERT_STREQ(v4.ToString().c_str(), "1.1.0")        << m_errToStringIncorrect;
}
