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
#include "PlatformConfigurationUnitTests.h"
#include "native/utilities/PlatformConfiguration.h"
#include "native/AssetManager/assetScanFolderInfo.h"
#include "native/utilities/assetUtils.h"
#include <QRegExp>
#include <QSet>
#include <QString>
#include <QTemporaryDir>
#include <QDir>

using namespace UnitTestUtils;
using namespace AssetProcessor;

void PlatformConfigurationTests::StartTest()
{
    {
        // put everything in a scope so that it dies.
        QTemporaryDir tempEngineRoot;
        QDir tempPath(tempEngineRoot.path());

        QSet<QString> expectedFiles;
        // set up some interesting files:
        expectedFiles << tempPath.absoluteFilePath("rootfile2.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder1/rootfile1.txt"); // note:  Must override the actual root file
        expectedFiles << tempPath.absoluteFilePath("subfolder1/basefile.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder2/basefile.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder2/aaa/basefile.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder2/aaa/bbb/basefile.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder2/aaa/bbb/ccc/basefile.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder2/aaa/bbb/ccc/ddd/basefile.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder2/subfolder1/override.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder3/basefile.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder4/a/testfile.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder5/a/testfile.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder6/a/testfile.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder7/a/testfile.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder8/x/a/testfile.txt");


        // subfolder3 is not recursive so none of these should show up in any scan or override check
        expectedFiles << tempPath.absoluteFilePath("subfolder3/aaa/basefile.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder3/aaa/bbb/basefile.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder3/aaa/bbb/ccc/basefile.txt");

        expectedFiles << tempPath.absoluteFilePath("subfolder3/rootfile3.txt"); // must override rootfile3 in root
        expectedFiles << tempPath.absoluteFilePath("rootfile1.txt");
        expectedFiles << tempPath.absoluteFilePath("rootfile3.txt");
        expectedFiles << tempPath.absoluteFilePath("unrecognised.file"); // a file that should not be recognised
        expectedFiles << tempPath.absoluteFilePath("unrecognised2.file"); // a file that should not be recognised
        expectedFiles << tempPath.absoluteFilePath("subfolder1/test/test.format"); // a file that should be recognised
        expectedFiles << tempPath.absoluteFilePath("test.format"); // a file that should NOT be recognised

        expectedFiles << tempPath.absoluteFilePath("GameNameButWithExtra/somefile.meo"); // a file that lives in a folder that must not be mistaken for the wrong scan folder
        expectedFiles << tempPath.absoluteFilePath("GameName/otherfile.meo"); // a file that lives in a folder that must not be mistaken for the wrong scan folder

        for (const QString& expect : expectedFiles)
        {
            UNIT_TEST_EXPECT_TRUE(CreateDummyFile(expect));
        }

        PlatformConfiguration config;

        config.AddScanFolder(ScanFolderInfo(tempPath.filePath("subfolder3"), "", false, false), true); // subfolder 3 overrides subfolder2
        config.AddScanFolder(ScanFolderInfo(tempPath.filePath("subfolder2"), "", false, true), true); // subfolder 2 overrides subfolder1
        config.AddScanFolder(ScanFolderInfo(tempPath.filePath("subfolder1"), "subfolder1", false, true), true); // subfolder1 overrides root
        config.AddScanFolder(ScanFolderInfo(tempPath.filePath("subfolder4"), "", false, true), true); // subfolder4 overrides subfolder5
        config.AddScanFolder(ScanFolderInfo(tempPath.filePath("subfolder5"), "b/c", false, true), true); // subfolder5 overrides subfolder6
        config.AddScanFolder(ScanFolderInfo(tempPath.filePath("subfolder6"), "b/c", false, true), true); // subfolder6 overrides subfolder7
        config.AddScanFolder(ScanFolderInfo(tempPath.filePath("subfolder7"), "", false, true), true); // subfolder7 overrides subfolder8
        config.AddScanFolder(ScanFolderInfo(tempPath.filePath("subfolder8/x"), "", false, true), true); // subfolder8 overrides root
        config.AddScanFolder(ScanFolderInfo(tempPath.absolutePath(), "", true, false), true); // add the root

        // these are checked for later.
        config.AddScanFolder(ScanFolderInfo(tempPath.filePath("GameName"), "", false, true), true);
        config.AddScanFolder(ScanFolderInfo(tempPath.filePath("GameNameButWithExtra"), "WithExtra", false, true), true);


        config.EnablePlatform("pc", true);
        config.EnablePlatform("es3", true);
        config.EnablePlatform("durango", false);

        AssetRecognizer rec;
        AssetPlatformSpec specpc;
        AssetPlatformSpec speces3;
        AssetPlatformSpec specdurango;
        specpc.m_extraRCParams = ""; // blank must work
        speces3.m_extraRCParams = "testextraparams";

        rec.m_name = "txt files";
        rec.m_patternMatcher = AssetUtilities::FilePatternMatcher("*.txt", AssetBuilderSDK::AssetBuilderPattern::Wildcard);
        rec.m_platformSpecs.insert("pc", specpc);
        rec.m_platformSpecs.insert("es3", speces3);
        rec.m_platformSpecs.insert("durango", specdurango);
        config.AddRecognizer(rec);

        // test dual-recognisers - two recognisers for the same pattern.
        rec.m_name = "txt files 2";
        config.AddRecognizer(rec);
        rec.m_patternMatcher = AssetUtilities::FilePatternMatcher(".*\\/test\\/.*\\.format", AssetBuilderSDK::AssetBuilderPattern::Regex);
        rec.m_name = "format files that live in a folder called test";
        config.AddRecognizer(rec);

        // --------------------------- SCAN FOLDER TEST ------------------------

        UNIT_TEST_EXPECT_TRUE(config.PlatformCount() == 2);
        UNIT_TEST_EXPECT_TRUE(config.PlatformAt(0) == "pc");
        UNIT_TEST_EXPECT_TRUE(config.PlatformAt(1) == "es3");

        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderCount() == 11);
        UNIT_TEST_EXPECT_FALSE(config.GetScanFolderAt(0).IsRoot());
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(8).IsRoot());
        UNIT_TEST_EXPECT_FALSE(config.GetScanFolderAt(0).RecurseSubFolders());
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(1).RecurseSubFolders());
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(2).RecurseSubFolders());
        UNIT_TEST_EXPECT_FALSE(config.GetScanFolderAt(8).RecurseSubFolders());

        // --------------------------- RECOGNIZER TEST ------------------------

        RecognizerPointerContainer results;
        UNIT_TEST_EXPECT_TRUE(config.GetMatchingRecognizers(tempPath.absoluteFilePath("subfolder1/rootfile1.txt"), results));
        UNIT_TEST_EXPECT_TRUE(results.size() == 2);
        UNIT_TEST_EXPECT_TRUE(results[0]);
        UNIT_TEST_EXPECT_TRUE(results[1]);
        UNIT_TEST_EXPECT_TRUE(results[0]->m_name != results[1]->m_name);
        UNIT_TEST_EXPECT_TRUE(results[0]->m_name == "txt files" || results[1]->m_name == "txt files");
        UNIT_TEST_EXPECT_TRUE(results[0]->m_name == "txt files 2" || results[1]->m_name == "txt files 2");

        results.clear();
        UNIT_TEST_EXPECT_FALSE(config.GetMatchingRecognizers(tempPath.absoluteFilePath("test.format"), results));
        UNIT_TEST_EXPECT_TRUE(config.GetMatchingRecognizers(tempPath.absoluteFilePath("subfolder1/test/test.format"), results));
        UNIT_TEST_EXPECT_TRUE(results.size() == 1);
        UNIT_TEST_EXPECT_TRUE(results[0]->m_name == "format files that live in a folder called test");

        // double call:
        UNIT_TEST_EXPECT_FALSE(config.GetMatchingRecognizers(tempPath.absoluteFilePath("unrecognised.file"), results));
        UNIT_TEST_EXPECT_FALSE(config.GetMatchingRecognizers(tempPath.absoluteFilePath("unrecognised.file"), results));

        // files which do and dont exist:
        UNIT_TEST_EXPECT_FALSE(config.GetMatchingRecognizers(tempPath.absoluteFilePath("unrecognised2.file"), results));
        UNIT_TEST_EXPECT_FALSE(config.GetMatchingRecognizers(tempPath.absoluteFilePath("unrecognised3.file"), results));

        // --------------------- GetOverridingFile --------------------------
        UNIT_TEST_EXPECT_TRUE(config.GetOverridingFile("rootfile3.txt", tempPath.filePath("subfolder3")) == QString());
        UNIT_TEST_EXPECT_TRUE(config.GetOverridingFile("rootfile3.txt", tempPath.absolutePath()) == tempPath.absoluteFilePath("subfolder3/rootfile3.txt"));
        UNIT_TEST_EXPECT_TRUE(config.GetOverridingFile("subfolder1/whatever.txt", tempPath.filePath("subfolder1")) == QString());
        UNIT_TEST_EXPECT_TRUE(config.GetOverridingFile("subfolder1/override.txt", tempPath.filePath("subfolder1")) == tempPath.absoluteFilePath("subfolder2/subfolder1/override.txt"));
        UNIT_TEST_EXPECT_TRUE(config.GetOverridingFile("b/c/a/testfile.txt", tempPath.filePath("subfolder6")) == tempPath.absoluteFilePath("subfolder5/a/testfile.txt"));
        UNIT_TEST_EXPECT_TRUE(config.GetOverridingFile("a/testfile.txt", tempPath.filePath("subfolder7")) == tempPath.absoluteFilePath("subfolder4/a/testfile.txt"));
        UNIT_TEST_EXPECT_TRUE(config.GetOverridingFile("a/testfile.txt", tempPath.filePath("subfolder8/x")) == tempPath.absoluteFilePath("subfolder4/a/testfile.txt"));

        // files which dont exist:
        UNIT_TEST_EXPECT_TRUE(config.GetOverridingFile("rootfile3", tempPath.filePath("subfolder3")) == QString());

        // watch folders which dont exist should still return the best match:
        UNIT_TEST_EXPECT_TRUE(config.GetOverridingFile("rootfile3.txt", tempPath.filePath("nonesuch")) != QString());

        // subfolder 3 is first, but non-recursive, so it should NOT resolve this:
        UNIT_TEST_EXPECT_TRUE(config.GetOverridingFile("aaa/bbb/basefile.txt", tempPath.filePath("subfolder2")) == QString());

        // ------------------------ FindFirstMatchingFile --------------------------
        // must not find the one in subfolder3 because its not a recursive watch:
        UNIT_TEST_EXPECT_TRUE(config.FindFirstMatchingFile("aaa/bbb/basefile.txt") == tempPath.filePath("subfolder2/aaa/bbb/basefile.txt"));

        // however, stuff at the root is overridden:
        UNIT_TEST_EXPECT_TRUE(config.FindFirstMatchingFile("rootfile3.txt") == tempPath.filePath("subfolder3/rootfile3.txt"));

        // not allowed to find files which do not exist:
        UNIT_TEST_EXPECT_TRUE(config.FindFirstMatchingFile("asdasdsa.txt") == QString());

        // find things in the root folder, too
        UNIT_TEST_EXPECT_TRUE(config.FindFirstMatchingFile("rootfile2.txt") == tempPath.filePath("rootfile2.txt"));

        // different regex rule should not interfere
        UNIT_TEST_EXPECT_TRUE(config.FindFirstMatchingFile("test/test.format") == tempPath.filePath("subfolder1/test/test.format"));

        UNIT_TEST_EXPECT_TRUE(config.FindFirstMatchingFile("b/c/a/testfile.txt") == tempPath.filePath("subfolder5/a/testfile.txt"));
        UNIT_TEST_EXPECT_TRUE(config.FindFirstMatchingFile("a/testfile.txt") == tempPath.filePath("subfolder4/a/testfile.txt"));

        // ---------------------------- GetScanFolderForFile -----------------
        // other functions depend on this one, test it first:
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderForFile(tempPath.filePath("rootfile3.txt")));
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderForFile(tempPath.filePath("subfolder3/rootfile3.txt"))->ScanPath() == tempPath.filePath("subfolder3"));

        // this file exists and is in subfolder3, but subfolder3 is non-recursive, so it must not find it:
        UNIT_TEST_EXPECT_FALSE(config.GetScanFolderForFile(tempPath.filePath("subfolder3/aaa/bbb/basefile.txt")));

        // test of root files in actual root folder:
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderForFile(tempPath.filePath("rootfile2.txt")));
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderForFile(tempPath.filePath("rootfile2.txt"))->ScanPath() == tempPath.absolutePath());

        // test of output prefix lookup
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderForFile(tempPath.filePath("subfolder1/whatever.txt"))->OutputPrefix() == "subfolder1");

        // make sure that it does not mistake the GameName folder and the GameNameWithExtra folder for each other.

        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderForFile(tempPath.filePath("GameNameButWithExtra/somefile.meo"))->OutputPrefix() == "WithExtra");
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderForFile(tempPath.filePath("GameName/otherfile.meo"))->OutputPrefix() == "");


        // -------------------------- ConvertToRelativePath  ------------------------------

        QString fileName;
        QString scanFolderPath;

        // scan folders themselves should still convert to relative paths.
        UNIT_TEST_EXPECT_TRUE(config.ConvertToRelativePath(tempPath.absolutePath(), fileName, scanFolderPath));
        UNIT_TEST_EXPECT_TRUE(fileName == "");
        UNIT_TEST_EXPECT_TRUE(scanFolderPath == tempPath.absolutePath());

        // a root file that actually exists in a root folder:
        UNIT_TEST_EXPECT_TRUE(config.ConvertToRelativePath(tempPath.absoluteFilePath("rootfile2.txt"), fileName, scanFolderPath));
        UNIT_TEST_EXPECT_TRUE(fileName == "rootfile2.txt");
        UNIT_TEST_EXPECT_TRUE(scanFolderPath == tempPath.absolutePath());

        // find overridden file from root that is overridden in a higher priority folder:
        UNIT_TEST_EXPECT_TRUE(config.ConvertToRelativePath(tempPath.absoluteFilePath("subfolder3/rootfile3.txt"), fileName, scanFolderPath));
        UNIT_TEST_EXPECT_TRUE(fileName == "rootfile3.txt");
        UNIT_TEST_EXPECT_TRUE(scanFolderPath == tempPath.filePath("subfolder3"));

        // must not find this, since its in a non-recursive folder:
        UNIT_TEST_EXPECT_FALSE(config.ConvertToRelativePath(tempPath.absoluteFilePath("subfolder3/aaa/basefile.txt"), fileName, scanFolderPath));

        // must not find this since its not even in any folder we care about:
        UNIT_TEST_EXPECT_FALSE(config.ConvertToRelativePath(tempPath.absoluteFilePath("subfolder8/aaa/basefile.txt"), fileName, scanFolderPath));

        // deep folder:
        UNIT_TEST_EXPECT_TRUE(config.ConvertToRelativePath(tempPath.absoluteFilePath("subfolder2/aaa/bbb/ccc/ddd/basefile.txt"), fileName, scanFolderPath));
        UNIT_TEST_EXPECT_TRUE(fileName == "aaa/bbb/ccc/ddd/basefile.txt");
        UNIT_TEST_EXPECT_TRUE(scanFolderPath == tempPath.filePath("subfolder2"));

        // verify that output relative paths include output prefix
        UNIT_TEST_EXPECT_TRUE(config.ConvertToRelativePath(tempPath.absoluteFilePath("subfolder1/whatever.txt"), fileName, scanFolderPath));
        UNIT_TEST_EXPECT_TRUE(fileName == "subfolder1/whatever.txt");
        UNIT_TEST_EXPECT_TRUE(scanFolderPath == tempPath.filePath("subfolder1"));
    }

    {
        QTemporaryDir tempEngineRoot;
        QDir tempPath(tempEngineRoot.path());
        // ---- test the Gems file reading ----
        PlatformConfiguration config;
        QString badGemString = "agfhakdjahdksahjda";
        QString emptyGemString = "";
        QString nonexistantGemFileName = tempPath.filePath("blah.txt");

        // note - missing guid and version on at least one
        // note - more than one gem
        QString realString =
            "{                                                                  \n"
            "   \"GemListFormatVersion\" : 2,                                   \n"
            "   \"Gems\" :                                                      \n"
            "   [                                                               \n"
            "       {                                                           \n"
            "           \"Path\" : \"Gems/AAAA\",                               \n"
            "           \"Uuid\" : \"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\",        \n"
            "           \"Version\" : \"1.0.0\"                                 \n"
            "       },                                                          \n"
            "       {                                                           \n"
            "           \"Path\" : \"Gems/FFFF\",                               \n"
            "           \"Uuid\" : \"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\",        \n"
            "           \"Version\" : \"2.0.0\"                                 \n"
            "       }                                                           \n"
            "   ]                                                               \n"
            "}                                                                  \n";

        QString malformedJSON =
            "{\n"
            "   \"GemListFormatVersion\" : \"1.0.0\"\n"      // note, missing comma here.
            "   \"Gems\" : \n"
            "   [\n"
            "       {\n"
            "           \"Path\" : \"Gems/AAAA\",                               \n"
            "           \"Uuid\" : \"e5f049ad7f534847a89c27b7339cf6a6\",\n"
            "           \"Version\" : \"1.0.0\"\n"
            "       },\n"
            "       {\n"
            "           \"Uuid\" : \"d48ca459d0644521aad37f08466ef83a\",\n"
            "           \"Version\" : \"2.0.0\"\n"
            "       },\n"
            "   ]\n"
            "}\n";

        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(tempPath.filePath("gemstest_empty.json"), emptyGemString));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(tempPath.filePath("gemstest_ok.json"), realString));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(tempPath.filePath("gemstest_badjson.json"), malformedJSON));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(tempPath.filePath("gemstest_badstring.json"), badGemString));

        UNIT_TEST_EXPECT_TRUE(config.ReadGems(tempPath.filePath(nonexistantGemFileName)) == 0);
        UNIT_TEST_EXPECT_TRUE(config.ReadGems(tempPath.filePath("gemstest_empty.json")) == 0);
        UNIT_TEST_EXPECT_TRUE(config.ReadGems(tempPath.filePath("gemstest_badjson.json")) == 0);
        UNIT_TEST_EXPECT_TRUE(config.ReadGems(tempPath.filePath("gemstest_badstring.json")) == 0);
        UNIT_TEST_EXPECT_TRUE(config.ReadGems(tempPath.filePath("gemstest_ok.json")) == 2); // 2 gems expected

        QDir realEngineRoot;
        AssetUtilities::ResetEngineRoot();
        UNIT_TEST_EXPECT_TRUE(AssetUtilities::ComputeEngineRoot(realEngineRoot));
        UNIT_TEST_EXPECT_TRUE(!realEngineRoot.absolutePath().isEmpty());

        QString expectedGemFolder = realEngineRoot.absolutePath() + "/Gems/AAAA/Assets";

        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderCount() == 4);
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(0).IsRoot() == false);
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(0).OutputPrefix().isEmpty());
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(0).RecurseSubFolders());
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(0).GetOrder() >= 100);
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(0).ScanPath().compare(expectedGemFolder, Qt::CaseInsensitive) == 0);

        expectedGemFolder = realEngineRoot.absolutePath() + "/Gems/AAAA";
        QString expectedGemJSONFolder("Gems/AAAA");

        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(1).IsRoot() == true);
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(1).OutputPrefix() == expectedGemJSONFolder);
        UNIT_TEST_EXPECT_TRUE(!config.GetScanFolderAt(1).RecurseSubFolders());
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(1).GetOrder() >= 100);
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(1).ScanPath().compare(expectedGemFolder, Qt::CaseInsensitive) == 0);

        expectedGemFolder = realEngineRoot.absolutePath() + "/Gems/FFFF/Assets";
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(2).IsRoot() == false);
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(2).OutputPrefix().isEmpty());
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(2).RecurseSubFolders());
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(2).GetOrder() > config.GetScanFolderAt(0).GetOrder());
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(2).ScanPath().compare(expectedGemFolder, Qt::CaseInsensitive) == 0);

        expectedGemFolder = realEngineRoot.absolutePath() + "/Gems/FFFF";
        expectedGemJSONFolder = QString("Gems/FFFF");

        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(3).IsRoot() == true);
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(3).OutputPrefix() == expectedGemJSONFolder);
        UNIT_TEST_EXPECT_TRUE(!config.GetScanFolderAt(3).RecurseSubFolders());
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(3).GetOrder() >= 100);
        UNIT_TEST_EXPECT_TRUE(config.GetScanFolderAt(3).ScanPath().compare(expectedGemFolder, Qt::CaseInsensitive) == 0);

        config.AddMetaDataType("xxxx", "");
        config.AddMetaDataType("yyyy", "zzzz");
        UNIT_TEST_EXPECT_TRUE(config.MetaDataFileTypesCount() == 2);
        UNIT_TEST_EXPECT_TRUE(QString::compare(config.GetMetaDataFileTypeAt(1).first, "yyyy", Qt::CaseInsensitive) == 0);
        UNIT_TEST_EXPECT_TRUE(QString::compare(config.GetMetaDataFileTypeAt(1).second, "zzzz", Qt::CaseInsensitive) == 0);
    }

    Q_EMIT UnitTestPassed();
}

REGISTER_UNIT_TEST(PlatformConfigurationTests)

