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

#include <SceneAPI/SceneCore/Containers/Utilities/RuleNameUnicity.h>
#include <AzCore/std/containers/vector.h>
#include <AzTest/AzTest.h>

namespace AZ
{
    namespace SceneAPI
    {
        TEST(UniqueNameGeneration, SuccessCases_WorksWithEmptyStartingSet_FindsExpectedName)
        {
            AZStd::vector<AZStd::string> testNames;
            AZStd::string result = GenerateUniqueRuleKey(testNames, "TestSet");
            EXPECT_EQ(result, "TestSet");
        }

        TEST(UniqueNameGeneration, SuccessCases_WorksWithRootNameAsSingleEntry_FindsExpectedName)
        {
            AZStd::vector<AZStd::string> testNames = { "TestSet" };
            AZStd::string result = GenerateUniqueRuleKey(testNames, "TestSet");
            EXPECT_EQ(result, "TestSet-1");
        }

        TEST(UniqueNameGeneration, SuccessCases_WorksWithSingleEntry_FindsExpectedName)
        {
            AZStd::vector<AZStd::string> testNames = { "TestSet-1" };
            AZStd::string result = GenerateUniqueRuleKey(testNames, "TestSet");
            EXPECT_EQ(result, "TestSet-2");
        }

        TEST(UniqueNameGeneration, SuccessCases_WorksWithSingleDoubleDigitEntry_FindsExpectedName)
        {
            AZStd::vector<AZStd::string> testNames = { "TestSet-12" };
            AZStd::string result = GenerateUniqueRuleKey(testNames, "TestSet");
            EXPECT_EQ(result, "TestSet-13");
        }


        TEST(UniqueNameGeneration, SuccessCases_WorksWithUnrelatedEntry_FindsExpectedName)
        {
            AZStd::vector<AZStd::string> testNames = { "Different-1" };
            AZStd::string result = GenerateUniqueRuleKey(testNames, "TestSet");
            EXPECT_EQ(result, "TestSet");
        }

        TEST(UniqueNameGeneration, SuccessCases_WorksWithSeveralEntries_FindsExpectedName)
        {
            AZStd::vector<AZStd::string> testNames = { "TestSet-1", "TestSet-2" };
            AZStd::string result = GenerateUniqueRuleKey(testNames, "TestSet");
            EXPECT_EQ(result, "TestSet-3");
        }

        TEST(UniqueNameGeneration, SuccessCases_WorksWithSeveralMixedEntries_FindsExpectedName)
        {
            AZStd::vector<AZStd::string> testNames = { "TestSet-1", "Different-1", "TestSet-2" };
            AZStd::string result = GenerateUniqueRuleKey(testNames, "TestSet");
            EXPECT_EQ(result, "TestSet-3");
        }

        TEST(UniqueNameGeneration, SuccessCases_WorksWithSeveralMixedUpOrder_FindsExpectedName)
        {
            AZStd::vector<AZStd::string> testNames = { "TestSet-2", "TestSet-3", "TestSet-1" };
            AZStd::string result = GenerateUniqueRuleKey(testNames, "TestSet");
            EXPECT_EQ(result, "TestSet-4");
        }

        TEST(UniqueNameGeneration, SuccessCases_AlwaysFindsLargest_FindsExpectedName)
        {
            AZStd::vector<AZStd::string> testNames = { "TestSet-2", "TestSet-15", "TestSet-1" };
            AZStd::string result = GenerateUniqueRuleKey(testNames, "TestSet");
            EXPECT_EQ(result, "TestSet-16");
        }

        TEST(UniqueNameGeneration, SuccessCases_AlwaysFindsLargestWithRootNamePresent_FindsExpectedName)
        {
            AZStd::vector<AZStd::string> testNames = { "TestSet-2", "TestSet", "TestSet-15", "TestSet-1" };
            AZStd::string result = GenerateUniqueRuleKey(testNames, "TestSet");
            EXPECT_EQ(result, "TestSet-16");
        }

        TEST(UniqueNameGeneration, NonStandardParameters_EmptyBaseName_ReturnsValidName)
        {
            AZStd::vector<AZStd::string> testNames = { "TestSet-1" };
            AZStd::string result = GenerateUniqueRuleKey(testNames, "");
            EXPECT_EQ(result, "1");
        }

        TEST(UniqueNameGeneration, NonStandardParameters_EmptyBaseNameWithValidSequence_ReturnsValidName)
        {
            AZStd::vector<AZStd::string> testNames = { "1", "2" };
            AZStd::string result = GenerateUniqueRuleKey(testNames, "");
            EXPECT_EQ(result, "3");
        }

        TEST(UniqueNameGeneration, NonStandardParameters_IgnoresNonNumericSuffixes_DoesntConsiderNonNumericSufixes)
        {
            AZStd::vector<AZStd::string> testNames = { "TestSet-1", "TestSet-2", "TestCase-3QWERTY" };
            AZStd::string result = GenerateUniqueRuleKey(testNames, "TestSet");
            EXPECT_EQ(result, "TestSet-3");
        }

        TEST(UniqueNameGeneration, MixedCasing_MixedCasing_SkipsInvalidMixedCasing)
        {
            AZStd::vector<AZStd::string> testNames = { "TestSet-1", "testSet-2" };
            AZStd::string result = GenerateUniqueRuleKey(testNames, "TestSet");
            EXPECT_EQ(result, "TestSet-2");
        }
    }
}