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

#include "rccontroller.h"
#include <QStringList>
#include <QTimer>
#include <QThread>
#include <QtMath> // for qCeil
#include <QCoreApplication>

#include "native/utilities/AssetUtils.h"
#include "native/utilities/PlatformConfiguration.h"
#include "native/AssetManager/AssetData.h"
#include "native/resourcecompiler/RCCommon.h"

#include <AzFramework/Asset/AssetProcessorMessages.h>

namespace AssetProcessor
{
    RCController::RCController(int cfg_minJobs, int cfg_maxJobs, QObject* parent)
        : QObject(parent)
        , m_dispatchingJobs(false)
        , m_shuttingDown(false)
    {
        AssetProcessorPlatformBus::Handler::BusConnect();

        // Determine a good starting value for max jobs
        int maxJobs = QThread::idealThreadCount();
        if (maxJobs == -1)
        {
            maxJobs = 3;
        }

        maxJobs = qMax<int>(maxJobs - 1, 1);

        m_maxJobs = cfg_maxJobs ? qMax<unsigned int>(cfg_minJobs, qMin<unsigned int>(cfg_maxJobs, maxJobs)) : maxJobs;

        m_RCQueueSortModel.AttachToModel(&m_RCJobListModel);
    }

    RCController::~RCController()
    {
        AssetProcessorPlatformBus::Handler::BusDisconnect();
        m_RCQueueSortModel.AttachToModel(nullptr);
    }

    RCJobListModel* RCController::getQueueModel()
    {
        return &m_RCJobListModel;
    }

    void RCController::startJob(RCJob* rcJob)
    {
        Q_ASSERT(rcJob);
        // request to be notified when job is done
        QObject::connect(rcJob, &RCJob::finished, this, [this, rcJob]()
        {
            finishJob(rcJob);
        }, Qt::QueuedConnection);

        QObject::connect(rcJob, &RCJob::BeginWork, this, [this, rcJob]()
        {
            m_RCJobListModel.markAsStarted(rcJob);
        }, Qt::QueuedConnection);

        // Mark as "being processed" by moving to Processing list
        m_RCJobListModel.markAsProcessing(rcJob);
        Q_EMIT JobStatusChanged(rcJob->GetJobEntry(), AzToolsFramework::AssetSystem::JobStatus::InProgress);
        rcJob->start();
        Q_EMIT JobStarted(rcJob->GetInputFileRelativePath(), rcJob->GetPlatform());
    }

    void RCController::QuitRequested()
    {
        m_shuttingDown = true;

        if (m_RCJobListModel.jobsInFlight() == 0)
        {
            Q_EMIT ReadyToQuit(this);
            return;
        }

        QTimer::singleShot(10, this, SLOT(QuitRequested()));
    }

    int RCController::NumberOfPendingCriticalJobsPerPlatform(QString platform)
    {
        return m_pendingCriticalJobsPerPlatform[platform.toLower()];
    }

    void RCController::finishJob(RCJob* rcJob)
    {
        QString platform = rcJob->GetPlatform();
        auto found = m_jobsCountPerPlatform.find(platform);
        if (found != m_jobsCountPerPlatform.end())
        {
            int prevCount = found.value();
            if (prevCount > 0)
            {
                int newCount = prevCount - 1;
                m_jobsCountPerPlatform[platform] = newCount;
                Q_EMIT JobsInQueuePerPlatform(platform, newCount);
            }
        }

        CheckCompileAssetsGroup(rcJob->GetElementID(), rcJob->GetState());

        if (rcJob->IsCritical())
        {
            int criticalJobsCount = m_pendingCriticalJobsPerPlatform[platform.toLower()] - 1;
            m_pendingCriticalJobsPerPlatform[platform.toLower()] = criticalJobsCount;
        }

        if (rcJob->GetState() != RCJob::completed) 
        {
            Q_EMIT fileFailed(rcJob->GetJobEntry());
        }
        else
        {
            Q_EMIT fileCompiled(rcJob->GetJobEntry(), AZStd::move(rcJob->GetProcessJobResponse()));
        }

        // Move to Completed list which will mark as "completed"
        // unless a different state has been set.
        m_RCJobListModel.markAsCompleted(rcJob);

        if (!m_shuttingDown)
        {
            // Start next job only if we are not shutting down
            dispatchJobs();

            // if there is no next job, and nothing is in flight, we are done.
            if (IsIdle())
            {
                Q_EMIT BecameIdle();
            }
        }
    }

    bool RCController::IsIdle()
    {
        return ((!m_RCQueueSortModel.GetNextPendingJob()) && (m_RCJobListModel.jobsInFlight() == 0));
    }

    void RCController::jobSubmitted(JobDetails details)
    {
        AssetProcessor::QueueElementID checkFile(details.m_jobEntry.m_relativePathToFile, details.m_jobEntry.m_platform, details.m_jobEntry.m_jobKey);

        if (m_RCJobListModel.isInQueue(checkFile))
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Job is already in queue - ignored [%s, %s, %s]\n", checkFile.GetInputAssetName().toUtf8().data(), checkFile.GetPlatform().toUtf8().data(), checkFile.GetJobDescriptor().toUtf8().data());
            return;
        }

        RCJob* rcJob = new RCJob(&m_RCJobListModel);
        rcJob->Init(details); // note - move operation.  From this point on you must use the job details to refer to it.

        m_RCJobListModel.addNewJob(rcJob);
        QString platformName = rcJob->GetPlatform();// we need to get the actual platform from the rcJob
        if (rcJob->IsCritical())
        {
            int criticalJobsCount = m_pendingCriticalJobsPerPlatform[platformName.toLower()] + 1;
            m_pendingCriticalJobsPerPlatform[platformName.toLower()] = criticalJobsCount;
        }
        auto found = m_jobsCountPerPlatform.find(platformName);
        if (found != m_jobsCountPerPlatform.end())
        {
            int newCount = found.value() + 1;
            m_jobsCountPerPlatform[platformName] = newCount;
        }
        else
        {
            m_jobsCountPerPlatform[platformName] = 1;
        }
        Q_EMIT JobsInQueuePerPlatform(platformName, m_jobsCountPerPlatform[platformName]);
        Q_EMIT JobStatusChanged(rcJob->GetJobEntry(), AzToolsFramework::AssetSystem::JobStatus::Queued);

        // Start the job we just received if no job currently running
        if ((!m_shuttingDown) && (!m_dispatchingJobs))
        {
            QMetaObject::invokeMethod(this, "dispatchJobs", Qt::QueuedConnection);
        }
    }

    void RCController::dispatchJobs()
    {
        if (!m_dispatchingJobs)
        {
            m_dispatchingJobs = true;
            RCJob* rcJob = m_RCQueueSortModel.GetNextPendingJob();

            while (m_RCJobListModel.jobsInFlight() < m_maxJobs && rcJob && !m_shuttingDown)
            {
                startJob(rcJob);
                rcJob = m_RCQueueSortModel.GetNextPendingJob();
            }
            m_dispatchingJobs = false;
        }
    }

    void RCController::OnRequestCompileGroup(AssetProcessor::NetworkRequestID groupID, QString platform, QString searchTerm)
    {
        // someone has asked for a compile group to be created that conforms to that search term.
        // the goal here is to use a heuristic to find any assets that match the search term and place them in a new group
        // then respond with the appropriate response.

        // lets do some minimal processing on the search term
        QSet<AssetProcessor::QueueElementID> results;
        m_RCJobListModel.PerformHeuristicSearch(AssetUtilities::NormalizeAndRemoveAlias(searchTerm), platform, results);

        if (results.isEmpty())
        {
            // nothing found
            Q_EMIT CompileGroupCreated(groupID, AzFramework::AssetSystem::AssetStatus_Unknown);
        }
        else
        {
            for (const auto element : results)
            {
                m_RCQueueSortModel.AddCompileRequest(element, true);
            }
            m_activeCompileGroups.push_back(AssetCompileGroup());
            m_activeCompileGroups.back().m_groupMembers.swap(results);
            m_activeCompileGroups.back().m_requestID = groupID;

            Q_EMIT CompileGroupCreated(groupID, AzFramework::AssetSystem::AssetStatus_Queued);
        }
    }

    void RCController::CheckCompileAssetsGroup(const AssetProcessor::QueueElementID& queuedElement, RCJob::JobState state)
    {
        if (m_activeCompileGroups.empty())
        {
            return;
        }

        // start at the end so that we can actually erase the compile groups and not skip any:
        for (int groupIdx = m_activeCompileGroups.size() - 1; groupIdx >= 0; --groupIdx)
        {
            AssetCompileGroup& compileGroup = m_activeCompileGroups[groupIdx];
            auto it = compileGroup.m_groupMembers.find(queuedElement);
            if (it != compileGroup.m_groupMembers.end())
            {
                // this compile group contains the expected group!
                m_RCQueueSortModel.RemoveCompileRequest(queuedElement, true);
                compileGroup.m_groupMembers.erase(it);
                if ((compileGroup.m_groupMembers.isEmpty()) || (state != RCJob::completed))
                {
                    // if we get here, we're either empty (and successed) or we failed one and have now failed
                    Q_EMIT CompileGroupFinished(compileGroup.m_requestID, state != RCJob::completed ? AzFramework::AssetSystem::AssetStatus_Failed : AzFramework::AssetSystem::AssetStatus_Compiled);
                    m_activeCompileGroups.removeAt(groupIdx);
                }
            }
        }
    }
} // Namespace AssetProcessor

#include <native/resourcecompiler/rccontroller.moc>
