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
#include "RCcontrollerUnitTests.h"
#include <native/resourcecompiler/rcjoblistmodel.h>
#include "native/resourcecompiler/rcjob.h"
#include <QModelIndex>
#include <AzFramework/Asset/AssetProcessorMessages.h>
#include <QDebug>
#include <QString>
#include <QCoreApplication>
#include <QTemporaryDir>
#include "native/assetprocessor.h"
#include "native/utilities/assetUtils.h"

using namespace AssetProcessor;
using namespace AzFramework::AssetSystem;

class MockRCJob : public RCJob
{
    void DoWork(AssetBuilderSDK::ProcessJobResponse& /*result*/, BuilderParams& /*builderParams*/, AssetProcessor::QuitListener& /*listener*/) override
    {
        m_DoWorkCalled = true;
    }
public:
    bool m_DoWorkCalled = false;
};

RCcontrollerUnitTests::RCcontrollerUnitTests()
    : m_rcController(1, 4)
{
}

void RCcontrollerUnitTests::StartTest()
{
    PrepareRCController();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    RunRCControllerTests();
}

void RCcontrollerUnitTests::PrepareRCController()
{
    // Create 6 jobs
    using namespace AssetProcessor;
    RCJobListModel* rcJobListModel = m_rcController.getQueueModel();

    AssetProcessor::JobDetails jobDetails;
    jobDetails.m_jobEntry.m_relativePathToFile = "someFile0.txt";
    jobDetails.m_jobEntry.m_platform = "pc";
    jobDetails.m_jobEntry.m_jobKey = "Text files";
    jobDetails.m_jobEntry.m_jobId = 1;

    RCJob* job0 = new RCJob(rcJobListModel);
    job0->Init(jobDetails);
    rcJobListModel->addNewJob(job0);

    RCJob* job1 = new RCJob(rcJobListModel);
    jobDetails.m_jobEntry.m_relativePathToFile = "someFile1.txt";
    jobDetails.m_jobEntry.m_platform = "pc";
    jobDetails.m_jobEntry.m_jobKey = "Text files";
    jobDetails.m_jobEntry.m_jobId = 2;
    job1->Init(jobDetails);
    rcJobListModel->addNewJob(job1);

    RCJob* job2 = new RCJob(rcJobListModel);
    jobDetails.m_jobEntry.m_relativePathToFile = "someFile2.txt";
    jobDetails.m_jobEntry.m_platform = "pc";
    jobDetails.m_jobEntry.m_jobKey = "Text files";
    jobDetails.m_jobEntry.m_jobId = 3;
    job2->Init(jobDetails);
    rcJobListModel->addNewJob(job2);

    RCJob* job3 = new RCJob(rcJobListModel);
    jobDetails.m_jobEntry.m_relativePathToFile = "someFile3.txt";
    jobDetails.m_jobEntry.m_platform = "pc";
    jobDetails.m_jobEntry.m_jobKey = "Text files";
    jobDetails.m_jobEntry.m_jobId = 4;
    job3->Init(jobDetails);
    rcJobListModel->addNewJob(job3);

    RCJob* job4 = new RCJob(rcJobListModel);
    jobDetails.m_jobEntry.m_relativePathToFile = "someFile4.txt";
    jobDetails.m_jobEntry.m_platform = "pc";
    jobDetails.m_jobEntry.m_jobKey = "Text files";
    jobDetails.m_jobEntry.m_jobId = 5;
    job4->Init(jobDetails);
    rcJobListModel->addNewJob(job4);

    RCJob* job5 = new RCJob(rcJobListModel);
    jobDetails.m_jobEntry.m_relativePathToFile = "someFile5.txt";
    jobDetails.m_jobEntry.m_platform = "pc";
    jobDetails.m_jobEntry.m_jobKey = "Text files";
    jobDetails.m_jobEntry.m_jobId = 6;
    job5->Init(jobDetails);
    rcJobListModel->addNewJob(job5);

    // Complete 1 job
    RCJob* rcJob = job0;
    rcJobListModel->markAsProcessing(rcJob);
    rcJob->SetState(RCJob::completed);
    rcJobListModel->markAsCompleted(rcJob);

    // Put 1 job in processing state
    rcJob = job1;
    rcJobListModel->markAsProcessing(rcJob);
    m_expectedProcessingJobID = rcJob->jobID();
}

void RCcontrollerUnitTests::RunRCControllerTests()
{
    // doing this prevents any new jobs being queued:
    m_rcController.QuitRequested();

    int jobsInQueueCount = 0;
    QString platformInQueueCount;
    bool gotJobsInQueueCall = false;

    QObject::connect(&m_rcController, &RCController::JobsInQueuePerPlatform, this, [&](QString platformName, int newCount)
    {
        gotJobsInQueueCall = true;
        platformInQueueCount = platformName;
        jobsInQueueCount = newCount;
    });

    RCJobListModel* rcJobListModel = m_rcController.getQueueModel();
    int returnedCount = rcJobListModel->rowCount(QModelIndex());
    int expectedCount = 5; // finished ones should be removed, so it shouldn't show up

    if (returnedCount != expectedCount)
    {
        Q_EMIT UnitTestFailed("RCJobListModel has " + QString(returnedCount) + " elements, which is invalid. Expected " + expectedCount);
        return;
    }

    QModelIndex rcJobIndex;
    QString rcJobJobID;
    QString rcJobCommand;
    QString rcJobState;

    for (int i = 0; i < expectedCount; i++)
    {
        rcJobIndex = rcJobListModel->index(i, 0, QModelIndex());

        if (!rcJobIndex.isValid())
        {
            Q_EMIT UnitTestFailed("ModelIndex for row " + QString(i) + " is invalid.");
            return;
        }

        if (rcJobIndex.row() >= expectedCount)
        {
            Q_EMIT UnitTestFailed("ModelIndex for row " + QString(i) + " is invalid (outside expected range).");
            return;
        }

        rcJobJobID = rcJobListModel->data(rcJobIndex, RCJobListModel::jobIDRole).toString();
        rcJobCommand = rcJobListModel->data(rcJobIndex, RCJobListModel::displayNameRole).toString();
        rcJobState = rcJobListModel->data(rcJobIndex, RCJobListModel::stateRole).toString();

        // order is no longer guarinteed by the model.  If your view wants ordered data
        // then use a sort filter proxy model. note that finished jobs are removed.
        if (rcJobJobID.toUInt() == m_expectedProcessingJobID)
        {
            if (rcJobState.compare("Processing") != 0)
            {
                Q_EMIT UnitTestFailed("RCJob at index " + QString("%1").arg(i) + " has unexpected state: " + rcJobState);
                return;
            }
        }
        else
        {
            if (rcJobState.compare("Pending") != 0)
            {
                Q_EMIT UnitTestFailed("RCJob at index " + QString("%1").arg(i) + " has unexpected state: " + rcJobState);
                return;
            }
        }
    }

    // ----------------- test the Compile Group functionality
    // add some jobs to play with:

    QStringList tempJobNames;

    // test - exact match
    tempJobNames << "c:/somerandomfolder/dev/blah/test.dds";
    tempJobNames << "c:/somerandomfolder/dev/blah/test.cre"; // must not match

    // test - NO MATCH
    tempJobNames << "c:/somerandomfolder/dev/wap/wap.wap";

    // test - Multiple match, ignoring extensions
    tempJobNames << "c:/somerandomfolder/dev/abc/123.456";
    tempJobNames << "c:/somerandomfolder/dev/abc/123.567";
    tempJobNames << "c:/somerandomfolder/dev/def/123.456"; // must not match
    tempJobNames << "c:/somerandomfolder/dev/def/123.567"; // must not match

    // test - Multiple match, wide search, ignoring extensions and postfixes like underscore
    tempJobNames << "c:/somerandomfolder/dev/aaa/bbb/123.456";
    tempJobNames << "c:/somerandomfolder/dev/aaa/bbb/123.567";
    tempJobNames << "c:/somerandomfolder/dev/aaa/bbb/123.890";
    tempJobNames << "c:/somerandomfolder/dev/aaa/ccc/123.567"; // must not match!
    tempJobNames << "c:/somerandomfolder/dev/aaa/ccc/456.567"; // must not match

    // test - compile group fails the moment any file in it fails
    tempJobNames << "c:/somerandomfolder/mmmnnnoo/123.456";
    tempJobNames << "c:/somerandomfolder/mmmnnnoo/123.567";

    QList<RCJob*> createdJobs;
    AssetBuilderSDK::AssetBuilderDesc builderDesc;

    for (const QString& name : tempJobNames)
    {
        RCJob* job = new RCJob(rcJobListModel);
        AssetProcessor::JobDetails jobDetails;
        jobDetails.m_jobEntry.m_relativePathToFile = name;
        jobDetails.m_jobEntry.m_platform = "pc";
        jobDetails.m_jobEntry.m_jobKey = "Compile Stuff";
        job->Init(jobDetails);
        rcJobListModel->addNewJob(job);
        createdJobs.push_back(job);
    }

    // double them up for "es3" to make sure that platform is respected
    for (const QString& name : tempJobNames)
    {
        RCJob* job0 = new RCJob(rcJobListModel);
        AssetProcessor::JobDetails jobDetails;
        jobDetails.m_jobEntry.m_relativePathToFile = name;
        jobDetails.m_jobEntry.m_platform = "es3";
        jobDetails.m_jobEntry.m_jobKey = "Compile Other Stuff";
        job0->Init(jobDetails);
        rcJobListModel->addNewJob(job0);
    }

    bool gotCreated = false;
    bool gotCompleted = false;
    NetworkRequestID gotGroupID;
    AssetStatus gotStatus = AssetStatus_Unknown;
    QObject::connect(&m_rcController, &RCController::CompileGroupCreated, this, [&](NetworkRequestID groupID, AssetStatus status)
    {
        gotCreated = true;
        gotGroupID = groupID;
        gotStatus = status;
    });

    QObject::connect(&m_rcController, &RCController::CompileGroupFinished, this, [&](NetworkRequestID groupID, AssetStatus status)
    {
        gotCompleted = true;
        gotGroupID = groupID;
        gotStatus = status;
    });


    // EXACT MATCH TEST (with prefixes and such)
    NetworkRequestID requestID(1, 1234);
    m_rcController.OnRequestCompileGroup(requestID, "pc", "@assets@/blah/test.dds");
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    // this should have matched exactly one item, and when we finish that item, it should terminate:
    UNIT_TEST_EXPECT_TRUE(gotCreated);
    UNIT_TEST_EXPECT_FALSE(gotCompleted);
    UNIT_TEST_EXPECT_TRUE(gotGroupID == requestID);
    UNIT_TEST_EXPECT_TRUE(gotStatus == AssetStatus_Queued);

    gotCreated = false;
    gotCompleted = false;

    // FINISH that job, we expect the finished message:

    rcJobListModel->markAsProcessing(createdJobs[0]);
    createdJobs[0]->SetState(RCJob::completed);
    m_rcController.finishJob(createdJobs[0]);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    UNIT_TEST_EXPECT_FALSE(gotCreated);
    UNIT_TEST_EXPECT_TRUE(gotCompleted);
    UNIT_TEST_EXPECT_TRUE(gotGroupID == requestID);
    UNIT_TEST_EXPECT_TRUE(gotStatus == AssetStatus_Compiled);

    // ----------------------- NO MATCH TEST -----------------
    // give it a name that for sure does not match:
    gotCreated = false;
    gotCompleted = false;
    m_rcController.OnRequestCompileGroup(requestID, "pc", "bibbidybobbidy.boo");
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    UNIT_TEST_EXPECT_TRUE(gotCreated);
    UNIT_TEST_EXPECT_FALSE(gotCompleted);
    UNIT_TEST_EXPECT_TRUE(gotGroupID == requestID);
    UNIT_TEST_EXPECT_TRUE(gotStatus == AssetStatus_Unknown);

    // ----------------------- NO MATCH TEST (invalid platform) -----------------
    // give it a name that for sure does not match due to platform
    gotCreated = false;
    gotCompleted = false;
    m_rcController.OnRequestCompileGroup(requestID, "aaaaaa", "blah/test.cre");
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    UNIT_TEST_EXPECT_TRUE(gotCreated);
    UNIT_TEST_EXPECT_FALSE(gotCompleted);
    UNIT_TEST_EXPECT_TRUE(gotGroupID == requestID);
    UNIT_TEST_EXPECT_TRUE(gotStatus == AssetStatus_Unknown);

    // -------------------------------- test - Multiple match, ignoring extensions
    // Give it something that should match 3 and 4  but not 5 and not 6

    gotCreated = false;
    gotCompleted = false;
    m_rcController.OnRequestCompileGroup(requestID, "pc", "abc/123.nnn");
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    UNIT_TEST_EXPECT_TRUE(gotCreated);
    UNIT_TEST_EXPECT_FALSE(gotCompleted);
    UNIT_TEST_EXPECT_TRUE(gotGroupID == requestID);
    UNIT_TEST_EXPECT_TRUE(gotStatus == AssetStatus_Queued);

    // complete one of them.  It should still be a busy group
    gotCreated = false;
    gotCompleted = false;
    rcJobListModel->markAsProcessing(createdJobs[3]);
    createdJobs[3]->SetState(RCJob::completed);
    m_rcController.finishJob(createdJobs[3]);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    UNIT_TEST_EXPECT_FALSE(gotCreated);
    UNIT_TEST_EXPECT_FALSE(gotCompleted);

    // finish the other
    gotCreated = false;
    gotCompleted = false;
    rcJobListModel->markAsProcessing(createdJobs[4]);
    createdJobs[4]->SetState(RCJob::completed);
    m_rcController.finishJob(createdJobs[4]);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    UNIT_TEST_EXPECT_TRUE(gotCompleted);
    UNIT_TEST_EXPECT_FALSE(gotCreated);
    UNIT_TEST_EXPECT_TRUE(gotGroupID == requestID);
    UNIT_TEST_EXPECT_TRUE(gotStatus == AssetStatus_Compiled);

    // test - Multiple match, wide search, ignoring extensions and postfixes like underscore:
    // we expect 7, 8, and 9 to match

    gotCreated = false;
    gotCompleted = false;
    m_rcController.OnRequestCompileGroup(requestID, "pc", "aaa/bbb/123_45.abc");
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    UNIT_TEST_EXPECT_TRUE(gotCreated);
    UNIT_TEST_EXPECT_FALSE(gotCompleted);
    UNIT_TEST_EXPECT_TRUE(gotGroupID == requestID);
    UNIT_TEST_EXPECT_TRUE(gotStatus == AssetStatus_Queued);

    // complete two of them.  It should still be a busy group!
    gotCreated = false;
    gotCompleted = false;

    rcJobListModel->markAsProcessing(createdJobs[7]);
    createdJobs[7]->SetState(RCJob::completed);
    m_rcController.finishJob(createdJobs[7]);

    rcJobListModel->markAsProcessing(createdJobs[8]);
    createdJobs[8]->SetState(RCJob::completed);
    m_rcController.finishJob(createdJobs[8]);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    UNIT_TEST_EXPECT_FALSE(gotCreated);
    UNIT_TEST_EXPECT_FALSE(gotCompleted);

    // finish the final one
    rcJobListModel->markAsProcessing(createdJobs[9]);
    createdJobs[9]->SetState(RCJob::completed);
    m_rcController.finishJob(createdJobs[9]);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    UNIT_TEST_EXPECT_TRUE(gotCompleted);
    UNIT_TEST_EXPECT_FALSE(gotCreated);
    UNIT_TEST_EXPECT_TRUE(gotGroupID == requestID);
    UNIT_TEST_EXPECT_TRUE(gotStatus == AssetStatus_Compiled);

    // ---------------- TEST :  Ensure that a group fails when any member of it fails.
    gotCreated = false;
    gotCompleted = false;
    m_rcController.OnRequestCompileGroup(requestID, "pc", "mmmnnnoo/123.ZZZ"); // should match exactly 2 elements
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    UNIT_TEST_EXPECT_TRUE(gotCreated);
    UNIT_TEST_EXPECT_FALSE(gotCompleted);
    UNIT_TEST_EXPECT_TRUE(gotGroupID == requestID);
    UNIT_TEST_EXPECT_TRUE(gotStatus == AssetStatus_Queued);

    gotCreated = false;
    gotCompleted = false;

    rcJobListModel->markAsProcessing(createdJobs[12]);
    createdJobs[12]->SetState(RCJob::failed);
    m_rcController.finishJob(createdJobs[12]);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    // this should have failed it immediately.

    UNIT_TEST_EXPECT_TRUE(gotCompleted);
    UNIT_TEST_EXPECT_FALSE(gotCreated);
    UNIT_TEST_EXPECT_TRUE(gotGroupID == requestID);
    UNIT_TEST_EXPECT_TRUE(gotStatus == AssetStatus_Failed);

    /// --- TEST ------------- RC controller should not accept duplicate jobs.

    JobDetails details;
    details.m_jobEntry = JobEntry(1, "d:/test/test1.txt", "test1.txt", AZ::Uuid("{7954065D-CFD1-4666-9E4C-3F36F417C7AC}"), "pc", "Test Job", 1234);
    gotJobsInQueueCall = false;
    int priorJobs = jobsInQueueCount;
    m_rcController.jobSubmitted(details);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    UNIT_TEST_EXPECT_TRUE(gotJobsInQueueCall);
    UNIT_TEST_EXPECT_TRUE(jobsInQueueCount = priorJobs + 1);
    priorJobs = jobsInQueueCount;
    gotJobsInQueueCall = false;

    // submit same job:
    details.m_jobEntry = JobEntry(2, "d:/test/test1.txt", "test1.txt", AZ::Uuid("{7954065D-CFD1-4666-9E4C-3F36F417C7AC}"), "pc", "Test Job", 1234);
    m_rcController.jobSubmitted(details);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    UNIT_TEST_EXPECT_FALSE(gotJobsInQueueCall);

    // submit same job but different platform:
    details.m_jobEntry = JobEntry(3, "d:/test/test1.txt", "test1.txt", AZ::Uuid("{7954065D-CFD1-4666-9E4C-3F36F417C7AC}"), "es3", "Test Job", 1234);
    m_rcController.jobSubmitted(details);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    UNIT_TEST_EXPECT_TRUE(gotJobsInQueueCall);
    UNIT_TEST_EXPECT_TRUE(jobsInQueueCount = priorJobs + 1);


    ////--------------- RCJob Test with critical locking TRUE
    QTemporaryDir dir;
    QDir tempPath(dir.path());
    // test task generation while a file is in still in use
    QString fileInUsePath = AssetUtilities::NormalizeFilePath(tempPath.absoluteFilePath("subfolder4/needsLock.tiff"));

    UNIT_TEST_EXPECT_TRUE(UnitTestUtils::CreateDummyFile(fileInUsePath, "xxx"));

    QFile lockFileTest(fileInUsePath);
#if defined(AZ_PLATFORM_WINDOWS)
    // on windows, its enough to just open the file:
    lockFileTest.open(QFile::ReadOnly);
#else
    int handleOfLock = open(fileInUsePath.toUtf8().constData(), O_RDONLY | O_EXLOCK | O_NONBLOCK);
    UNIT_TEST_EXPECT_TRUE(handleOfLock != -1);
#endif

    MockRCJob rcJob;
    bool beginWork = false;
    QObject::connect(&rcJob, &RCJob::BeginWork, this, [this, &beginWork]()
    {
        beginWork = true;
    }
    );
    bool jobFinished = false;
    QObject::connect(&rcJob, &RCJob::jobFinished, this, [this, &jobFinished](AssetBuilderSDK::ProcessJobResponse /*result*/)
    {
        jobFinished = true;
    }
    );
    rcJob.SetInputFileAbsolutePath(fileInUsePath.toUtf8().data());
    rcJob.SetCheckExclusiveLock(true);
    rcJob.start();

    UNIT_TEST_EXPECT_FALSE(UnitTestUtils::BlockUntil(beginWork, 5000));

#if defined(AZ_PLATFORM_WINDOWS)
    // Once we release the file, it should process normally
    lockFileTest.close();
#else
    close(handleOfLock);
#endif

    //Once we release the lock we should see jobStarted and jobFinished
    UNIT_TEST_EXPECT_TRUE(UnitTestUtils::BlockUntil(jobFinished, 5000));
    UNIT_TEST_EXPECT_TRUE(beginWork);
    UNIT_TEST_EXPECT_TRUE(rcJob.m_DoWorkCalled);

    Q_EMIT UnitTestPassed();
}

#include <native/unittests/RCcontrollerUnitTests.moc>


REGISTER_UNIT_TEST(RCcontrollerUnitTests)

