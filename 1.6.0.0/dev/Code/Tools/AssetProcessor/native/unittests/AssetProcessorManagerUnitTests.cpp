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
#if defined(UNIT_TEST)

#include <AzCore/base.h>

#if defined AZ_PLATFORM_WINDOWS
// a compiler bug appears to cause this file to take a really, really long time to optimize (many minutes).
// its used for unit testing only.
#pragma optimize("", off)
#endif

#include "AssetProcessorManagerUnitTests.h"

#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/Casting/lossy_cast.h>
#include <AzCore/IO/FileIO.h>
#include <AzFramework/IO/LocalFileIO.h>

#include <AzToolsFramework/Asset/AssetProcessorMessages.h>

#include "MockApplicationManager.h"
#include "native/AssetManager/assetProcessorManager.h"
#include "native/utilities/PlatformConfiguration.h"
#include "native/utilities/AssetBuilderInfo.h"
#include "native/AssetManager/assetScanFolderInfo.h"
#include "native/utilities/AssetUtils.h"
#include "native/AssetManager/AssetData.h"
#include "native/FileWatcher/FileWatcherAPI.h"
#include "native/FileWatcher/FileWatcher.h"
#include "native/unittests/MockConnectionHandler.h"
#include "native/resourcecompiler/RCBuilder.h"


#include <QTemporaryDir>
#include <QString>
#include <QCoreApplication>
#include <QSet>
#include <QList>
#include <QTime>
#include <QThread>
#include <QPair>

namespace AssetProcessor
{
    using namespace UnitTestUtils;
    using namespace AzFramework::AssetSystem;
    using namespace AzToolsFramework::AssetSystem;

    REGISTER_UNIT_TEST(AssetProcessorManagerUnitTests)

    namespace
    {
        //! Automatically restore current directory when this leaves scope:
        class ScopedDir
        {
        public:
            ScopedDir(QString newDir)
            {
                m_originalDir = QDir::currentPath();
                QDir::setCurrent(newDir);

                m_localFileIO = aznew AZ::IO::LocalFileIO();

                m_priorFileIO = AZ::IO::FileIOBase::GetInstance();
                if (m_priorFileIO)
                {
                    AZ::IO::FileIOBase::SetInstance(nullptr);
                }
                
                AZ::IO::FileIOBase::SetInstance(m_localFileIO);

                m_localFileIO->SetAlias("@assets@", (newDir + QString("/ALIAS/assets")).toUtf8().constData());
                m_localFileIO->SetAlias("@log@", (newDir + QString("/ALIAS/logs")).toUtf8().constData());
                m_localFileIO->SetAlias("@cache@", (newDir + QString("/ALIAS/cache")).toUtf8().constData());
                m_localFileIO->SetAlias("@user@", (newDir + QString("/ALIAS/user")).toUtf8().constData());
                m_localFileIO->SetAlias("@root@", (newDir + QString("/ALIAS/root")).toUtf8().constData());

            }
            ~ScopedDir()
            {
                AZ::IO::FileIOBase::SetInstance(nullptr);
                delete m_localFileIO;
                m_localFileIO = nullptr;
                if (m_priorFileIO)
                {
                    AZ::IO::FileIOBase::SetInstance(m_priorFileIO);
                }
                QDir::setCurrent(m_originalDir);
            }

        private:
            QString m_originalDir;
            AZ::IO::FileIOBase* m_priorFileIO = nullptr;
            AZ::IO::FileIOBase* m_localFileIO = nullptr;
        };

        /// This functions sorts the processed result list by platform name
        /// if platform is same than it sorts by job description
        void sortAssetToProcessResultList(QList<JobDetails>& processResults)
        {
            //Sort the processResults based on platforms
            qSort(processResults.begin(), processResults.end(),
                [](const JobDetails& first, const JobDetails& second) -> bool
            {
                if (QString::compare(first.m_jobEntry.m_platform, second.m_jobEntry.m_platform, Qt::CaseInsensitive) == 0)
                {
                    return first.m_jobEntry.m_jobKey.toLower() < second.m_jobEntry.m_jobKey.toLower();
                }

                return first.m_jobEntry.m_platform.toLower() < second.m_jobEntry.m_platform.toLower();
            }
                );

            //AZ_TracePrintf("test", "-------------------------\n");
        }

        void ComputeFingerprints(unsigned int& fingerprintForPC, unsigned int& fingerprintForES3, PlatformConfiguration& config, QString filePath, QString relPath)
        {
            QString extraInfoForPC;
            QString extraInfoForES3;
            RecognizerPointerContainer output;
            config.GetMatchingRecognizers(filePath, output);
            for (const AssetRecognizer* assetRecogniser : output)
            {
                extraInfoForPC.append(assetRecogniser->m_platformSpecs["pc"].m_extraRCParams);
                extraInfoForES3.append(assetRecogniser->m_platformSpecs["es3"].m_extraRCParams);
                extraInfoForPC.append(assetRecogniser->m_version);
                extraInfoForES3.append(assetRecogniser->m_version);
            }

            //Calculating fingerprints for the file for pc and es3 platforms
            JobEntry jobEntryPC(0, filePath, relPath, 0, "pc", "", 0);
            JobEntry jobEntryES3(0, filePath, relPath, 0, "es3", "", 0);

            JobDetails jobDetailsPC;
            jobDetailsPC.m_extraInformationForFingerprinting = extraInfoForPC;
            jobDetailsPC.m_jobEntry = jobEntryPC;
            JobDetails jobDetailsES3;
            jobDetailsES3.m_extraInformationForFingerprinting = extraInfoForES3;
            jobDetailsES3.m_jobEntry = jobEntryES3;
            fingerprintForPC = AssetUtilities::GenerateFingerprint(jobDetailsPC);
            fingerprintForES3 = AssetUtilities::GenerateFingerprint(jobDetailsES3);
        }
    }

    void AssetProcessorManagerUnitTests::StartTest()
    {
        // the asset processor manager is generally sitting on top of many other systems.
        // we have tested those systems individually in other unit tests, but we need to create
        // a simulated environment to test the manager itself.
        // for the manager, the only things we care about is that it emits the correct signals
        // when the appropriate stimulus is given and that state is appropriately updated
        // we want to make no modifications to the real asset database or any other file
        // except the temp folder while this goes on.

        // attach a file monitor to ensure this occurs.
 
        MockApplicationManager  mockAppManager;
        mockAppManager.BusConnect();

        QDir oldRoot;
        AssetUtilities::ComputeEngineRoot(oldRoot);
        AssetUtilities::ResetEngineRoot();
        QStringList collectedChanges;
        FileWatcher fileWatcher;
        FolderWatchCallbackEx folderWatch(oldRoot.absolutePath(), QString(), true);
        fileWatcher.AddFolderWatch(&folderWatch);
        connect(&folderWatch, &FolderWatchCallbackEx::fileChange,
            [&collectedChanges](FileChangeInfo info)
        {
            //do not add log files and folders
            QFileInfo fileInfo(info.m_filePath);
            if (!QRegExp(".*.log", Qt::CaseInsensitive, QRegExp::RegExp).exactMatch(info.m_filePath) && !fileInfo.isDir())
            {
                collectedChanges.append(info.m_filePath);
            }
        });

        QTemporaryDir dir;
        ScopedDir changeDir(dir.path());
        QDir tempPath(dir.path());
        NetworkRequestID requestId(1, 1);

        fileWatcher.AddFolderWatch(&folderWatch);

        CreateDummyFile(tempPath.absoluteFilePath("bootstrap.cfg"), QString("sys_game_folder=SamplesProject\n"));

        // system is already actually initialized, along with gEnv, so this will always return that game name.
        QString gameName = AssetUtilities::ComputeGameName();

        // update the engine root
        AssetUtilities::ResetEngineRoot();
        QDir newRoot;
        AssetUtilities::ComputeEngineRoot(newRoot, &tempPath);

        UNIT_TEST_EXPECT_FALSE(gameName.isEmpty());
        // should create cache folder in the root, and read everything from there.

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
        expectedFiles << tempPath.absoluteFilePath("subfolder3/basefile.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder8/a/b/c/test.txt");

        // subfolder3 is not recursive so none of these should show up in any scan or override check
        expectedFiles << tempPath.absoluteFilePath("subfolder3/aaa/basefile.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder3/aaa/bbb/basefile.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder3/aaa/bbb/ccc/basefile.txt");

        expectedFiles << tempPath.absoluteFilePath("subfolder3/uniquefile.txt"); // only exists in subfolder3

        expectedFiles << tempPath.absoluteFilePath("subfolder3/rootfile3.txt"); // must override rootfile3 in root
        expectedFiles << tempPath.absoluteFilePath("rootfile1.txt");
        expectedFiles << tempPath.absoluteFilePath("rootfile3.txt");
        expectedFiles << tempPath.absoluteFilePath("unrecognised.file"); // a file that should not be recognised
        expectedFiles << tempPath.absoluteFilePath("unrecognised2.file"); // a file that should not be recognised
        expectedFiles << tempPath.absoluteFilePath("subfolder1/test/test.format"); // a file that should be recognised
        expectedFiles << tempPath.absoluteFilePath("test.format"); // a file that should NOT be recognised
        expectedFiles << tempPath.absoluteFilePath("subfolder3/somefile.xxx");
        expectedFiles << tempPath.absoluteFilePath("subfolder3/savebackup/test.txt");//file that should be excluded
        expectedFiles << tempPath.absoluteFilePath("subfolder3/somerandomfile.random");

        // these will be used for the "check lock" tests
        expectedFiles << tempPath.absoluteFilePath("subfolder4/needsLock.tiff");
        expectedFiles << tempPath.absoluteFilePath("subfolder4/noLockNeeded.txt");

        // this will be used for the "rename a folder" test.
        expectedFiles << tempPath.absoluteFilePath("subfolder1/rename_this/somefile1.txt");
        expectedFiles << tempPath.absoluteFilePath("subfolder1/rename_this/somefolder/somefile2.txt");

        // this will be used for the "rename a folder" test with deep folders that don't contain files:
        expectedFiles << tempPath.absoluteFilePath("subfolder1/rename_this_secondly/somefolder/somefile2.txt");

        // this will be used for the "delete a SOURCE file" test.
        expectedFiles << tempPath.absoluteFilePath("subfolder1/to_be_deleted/some_deleted_file.txt");

        // this will be used for the "fewer products than last time" test.
        expectedFiles << tempPath.absoluteFilePath("subfolder1/fewer_products/test.txt");

        for (const QString& expect : expectedFiles)
        {
            UNIT_TEST_EXPECT_TRUE(CreateDummyFile(expect));
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Created file %s with msecs %llu", expect.toUtf8().data(),
                QFileInfo(expect).lastModified().toMSecsSinceEpoch());

#if defined(AZ_PLATFORM_WINDOWS)
            QThread::msleep(35); // give at least some milliseconds so that the files never share the same timestamp exactly
#else
            // on platforms such as mac, the file time resolution is only a second :(
            QThread::msleep(1001);
#endif
        }

        PlatformConfiguration config;
        config.AddScanFolder(ScanFolderInfo(tempPath.filePath("subfolder4"), "", false, false, -6)); // subfolder 4 overrides subfolder3
        config.AddScanFolder(ScanFolderInfo(tempPath.filePath("subfolder3"), "", false, false, -5)); // subfolder 3 overrides subfolder2
        config.AddScanFolder(ScanFolderInfo(tempPath.filePath("subfolder2"), "", false, true, -2)); // subfolder 2 overrides subfolder1
        config.AddScanFolder(ScanFolderInfo(tempPath.filePath("subfolder1"), "", false, true, -1)); // subfolder1 overrides root
        config.AddScanFolder(ScanFolderInfo(tempPath.absolutePath(), "", true, false, 0)); // add the root


        config.EnablePlatform("pc", true);
        config.EnablePlatform("es3", true);
        config.EnablePlatform("durango", false);

        config.AddMetaDataType("exportsettings", QString());

        AssetRecognizer rec;
        AssetPlatformSpec specpc;
        AssetPlatformSpec speces3;

        speces3.m_extraRCParams = "somerandomparam";
        rec.m_name = "random files";
        rec.m_patternMatcher = AssetUtilities::FilePatternMatcher("*.random", AssetBuilderSDK::AssetBuilderPattern::Wildcard);
        rec.m_platformSpecs.insert("pc", specpc);
        config.AddRecognizer(rec);
        UNIT_TEST_EXPECT_TRUE(mockAppManager.RegisterAssetRecognizerAsBuilder(rec));

        specpc.m_extraRCParams = ""; // blank must work
        speces3.m_extraRCParams = "testextraparams";

        const char* builderTxt1Name = "txt files";
        rec.m_name = builderTxt1Name;
        rec.m_patternMatcher = AssetUtilities::FilePatternMatcher("*.txt", AssetBuilderSDK::AssetBuilderPattern::Wildcard);
        rec.m_platformSpecs.insert("pc", specpc);
        rec.m_platformSpecs.insert("es3", speces3);

        AZ::Uuid builderTxt1Uuid;
        
        config.AddRecognizer(rec);
        mockAppManager.RegisterAssetRecognizerAsBuilder(rec);
        {
            AZStd::shared_ptr<InternalMockBuilder> builderTxt1Builder;
            UNIT_TEST_EXPECT_TRUE(mockAppManager.GetBuilderByID(builderTxt1Name, builderTxt1Builder));
            builderTxt1Uuid = builderTxt1Builder->GetUuid();
        }

        // test dual-recognisers - two recognisers for the same pattern.
        const char* builderTxt2Name = "txt files 2 (builder2)";
        rec.m_name = builderTxt2Name;
        config.AddRecognizer(rec);
        mockAppManager.RegisterAssetRecognizerAsBuilder(rec);

        rec.m_patternMatcher = AssetUtilities::FilePatternMatcher(".*\\/test\\/.*\\.format", AssetBuilderSDK::AssetBuilderPattern::Regex);
        rec.m_name = "format files that live in a folder called test";
        config.AddRecognizer(rec);
        mockAppManager.RegisterAssetRecognizerAsBuilder(rec);

        // tiff file recognizer
        rec.m_name = "tiff files";
        rec.m_patternMatcher = AssetUtilities::FilePatternMatcher("*.tiff", AssetBuilderSDK::AssetBuilderPattern::Wildcard);
        rec.m_platformSpecs.clear();
        rec.m_platformSpecs.insert("pc", specpc);
        rec.m_testLockSource = true;
        config.AddRecognizer(rec);
        mockAppManager.RegisterAssetRecognizerAsBuilder(rec);

        rec.m_platformSpecs.clear();
        rec.m_testLockSource = false;

        specpc.m_extraRCParams = "pcparams";
        speces3.m_extraRCParams = "es3params";

        rec.m_name = "xxx files";
        rec.m_patternMatcher = AssetUtilities::FilePatternMatcher("*.xxx", AssetBuilderSDK::AssetBuilderPattern::Wildcard);
        rec.m_platformSpecs.insert("pc", specpc);
        rec.m_platformSpecs.insert("es3", speces3);
        config.AddRecognizer(rec);
        mockAppManager.RegisterAssetRecognizerAsBuilder(rec);

        // two recognizers for the same pattern.
        rec.m_name = "xxx files 2 (builder2)";
        specpc.m_extraRCParams = "pcparams2";
        speces3.m_extraRCParams = "es3params2";
        rec.m_platformSpecs.insert("pc", specpc);
        rec.m_platformSpecs.insert("es3", speces3);
        config.AddRecognizer(rec);
        mockAppManager.RegisterAssetRecognizerAsBuilder(rec);

        ExcludeAssetRecognizer excludeRecogniser;
        excludeRecogniser.m_name = "backup";
        excludeRecogniser.m_patternMatcher = AssetUtilities::FilePatternMatcher(".*\\/savebackup\\/.*", AssetBuilderSDK::AssetBuilderPattern::Regex);
        config.AddExcludeRecognizer(excludeRecogniser);

        AssetProcessorManager apm(&config);

        QDir cacheRoot;
        UNIT_TEST_EXPECT_TRUE(AssetUtilities::ComputeProjectCacheRoot(cacheRoot));
        QString normalizedCacheRoot = AssetUtilities::NormalizeDirectoryPath(cacheRoot.absolutePath());

        // make sure it picked up the one in the current folder

        QString normalizedDirPathCheck = AssetUtilities::NormalizeDirectoryPath(QDir::current().absoluteFilePath("Cache/" + gameName));
        UNIT_TEST_EXPECT_TRUE(normalizedCacheRoot == normalizedDirPathCheck);
        QDir normalizedCacheRootDir(normalizedCacheRoot);

        QList<JobDetails> processResults;
        QList<QPair<QString, QString> > changedInputResults;
        QList<QPair<QString, QString> > changedProductResults;
        QList<QPair<QString, QString> > failedInputResults;

        bool idling = false;
        unsigned int CRC_Of_Change_Message =  AssetUtilities::ComputeCRC32Lowercase("AssetProcessorManager::assetChanged");

        connect(&apm, &AssetProcessorManager::AssetToProcess, this,
            [&processResults](JobDetails details)
        {
            //AZ_TracePrintf("Results", "ProcessResult: %s - %s - %s - %s - %u\n", file.toUtf8().data(), platform.toUtf8().data(), jobDescription.toUtf8().data(), destination.toUtf8().data(), originalFingerprint);
            processResults.push_back(AZStd::move(details));
        }
            );

        connect(&apm, &AssetProcessorManager::ProductChanged, this,
            [&changedProductResults](QString relativePath, QString platform)
        {
            changedProductResults.push_back(QPair<QString, QString>(relativePath, platform));
        }
            );

        connect(&apm, &AssetProcessorManager::InputAssetProcessed, this,
            [&changedInputResults](QString relativePath, QString platform)
        {
            changedInputResults.push_back(QPair<QString, QString>(relativePath, platform));
        }
            );

        connect(&apm, &AssetProcessorManager::InputAssetFailed, this,
            [&failedInputResults](QString relativePath, QString platform)
        {
            failedInputResults.push_back(QPair<QString, QString>(relativePath, platform));
        }
            );

        connect(&apm, &AssetProcessorManager::OnBecameIdle, this,
            [&idling]()
        {
            idling = true;
        }
            );

        QStringList removedProducts;
        QStringList removedProductPlatforms;
        QMetaObject::Connection deleteNotify = QObject::connect(&apm, &AssetProcessorManager::ProductRemoved, this, [&](QString productValue, QString platform)
        {
            removedProducts.push_back(productValue);
            removedProductPlatforms.push_back(platform);

            using AzFramework::AssetSystem::AssetNotificationMessage;
            AssetNotificationMessage message(AZ::OSString(productValue.toUtf8().data()), AssetNotificationMessage::AssetRemoved);
            EBUS_EVENT(AssetProcessor::ConnectionBus, SendPerPlatform, 0, message, platform);
        });


        AssetProcessor::MockConnectionHandler connection;
        connection.BusConnect(1);
        QList<QPair<unsigned int, QByteArray> > payloadList;
        connection.m_callback = [&](unsigned int type, unsigned int serial, const QByteArray payload)
        {
            payloadList.append(qMakePair(type, payload));
        };

        // run the tests.
        // first, feed it things which it SHOULD ignore and should NOT generate any tasks:

        // the following is a file which does exist but should not be processed as it is in a non-watched folder (not recursive)
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString,  tempPath.absoluteFilePath("subfolder3/aaa/basefile.txt")));

        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));
        UNIT_TEST_EXPECT_TRUE(processResults.isEmpty());
        UNIT_TEST_EXPECT_TRUE(changedInputResults.isEmpty());
        UNIT_TEST_EXPECT_TRUE(changedProductResults.isEmpty());

        // an imaginary non-existant file should also fail even if it matches filters:
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString,  tempPath.absoluteFilePath("subfolder3/basefileaaaaa.txt")));

        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));
        UNIT_TEST_EXPECT_TRUE(processResults.isEmpty());
        UNIT_TEST_EXPECT_TRUE(changedInputResults.isEmpty());
        UNIT_TEST_EXPECT_TRUE(changedProductResults.isEmpty());

        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString,  tempPath.absoluteFilePath("basefileaaaaa.txt")));

        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));
        UNIT_TEST_EXPECT_TRUE(processResults.isEmpty());
        UNIT_TEST_EXPECT_TRUE(changedInputResults.isEmpty());
        UNIT_TEST_EXPECT_TRUE(changedProductResults.isEmpty());

        // block until no more events trickle in:
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        processResults.clear();

        // give it a file that should actually cause the generation of a task:

        QString inputFilePath = AssetUtilities::NormalizeFilePath(tempPath.absoluteFilePath("subfolder3/uniquefile.txt"));

        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, inputFilePath));

        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));

        // block until no more events trickle in:
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        sortAssetToProcessResultList(processResults);

        UNIT_TEST_EXPECT_TRUE(processResults.size() == 4); // 2 each for pc and es3,since we have two recognizer for .txt file
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_platform == processResults[1].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE(processResults[2].m_jobEntry.m_platform == processResults[3].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE((processResults[0].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[1].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[2].m_jobEntry.m_platform == "pc"));
        UNIT_TEST_EXPECT_TRUE((processResults[3].m_jobEntry.m_platform == "pc"));
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_computedFingerprint != 0);
        UNIT_TEST_EXPECT_TRUE(processResults[1].m_jobEntry.m_computedFingerprint != 0);

        QList<int> es3JobsIndex;
        QList<int> pcJobsIndex;
        for (int checkIdx = 0; checkIdx < 4; ++checkIdx)
        {
            UNIT_TEST_EXPECT_TRUE(processResults[checkIdx].m_jobEntry.m_absolutePathToFile == inputFilePath);
            UNIT_TEST_EXPECT_TRUE(processResults[checkIdx].m_jobEntry.m_relativePathToFile == "uniquefile.txt");
            QString platformFolder = cacheRoot.filePath(processResults[checkIdx].m_jobEntry.m_platform + "/" + gameName);
            platformFolder = AssetUtilities::NormalizeDirectoryPath(platformFolder);
            UNIT_TEST_EXPECT_TRUE(processResults[checkIdx].m_destinationPath.startsWith(platformFolder));
            UNIT_TEST_EXPECT_TRUE(processResults[checkIdx].m_jobEntry.m_computedFingerprint != 0);

            QMetaObject::invokeMethod(&apm, "OnJobStatusChanged", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[checkIdx].m_jobEntry), Q_ARG(JobStatus, JobStatus::Queued));

            QCoreApplication::processEvents(QEventLoop::AllEvents);

            // create log files, so that we can test the correct retrieval

            // we create all of them except for #1
            if (checkIdx != 1)
            {
                JobInfo info;
                info.m_jobId = processResults[checkIdx].m_jobEntry.m_jobId;
                info.m_builderGuid = processResults[checkIdx].m_jobEntry.m_builderUUID;
                info.m_jobKey = processResults[checkIdx].m_jobEntry.m_jobKey.toUtf8().data();
                info.m_platform = processResults[checkIdx].m_jobEntry.m_platform.toUtf8().data();
                info.m_sourceFile = processResults[checkIdx].m_jobEntry.m_relativePathToFile.toUtf8().data();

                AZStd::string logFolder = AZStd::string::format("%s/%s", AssetUtilities::ComputeJobLogFolder().c_str(), AssetUtilities::ComputeJobLogFileName(info).c_str());
                AZ::IO::HandleType logHandle;
                AZ::IO::LocalFileIO::GetInstance()->CreatePath(AssetUtilities::ComputeJobLogFolder().c_str());
                UNIT_TEST_EXPECT_TRUE(AZ::IO::LocalFileIO::GetInstance()->Open(logFolder.c_str(), AZ::IO::OpenMode::ModeWrite | AZ::IO::OpenMode::ModeBinary, logHandle));
                AZStd::string logLine = AZStd::string::format("Log stored for job %lli\n", processResults[checkIdx].m_jobEntry.m_jobId);
                AZ::IO::LocalFileIO::GetInstance()->Write(logHandle, logLine.c_str(), logLine.size());
                AZ::IO::LocalFileIO::GetInstance()->Close(logHandle);
            }
        }


        // ----------------------- test job info requests, while we have some assets in flight ---------------------------

        // by this time, querying for the status of those jobs should be possible since the "OnJobStatusChanged" event should have bubbled through
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            AssetJobsInfoRequest requestInfo;
            AssetJobsInfoResponse jobResponse;
            
            requestInfo.m_assetPath = inputFilePath.toUtf8().data();

            {
                // send our request:
                payloadList.clear();
                connection.m_sent = false;
                apm.ProcessGetAssetJobRequest(requestId, &requestInfo);
            

                // wait for it to process:
                QCoreApplication::processEvents(QEventLoop::AllEvents);

                UNIT_TEST_EXPECT_TRUE(connection.m_sent); // we expect a response to have arrived.

                unsigned int jobinformationResultIndex = -1;
                for (int index = 0; index < payloadList.size(); index++)
                {
                    unsigned int type = payloadList.at(index).first;
                    if (type == requestInfo.GetMessageType())
                    {
                        jobinformationResultIndex = index;
                    }
                }
                UNIT_TEST_EXPECT_FALSE(jobinformationResultIndex == -1);
                UNIT_TEST_EXPECT_TRUE(AZ::Utils::LoadObjectFromBufferInPlace(payloadList.at(jobinformationResultIndex).second.data(), payloadList.at(jobinformationResultIndex).second.size(), jobResponse));
            }
            
            UNIT_TEST_EXPECT_TRUE(jobResponse.m_isSuccess);
            UNIT_TEST_EXPECT_TRUE(jobResponse.m_jobList.size() == processResults.size());

            // make sure each job corresponds to one in the process results list (but note that the order is not important).
            for (int oldJobIdx = azlossy_cast<int>(jobResponse.m_jobList.size()) - 1; oldJobIdx >= 0; --oldJobIdx)
            {
                bool foundIt = false;
                const JobInfo& jobInfo = jobResponse.m_jobList[oldJobIdx];

                // validate EVERY field
                UNIT_TEST_EXPECT_TRUE(jobInfo.m_status == JobStatus::Queued);
                UNIT_TEST_EXPECT_TRUE(jobInfo.m_jobId > 0); // jobs should never have zero as their key ever, they should all be unique.
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_sourceFile.empty());
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_platform.empty());
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_jobKey.empty());
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_builderGuid.IsNull());

                for (const JobDetails& details : processResults)
                {
                    if ((QString::compare(jobInfo.m_sourceFile.c_str(), details.m_jobEntry.m_relativePathToFile, Qt::CaseInsensitive) == 0) &&
                        (QString::compare(jobInfo.m_platform.c_str(), details.m_jobEntry.m_platform, Qt::CaseInsensitive) == 0) &&
                        (QString::compare(jobInfo.m_jobKey.c_str(), details.m_jobEntry.m_jobKey, Qt::CaseInsensitive) == 0) &&
                        (jobInfo.m_builderGuid == details.m_jobEntry.m_builderUUID) &&
                        (jobInfo.m_jobId == details.m_jobEntry.m_jobId) )
                    {
                        foundIt = true;
                        break;
                    }
                }
                UNIT_TEST_EXPECT_TRUE(foundIt);
            }
        }

        // ------------- JOB LOG TEST -------------------
        for (int checkIdx = 0; checkIdx < 4; ++checkIdx)
        {
            const JobDetails& details = processResults[checkIdx];
            // request job logs.
            AssetJobLogRequest requestLog;
            AssetJobLogResponse requestResponse;
            requestLog.m_jobId = details.m_jobEntry.m_jobId;

            {
                // send our request:
                payloadList.clear();
                connection.m_sent = false;
                apm.ProcessGetAssetJobLogRequest(requestId, &requestLog);

                // wait for it to process:
                QCoreApplication::processEvents(QEventLoop::AllEvents);

                UNIT_TEST_EXPECT_TRUE(connection.m_sent); // we expect a response to have arrived.

                unsigned int jobLogResponseIndex = -1;
                for (int index = 0; index < payloadList.size(); index++)
                {
                    unsigned int type = payloadList.at(index).first;
                    if (type == requestLog.GetMessageType())
                    {
                        jobLogResponseIndex = index;
                    }
                }
                UNIT_TEST_EXPECT_FALSE(jobLogResponseIndex == -1);
                UNIT_TEST_EXPECT_TRUE(AZ::Utils::LoadObjectFromBufferInPlace(payloadList.at(jobLogResponseIndex).second.data(), payloadList.at(jobLogResponseIndex).second.size(), requestResponse));


                if (checkIdx != 1)
                {
                    UNIT_TEST_EXPECT_TRUE(requestResponse.m_isSuccess);
                    UNIT_TEST_EXPECT_FALSE(requestResponse.m_jobLog.empty());
                    AZStd::string checkString = AZStd::string::format("Log stored for job %lli\n", processResults[checkIdx].m_jobEntry.m_jobId);
                    UNIT_TEST_EXPECT_TRUE(requestResponse.m_jobLog.find(checkString.c_str()) != AZStd::string::npos);
                }
                else
                {
                    // the [1] index was not written so it should be failed and empty
                    UNIT_TEST_EXPECT_FALSE(requestResponse.m_isSuccess);
                }

            }
        }

        // now indicate the job has started.
        for (const JobDetails& details : processResults)
        {
            apm.OnJobStatusChanged(details.m_jobEntry, JobStatus::InProgress);
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents);


        // ----------------------- test job info requests, while we have some assets in flight ---------------------------

        // by this time, querying for the status of those jobs should be possible since the "OnJobStatusChanged" event should have bubbled through
        // and this time, it should be "in progress"
        {

            QCoreApplication::processEvents(QEventLoop::AllEvents);
            AssetJobsInfoRequest requestInfo;

            requestInfo.m_assetPath = inputFilePath.toUtf8().data();
            {
                // send our request:
                payloadList.clear();
                connection.m_sent = false;
                apm.ProcessGetAssetJobRequest(requestId, &requestInfo);
            }

            // wait for it to process:
            QCoreApplication::processEvents(QEventLoop::AllEvents);

            UNIT_TEST_EXPECT_TRUE(connection.m_sent); // we expect a response to have arrived.

            unsigned int jobinformationResultIndex = -1;
            for (int index = 0; index < payloadList.size(); index++)
            {
                unsigned int type = payloadList.at(index).first;
                if (type == requestInfo.GetMessageType())
                {
                    jobinformationResultIndex = index;
                }
            }
            UNIT_TEST_EXPECT_FALSE(jobinformationResultIndex == -1);

            AssetJobsInfoResponse jobResponse;
            UNIT_TEST_EXPECT_TRUE(AZ::Utils::LoadObjectFromBufferInPlace(payloadList.at(jobinformationResultIndex).second.data(), payloadList.at(jobinformationResultIndex).second.size(), jobResponse));

            UNIT_TEST_EXPECT_TRUE(jobResponse.m_isSuccess);
            UNIT_TEST_EXPECT_TRUE(jobResponse.m_jobList.size() == processResults.size());

            // make sure each job corresponds to one in the process results list (but note that the order is not important).
            for (int oldJobIdx = azlossy_cast<int>(jobResponse.m_jobList.size()) - 1; oldJobIdx >= 0; --oldJobIdx)
            {
                bool foundIt = false;
                const JobInfo& jobInfo = jobResponse.m_jobList[oldJobIdx];

                // validate EVERY field
                UNIT_TEST_EXPECT_TRUE(jobInfo.m_status == JobStatus::InProgress);
                UNIT_TEST_EXPECT_TRUE(jobInfo.m_jobId > 0); // jobs should never have zero as their key ever, they should all be unique.
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_sourceFile.empty());
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_platform.empty());
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_jobKey.empty());
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_builderGuid.IsNull());

                for (const JobDetails& details : processResults)
                {
                    if ((QString::compare(jobInfo.m_sourceFile.c_str(), details.m_jobEntry.m_relativePathToFile, Qt::CaseInsensitive) == 0) &&
                        (QString::compare(jobInfo.m_platform.c_str(), details.m_jobEntry.m_platform, Qt::CaseInsensitive) == 0) &&
                        (QString::compare(jobInfo.m_jobKey.c_str(), details.m_jobEntry.m_jobKey, Qt::CaseInsensitive) == 0) &&
                        (jobInfo.m_builderGuid == details.m_jobEntry.m_builderUUID) &&
                        (jobInfo.m_jobId == details.m_jobEntry.m_jobId))
                    {
                        foundIt = true;
                        break;
                    }
                }
                UNIT_TEST_EXPECT_TRUE(foundIt);
            }
        }

        // ---------- test successes ----------


        QStringList es3outs;
        es3outs.push_back(cacheRoot.filePath(QString("es3/") + gameName + "/basefile.arc1"));
        es3outs.push_back(cacheRoot.filePath(QString("es3/") + gameName + "/basefile.arc2"));

        // feed it the messages its waiting for (create the files)
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(es3outs[0], "products."));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(es3outs[1], "products."))

        //Invoke Asset Processed for es3 platform , txt files job description
        AssetBuilderSDK::ProcessJobResponse response;
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(es3outs[0].toUtf8().data()));
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(es3outs[1].toUtf8().data()));
        
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[0].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        // let events bubble through:
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        UNIT_TEST_EXPECT_TRUE(changedProductResults.size() == 2);
        UNIT_TEST_EXPECT_TRUE(changedInputResults.size() == 1);
        UNIT_TEST_EXPECT_TRUE(failedInputResults.isEmpty());

        // always RELATIVE, always with the product name.
        UNIT_TEST_EXPECT_TRUE(changedProductResults[0].first == "basefile.arc1");
        UNIT_TEST_EXPECT_TRUE(changedProductResults[1].first == "basefile.arc2");
        UNIT_TEST_EXPECT_TRUE(changedProductResults[0].second == "es3");
        UNIT_TEST_EXPECT_TRUE(changedProductResults[1].second == "es3");

        UNIT_TEST_EXPECT_TRUE(AssetUtilities::NormalizeFilePath(changedInputResults[0].first) == AssetUtilities::NormalizeFilePath(inputFilePath));

        // ----------------------- test job info requests, when some assets are done.
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            AssetJobsInfoRequest requestInfo;

            requestInfo.m_assetPath = inputFilePath.toUtf8().data();

            {
                // send our request:
                payloadList.clear();
                connection.m_sent = false;
                apm.ProcessGetAssetJobRequest(requestId, &requestInfo);
            }

            // wait for it to process:
            QCoreApplication::processEvents(QEventLoop::AllEvents);

            UNIT_TEST_EXPECT_TRUE(connection.m_sent); // we expect a response to have arrived.

            unsigned int jobinformationResultIndex = -1;
            for (int index = 0; index < payloadList.size(); index++)
            {
                unsigned int type = payloadList.at(index).first;
                if (type == requestInfo.GetMessageType())
                {
                    jobinformationResultIndex = index;
                }
            }
            UNIT_TEST_EXPECT_FALSE(jobinformationResultIndex == -1);

            AssetJobsInfoResponse jobResponse;
            UNIT_TEST_EXPECT_TRUE(AZ::Utils::LoadObjectFromBufferInPlace(payloadList.at(jobinformationResultIndex).second.data(), payloadList.at(jobinformationResultIndex).second.size(), jobResponse));

            UNIT_TEST_EXPECT_TRUE(jobResponse.m_isSuccess);
            UNIT_TEST_EXPECT_TRUE(jobResponse.m_jobList.size() == processResults.size());

            // make sure each job corresponds to one in the process results list (but note that the order is not important).
            for (int oldJobIdx = azlossy_cast<int>(jobResponse.m_jobList.size()) - 1; oldJobIdx >= 0; --oldJobIdx)
            {
                bool foundIt = false;
                const JobInfo& jobInfo = jobResponse.m_jobList[oldJobIdx];

                // validate EVERY field
                UNIT_TEST_EXPECT_TRUE(jobInfo.m_jobId > 0); // jobs should never have zero as their key ever, they should all be unique.
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_sourceFile.empty());
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_platform.empty());
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_jobKey.empty());
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_builderGuid.IsNull());

                for (int detailsIdx = 0; detailsIdx < processResults.size(); ++detailsIdx)
                {
                    const JobDetails& details = processResults[detailsIdx];

                    if ((QString::compare(jobInfo.m_sourceFile.c_str(), details.m_jobEntry.m_relativePathToFile, Qt::CaseInsensitive) == 0) &&
                        (QString::compare(jobInfo.m_platform.c_str(), details.m_jobEntry.m_platform, Qt::CaseInsensitive) == 0) &&
                        (QString::compare(jobInfo.m_jobKey.c_str(), details.m_jobEntry.m_jobKey, Qt::CaseInsensitive) == 0) &&
                        (jobInfo.m_builderGuid == details.m_jobEntry.m_builderUUID) &&
                        (jobInfo.m_jobId == details.m_jobEntry.m_jobId))
                    {
                        foundIt = true;

                        if (detailsIdx == 0) // we only said that the first job was done
                        {
                            UNIT_TEST_EXPECT_TRUE(jobInfo.m_status == JobStatus::Completed);
                        }
                        else
                        {
                            UNIT_TEST_EXPECT_TRUE(jobInfo.m_status == JobStatus::InProgress);
                        }
                        
                        break;
                    }
                }
                UNIT_TEST_EXPECT_TRUE(foundIt);
            }
        }

        changedInputResults.clear();
        changedProductResults.clear();
        failedInputResults.clear();

        es3outs.clear();
        es3outs.push_back(cacheRoot.filePath(QString("es3/") + gameName + "/basefile.azm"));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(es3outs[0], "products."));

        //Invoke Asset Processed for es3 platform , txt files2 job description
        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(es3outs[0].toUtf8().data()));

        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[1].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        // let events bubble through:
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        UNIT_TEST_EXPECT_TRUE(changedProductResults.size() == 1);
        UNIT_TEST_EXPECT_TRUE(changedInputResults.size() == 1);
        UNIT_TEST_EXPECT_TRUE(failedInputResults.isEmpty());

        // always RELATIVE, always with the product name.
        UNIT_TEST_EXPECT_TRUE(changedProductResults[0].first == "basefile.azm");
        UNIT_TEST_EXPECT_TRUE(changedProductResults[0].second == "es3");

        changedInputResults.clear();
        changedProductResults.clear();
        failedInputResults.clear();

        QStringList pcouts;
        pcouts.push_back(cacheRoot.filePath(QString("pc/") + gameName + "/basefile.arc1"));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(pcouts[0], "products."));

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcouts[0].toUtf8().data()));

        //Invoke Asset Processed for pc platform , txt files job description
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[2].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        // let events bubble through:
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        UNIT_TEST_EXPECT_TRUE(changedProductResults.size() == 1);
        UNIT_TEST_EXPECT_TRUE(changedInputResults.size() == 1);
        UNIT_TEST_EXPECT_TRUE(failedInputResults.isEmpty());

        // always RELATIVE, always with the product name.
        UNIT_TEST_EXPECT_TRUE(changedProductResults[0].first == "basefile.arc1");
        UNIT_TEST_EXPECT_TRUE(changedProductResults[0].second == "pc");

        UNIT_TEST_EXPECT_TRUE(AssetUtilities::NormalizeFilePath(changedInputResults[0].first) == AssetUtilities::NormalizeFilePath(inputFilePath));

        changedInputResults.clear();
        changedProductResults.clear();
        failedInputResults.clear();

        pcouts.clear();
        pcouts.push_back(cacheRoot.filePath(QString("pc/") + gameName + "/basefile.azm"));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(pcouts[0], "products."));

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcouts[0].toUtf8().data()));

        //Invoke Asset Processed for pc platform , txt files 2 job description
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[3].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        // let events bubble through:
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        UNIT_TEST_EXPECT_TRUE(changedProductResults.size() == 1);
        UNIT_TEST_EXPECT_TRUE(changedInputResults.size() == 1);
        UNIT_TEST_EXPECT_TRUE(failedInputResults.isEmpty());

        // always RELATIVE, always with the product name.
        UNIT_TEST_EXPECT_TRUE(changedProductResults[0].first == "basefile.azm");
        UNIT_TEST_EXPECT_TRUE(changedProductResults[0].second == "pc");

        UNIT_TEST_EXPECT_TRUE(AssetUtilities::NormalizeFilePath(changedInputResults[0].first) == AssetUtilities::NormalizeFilePath(inputFilePath));

        // all four should now be complete:
        // ----------------------- test job info requests, now that all are done ---------------------------

        // by this time, querying for the status of those jobs should be possible since the "OnJobStatusChanged" event should have bubbled through
        // and this time, it should be "in progress"
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            AssetJobsInfoRequest requestInfo;

            requestInfo.m_assetPath = inputFilePath.toUtf8().data();

            {
                // send our request:
                payloadList.clear();
                connection.m_sent = false;
                apm.ProcessGetAssetJobRequest(requestId, &requestInfo);
            }

            // wait for it to process:
            QCoreApplication::processEvents(QEventLoop::AllEvents);

            UNIT_TEST_EXPECT_TRUE(connection.m_sent); // we expect a response to have arrived.

            unsigned int jobinformationResultIndex = -1;
            for (int index = 0; index < payloadList.size(); index++)
            {
                unsigned int type = payloadList.at(index).first;
                if (type == requestInfo.GetMessageType())
                {
                    jobinformationResultIndex = index;
                }
            }
            UNIT_TEST_EXPECT_FALSE(jobinformationResultIndex == -1);

            AssetJobsInfoResponse jobResponse;
            UNIT_TEST_EXPECT_TRUE(AZ::Utils::LoadObjectFromBufferInPlace(payloadList.at(jobinformationResultIndex).second.data(), payloadList.at(jobinformationResultIndex).second.size(), jobResponse));

            UNIT_TEST_EXPECT_TRUE(jobResponse.m_isSuccess);
            UNIT_TEST_EXPECT_TRUE(jobResponse.m_jobList.size() == processResults.size());

            // make sure each job corresponds to one in the process results list (but note that the order is not important).
            for (int oldJobIdx = azlossy_cast<int>(jobResponse.m_jobList.size()) - 1; oldJobIdx >= 0; --oldJobIdx)
            {
                bool foundIt = false;
                const JobInfo& jobInfo = jobResponse.m_jobList[oldJobIdx];

                // validate EVERY field
                UNIT_TEST_EXPECT_TRUE(jobInfo.m_status == JobStatus::Completed);
                UNIT_TEST_EXPECT_TRUE(jobInfo.m_jobId > 0); // jobs should never have zero as their key ever, they should all be unique.
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_sourceFile.empty());
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_platform.empty());
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_jobKey.empty());
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_builderGuid.IsNull());

                for (const JobDetails& details : processResults)
                {
                    if ((QString::compare(jobInfo.m_sourceFile.c_str(), details.m_jobEntry.m_relativePathToFile, Qt::CaseInsensitive) == 0) &&
                        (QString::compare(jobInfo.m_platform.c_str(), details.m_jobEntry.m_platform, Qt::CaseInsensitive) == 0) &&
                        (QString::compare(jobInfo.m_jobKey.c_str(), details.m_jobEntry.m_jobKey, Qt::CaseInsensitive) == 0) &&
                        (jobInfo.m_builderGuid == details.m_jobEntry.m_builderUUID) &&
                        (jobInfo.m_jobId == details.m_jobEntry.m_jobId))
                    {
                        foundIt = true;
                        break;
                    }
                }
                UNIT_TEST_EXPECT_TRUE(foundIt);
            }
        }

        changedInputResults.clear();
        changedProductResults.clear();
        failedInputResults.clear();
        processResults.clear();

        // feed it the exact same file again.
        // this should result in NO ADDITIONAL processes since nothing has changed.
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, inputFilePath));
        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));

        UNIT_TEST_EXPECT_TRUE(processResults.isEmpty());
        UNIT_TEST_EXPECT_TRUE(changedInputResults.isEmpty());
        UNIT_TEST_EXPECT_TRUE(changedProductResults.isEmpty());
        UNIT_TEST_EXPECT_TRUE(failedInputResults.isEmpty());

        // delete one of the products and tell it that it changed
        // it should reprocess that file, for that platform only:

        payloadList.clear();
        connection.m_sent = false;

        AssetNotificationMessage assetNotifMessage;
        SourceFileNotificationMessage sourceFileChangedMessage;

        // this should result in NO ADDITIONAL processes since nothing has changed.
        UNIT_TEST_EXPECT_TRUE(QFile::remove(pcouts[0]));
        QMetaObject::invokeMethod(&apm, "AssessDeletedFile", Qt::QueuedConnection, Q_ARG(QString, pcouts[0]));
        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));

        UNIT_TEST_EXPECT_TRUE(connection.m_sent);
        UNIT_TEST_EXPECT_TRUE(payloadList.size() == 2);// We should receive two messages one assetRemoved message and the other SourceAssetChanged message
        unsigned int messageLoadCount = 0;
        for (auto payload : payloadList)
        {
            if (payload.first == AssetNotificationMessage::MessageType())
            {
                UNIT_TEST_EXPECT_TRUE(AZ::Utils::LoadObjectFromBufferInPlace(payload.second.data(), payload.second.size(), assetNotifMessage));
                UNIT_TEST_EXPECT_TRUE(assetNotifMessage.m_type == AssetNotificationMessage::AssetRemoved);
                ++messageLoadCount;
            }
            else if (payload.first == SourceFileNotificationMessage::MessageType())
            {
                UNIT_TEST_EXPECT_TRUE(AZ::Utils::LoadObjectFromBufferInPlace(payload.second.data(), payload.second.size(), sourceFileChangedMessage));
                UNIT_TEST_EXPECT_TRUE(sourceFileChangedMessage.m_type == SourceFileNotificationMessage::FileChanged);
                ++messageLoadCount;
            }
        }

        UNIT_TEST_EXPECT_TRUE(messageLoadCount == payloadList.size()); // ensure we found both messages
        UNIT_TEST_EXPECT_TRUE(pcouts[0].contains(QString(assetNotifMessage.m_data.c_str()), Qt::CaseInsensitive));

        QDir scanFolder(sourceFileChangedMessage.m_scanFolder.c_str());
        QString pathToCheck(scanFolder.filePath(sourceFileChangedMessage.m_relPath.c_str()));
        UNIT_TEST_EXPECT_TRUE(QString::compare(inputFilePath, pathToCheck, Qt::CaseInsensitive) == 0);

        // should have asked to launch only the PC process because the other assets are already done for the other plat
        UNIT_TEST_EXPECT_TRUE(processResults.size() == 1);
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_platform == "pc");
        UNIT_TEST_EXPECT_TRUE(AssetUtilities::NormalizeFilePath(processResults[0].m_jobEntry.m_absolutePathToFile) == AssetUtilities::NormalizeFilePath(inputFilePath));

        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(pcouts[0], "products2"));
        UNIT_TEST_EXPECT_TRUE(failedInputResults.isEmpty());
        // tell it were done again!

        changedInputResults.clear();
        changedProductResults.clear();
        failedInputResults.clear();

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcouts[0].toUtf8().data()));

        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[0].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        // let events bubble through:
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        UNIT_TEST_EXPECT_TRUE(changedProductResults.size() == 1);
        UNIT_TEST_EXPECT_TRUE(changedInputResults.size() == 1);

        // always RELATIVE, always with the product name.
        UNIT_TEST_EXPECT_TRUE(changedProductResults[0].first == "basefile.azm");
        UNIT_TEST_EXPECT_TRUE(changedProductResults[0].second == "pc");
        UNIT_TEST_EXPECT_TRUE(AssetUtilities::NormalizeFilePath(changedInputResults[0].first) == AssetUtilities::NormalizeFilePath(inputFilePath));

        changedInputResults.clear();
        changedProductResults.clear();
        processResults.clear();

        connection.m_sent = false;
        payloadList.clear();

        // modify the input file, then
        // feed it the exact same file again.
        // it should spawn BOTH compilers:
        UNIT_TEST_EXPECT_TRUE(QFile::remove(inputFilePath));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(inputFilePath, "new!"));
        AZ_TracePrintf(AssetProcessor::DebugChannel, "-------------------------------------------\n");

        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, inputFilePath));
        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));

        UNIT_TEST_EXPECT_TRUE(connection.m_sent);
        UNIT_TEST_EXPECT_TRUE(payloadList.size() == 1);// We should always receive only one of these messages
        UNIT_TEST_EXPECT_TRUE(AZ::Utils::LoadObjectFromBufferInPlace(payloadList.at(0).second.data(), payloadList.at(0).second.size(), sourceFileChangedMessage));
        scanFolder = QDir(sourceFileChangedMessage.m_scanFolder.c_str());
        pathToCheck = scanFolder.filePath(sourceFileChangedMessage.m_relPath.c_str());
        UNIT_TEST_EXPECT_TRUE(QString::compare(inputFilePath, pathToCheck, Qt::CaseInsensitive) == 0);

        sortAssetToProcessResultList(processResults);

        // --------- same result as above ----------
        UNIT_TEST_EXPECT_TRUE(processResults.size() == 4); // 2 each for pc and es3,since we have two recognizer for .txt file
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_platform == processResults[1].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE(processResults[2].m_jobEntry.m_platform == processResults[3].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE((processResults[0].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[1].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[2].m_jobEntry.m_platform == "pc"));
        UNIT_TEST_EXPECT_TRUE((processResults[3].m_jobEntry.m_platform == "pc"));
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_computedFingerprint != 0);
        UNIT_TEST_EXPECT_TRUE(processResults[1].m_jobEntry.m_computedFingerprint != 0);

        for (int checkIdx = 0; checkIdx < 4; ++checkIdx)
        {
            QString processFile1 = processResults[checkIdx].m_jobEntry.m_absolutePathToFile;
            UNIT_TEST_EXPECT_TRUE(processFile1 == inputFilePath);
            QString platformFolder = cacheRoot.filePath(processResults[checkIdx].m_jobEntry.m_platform + "/" + gameName);
            platformFolder = AssetUtilities::NormalizeDirectoryPath(platformFolder);
            processFile1 = processResults[checkIdx].m_destinationPath;
            UNIT_TEST_EXPECT_TRUE(processFile1.startsWith(platformFolder));
            UNIT_TEST_EXPECT_TRUE(processResults[checkIdx].m_jobEntry.m_computedFingerprint != 0);
        }

        // this time make different products:

        QStringList oldes3outs;
        QStringList oldpcouts;
        oldes3outs = es3outs;
        oldpcouts.append(pcouts);
        QStringList es3outs2;
        QStringList pcouts2;
        es3outs.clear();
        pcouts.clear();
        es3outs.push_back(cacheRoot.filePath(QString("es3/") + gameName + "/basefilea.arc1"));
        es3outs2.push_back(cacheRoot.filePath(QString("es3/") + gameName + "/basefilea.azm"));
        // note that the ES3 outs have changed
        // but the pc outs are still the same.
        pcouts.push_back(cacheRoot.filePath(QString("pc/") + gameName + "/basefile.arc1"));
        pcouts2.push_back(cacheRoot.filePath(QString("pc/") + gameName + "/basefile.azm"));

        // feed it the messages its waiting for (create the files)
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(es3outs[0], "newfile."));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(pcouts[0], "newfile."));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(es3outs2[0], "newfile."));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(pcouts2[0], "newfile."));

        changedInputResults.clear();
        changedProductResults.clear();

        // send all the done messages simultaneously:

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(es3outs[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[0].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(es3outs2[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[1].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcouts[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[2].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcouts2[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[3].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        // let events bubble through:
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        UNIT_TEST_EXPECT_TRUE(changedInputResults.size() == 4);
        UNIT_TEST_EXPECT_TRUE(changedProductResults.size() == 4);

        // original products must no longer exist since it should have found and deleted them!
        for (QString outFile: oldes3outs)
        {
            UNIT_TEST_EXPECT_FALSE(QFile::exists(outFile));
        }

        // the old pc products should still exist because they were emitted this time around.
        for (QString outFile: oldpcouts)
        {
            UNIT_TEST_EXPECT_TRUE(QFile::exists(outFile));
        }

        changedInputResults.clear();
        changedProductResults.clear();
        processResults.clear();

        // add a fingerprint file thats next to the original file
        // feed it the exportsettings file again.
        // it should spawn BOTH compilers again.
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(inputFilePath + ".exportsettings", "new!"));
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, inputFilePath + ".exportsettings"));
        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));
        sortAssetToProcessResultList(processResults);

        // --------- same result as above ----------
        UNIT_TEST_EXPECT_TRUE(processResults.size() == 4); // pc and es3
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_platform == processResults[1].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE(processResults[2].m_jobEntry.m_platform == processResults[3].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE((processResults[0].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[1].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[2].m_jobEntry.m_platform == "pc"));
        UNIT_TEST_EXPECT_TRUE((processResults[3].m_jobEntry.m_platform == "pc"));
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_computedFingerprint != 0);

        // send all the done messages simultaneously:
        for (int checkIdx = 0; checkIdx < 4; ++checkIdx)
        {
            QString processFile1 = processResults[checkIdx].m_jobEntry.m_absolutePathToFile;
            UNIT_TEST_EXPECT_TRUE(processFile1 == inputFilePath);
            QString platformFolder = cacheRoot.filePath(processResults[checkIdx].m_jobEntry.m_platform + "/" + gameName);
            platformFolder = AssetUtilities::NormalizeDirectoryPath(platformFolder);
            processFile1 = processResults[checkIdx].m_destinationPath;
            UNIT_TEST_EXPECT_TRUE(processFile1.startsWith(platformFolder));
            UNIT_TEST_EXPECT_TRUE(processResults[checkIdx].m_jobEntry.m_computedFingerprint != 0);
        }


        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(es3outs[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[0].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(es3outs2[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[1].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcouts[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[2].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcouts2[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[3].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        // let events bubble through:
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        // --- delete the input asset and make sure it cleans up all products.

        changedInputResults.clear();
        changedProductResults.clear();
        processResults.clear();


        // first, delete the fingerprint file, this should result in normal reprocess:
        QFile::remove(inputFilePath + ".exportsettings");
        QMetaObject::invokeMethod(&apm, "AssessDeletedFile", Qt::QueuedConnection, Q_ARG(QString, inputFilePath + ".exportsettings"));
        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));

        sortAssetToProcessResultList(processResults);

        // --------- same result as above ----------
        UNIT_TEST_EXPECT_TRUE(processResults.size() == 4); // 2 each for pc and es3,since we have two recognizer for .txt file
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_platform == processResults[1].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE(processResults[2].m_jobEntry.m_platform == processResults[3].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE((processResults[0].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[1].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[2].m_jobEntry.m_platform == "pc"));
        UNIT_TEST_EXPECT_TRUE((processResults[3].m_jobEntry.m_platform == "pc"));
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_computedFingerprint != 0);
        UNIT_TEST_EXPECT_TRUE(processResults[1].m_jobEntry.m_computedFingerprint != 0);


        // send all the done messages simultaneously:

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(es3outs[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[0].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(es3outs2[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[1].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcouts[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[2].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcouts2[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[3].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        // let events bubble through:
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        // deleting the fingerprint file should not have erased the products
        UNIT_TEST_EXPECT_TRUE(QFile::exists(pcouts[0]));
        UNIT_TEST_EXPECT_TRUE(QFile::exists(es3outs[0]));
        UNIT_TEST_EXPECT_TRUE(QFile::exists(pcouts2[0]));
        UNIT_TEST_EXPECT_TRUE(QFile::exists(es3outs2[0]));

        changedInputResults.clear();
        changedProductResults.clear();
        processResults.clear();

        connection.m_sent = false;
        payloadList.clear();

        // delete the original input.
        QFile::remove(inputFilePath);

        SourceFileNotificationMessage sourceFileRemovedMessage;
        QMetaObject::invokeMethod(&apm, "AssessDeletedFile", Qt::QueuedConnection, Q_ARG(QString, inputFilePath));
        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));
        UNIT_TEST_EXPECT_TRUE(payloadList.size() == 5);// We should receive five messages one sourceRemoved and four assetRemoved

        messageLoadCount = 0;
        unsigned int countAssetRemoved = 0;

        for (auto payload : payloadList)
        {
            if (payload.first == AssetNotificationMessage::MessageType())
            {
                UNIT_TEST_EXPECT_TRUE(AZ::Utils::LoadObjectFromBufferInPlace(payload.second.data(), payload.second.size(), assetNotifMessage));
                UNIT_TEST_EXPECT_TRUE(assetNotifMessage.m_type == AssetNotificationMessage::AssetRemoved);
                ++messageLoadCount;
                ++countAssetRemoved;
            }
            else if (payload.first == SourceFileNotificationMessage::MessageType())
            {
                UNIT_TEST_EXPECT_TRUE(AZ::Utils::LoadObjectFromBufferInPlace(payload.second.data(), payload.second.size(), sourceFileRemovedMessage));
                UNIT_TEST_EXPECT_TRUE(sourceFileRemovedMessage.m_type == SourceFileNotificationMessage::FileRemoved);
                ++messageLoadCount;
            }
        }

        UNIT_TEST_EXPECT_TRUE(connection.m_sent);
        UNIT_TEST_EXPECT_TRUE(messageLoadCount == payloadList.size()); // make sure all messages are accounted for
        UNIT_TEST_EXPECT_TRUE(countAssetRemoved == 4);//Since we have 4 products for this file;
        scanFolder = QDir(sourceFileRemovedMessage.m_scanFolder.c_str());
        pathToCheck = scanFolder.filePath(sourceFileRemovedMessage.m_relPath.c_str());
        UNIT_TEST_EXPECT_TRUE(QString::compare(inputFilePath, pathToCheck, Qt::CaseInsensitive) == 0);

        // nothing to process, but products should be gone!
        UNIT_TEST_EXPECT_TRUE(processResults.isEmpty());
        UNIT_TEST_EXPECT_TRUE(changedInputResults.isEmpty());
        UNIT_TEST_EXPECT_TRUE(changedProductResults.isEmpty());

        UNIT_TEST_EXPECT_FALSE(QFile::exists(pcouts[0]));
        UNIT_TEST_EXPECT_FALSE(QFile::exists(es3outs[0]));
        UNIT_TEST_EXPECT_FALSE(QFile::exists(pcouts2[0]));
        UNIT_TEST_EXPECT_FALSE(QFile::exists(es3outs2[0]));

        changedInputResults.clear();
        failedInputResults.clear();
        changedProductResults.clear();
        processResults.clear();

        // test: if an asset fails, it should recompile it next time, and not report success
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(inputFilePath, "new2"));
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, inputFilePath));
        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));

        sortAssetToProcessResultList(processResults);

        // --------- same result as above ----------
        UNIT_TEST_EXPECT_TRUE(processResults.size() == 4); // 2 each for pc and es3,since we have two recognizer for .txt file
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_platform == processResults[1].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE(processResults[2].m_jobEntry.m_platform == processResults[3].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE((processResults[0].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[1].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[2].m_jobEntry.m_platform == "pc"));
        UNIT_TEST_EXPECT_TRUE((processResults[3].m_jobEntry.m_platform == "pc"));
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_computedFingerprint != 0);

        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(es3outs[0], "newfile."));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(es3outs2[0], "newfile."));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(pcouts2[0], "newfile."));

        // send both done messages simultaneously!
        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(es3outs[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[0].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(es3outs2[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[1].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        // send one failure only for PC :
        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcouts[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetFailed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[2].m_jobEntry));

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcouts2[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[3].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        // let events bubble through:
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        // ----------------------- test job info requests, some assets have failed (specifically, the [2] index job entry
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            AssetJobsInfoRequest requestInfo;

            requestInfo.m_assetPath = inputFilePath.toUtf8().data();

            {
                // send our request:
                payloadList.clear();
                connection.m_sent = false;
                apm.ProcessGetAssetJobRequest(requestId, &requestInfo);
            }

            // wait for it to process:
            QCoreApplication::processEvents(QEventLoop::AllEvents);

            UNIT_TEST_EXPECT_TRUE(connection.m_sent); // we expect a response to have arrived.

            unsigned int jobinformationResultIndex = -1;
            for (int index = 0; index < payloadList.size(); index++)
            {
                unsigned int type = payloadList.at(index).first;
                if (type == requestInfo.GetMessageType())
                {
                    jobinformationResultIndex = index;
                }
            }
            UNIT_TEST_EXPECT_FALSE(jobinformationResultIndex == -1);

            AssetJobsInfoResponse jobResponse;
            UNIT_TEST_EXPECT_TRUE(AZ::Utils::LoadObjectFromBufferInPlace(payloadList.at(jobinformationResultIndex).second.data(), payloadList.at(jobinformationResultIndex).second.size(), jobResponse));

            UNIT_TEST_EXPECT_TRUE(jobResponse.m_isSuccess);
            UNIT_TEST_EXPECT_TRUE(jobResponse.m_jobList.size() == processResults.size());

            // make sure each job corresponds to one in the process results list (but note that the order is not important).
            for (int oldJobIdx = azlossy_cast<int>(jobResponse.m_jobList.size()) - 1; oldJobIdx >= 0; --oldJobIdx)
            {
                bool foundIt = false;
                const JobInfo& jobInfo = jobResponse.m_jobList[oldJobIdx];

                // validate EVERY field
                UNIT_TEST_EXPECT_TRUE(jobInfo.m_jobId > 0); // jobs should never have zero as their key ever, they should all be unique.
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_sourceFile.empty());
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_platform.empty());
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_jobKey.empty());
                UNIT_TEST_EXPECT_FALSE(jobInfo.m_builderGuid.IsNull());

                for (int detailsIdx = 0; detailsIdx < processResults.size(); ++detailsIdx)
                {
                    const JobDetails& details = processResults[detailsIdx];

                    if ((QString::compare(jobInfo.m_sourceFile.c_str(), details.m_jobEntry.m_relativePathToFile, Qt::CaseInsensitive) == 0) &&
                        (QString::compare(jobInfo.m_platform.c_str(), details.m_jobEntry.m_platform, Qt::CaseInsensitive) == 0) &&
                        (QString::compare(jobInfo.m_jobKey.c_str(), details.m_jobEntry.m_jobKey, Qt::CaseInsensitive) == 0) &&
                        (jobInfo.m_builderGuid == details.m_jobEntry.m_builderUUID) &&
                        (jobInfo.m_jobId == details.m_jobEntry.m_jobId))
                    {
                        foundIt = true;

                        if (detailsIdx == 2) // we only said that the index [2] job was dead
                        {
                            UNIT_TEST_EXPECT_TRUE(jobInfo.m_status == JobStatus::Failed);
                        }
                        else
                        {
                            UNIT_TEST_EXPECT_TRUE(jobInfo.m_status == JobStatus::Completed);
                        }

                        break;
                    }
                }
                UNIT_TEST_EXPECT_TRUE(foundIt);
            }
        }

        // we should have get three success:
        UNIT_TEST_EXPECT_TRUE(changedInputResults.size() == 3);
        UNIT_TEST_EXPECT_TRUE(changedProductResults.size() == 3);
        UNIT_TEST_EXPECT_TRUE(failedInputResults.size() == 1);

        // which should be for the ES3:
        UNIT_TEST_EXPECT_TRUE(AssetUtilities::NormalizeFilePath(changedInputResults[0].first) == inputFilePath);

        // always RELATIVE, always with the product name.
        UNIT_TEST_EXPECT_TRUE(changedProductResults[0].first == "basefilea.arc1" || changedProductResults[0].first == "basefilea.azm");
        UNIT_TEST_EXPECT_TRUE(changedProductResults[0].second == "es3");
        UNIT_TEST_EXPECT_TRUE(AssetUtilities::NormalizeFilePath(failedInputResults[0].first) == inputFilePath);
        UNIT_TEST_EXPECT_TRUE(failedInputResults[0].second == "pc");

        // now if we notify again, only the pc should process:
        changedInputResults.clear();
        changedProductResults.clear();
        processResults.clear();

        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, inputFilePath));
        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));

        // --------- same result as above ----------
        UNIT_TEST_EXPECT_TRUE(processResults.size() == 1); // pc only
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_platform == "pc");

        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(pcouts[0], "new1"));

        // send one failure only for PC :

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcouts[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[0].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        // let events bubble through:
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        // we should have got only one success:
        UNIT_TEST_EXPECT_TRUE(changedInputResults.size() == 1);
        UNIT_TEST_EXPECT_TRUE(changedProductResults.size() == 1);

        // always RELATIVE, always with the product name.
        UNIT_TEST_EXPECT_TRUE(changedProductResults[0].first == "basefile.arc1");
        UNIT_TEST_EXPECT_TRUE(changedProductResults[0].second == "pc");

        changedInputResults.clear();
        changedProductResults.clear();
        processResults.clear();
        failedInputResults.clear();


        //----------This file is used for testing ProcessGetFullAssetPath function //
        inputFilePath = AssetUtilities::NormalizeFilePath(tempPath.absoluteFilePath("subfolder3/somerandomfile.random"));
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, inputFilePath));
        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));

        sortAssetToProcessResultList(processResults);

        // --------- same result as above ----------
        UNIT_TEST_EXPECT_TRUE(processResults.size() == 1); // 1 for pc
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_platform == "pc");

        pcouts.clear();

        pcouts.push_back(cacheRoot.filePath(QString("pc/") + gameName + "/subfolder3/randomfileoutput.random"));
        pcouts.push_back(cacheRoot.filePath(QString("pc/") + gameName + "/subfolder3/randomfileoutput.random1"));
        pcouts.push_back(cacheRoot.filePath(QString("pc/") + gameName + "/subfolder3/randomfileoutput.random2"));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(pcouts[0], "products."));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(pcouts[1], "products."));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(pcouts[2], "products."));

        //Invoke Asset Processed for pc platform , txt files job description
        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcouts[0].toUtf8().data()));
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcouts[1].toUtf8().data()));
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcouts[2].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[0].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        // let events bubble through:
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        UNIT_TEST_EXPECT_TRUE(changedProductResults.size() == 3);
        UNIT_TEST_EXPECT_TRUE(changedInputResults.size() == 1);
        UNIT_TEST_EXPECT_TRUE(failedInputResults.isEmpty());

        changedInputResults.clear();
        changedProductResults.clear();
        processResults.clear();
        failedInputResults.clear();


        // -------------- override test -----------------
        // set up by letting it compile basefile.txt from 3:
        inputFilePath = AssetUtilities::NormalizeFilePath(tempPath.absoluteFilePath("subfolder3/basefile.txt"));
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, inputFilePath));
        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));

        sortAssetToProcessResultList(processResults);

        // --------- same result as above ----------
        UNIT_TEST_EXPECT_TRUE(processResults.size() == 4); // 2 each for pc and es3,since we have two recognizer for .txt file
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_platform == processResults[1].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE(processResults[2].m_jobEntry.m_platform == processResults[3].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE((processResults[0].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[1].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[2].m_jobEntry.m_platform == "pc"));
        UNIT_TEST_EXPECT_TRUE((processResults[3].m_jobEntry.m_platform == "pc"));
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_computedFingerprint != 0);



        es3outs.clear();
        es3outs2.clear();
        pcouts.clear();
        pcouts2.clear();
        es3outs.push_back(cacheRoot.filePath(QString("es3/") + gameName + "/basefilez.arc1"));
        es3outs2.push_back(cacheRoot.filePath(QString("es3/") + gameName + "/basefileaz.azm"));
        // note that the ES3 outs have changed
        // but the pc outs are still the same.
        pcouts.push_back(cacheRoot.filePath(QString("pc/") + gameName + "/basefile.arc1"));
        pcouts2.push_back(cacheRoot.filePath(QString("pc/") + gameName + "/basefile.azm"));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(es3outs[0], "newfile."));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(pcouts[0], "newfile."));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(es3outs2[0], "newfile."));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(pcouts2[0], "newfile."));
        changedInputResults.clear();
        changedProductResults.clear();

        // send all the done messages simultaneously:
        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(es3outs[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[0].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(es3outs2[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[1].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcouts[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[2].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        response.m_outputProducts.clear();
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcouts2[0].toUtf8().data()));
        QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[3].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));

        // let events bubble through:
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        // we should have got only one success:
        UNIT_TEST_EXPECT_TRUE(changedInputResults.size() == 4);
        UNIT_TEST_EXPECT_TRUE(changedProductResults.size() == 4);
        UNIT_TEST_EXPECT_TRUE(failedInputResults.size() == 0);

        // ------------- setup complete, now do the test...
        // now feed it a file that has been overridden by a more important later file
        inputFilePath = AssetUtilities::NormalizeFilePath(tempPath.absoluteFilePath("subfolder1/basefile.txt"));

        changedInputResults.clear();
        changedProductResults.clear();
        processResults.clear();

        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, inputFilePath));

        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));
        UNIT_TEST_EXPECT_TRUE(processResults.isEmpty());
        UNIT_TEST_EXPECT_TRUE(changedInputResults.isEmpty());
        UNIT_TEST_EXPECT_TRUE(changedProductResults.isEmpty());

        // since it was overridden, nothing should occur.
        //AZ_TracePrintf("Asset Processor", "Preparing the assessDeletedFiles invocation...\n");

        // delete the highest priority override file and ensure that it generates tasks
        // for the next highest priority!  Basically, deleting this file should "reveal" the file underneath it in the other subfolder
        QString deletedFile = tempPath.absoluteFilePath("subfolder3/basefile.txt");
        QString expectedReplacementInputFile = AssetUtilities::NormalizeFilePath(tempPath.absoluteFilePath("subfolder2/basefile.txt"));

        UNIT_TEST_EXPECT_TRUE(QFile::remove(deletedFile));
        // sometimes the above deletion actually takes a moment to trickle, for some reason, and it doesn't actually get that the file was erased.
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        UNIT_TEST_EXPECT_FALSE(QFile::exists(deletedFile));

        QMetaObject::invokeMethod(&apm, "AssessDeletedFile", Qt::QueuedConnection, Q_ARG(QString,  deletedFile));

        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));

        sortAssetToProcessResultList(processResults);

        // --------- same result as above ----------
        UNIT_TEST_EXPECT_TRUE(processResults.size() == 4); // 2 each for pc and es3,since we have two recognizer for .txt file
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_platform == processResults[1].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE(processResults[2].m_jobEntry.m_platform == processResults[3].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE((processResults[0].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[1].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[2].m_jobEntry.m_platform == "pc"));
        UNIT_TEST_EXPECT_TRUE((processResults[3].m_jobEntry.m_platform == "pc"));
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_computedFingerprint != 0);

        for (int checkIdx = 0; checkIdx < 4; ++checkIdx)
        {
            QString processFile1 = processResults[checkIdx].m_jobEntry.m_absolutePathToFile;
            UNIT_TEST_EXPECT_TRUE(processFile1 == expectedReplacementInputFile);
            QString platformFolder = cacheRoot.filePath(processResults[checkIdx].m_jobEntry.m_platform + "/" + gameName);
            platformFolder = AssetUtilities::NormalizeDirectoryPath(platformFolder);
            processFile1 = processResults[checkIdx].m_destinationPath;
            UNIT_TEST_EXPECT_TRUE(processFile1.startsWith(platformFolder));
            UNIT_TEST_EXPECT_TRUE(processResults[checkIdx].m_jobEntry.m_computedFingerprint != 0);
        }

        if (!collectedChanges.isEmpty())
        {
            for (const QString& invalid : collectedChanges)
            {
                AZ_TracePrintf(AssetProcessor::DebugChannel, "Invalid change made: %s.\n", invalid.toUtf8().data());
            }
            Q_EMIT UnitTestFailed("Changes were made to the real file system, this is not allowed during this test.");
            return;
        }

        // -------------- FindSourceAssetName Test -----------------

        QString testSourceFile("test/folder1/test.txt");
        QString testPlatform("pc");
        QString testjobDescription("txt");
        QStringList testProductList;
        testProductList.append("pc/" + gameName + "/test/folder1/test.txt");

        apm.AddEntriesInDatabaseForUnitTest(testSourceFile, testPlatform, testjobDescription, testProductList);

        QString inputFile;
        QString platform;
        AssetStatus status = AssetStatus_Unknown;
        QString testAssetpath("@assets@\\test\\folder1\\test.txt");
        testAssetpath = "@assets@test\\folder1\\test.txt";
        testAssetpath = "@assets@test\\folder2\\test.txt";

        testSourceFile = "subfolder8/a/b/c/test.txt";
        testProductList.clear();
        testProductList.append("pc/" + gameName + "/subfolder8/a/b/c/test.txt");
        apm.AddEntriesInDatabaseForUnitTest(testSourceFile, testPlatform, testjobDescription, testProductList);
        status = AssetStatus_Unknown;
        testAssetpath = "subfolder8/a/b/c/test.txt";
        QString relPath("subfolder3/somefile.xxx");
        QString filePath = tempPath.absoluteFilePath(relPath);
        unsigned int fingerprintForPC = 0;
        unsigned int fingerprintForES3 = 0;

        ComputeFingerprints(fingerprintForPC, fingerprintForES3, config, filePath, relPath);

        processResults.clear();

        //AZ_TracePrintf("Test", "------------");

        inputFilePath = AssetUtilities::NormalizeFilePath(filePath);
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, inputFilePath));
        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 50000));

        sortAssetToProcessResultList(processResults);

        UNIT_TEST_EXPECT_TRUE(processResults.size() == 4); // // 2 each for pc and es3,since we have two recognizer for .xxx file
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_platform == processResults[1].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE(processResults[2].m_jobEntry.m_platform == processResults[3].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE((processResults[0].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[1].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[2].m_jobEntry.m_platform == "pc"));
        UNIT_TEST_EXPECT_TRUE((processResults[3].m_jobEntry.m_platform == "pc"));

        config.RemoveRecognizer("xxx files 2 (builder2)");
        UNIT_TEST_EXPECT_TRUE(mockAppManager.UnRegisterAssetRecognizerAsBuilder("xxx files 2 (builder2)"));

        //Changing specs for pc
        specpc.m_extraRCParams = "new pcparams";
        rec.m_platformSpecs.insert("pc", specpc);

        config.AddRecognizer(rec);
        mockAppManager.RegisterAssetRecognizerAsBuilder(rec);

        processResults.clear();
        inputFilePath = AssetUtilities::NormalizeFilePath(filePath);
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, inputFilePath));
        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));

        // we never actually submitted any fingerprints or indicated success, so the same number of jobs should occur as before
        sortAssetToProcessResultList(processResults);

        UNIT_TEST_EXPECT_TRUE(processResults.size() == 4); // // 2 each for pc and es3,since we have two recognizer for .xxx file
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_platform == processResults[1].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE(processResults[2].m_jobEntry.m_platform == processResults[3].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE((processResults[0].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[1].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[2].m_jobEntry.m_platform == "pc"));
        UNIT_TEST_EXPECT_TRUE((processResults[3].m_jobEntry.m_platform == "pc"));

        // tell it that all those assets are now successfully done:
        for (auto processResult : processResults)
        {
            QString outputFile = normalizedCacheRootDir.absoluteFilePath(processResult.m_destinationPath + "/doesn'tmatter.dds" + processResult.m_jobEntry.m_jobKey);
            CreateDummyFile(outputFile);
            AssetBuilderSDK::ProcessJobResponse response;
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
            response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(outputFile.toUtf8().data()));
            apm.AssetProcessed(processResult.m_jobEntry, response);
        }

        // now re-perform the same test, this time only the pc ones should re-appear.  
        // this should happen because we're changing the extra params, which should be part of the fingerprint
        // if this unit test fails, check to make sure that the extra params are being ingested into the fingerprint computation functions
        // and also make sure that the jobs that are for the remaining es3 platform don't change.

        // store the UUID so that we can insert the new one with the same UUID
        AZStd::shared_ptr<InternalMockBuilder> builderTxt2Builder;
        UNIT_TEST_EXPECT_TRUE(mockAppManager.GetBuilderByID("xxx files 2 (builder2)", builderTxt2Builder));
        AZ::Uuid builderUuid = builderTxt2Builder->GetUuid();
        builderTxt2Builder.reset();

        config.RemoveRecognizer("xxx files 2 (builder2)");
        mockAppManager.UnRegisterAssetRecognizerAsBuilder("xxx files 2 (builder2)");
        //Changing specs for pc
        specpc.m_extraRCParams = "new pcparams---"; // make sure the xtra params are different.
        rec.m_platformSpecs.remove("pc");
        rec.m_platformSpecs.insert("pc", specpc);

        config.AddRecognizer(rec);
        mockAppManager.RegisterAssetRecognizerAsBuilder(rec, &builderUuid);

        processResults.clear();

        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, inputFilePath));

        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));

        sortAssetToProcessResultList(processResults);

        UNIT_TEST_EXPECT_TRUE(processResults.size() == 1); // only 1 for pc
        UNIT_TEST_EXPECT_TRUE((processResults[0].m_jobEntry.m_platform == "pc"));

        // ---------------------

        unsigned int newfingerprintForPC = 0;
        unsigned int newfingerprintForES3 = 0;

        ComputeFingerprints(newfingerprintForPC, newfingerprintForES3, config, filePath, relPath);

        UNIT_TEST_EXPECT_TRUE(newfingerprintForPC != fingerprintForPC);//Fingerprints should be different
        UNIT_TEST_EXPECT_TRUE(newfingerprintForES3 == fingerprintForES3);//Fingerprints are same

        config.RemoveRecognizer("xxx files 2 (builder2)");
        mockAppManager.UnRegisterAssetRecognizerAsBuilder("xxx files 2 (builder2)");

        //Changing version
        rec.m_version = "1.0";
        config.AddRecognizer(rec);
        mockAppManager.RegisterAssetRecognizerAsBuilder(rec, &builderUuid);

        processResults.clear();

        inputFilePath = AssetUtilities::NormalizeFilePath(filePath);
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, inputFilePath));
        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));
        UNIT_TEST_EXPECT_TRUE(processResults.size() == 2); // pc and es3
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_platform != processResults[1].m_jobEntry.m_platform);
        UNIT_TEST_EXPECT_TRUE((processResults[0].m_jobEntry.m_platform == "pc") || (processResults[0].m_jobEntry.m_platform == "es3"));
        UNIT_TEST_EXPECT_TRUE((processResults[1].m_jobEntry.m_platform == "pc") || (processResults[1].m_jobEntry.m_platform == "es3"));

        unsigned int newfingerprintForPCAfterVersionChange = 0;
        unsigned int newfingerprintForES3AfterVersionChange = 0;

        ComputeFingerprints(newfingerprintForPCAfterVersionChange, newfingerprintForES3AfterVersionChange, config, filePath, relPath);

        UNIT_TEST_EXPECT_TRUE((newfingerprintForPCAfterVersionChange != fingerprintForPC) || (newfingerprintForPCAfterVersionChange != newfingerprintForPC));//Fingerprints should be different
        UNIT_TEST_EXPECT_TRUE((newfingerprintForES3AfterVersionChange != fingerprintForES3) || (newfingerprintForES3AfterVersionChange != newfingerprintForES3));//Fingerprints should be different

        //------Test for Files which are excluded
        processResults.clear();
        inputFilePath = AssetUtilities::NormalizeFilePath(tempPath.absoluteFilePath("subfolder3/savebackup/test.txt"));
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, inputFilePath));
        UNIT_TEST_EXPECT_FALSE(BlockUntil(idling, 3000)); //Processing a file that will be excluded should not cause assetprocessor manager to emit the onBecameIdle signal because its state should not change
        UNIT_TEST_EXPECT_TRUE(processResults.size() == 0);

        // ----- Test the GetAssetID function, which given a full path to an asset, checks the mappings and turns it into an Asset ID ---
        {
            QString outputString;
            int resultCode = 0;

            QMetaObject::Connection conn = QObject::connect(&apm, &AssetProcessorManager::SendUnitTestInfo,
                    [this, &outputString, &resultCode](QString path, bool result)
            {
                resultCode = result;
                outputString = path;
            });

            QString fileToCheck = tempPath.absoluteFilePath("subfolder3/basefile.txt");
            AzToolsFramework::AssetSystem::GetAssetIdRequest request(fileToCheck.toUtf8().data());
            
            unsigned int connId = 0;
            unsigned int serial = 0;
            NetworkRequestID requestId(connId, serial);
            apm.ProcessGetAssetID(requestId, &request);
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == QString("basefile.arc1"));  // since it outputs a product, its actually supposed to give the actual ASSETID of the first result.

            fileToCheck = tempPath.absoluteFilePath("subfolder2/aaa/basefile.txt");
            request.m_assetPath = fileToCheck.toUtf8().data();
            apm.ProcessGetAssetID(requestId, &request);
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == QString("aaa/basefile.txt"));

#if defined(AZ_PLATFORM_WINDOWS)
            fileToCheck = "d:\\test.txt";
#else
            fileToCheck = "/test.txt"; // rooted
#endif
            request.m_assetPath = fileToCheck.toUtf8().data();
            apm.ProcessGetAssetID(requestId, &request);
            UNIT_TEST_EXPECT_TRUE(resultCode == 0); // fail
            UNIT_TEST_EXPECT_TRUE(outputString == fileToCheck);

            //feed it a relative path
            fileToCheck = "\test.txt";
            request.m_assetPath = fileToCheck.toUtf8().data();
            apm.ProcessGetAssetID(requestId, &request);
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == QString("\test.txt"));

            //feed it a product path with gamename
            fileToCheck = normalizedCacheRootDir.filePath("pc/" + gameName + "/aaa/basefile.txt");
            request.m_assetPath = fileToCheck.toUtf8().data();
            apm.ProcessGetAssetID(requestId, &request);
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == QString("aaa/basefile.txt"));

            //feed it a product path without gamename
            fileToCheck = normalizedCacheRootDir.filePath("pc/basefile.txt");
            request.m_assetPath = fileToCheck.toUtf8().data();
            apm.ProcessGetAssetID(requestId, &request);
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == QString("basefile.txt"));

            //feed it a product path with gamename but poor casing (test 1:  the pc platform is not matching case)
            fileToCheck = normalizedCacheRootDir.filePath("Pc/" + gameName + "/aaa/basefile.txt");
            request.m_assetPath = fileToCheck.toUtf8().data();
            apm.ProcessGetAssetID(requestId, &request);
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == QString("aaa/basefile.txt"));

            //feed it a product path with gamename but poor casing (test 2:  the gameName is not matching case)
            fileToCheck = normalizedCacheRootDir.filePath("pc/" + gameName.toUpper() + "/aaa/basefile.txt");
            request.m_assetPath = fileToCheck.toUtf8().data();
            apm.ProcessGetAssetID(requestId, &request);
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == QString("aaa/basefile.txt"));

            //feed it a product path that resolves to a directory name instead of a file.
            fileToCheck = normalizedCacheRootDir.filePath("pc/" + gameName.toUpper() + "/aaa");
            request.m_assetPath = fileToCheck.toUtf8().data();
            apm.ProcessGetAssetID(requestId, &request);
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == QString("aaa"));

            // ----- Test the ProcessGetFullAssetPath function

            //feed it an assetId
            fileToCheck = "subfolder3/randomfileoutput.random1";
            QString testPlatform("pc");
            AzFramework::AssetSystem::GetAssetPathRequest pathRequest(fileToCheck.toUtf8().data());
            pathRequest.m_assetId = fileToCheck.toUtf8().data();
            apm.ProcessGetFullAssetPath(requestId, &pathRequest, testPlatform);
            outputString.remove(0, tempPath.path().length() + 1);//adding one for the native separator
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == QString("subfolder3/somerandomfile.random"));

            //feed it another assetId
            fileToCheck = "subfolder3/randomfileoutput.random2";
            pathRequest.m_assetId = fileToCheck.toUtf8().data();
            apm.ProcessGetFullAssetPath(requestId, &pathRequest, testPlatform);
            outputString.remove(0, tempPath.path().length() + 1);//adding one for the native separator
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == QString("subfolder3/somerandomfile.random"));

            //feed it the same assetId with different separators
            fileToCheck = "subfolder3\\randomfileoutput.random2";
            pathRequest.m_assetId = fileToCheck.toUtf8().data();
            apm.ProcessGetFullAssetPath(requestId, &pathRequest, testPlatform);
            outputString.remove(0, tempPath.path().length() + 1);//adding one for the native separator
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == QString("subfolder3/somerandomfile.random"));

            //feed it a full path
            fileToCheck = tempPath.filePath("somefolder/somefile.txt");
            pathRequest.m_assetId = fileToCheck.toUtf8().data();
            apm.ProcessGetFullAssetPath(requestId, &pathRequest, testPlatform);
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == fileToCheck);

            //feed it a path with alias and asset id
            fileToCheck = "@assets@/subfolder3/randomfileoutput.random1";
            pathRequest.m_assetId = fileToCheck.toUtf8().data();
            apm.ProcessGetFullAssetPath(requestId, &pathRequest, testPlatform);
            outputString.remove(0, tempPath.path().length() + 1);//adding one for the native separator
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == QString("subfolder3/somerandomfile.random"));

            //feed it a path with some random alias and asset id
            fileToCheck = "@somerandomalias@/subfolder3/randomfileoutput.random1";
            pathRequest.m_assetId = fileToCheck.toUtf8().data();
            apm.ProcessGetFullAssetPath(requestId, &pathRequest, testPlatform);
            outputString.remove(0, tempPath.path().length() + 1);//adding one for the native separator
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == QString("subfolder3/somerandomfile.random"));

            //feed it a path with some random alias and asset id but no separator
            fileToCheck = "@somerandomalias@subfolder3/randomfileoutput.random1";
            pathRequest.m_assetId = fileToCheck.toUtf8().data();
            apm.ProcessGetFullAssetPath(requestId, &pathRequest, testPlatform);
            outputString.remove(0, tempPath.path().length() + 1);//adding one for the native separator
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == QString("subfolder3/somerandomfile.random"));

            //feed it a path with alias and input name
            fileToCheck = "@assets@/somerandomfile.random";
            pathRequest.m_assetId = fileToCheck.toUtf8().data();
            apm.ProcessGetFullAssetPath(requestId, &pathRequest, testPlatform);
            outputString.remove(0, tempPath.path().length() + 1);//adding one for the native separator
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == QString("subfolder3/somerandomfile.random"));

            //feed it an absolute path with cacheroot
            fileToCheck = normalizedCacheRootDir.filePath("pc/" + gameName + "/subfolder3/randomfileoutput.random1");
            pathRequest.m_assetId = fileToCheck.toUtf8().data();
            apm.ProcessGetFullAssetPath(requestId, &pathRequest, testPlatform);
            outputString.remove(0, tempPath.path().length() + 1);//adding one for the native separator
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == QString("subfolder3/somerandomfile.random"));

            //feed it a productName directly
            fileToCheck = "pc/" + gameName + "/subfolder3/randomfileoutput.random1";
            pathRequest.m_assetId = fileToCheck.toUtf8().data();
            apm.ProcessGetFullAssetPath(requestId, &pathRequest, testPlatform);
            outputString.remove(0, tempPath.path().length() + 1);//adding one for the native separator
            UNIT_TEST_EXPECT_TRUE(resultCode != 0);
            UNIT_TEST_EXPECT_TRUE(outputString == QString("subfolder3/somerandomfile.random"));
            QObject::disconnect(conn);
        }

        // -----------------------------------------------------------------------------------------------
        // -------------------------------------- FOLDER RENAMING TEST -----------------------------------
        // -----------------------------------------------------------------------------------------------
        // Test: Rename a source folder

        // test renaming an entire folder!

        QString fileToMove1 = tempPath.absoluteFilePath("subfolder1/rename_this/somefile1.txt");
        QString fileToMove2 = tempPath.absoluteFilePath("subfolder1/rename_this/somefolder/somefile2.txt");

        config.RemoveRecognizer(builderTxt2Name); // don't need this anymore.
        mockAppManager.UnRegisterAssetRecognizerAsBuilder(builderTxt2Name);

        processResults.clear();
        // put the two files on the map:
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, fileToMove1));
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, fileToMove2));

        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));
        sortAssetToProcessResultList(processResults);

        UNIT_TEST_EXPECT_TRUE(processResults.size() == 4); // 2 fils on 2 platforms

        for (int index = 0; index < processResults.size(); ++index)
        {
            QFileInfo fi(processResults[index].m_jobEntry.m_absolutePathToFile);
            QString pcout = QDir(processResults[index].m_destinationPath).absoluteFilePath(fi.fileName());
            UNIT_TEST_EXPECT_TRUE(CreateDummyFile(pcout, "products."));

            response.m_outputProducts.clear();
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
            response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcout.toUtf8().data()));
            QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[index].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));
        }


        // let events bubble through:
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

        // setup complete.  now RENAME that folder.

        QDir renamer;
        UNIT_TEST_EXPECT_TRUE(renamer.rename(tempPath.absoluteFilePath("subfolder1/rename_this"), tempPath.absoluteFilePath("subfolder1/done_renaming")));

        // renames appear as a delete then add of that folder:
        processResults.clear();
        removedProducts.clear();
        QMetaObject::invokeMethod(&apm, "AssessDeletedFile", Qt::QueuedConnection, Q_ARG(QString, tempPath.absoluteFilePath("subfolder1/rename_this")));

        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));

        sortAssetToProcessResultList(processResults);

        UNIT_TEST_EXPECT_TRUE(processResults.size() == 0); // nothing to process
        UNIT_TEST_EXPECT_TRUE(removedProducts.size() == 4); // we are aware that 4 products went missing

        processResults.clear();
        QMetaObject::invokeMethod(&apm, "AssessAddedFile", Qt::QueuedConnection, Q_ARG(QString, tempPath.absoluteFilePath("subfolder1/done_renaming")));

        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));

        sortAssetToProcessResultList(processResults);

        UNIT_TEST_EXPECT_TRUE(processResults.size() == 4); // 2 files on 2 platforms

        // -----------------------------------------------------------------------------------------------
        // -------------------------------------- FOLDER RENAMING TEST -----------------------------------
        // -----------------------------------------------------------------------------------------------
        // Test: Rename a cache folder

        QStringList outputsCreated;

        for (int index = 0; index < processResults.size(); ++index)
        {
            QFileInfo fi(processResults[index].m_jobEntry.m_absolutePathToFile);
            QString pcout = QDir(processResults[index].m_destinationPath).absoluteFilePath(fi.fileName());
            UNIT_TEST_EXPECT_TRUE(CreateDummyFile(pcout, "products."));
            
            response.m_outputProducts.clear();
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
            response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcout.toUtf8().data()));
            QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[index].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));
        }

        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

        // it now believes that there are a whole bunch of assets in subfolder1/done_renaming and they resulted in
        // a whole bunch of files to have been created in the asset cache, listed in processresults, and they exist in outputscreated...
        // rename the output folder:

        QString originalCacheFolderName = normalizedCacheRootDir.absoluteFilePath("pc") + "/" + gameName + "/done_renaming";
        QString newCacheFolderName = normalizedCacheRootDir.absoluteFilePath("pc") + "/" + gameName + "/renamed_again";

        UNIT_TEST_EXPECT_TRUE(renamer.rename(originalCacheFolderName, newCacheFolderName));

        // tell it that the products moved:
        processResults.clear();
        removedProducts.clear();
        QMetaObject::invokeMethod(&apm, "AssessDeletedFile", Qt::QueuedConnection, Q_ARG(QString, originalCacheFolderName));
        QMetaObject::invokeMethod(&apm, "AssessAddedFile", Qt::QueuedConnection, Q_ARG(QString, newCacheFolderName));

        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));

        sortAssetToProcessResultList(processResults);

        UNIT_TEST_EXPECT_TRUE(removedProducts.size() != 0); // should result in removed products inside the cache folder
        UNIT_TEST_EXPECT_TRUE(processResults.size() == 2); // ONLY the PC files need to be re-processed because only those were renamed.
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_platform == "pc");
        UNIT_TEST_EXPECT_TRUE(processResults[1].m_jobEntry.m_platform == "pc");


        // -----------------------------------------------------------------------------------------------
        // -------------------------------------- FOLDER RENAMING TEST -----------------------------------
        // -----------------------------------------------------------------------------------------------
        // Test: Rename folders that did not have files in them (but had child files, this was a bug at a point)

        fileToMove1 = tempPath.absoluteFilePath("subfolder1/rename_this_secondly/somefolder/somefile2.txt");

        processResults.clear();
        // put the two files on the map:
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, fileToMove1));


        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));
        sortAssetToProcessResultList(processResults);

        UNIT_TEST_EXPECT_TRUE(processResults.size() == 2); // 1 file on 2 platforms

        for (int index = 0; index < processResults.size(); ++index)
        {
            QFileInfo fi(processResults[index].m_jobEntry.m_absolutePathToFile);
            QString pcout = QDir(processResults[index].m_destinationPath).absoluteFilePath(fi.fileName());
            UNIT_TEST_EXPECT_TRUE(CreateDummyFile(pcout, "products."));

            response.m_outputProducts.clear();
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
            response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcout.toUtf8().data()));
            QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[index].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));
        }

        // let events bubble through:
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

        // setup complete.  now RENAME that folder.

        originalCacheFolderName = normalizedCacheRootDir.absoluteFilePath("pc") + "/" + gameName + "/rename_this_secondly";
        newCacheFolderName = normalizedCacheRootDir.absoluteFilePath("pc") + "/" + gameName + "/done_renaming_again";

        UNIT_TEST_EXPECT_TRUE(renamer.rename(originalCacheFolderName, newCacheFolderName));

        // tell it that the products moved:
        processResults.clear();
        removedProducts.clear();
        QMetaObject::invokeMethod(&apm, "AssessDeletedFile", Qt::QueuedConnection, Q_ARG(QString, originalCacheFolderName));
        QMetaObject::invokeMethod(&apm, "AssessAddedFile", Qt::QueuedConnection, Q_ARG(QString, newCacheFolderName));

        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));

        sortAssetToProcessResultList(processResults);

        UNIT_TEST_EXPECT_TRUE(removedProducts.size() != 0); // should result in removed products inside the cache folder
        UNIT_TEST_EXPECT_TRUE(processResults.size() == 1); // ONLY the PC files need to be re-processed because only those were renamed.
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_platform == "pc");

        // --------------------------------------------------------------------------------------------------
        // ------------------------------ TEST DELETED SOURCE RESULTING IN DELETED PRODUCTS -----------------
        // --------------------------------------------------------------------------------------------------

        // first, set up a whole pipeline to create, notify, and consume the file:
        fileToMove1 = tempPath.absoluteFilePath("subfolder1/to_be_deleted/some_deleted_file.txt");

        processResults.clear();
        // put the two files on the map:
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, fileToMove1));

        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));
        sortAssetToProcessResultList(processResults);

        UNIT_TEST_EXPECT_TRUE(processResults.size() == 2); // 1 file on 2 platforms

        QStringList createdDummyFiles;

        for (int index = 0; index < processResults.size(); ++index)
        {
            QFileInfo fi(processResults[index].m_jobEntry.m_absolutePathToFile);
            QString pcout = QDir(processResults[index].m_destinationPath).absoluteFilePath(fi.fileName());
            UNIT_TEST_EXPECT_TRUE(CreateDummyFile(pcout, "products."));

            response.m_outputProducts.clear();
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
            response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(pcout.toUtf8().data()));
            QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[index].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));
        }

        // let events bubble through:
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

        processResults.clear();
        removedProducts.clear();

        // setup complete.  now delete the source file:
        UNIT_TEST_EXPECT_TRUE(renamer.remove(fileToMove1));
        QMetaObject::invokeMethod(&apm, "AssessDeletedFile", Qt::QueuedConnection, Q_ARG(QString, fileToMove1));

        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));
        sortAssetToProcessResultList(processResults);
        UNIT_TEST_EXPECT_TRUE(removedProducts.size() == 2); // all products must be removed.
        UNIT_TEST_EXPECT_TRUE(processResults.size() == 0); // nothing should process

        for (int index = 0; index < createdDummyFiles.size(); ++index)
        {
            QFileInfo fi(createdDummyFiles[index]);
            UNIT_TEST_EXPECT_FALSE(fi.exists());
            // in fact, the directory must also no longer exist in the cache:
            UNIT_TEST_EXPECT_FALSE(fi.dir().exists());
        }

        // --------------------------------------------------------------------------------------------------
        // - TEST SOURCE FILE REPROCESSING RESULTING IN FEWER PRODUCTS NEXT TIME ----------------------------
        // (it needs to delete the products and it needs to notify listeners about it)
        // --------------------------------------------------------------------------------------------------

        // first, set up a whole pipeline to create, notify, and consume the file:
        fileToMove1 = tempPath.absoluteFilePath("subfolder1/fewer_products/test.txt");

        processResults.clear();
        // put the two files on the map:
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, fileToMove1));

        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));
        sortAssetToProcessResultList(processResults);

        UNIT_TEST_EXPECT_TRUE(processResults.size() == 2); // 1 file on 2 platforms

        createdDummyFiles.clear(); // keep track of the files which we expect to be gone next time

        for (int index = 0; index < processResults.size(); ++index)
        {
            response.m_outputProducts.clear();
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;

            // this time, ouput 2 files for each job instead of just one:
            QFileInfo fi(processResults[index].m_jobEntry.m_absolutePathToFile);

            response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(QDir(processResults[index].m_destinationPath).absoluteFilePath(fi.fileName() + ".0.txt").toUtf8().data()));
            response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(QDir(processResults[index].m_destinationPath).absoluteFilePath(fi.fileName() + ".1.txt").toUtf8().data()));

            createdDummyFiles.push_back(response.m_outputProducts[0].m_productFileName.c_str()); // we're only gong to delete this one out of the two, which is why we don't push the other one.

            UNIT_TEST_EXPECT_TRUE(CreateDummyFile(response.m_outputProducts[0].m_productFileName.c_str(), "product 0"));
            UNIT_TEST_EXPECT_TRUE(CreateDummyFile(response.m_outputProducts[1].m_productFileName.c_str(), "product 1"));
            
            QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[index].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));
        }

        // let events bubble through:
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

        // at this point, we have a cache with the four files (2 for each platform)
        // we're going to resubmit the job with different data
        UNIT_TEST_EXPECT_TRUE(renamer.remove(fileToMove1));
        UNIT_TEST_EXPECT_TRUE(CreateDummyFile(fileToMove1, "fresh data!"));

        processResults.clear();

        // tell file changed:
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, fileToMove1));
        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));

        sortAssetToProcessResultList(processResults);

        UNIT_TEST_EXPECT_TRUE(processResults.size() == 2); // 1 file on 2 platforms

        removedProducts.clear();
        removedProductPlatforms.clear();
        changedProductResults.clear();

        for (int index = 0; index < processResults.size(); ++index)
        {
            response.m_outputProducts.clear();
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;

            // this time, ouput only one file for each job instead of just one:
            QFileInfo fi(processResults[index].m_jobEntry.m_absolutePathToFile);

            response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(QDir(processResults[index].m_destinationPath).absoluteFilePath(fi.fileName() + ".1.txt").toUtf8().data()));
            UNIT_TEST_EXPECT_TRUE(CreateDummyFile(response.m_outputProducts[0].m_productFileName.c_str(), "product 1 changed"));

            QMetaObject::invokeMethod(&apm, "AssetProcessed", Qt::QueuedConnection, Q_ARG(JobEntry, processResults[index].m_jobEntry), Q_ARG(AssetBuilderSDK::ProcessJobResponse, response));
        }

        // let events bubble through:
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

        sortAssetToProcessResultList(processResults);

        // we should have gotten 2 product removed, 2 product changed
        UNIT_TEST_EXPECT_TRUE(removedProducts.size() == 2);
        UNIT_TEST_EXPECT_TRUE(changedProductResults.size() == 2);
        UNIT_TEST_EXPECT_TRUE(removedProducts[0] == "fewer_products/test.txt.0.txt");
        UNIT_TEST_EXPECT_TRUE(removedProductPlatforms[0] == "es3");
        UNIT_TEST_EXPECT_TRUE(removedProducts[1] == "fewer_products/test.txt.0.txt");
        UNIT_TEST_EXPECT_TRUE(removedProductPlatforms[1] == "pc");

        UNIT_TEST_EXPECT_TRUE(changedProductResults[0].first == "fewer_products/test.txt.1.txt");
        UNIT_TEST_EXPECT_TRUE(changedProductResults[0].second == "es3");
        UNIT_TEST_EXPECT_TRUE(changedProductResults[1].first == "fewer_products/test.txt.1.txt");
        UNIT_TEST_EXPECT_TRUE(changedProductResults[1].second == "pc");

        // and finally, the actual removed products should be gone from the HDD:
        for (int index = 0; index < createdDummyFiles.size(); ++index)
        {
            QFileInfo fi(createdDummyFiles[index]);
            UNIT_TEST_EXPECT_FALSE(fi.exists());
            // the directory must still exist because there were other files in there (no accidental deletions!)
            UNIT_TEST_EXPECT_TRUE(fi.dir().exists());
        }

        // -----------------------------------------------------------------------------------------------
        // ------------------- ASSETBUILDER TEST---------------------------------------------------
        //------------------------------------------------------------------------------------------------

        mockAppManager.ResetMatchingBuildersInfoFunctionCalls();
        mockAppManager.ResetMockBuilderCreateJobCalls();
        mockAppManager.UnRegisterAllBuilders();

        AssetRecognizer abt_rec1;
        AssetPlatformSpec abt_speces3;
        abt_rec1.m_name = "UnitTestTextBuilder1";
        abt_rec1.m_patternMatcher = AssetUtilities::FilePatternMatcher("*.txt", AssetBuilderSDK::AssetBuilderPattern::Wildcard);
        //abt_rec1.m_regexp.setPatternSyntax(QRegExp::Wildcard);
        //abt_rec1.m_regexp.setPattern("*.txt");
        abt_rec1.m_platformSpecs.insert("es3", speces3);
        mockAppManager.RegisterAssetRecognizerAsBuilder(abt_rec1);

        AssetRecognizer abt_rec2;
        AssetPlatformSpec abt_specpc;
        abt_rec2.m_name = "UnitTestTextBuilder2";
        abt_rec2.m_patternMatcher = AssetUtilities::FilePatternMatcher("*.txt", AssetBuilderSDK::AssetBuilderPattern::Wildcard);
        //abt_rec2.m_regexp.setPatternSyntax(QRegExp::Wildcard);
        //abt_rec2.m_regexp.setPattern("*.txt");
        abt_rec2.m_platformSpecs.insert("pc", specpc);
        mockAppManager.RegisterAssetRecognizerAsBuilder(abt_rec2);

        processResults.clear();

        inputFilePath = AssetUtilities::NormalizeFilePath(tempPath.absoluteFilePath("subfolder3/uniquefile.txt"));

        // Pass the txt file through the asset pipeline
        QMetaObject::invokeMethod(&apm, "AssessModifiedFile", Qt::QueuedConnection, Q_ARG(QString, inputFilePath));
        UNIT_TEST_EXPECT_TRUE(BlockUntil(idling, 5000));
        UNIT_TEST_EXPECT_TRUE(mockAppManager.GetMatchingBuildersInfoFunctionCalls() == 1);
        UNIT_TEST_EXPECT_TRUE(mockAppManager.GetMockBuilderCreateJobCalls() == 2);  // Since we have two text builder registered

        AssetProcessor::BuilderInfoList builderInfoList;
        mockAppManager.GetMatchingBuildersInfo(AZStd::string(inputFilePath.toUtf8().data()), builderInfoList);
        auto builderInfoListCount = builderInfoList.size();
        UNIT_TEST_EXPECT_TRUE(builderInfoListCount == 2);

        for (auto& buildInfo : builderInfoList)
        {
            AZStd::shared_ptr<InternalMockBuilder> builder;
            UNIT_TEST_EXPECT_TRUE(mockAppManager.GetBuilderByID(buildInfo.m_name, builder));

            UNIT_TEST_EXPECT_TRUE(builder->GetCreateJobCalls() == 1);

            QString watchedFolder(AssetUtilities::NormalizeFilePath(builder->GetLastCreateJobRequest().m_watchFolder.c_str()));
            QString expectedWatchedFolder(tempPath.absoluteFilePath("subfolder3"));
            UNIT_TEST_EXPECT_TRUE(QString::compare(watchedFolder, expectedWatchedFolder, Qt::CaseInsensitive) == 0); // verify watchfolder

            QString filename(AssetUtilities::NormalizeFilePath(builder->GetLastCreateJobRequest().m_sourceFile.c_str()));
            QString expectFileName("uniquefile.txt");
            UNIT_TEST_EXPECT_TRUE(QString::compare(filename, expectFileName, Qt::CaseInsensitive) == 0); // verify filename
            builder->ResetCreateJobCalls();
        }

        sortAssetToProcessResultList(processResults);

        UNIT_TEST_EXPECT_TRUE(processResults.size() == 2); // 1 for pc and es3
        UNIT_TEST_EXPECT_TRUE(processResults[0].m_jobEntry.m_platform == "es3");
        UNIT_TEST_EXPECT_TRUE(processResults[1].m_jobEntry.m_platform == "pc");
        UNIT_TEST_EXPECT_TRUE(QString::compare(processResults[0].m_jobEntry.m_absolutePathToFile, inputFilePath, Qt::CaseInsensitive) == 0);
        UNIT_TEST_EXPECT_TRUE(QString::compare(processResults[1].m_jobEntry.m_absolutePathToFile, inputFilePath, Qt::CaseInsensitive) == 0);
        UNIT_TEST_EXPECT_TRUE(QString::compare(QString(processResults[0].m_jobEntry.m_jobKey), QString(abt_rec1.m_name)) == 0);
        UNIT_TEST_EXPECT_TRUE(QString::compare(QString(processResults[1].m_jobEntry.m_jobKey), QString(abt_rec2.m_name)) == 0);
        Q_EMIT UnitTestPassed();
    }

#include <native/unittests/AssetProcessorManagerUnitTests.moc>
} // namespace AssetProcessor

#endif



