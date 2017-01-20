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
#ifdef UNIT_TEST
#include "AssetProcessingStateDataUnitTests.h"
#include <AzCore/std/string/string.h>
#include "native/AssetManager/AssetProcessingStateData.h"
#include <QTemporaryDir>
#include <QStringList>
#include <QString>
#include <QDir>
#include <QDebug>
#include <QSet>
#include <AzToolsFramework/API/AssetDatabaseBus.h>
#include "native/AssetDatabase/AssetDatabase.h"

namespace AssetProcessingStateDataUnitTestInternal
{
    // a utility class to redirect the location the database is stored to a different location so that we don't
    // touch real data during unit tests.
    class FakeDatabaseLocationListener
        : protected AzToolsFramework::AssetDatabase::AssetDatabaseRequests::Bus::Handler
    {
    public:
        FakeDatabaseLocationListener(const char* desiredLocation)
            : m_location(desiredLocation)
        {
            BusConnect();
        }
    protected:
        // IMPLEMENTATION OF -------------- AzToolsFramework::AssetDatabase::AssetDatabaseRequests::Bus::Listener
        virtual void GetDatabaseLocation(AZStd::string& result) override
        {
            result = m_location;
        }
        // ------------------------------------------------------------

    private:
        AZStd::string m_location = "test.sqlite";
    };
}

// perform some operations on the state data given.  (Does not perform save and load tests)
void AssetProcessingStateDataUnitTest::BasicDataTest(AssetProcessor::LegacyDatabaseInterface* stateData)
{
    QStringList dummy;
    QString outName;
    QString outPlat;
    QString outJobDescription;

    UNIT_TEST_EXPECT_TRUE(stateData->GetFingerprint("ABCDE", "es3", "xyz") == 0);

    // put 2 keys in for the same asset, different platform
    // also put a key in for a different asset, same platform
    stateData->SetFingerprint("test1.txt", "es3", "xyz", 1);
    stateData->SetFingerprint("test1.txt", "durango", "xyz", 2);
    stateData->SetFingerprint("test2.txt", "pc", "xyz", 3);
    stateData->SetFingerprint("Test3.txt", "pc", "xyz", 4);
    stateData->SetFingerprint("Bow_gus.txt.png", "bowgus", "entry", 5); // made to not show up in any queries
    stateData->SetFingerprint("Bow!gustxt", "bowgus", "entry", 6); // made to not show up in any queries and be similar except for special _ character

    if (stateData->GetFingerprint("test1.txt", "es3", "xyz") != 1)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - [test1 es3] should be 1");
        return;
    }

    if (stateData->GetFingerprint("test1.txt", "durango", "xyz") != 2)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - [test1 durango] should be 2");
        return;
    }

    if (stateData->GetFingerprint("test1.txt", "pc", "xyz") != 0)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - [test1 pc] should be 0 (not exist)");
        return;
    }

    if (stateData->GetFingerprint("test2.txt", "pc", "xyz") != 3)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - [test2 pc] should be 3");
        return;
    }

    if (stateData->GetFingerprint("test2.txt", "durango", "xyz") != 0)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - [test2 durango] should be 0 (not exist)");
        return;
    }

    if (stateData->GetFingerprint("Bow_gus.txt.png", "bowgus", "entry") != 5)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - [test3 pc] should be 4");
        return;
    }

    if (stateData->GetFingerprint("Bow!gustxt", "bowgus", "entry") != 6)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - [test3 pc] should be 4");
        return;
    }

    // --------- test GetSourceFileNames -----
    QSet<QString> sources;
    stateData->GetSourceFileNames(sources);
    UNIT_TEST_EXPECT_TRUE(sources.count() == 5); // we want non-duplicates and while there are 6 entries above, only 5 are unique (one is the same but for a diff platform)
    UNIT_TEST_EXPECT_TRUE(sources.contains("test1.txt"));
    UNIT_TEST_EXPECT_TRUE(sources.contains("test2.txt"));
    UNIT_TEST_EXPECT_TRUE(sources.contains("Test3.txt")); // you may NOT lose the case of source files.
    UNIT_TEST_EXPECT_TRUE(sources.contains("Bow_gus.txt.png")); // you may NOT lose the case of source files or mess up the _
    UNIT_TEST_EXPECT_TRUE(sources.contains("Bow!gustxt")); // you may NOT lose the case of source files or mess up the !

    sources.clear();

    stateData->GetMatchingSourceFiles("tEsT", sources);

    UNIT_TEST_EXPECT_TRUE(sources.count() == 3); // we want non-duplicates.  note that the "bow" gus entry must not appear
    UNIT_TEST_EXPECT_TRUE(sources.contains("test1.txt"));
    UNIT_TEST_EXPECT_TRUE(sources.contains("test2.txt"));
    UNIT_TEST_EXPECT_TRUE(sources.contains("Test3.txt")); // you may NOT lose the case of source files.

    // test underscores:

    sources.clear();
    stateData->GetMatchingSourceFiles("bow_g", sources);
    UNIT_TEST_EXPECT_TRUE(sources.count() == 1); // there should only be one match here.
    UNIT_TEST_EXPECT_TRUE(sources.contains("Bow_gus.txt.png")); // you may NOT lose the case of source files or mess up the _ or replace it with the !

    sources.clear();
    stateData->GetMatchingSourceFiles("bow!g", sources);
    UNIT_TEST_EXPECT_TRUE(sources.count() == 1); // there should only be one match here.
    UNIT_TEST_EXPECT_TRUE(sources.contains("Bow!gustxt")); // you may NOT lose the case of source files or mess up the ! or replace it with the _

    sources.clear();
    stateData->GetSourceWithExtension("bow!g", sources);
    UNIT_TEST_EXPECT_TRUE(sources.count() == 0); // no matches are expected

    sources.clear();
    stateData->GetSourceWithExtension(".txt", sources);
    UNIT_TEST_EXPECT_TRUE(sources.count() == 3); // three UNIQUE sources, four actual platform sources
    UNIT_TEST_EXPECT_TRUE(sources.contains("test1.txt"));
    UNIT_TEST_EXPECT_TRUE(sources.contains("test2.txt"));
    UNIT_TEST_EXPECT_TRUE(sources.contains("Test3.txt")); // you may NOT lose the case of source files.

    sources.clear();
    stateData->GetSourceWithExtension(".png", sources);
    UNIT_TEST_EXPECT_TRUE(sources.count() == 1); // three UNIQUE sources, four actual platform sources
    UNIT_TEST_EXPECT_TRUE(sources.contains("Bow_gus.txt.png")); // you may NOT lose the case of source files.

    // -------------------- Test products inputs mappings ------------------

    if (stateData->GetProductsForSource("abc", "def", "xyz", dummy))
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - GetProductsForSource(abc def) should have failed.");
        return;
    }

    if (stateData->GetProductsForSource("", "", "", dummy))
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - GetProductsForSource( ) should have failed.");
        return;
    }

    dummy.append("test1durango");
    dummy.append("test2durango");
    dummy.append("bogusthatshouldnotappearinsearch");

    stateData->SetProductsForSource("abc", "durango", "xyz", dummy);

    dummy.clear();

    // We should not be able to find any products.  the above function should have failed
    // becuase we have not yet actually supplied a fingerprint yet for that product.
    if ((stateData->GetProductsForSource("abc", "durango", "xyz", dummy)) || (dummy.size() != 0))
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - a call to GetProductsForSource which should have failed did not");
        return;
    }

    dummy.append("test1durango");
    dummy.append("test2durango");
    dummy.append("bogusthatshouldnotappearinsearch");

    stateData->SetFingerprint("abc", "durango", "xyz", 123);
    stateData->SetFingerprint("abc", "pc", "xyz", 456);
    stateData->SetFingerprint("XYz", "pc", "xyZ", 789);

    stateData->SetProductsForSource("abc", "durango", "xyz", dummy);

    sources.clear();
    stateData->GetMatchingProductFiles("tEs", sources);
    UNIT_TEST_EXPECT_TRUE(sources.count() == 2); // we want non-duplicates.  note that the "bow" gus entry must not appear
    UNIT_TEST_EXPECT_TRUE(sources.contains("test1durango"));
    UNIT_TEST_EXPECT_TRUE(sources.contains("test2durango"));


    dummy.clear();
    dummy.append("test1pc");
    dummy.append("test2pc");
    stateData->SetProductsForSource("abc", "pc", "xyz", dummy);
    dummy.clear();
    dummy.append("test10pc");
    dummy.append("test11pc");
    stateData->SetProductsForSource("XYz", "pc", "xyZ", dummy);
    dummy.clear();

    if ((!stateData->GetProductsForSource("abc", "durango", "xyz", dummy)) || (dummy.size() != 3) || (dummy[0] != "test1durango") || (dummy[1] != "test2durango") || (dummy[2] != "bogusthatshouldnotappearinsearch"))
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - keys expected not found");
        return;
    }

    dummy.clear();
    if ((!stateData->GetProductsForSource("abc", "pc", "xyz", dummy)) || (dummy.size() != 2) || (dummy[0] != "test1pc") || (dummy[1] != "test2pc"))
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - keys expected not found");
        return;
    }

    dummy.clear();
    if (stateData->GetProductsForSource("abc", "es3", "xyz", dummy))
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - GetProductsForSource( ) should have failed on unknown platform");
        return;
    }

    if (stateData->GetProductsForSource("def", "durango", "xyz", dummy))
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - GetProductsForSource( ) should have failed on unknown name");
        return;
    }

    dummy.clear();

    if ((!stateData->GetProductsForSource("xyz", "pc", "xyz", dummy)) || (dummy.size() != 2) || (dummy[0] != "test10pc") || (dummy[1] != "test11pc"))
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - keys expected not found");
        return;
    }

    // -------- reverse lookup --------------
    if ((!stateData->GetSourceFromProduct("test1durango", outName, outPlat, outJobDescription)) ||
        (outName != "abc") ||
        (outPlat != "durango") ||
        (outJobDescription != "xyz"))
    {
        Q_EMIT UnitTestFailed("Reverse look up Failed - keys expected not found");
        return;
    }

    if ((!stateData->GetSourceFromProduct("test2durango", outName, outPlat, outJobDescription)) ||
        (outName != "abc") ||
        (outPlat != "durango") ||
        (outJobDescription != "xyz"))
    {
        Q_EMIT UnitTestFailed("Reverse look up Failed - keys expected not found");
        return;
    }

    if ((!stateData->GetSourceFromProduct("test1pc", outName, outPlat, outJobDescription)) ||
        (outName != "abc") ||
        (outPlat != "pc") ||
        (outJobDescription != "xyz"))
    {
        Q_EMIT UnitTestFailed("Reverse look up Failed - keys expected not found");
        return;
    }

    if ((!stateData->GetSourceFromProduct("test2pc", outName, outPlat, outJobDescription)) ||
        (outName != "abc") ||
        (outPlat != "pc") ||
        (outJobDescription != "xyz"))
    {
        Q_EMIT UnitTestFailed("Reverse look up Failed - keys expected not found");
        return;
    }

    if (stateData->GetSourceFromProduct("test3pc", outName, outPlat, outJobDescription))
    {
        Q_EMIT UnitTestFailed("Reverse look up failed - keys should not have been found! (1)");
        return;
    }

    // note the capitalization, we must ensure that the source path/name and the description
    // have the correct case
    if ((!stateData->GetSourceFromProduct("test10pc", outName, outPlat, outJobDescription)) ||
        (outName != "XYz") ||
        (outPlat != "pc") ||
        (outJobDescription != "xyZ"))
    {
        Q_EMIT UnitTestFailed("Reverse look up Failed - keys expected not found");
        return;
    }

    dummy.clear();
    dummy.append("test3pc");
    dummy.append("test4pc");
    stateData->SetProductsForSource("abc", "pc", "xyz", dummy); // overwrite it!
    dummy.clear();
    dummy.append("test3durango");
    dummy.append("test4durango");


    stateData->SetFingerprint("def", "durango", "xyz", 123);
    stateData->SetFingerprint("ghi", "pc", "xyz", 123);

    stateData->SetProductsForSource("def", "durango", "xyz", dummy); // different key, same platform
    stateData->SetProductsForSource("ghi", "pc", "xyz", QStringList()); // empty is ok!

    dummy.clear();
    if ((!stateData->GetProductsForSource("abc", "pc", "xyz", dummy)) || (dummy.size() != 2) || (dummy[0] != "test3pc") || (dummy[1] != "test4pc"))
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - keys expected not found after overwrite");
        return;
    }

    dummy.clear();
    if ((!stateData->GetProductsForSource("def", "durango", "xyz", dummy)) || (dummy.size() != 2) || (dummy[0] != "test3durango") || (dummy[1] != "test4durango"))
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - keys expected not found after overwrite (2)");
        return;
    }

    dummy.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetProductsForSource("ghi", "pc", "xyz", dummy)); // you must return true because we do know about the source.
    UNIT_TEST_EXPECT_TRUE(dummy.isEmpty()); // but no results should be published.

    // ------------- reverse lookup overwrites --------------
    // test2pc should not exist anymore, because teh above code should have overwritten "abc", "pc" with the new ones:

    if (stateData->GetSourceFromProduct("test1pc", outName, outPlat, outJobDescription))
    {
        Q_EMIT UnitTestFailed("Reverse look up failed - keys should not have been found! (2)");
        return;
    }

    if (stateData->GetSourceFromProduct("test2pc", outName, outPlat, outJobDescription))
    {
        Q_EMIT UnitTestFailed("Reverse look up failed - keys should not have been found! (3)");
        return;
    }

    if ((!stateData->GetSourceFromProduct("test3pc", outName, outPlat, outJobDescription)) ||
        (outName != "abc") ||
        (outPlat != "pc") ||
        (outJobDescription != "xyz"))
    {
        Q_EMIT UnitTestFailed("Reverse look up Failed - keys expected not found");
        return;
    }

    if ((!stateData->GetSourceFromProduct("test4pc", outName, outPlat, outJobDescription)) ||
        (outName != "abc") ||
        (outPlat != "pc") ||
        (outJobDescription != "xyz"))
    {
        Q_EMIT UnitTestFailed("Reverse look up Failed - keys expected not found");
        return;
    }


    if ((!stateData->GetSourceFromProduct("test3durango", outName, outPlat, outJobDescription)) ||
        (outName != "def") ||
        (outPlat != "durango") ||
        (outJobDescription != "xyz"))
    {
        Q_EMIT UnitTestFailed("Reverse look up Failed - keys expected not found");
        return;
    }

    if ((!stateData->GetSourceFromProduct("test4durango", outName, outPlat, outJobDescription)) ||
        (outName != "def") ||
        (outPlat != "durango") ||
        (outJobDescription != "xyz"))
    {
        Q_EMIT UnitTestFailed("Reverse look up Failed - keys expected not found");
        return;
    }

    // ----------- Test ClearProducts   ------------
    stateData->SetFingerprint("oooo", "someplat", "somejob", 123);

    dummy.clear();
    dummy.append("a dummy file");
    dummy.append("a dummy file 2");

    stateData->SetProductsForSource("oooo", "someplat", "somejob", dummy); // different key, same platform
    dummy.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetProductsForSource("oooo", "someplat", "somejob", dummy) && dummy.size() == 2);
    dummy.clear();
    stateData->ClearProducts("oooo", "someplat", "somejob");
    UNIT_TEST_EXPECT_TRUE(stateData->GetProductsForSource("oooo", "someplat", "somejob", dummy));
    // deleting the products does NOT delete the fingerprint and the above function must only return false if there were no SOURCE files (ie, invalid call)
    // not if there are no products.
    UNIT_TEST_EXPECT_TRUE(dummy.isEmpty());

    // ---------- CLEARING A FINGERPRINT SHOULD CLEAR ITS PRODUCTS TOO BUT ONLY THOSE! ----------
    stateData->SetFingerprint("oooo", "someplat", "somejob", 123);
    stateData->SetFingerprint("ooooA", "someplat", "somejob", 456);
    stateData->SetFingerprint("oooo", "someplatA", "somejob", 789);
    stateData->SetFingerprint("oooo", "someplat", "somejobA", 012); // a whole bunch of similar names to make sure it doesn't delete them all
    dummy.append("a dummy file");
    dummy.append("a dummy file 2");
    stateData->SetProductsForSource("oooo", "someplat", "somejob", dummy); // put the products back
    stateData->SetProductsForSource("ooooA", "someplat", "somejob", dummy); // put the products back
    stateData->SetProductsForSource("oooo", "someplatA", "somejob", dummy); // put the products back
    stateData->SetProductsForSource("oooo", "someplat", "somejobA", dummy); // put the products back
    dummy.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetProductsForSource("oooo", "someplat", "somejob", dummy) && dummy.size() == 2);
    dummy.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetProductsForSource("ooooA", "someplat", "somejob", dummy) && dummy.size() == 2);
    dummy.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetProductsForSource("oooo", "someplatA", "somejob", dummy) && dummy.size() == 2);
    dummy.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetProductsForSource("oooo", "someplat", "somejobA", dummy) && dummy.size() == 2);
    dummy.clear();

    UNIT_TEST_EXPECT_TRUE(stateData->GetFingerprint("oooo", "someplat", "somejob") == 123);
    stateData->ClearFingerprint("oooo", "someplat", "somejob");
    UNIT_TEST_EXPECT_TRUE(stateData->GetFingerprint("oooo", "someplat", "somejob") == 0);
    UNIT_TEST_EXPECT_FALSE(stateData->GetProductsForSource("oooo", "someplat", "somejob", dummy)); // now that we've cleared the fingerprint, this must return false (invalid call!)

    // make sure the others were NOT touched:
    dummy.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetProductsForSource("ooooA", "someplat", "somejob", dummy) && dummy.size() == 2);
    dummy.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetProductsForSource("oooo", "someplatA", "somejob", dummy) && dummy.size() == 2);
    dummy.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetProductsForSource("oooo", "someplat", "somejobA", dummy) && dummy.size() == 2);
}

void AssetProcessingStateDataUnitTest::AdvancedDataTest(AssetProcessor::LegacyDatabaseInterface* stateData)
{
    QStringList dummy;
    QString outName;
    QString outPlat;
    QString outJobDescription;
    QStringList jobDescriptions;

    stateData->SetFingerprint("test1", "es3", "xyz", 5); // overwrite existing!
    if (stateData->GetFingerprint("test1", "es3", "xyz") != 5)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - [test1 es3] should be 5 after reload and apply");
        return;
    }

    if (stateData->GetSourceFromProduct("test1pc", outName, outPlat, outJobDescription))
    {
        Q_EMIT UnitTestFailed("Reverse look up failed - keys should not have been found! (4)");
        return;
    }

    // -- make sure products came back:
    dummy.clear();
    if ((!stateData->GetProductsForSource("abc", "pc", "xyz", dummy)) || (dummy.size() != 2) || (dummy[0] != "test3pc") || (dummy[1] != "test4pc"))
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - keys expected not found after overwrite");
        return;
    }

    dummy.clear();
    if ((!stateData->GetProductsForSource("def", "durango", "xyz", dummy)) || (dummy.size() != 2) || (dummy[0] != "test3durango") || (dummy[1] != "test4durango"))
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - keys expected not found after overwrite (2)");
        return;
    }


    // -- reverse lookup - make sure the values have been restored!

    if (stateData->GetSourceFromProduct("test2pc", outName, outPlat, outJobDescription))
    {
        Q_EMIT UnitTestFailed("Reverse look up failed - keys should not have been found! (5)");
        return;
    }

    if ((!stateData->GetSourceFromProduct("test3pc", outName, outPlat, outJobDescription)) ||
        (outName != "abc") ||
        (outPlat != "pc") ||
        (outJobDescription != "xyz"))
    {
        Q_EMIT UnitTestFailed("Reverse look up Failed - keys expected not found");
        return;
    }

    if ((!stateData->GetSourceFromProduct("test4pc", outName, outPlat, outJobDescription)) ||
        (outName != "abc") ||
        (outPlat != "pc") ||
        (outJobDescription != "xyz"))
    {
        Q_EMIT UnitTestFailed("Reverse look up Failed - keys expected not found");
        return;
    }


    if ((!stateData->GetSourceFromProduct("test3durango", outName, outPlat, outJobDescription)) ||
        (outName != "def") ||
        (outPlat != "durango") ||
        (outJobDescription != "xyz"))
    {
        Q_EMIT UnitTestFailed("Reverse look up Failed - keys expected not found");
        return;
    }

    if ((!stateData->GetSourceFromProduct("test4durango", outName, outPlat, outJobDescription)) ||
        (outName != "def") ||
        (outPlat != "durango") ||
        (outJobDescription != "xyz"))
    {
        Q_EMIT UnitTestFailed("Reverse look up Failed - keys expected not found");
        return;
    }


    //-----Unit Test for GetJobDescriptionsForSource function



    stateData->GetJobDescriptionsForSource("test1", "es3", jobDescriptions);

    UNIT_TEST_EXPECT_TRUE(jobDescriptions.size() == 1);
    UNIT_TEST_EXPECT_TRUE(jobDescriptions[0] == "xyz");

    stateData->SetFingerprint("test1", "es3", "xxx", 1);

    jobDescriptions.clear();

    //Get the job description list again
    stateData->GetJobDescriptionsForSource("test1", "es3", jobDescriptions);

    UNIT_TEST_EXPECT_TRUE(jobDescriptions.size() == 2);

    UNIT_TEST_EXPECT_TRUE(jobDescriptions[0] == "xyz" || jobDescriptions[0] == "xxx");

    UNIT_TEST_EXPECT_TRUE(jobDescriptions[1] == "xyz" || jobDescriptions[1] == "xxx");

    jobDescriptions.clear();

    //Get the job description list again
    stateData->GetJobDescriptionsForSource("TeSt1", "es3", jobDescriptions);

    //------same result as above--------------
    UNIT_TEST_EXPECT_TRUE(jobDescriptions.size() == 2);

    UNIT_TEST_EXPECT_TRUE(jobDescriptions[0] == "xyz" || jobDescriptions[0] == "xxx");

    UNIT_TEST_EXPECT_TRUE(jobDescriptions[1] == "xyz" || jobDescriptions[1] == "xxx");

    jobDescriptions.clear();

    stateData->GetJobDescriptionsForSource("test1", "wrongInput", jobDescriptions);

    UNIT_TEST_EXPECT_TRUE(jobDescriptions.size() == 0);

    stateData->GetJobDescriptionsForSource("wrongInput", "es3", jobDescriptions);

    UNIT_TEST_EXPECT_TRUE(jobDescriptions.size() == 0);

    stateData->GetJobDescriptionsForSource("", "", jobDescriptions);

    UNIT_TEST_EXPECT_TRUE(jobDescriptions.size() == 0);

    stateData->SetFingerprint("testfile", "pc", "YOU", 89);

    jobDescriptions.clear();

    stateData->GetJobDescriptionsForSource("testfile", "pc", jobDescriptions);

    UNIT_TEST_EXPECT_TRUE(jobDescriptions.size() == 1);

    UNIT_TEST_EXPECT_TRUE(jobDescriptions[0].compare("you", Qt::CaseInsensitive) == 0);
}

void AssetProcessingStateDataUnitTest::TestReloadedData(AssetProcessor::LegacyDatabaseInterface* stateData)
{
    // make sure that data in the prior 2 tests has persisted.
    QStringList dummy;
    QString outName;
    QString outPlat;
    QString outJobDescription;
    QStringList jobDescriptions;

    stateData->GetJobDescriptionsForSource("testfile", "pc", jobDescriptions);

    UNIT_TEST_EXPECT_TRUE(jobDescriptions.size() == 1);

    UNIT_TEST_EXPECT_TRUE(jobDescriptions[0].compare("you", Qt::CaseInsensitive) == 0);

    // -- make sure products came back:
    dummy.clear();
    if ((!stateData->GetProductsForSource("abc", "pc", "xyz", dummy)) || (dummy.size() != 2) || (dummy[0] != "test3pc") || (dummy[1] != "test4pc"))
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - keys expected not found after overwrite");
        return;
    }

    dummy.clear();
    if ((!stateData->GetProductsForSource("def", "durango", "xyz", dummy)) || (dummy.size() != 2) || (dummy[0] != "test3durango") || (dummy[1] != "test4durango"))
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - keys expected not found after overwrite (2)");
        return;
    }


    // -- reverse lookup - make sure the values have been restored!

    if (stateData->GetSourceFromProduct("test2pc", outName, outPlat, outJobDescription))
    {
        Q_EMIT UnitTestFailed("Reverse look up failed - keys should not have been found! (6)");
        return;
    }

    if ((!stateData->GetSourceFromProduct("test3pc", outName, outPlat, outJobDescription)) ||
        (outName != "abc") ||
        (outPlat != "pc") ||
        (outJobDescription != "xyz"))
    {
        Q_EMIT UnitTestFailed("Reverse look up Failed - keys expected not found");
        return;
    }

    if ((!stateData->GetSourceFromProduct("test4pc", outName, outPlat, outJobDescription)) ||
        (outName != "abc") ||
        (outPlat != "pc") ||
        (outJobDescription != "xyz"))
    {
        Q_EMIT UnitTestFailed("Reverse look up Failed - keys expected not found");
        return;
    }


    if ((!stateData->GetSourceFromProduct("test3durango", outName, outPlat, outJobDescription)) ||
        (outName != "def") ||
        (outPlat != "durango") ||
        (outJobDescription != "xyz"))
    {
        Q_EMIT UnitTestFailed("Reverse look up Failed - keys expected not found");
        return;
    }

    if ((!stateData->GetSourceFromProduct("test4durango", outName, outPlat, outJobDescription)) ||
        (outName != "def") ||
        (outPlat != "durango") ||
        (outJobDescription != "xyz"))
    {
        Q_EMIT UnitTestFailed("Reverse look up Failed - keys expected not found");
        return;
    }
}

void AssetProcessingStateDataUnitTest::ExistenceTest(AssetProcessor::LegacyDatabaseInterface* stateData)
{
    UNIT_TEST_EXPECT_FALSE(stateData->DataExists());
    stateData->ClearData(); // this is expected to initialize a database.
    UNIT_TEST_EXPECT_TRUE(stateData->DataExists());
}

void AssetProcessingStateDataUnitTest::TestClearData(AssetProcessor::LegacyDatabaseInterface* stateData)
{
    QStringList dummy;
    dummy.append("a dummy file");
    dummy.append("a dummy file 2");

    stateData->SetFingerprint("oooo", "someplat", "somejob", 123);
    stateData->SetProductsForSource("oooo", "someplat", "somejob", dummy); // different key, same platform

    dummy.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetProductsForSource("oooo", "someplat", "somejob", dummy) && dummy.size() == 2);

    stateData->ClearData();

    QSet<QString> allFiles;
    stateData->GetSourceFileNames(allFiles);
}

void AssetProcessingStateDataUnitTest::AssetProcessingStateDataTest()
{
    using namespace AssetProcessingStateDataUnitTestInternal;

    QStringList dummy;
    QTemporaryDir tempDir;
    QDir dirPath(tempDir.path());

    bool testsFailed = false;
    connect(this, &UnitTestRun::UnitTestFailed, this, [&testsFailed]()
    {
        testsFailed = true;
    }, Qt::DirectConnection);

    {
        FakeDatabaseLocationListener listener(dirPath.filePath("converteddata.sqlite").toUtf8().constData());

        // during this test, we will store the data in coverteddata.sqlite
        // after each step along the way, we will blow away this data and tell the original statedata to migrate to the database
        // this allows us to make sure that the same results occur regardless of whether its starting from scratch or via migration
        // basic test of the original state data will also test to make sure the new database port is identical.

        QFile::remove(dirPath.filePath("statetest.ini"));
        AssetProcessor::AssetProcessingStateData stateData(nullptr);
        stateData.SetStoredLocation(dirPath.filePath("statetest.ini"));
        AssetProcessor::DatabaseConnection connection;
        UNIT_TEST_EXPECT_TRUE(connection.OpenDatabase()); // initializes it too

        ExistenceTest(&stateData);
        if (testsFailed)
        {
            return;
        }

        connection.ClearData(); // reset the database.
        stateData.MigrateToDatabase(connection);
        BasicDataTest(&stateData);
        BasicDataTest(&connection); // since the database now has the same state, it should pass the same tests:
        if (testsFailed)
        {
            return;
        }

        // an additional test of SaveData, which only exists on the ini-based state database (legacy)
        // strategy here is to save the data, alter it, then load the data and make sure it went back to what it was
        // at the point of save.
        {
            stateData.SetFingerprint("test2", "pc", "xyz", 3);
            stateData.SaveData();

            stateData.ClearProducts("def", "durango", "xyz");
            if (!stateData.GetProductsForSource("def", "durango", "xyz", dummy))
            {
                // GetProductsFromSource should only fail if the FINGERPRINT (source file) is deleted, since its whether or not the call succeeded.
                // its possible to have input files with no products, and it not be an error.
                Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - GetProductsForSource( ) should NOT have failed after clear");
                return;
            }

            stateData.ClearFingerprint("test2", "pc", "xyz");
            // ClearData must immediately clear data in memory cache
            if (stateData.GetFingerprint("test2", "pc", "xyz") != 0)
            {
                Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - [test2 pc] should be 0 (not exist) after clearing");
                return;
            }

            stateData.ClearFingerprint("abc", "pc", "xyz");

            if (stateData.GetProductsForSource("abc", "pc", "xyz", dummy))
            {
                Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed");
                return;
            }

            stateData.LoadData(); // reload from cache we are supposed to be back to what we were before savedata.

            if (stateData.GetFingerprint("test2", "pc", "xyz") != 3)
            {
                Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - [test2 pc] should be 3 after reloading!");
                return;
            }
        }

        connection.ClearData(); // reset the database.
        stateData.MigrateToDatabase(connection);
        AdvancedDataTest(&stateData);
        AdvancedDataTest(&connection); // since the database now has the same state, it should pass the same tests

        if (testsFailed)
        {
            return;
        }

        connection.ClearData(); // reset the database.
        stateData.MigrateToDatabase(connection);
        TestReloadedData(&stateData);
        TestReloadedData(&connection); // since the database now has the same state, it should pass the same tests
        if (testsFailed)
        {
            return;
        }
        // allow stateData to go out of scope, this should write the file automatically
    }

    {
        // this test makes sure that the reload of the data worked:
        AssetProcessor::AssetProcessingStateData stateData(nullptr);
        stateData.SetStoredLocation(dirPath.filePath("statetest.ini"));
        stateData.LoadData();
        if (stateData.GetFingerprint("test1", "es3", "xyz") != 5)
        {
            Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - [test1 es3] should be 5 after out-of-scope");
            return;
        }

        TestReloadedData(&stateData);
        if (testsFailed)
        {
            return;
        }
    }

    // now test the SQLIte version of the database on its own.
    {
        FakeDatabaseLocationListener listener(dirPath.filePath("statedatabase.sqlite").toUtf8().constData());
        AssetProcessor::DatabaseConnection connection;

        ExistenceTest(&connection);
        if (testsFailed)
        {
            return;
        }

        BasicDataTest(&connection);
        if (testsFailed)
        {
            return;
        }
        AdvancedDataTest(&connection);
        if (testsFailed)
        {
            return;
        }
        TestReloadedData(&connection);
        if (testsFailed)
        {
            return;
        }

        connection.CloseDatabase();
        connection.OpenDatabase();

        TestReloadedData(&connection);
        if (testsFailed)
        {
            return;
        }
    }

    Q_EMIT UnitTestPassed();
}

void AssetProcessingStateDataUnitTest::StartTest()
{
    AssetProcessingStateDataTest();
}

REGISTER_UNIT_TEST(AssetProcessingStateDataUnitTest)

#include <native/unittests/AssetProcessingStateDataUnitTests.moc>

#endif //UNIT_TEST
