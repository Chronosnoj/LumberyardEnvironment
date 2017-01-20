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
#if defined(MODEL_TEST)
#include "modeltest.h"
#endif

#include "rcjoblistmodel.h"
#include "native/assetprocessor.h"
#include <QHash>
#include <QString>
#include <QStringRef>

#include <AzCore/Debug/Trace.h>

namespace AssetProcessor
{
    RCJobListModel::RCJobListModel(QObject* parent)
        : QAbstractItemModel(parent)
    {
#if defined(MODEL_TEST)
        // Test model behaviour from creation in debug
        new ModelTest(this, this);
#endif
    }

    int RCJobListModel::rowCount(const QModelIndex& parent) const
    {
        if (parent.isValid())
        {
            return 0;
        }
        return itemCount();
    }

    QModelIndex RCJobListModel::parent(const QModelIndex& index) const
    {
        return QModelIndex();
    }

    QModelIndex RCJobListModel::index(int row, int column, const QModelIndex& parent) const
    {
        if (row >= rowCount(parent) || column >= columnCount(parent))
        {
            return QModelIndex();
        }
        return createIndex(row, column);
    }

    int RCJobListModel::columnCount(const QModelIndex& parent) const
    {
        return parent.isValid() ? 0 : Column::Max;
    }

    QVariant RCJobListModel::headerData(int section, Qt::Orientation orientation, int role) const
    {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        {
            switch (section)
            {
            case ColumnState:
                return tr("State");
            case ColumnJobId:
                return tr("Job Id");
            case ColumnCommand:
                return tr("Asset");
            case ColumnCompleted:
                return tr("Completed");
            case ColumnPlatform:
                return tr("Platform");
            default:
                break;
            }
        }

        return QAbstractItemModel::headerData(section, orientation, role);
    }

    unsigned int RCJobListModel::jobsInFlight() const
    {
        return m_jobsInFlight.size();
    }


    QVariant RCJobListModel::data(const QModelIndex& index, int role) const
    {
        if (!index.isValid())
        {
            return QVariant();
        }

        if (index.row() >= itemCount())
        {
            return QVariant();
        }

        switch (role)
        {
        case jobIDRole:
            return QVariant(getItem(index.row())->jobID());
        case stateRole:
            return RCJob::stateDescription(getItem(index.row())->GetState());
        case displayNameRole:
            return getItem(index.row())->GetInputFileRelativePath();
        case timeCreatedRole:
            return getItem(index.row())->timeCreated().toString("hh:mm:ss.zzz");
        case timeLaunchedRole:
            return getItem(index.row())->timeLaunched().toString("hh:mm:ss.zzz");
        case timeCompletedRole:
            return getItem(index.row())->timeCompleted().toString("hh:mm:ss.zzz");

        case Qt::DisplayRole:
            switch (index.column())
            {
            case ColumnJobId:
                return QVariant(getItem(index.row())->jobID());
            case ColumnState:
                return RCJob::stateDescription(getItem(index.row())->GetState());
            case ColumnCommand:
                return getItem(index.row())->GetInputFileRelativePath();
            case ColumnCompleted:
                return getItem(index.row())->timeCompleted().toString("hh:mm:ss.zzz");
            case ColumnPlatform:
                return getItem(index.row())->GetPlatform();
            default:
                break;
            }
        default:
            break;
        }

        return QVariant();
    }

    int RCJobListModel::itemCount() const
    {
        return m_jobs.size();
    }

    RCJob* RCJobListModel::getItem(int index) const
    {
        if (index >= 0 && index < m_jobs.size())
        {
            return m_jobs[index];
        }

        return nullptr; //invalid index
    }

    bool RCJobListModel::isEmpty()
    {
        return m_jobs.empty();
    }

    void RCJobListModel::addNewJob(RCJob* rcJob)
    {
        int posForInsert = m_jobs.size();
        beginInsertRows(QModelIndex(), posForInsert, posForInsert);

        m_jobs.push_back(rcJob);

        bool isPending = false;
        if (rcJob->GetState() == RCJob::pending)
        {
            m_jobsInQueueLookup.insert(rcJob->GetElementID(), rcJob);
            isPending = true;
        }
        endInsertRows();

        if (isPending)
        {
            EraseFailedJobs(rcJob->GetElementID());
        }
    }

    void RCJobListModel::EraseFailedJobs(const QueueElementID& target)
    {
        // remove failed jobs:
        auto foundInQueue = m_jobsFailedLookup.find(target);
        if (foundInQueue != m_jobsFailedLookup.end())
        {
            QList<RCJob*> jobsToRemove;
            while ((foundInQueue != m_jobsFailedLookup.end()) && foundInQueue.key() == target)
            {
                RCJob* job = foundInQueue.value();
                jobsToRemove.push_back(job);
                foundInQueue = m_jobsFailedLookup.erase(foundInQueue);
            }

            // there is at least one failed job in the queue for this same task, remove them!
            for (int jobIndex = m_jobs.size() - 1; jobIndex >= 0; --jobIndex)
            {
                RCJob* jobAtIndex = m_jobs.at(jobIndex);
                if (jobsToRemove.contains(jobAtIndex))
                {
                    beginRemoveRows(QModelIndex(), jobIndex, jobIndex);
                    jobAtIndex->deleteLater();
                    m_jobs.removeAt(jobIndex);
                    endRemoveRows();
                }
            }
        }
    }

    void RCJobListModel::markAsProcessing(RCJob* rcJob)
    {
        rcJob->SetState(RCJob::processing);
        rcJob->setTimeLaunched(QDateTime::currentDateTime());

        m_jobsInFlight.insert(rcJob);

        int jobIndex = m_jobs.lastIndexOf(rcJob);
        Q_EMIT dataChanged(index(jobIndex, 0, QModelIndex()), index(jobIndex, 0, QModelIndex()));
    }

    void RCJobListModel::markAsStarted(RCJob* rcJob)
    {
        auto foundInQueue = m_jobsInQueueLookup.find(rcJob->GetElementID());
        while ((foundInQueue != m_jobsInQueueLookup.end()) && (foundInQueue.value() == rcJob))
        {
            foundInQueue = m_jobsInQueueLookup.erase(foundInQueue);
        }
    }

    void RCJobListModel::markAsCompleted(RCJob* rcJob)
    {
        rcJob->setTimeCompleted(QDateTime::currentDateTime());

        auto foundInQueue = m_jobsInQueueLookup.find(rcJob->GetElementID());
        while ((foundInQueue != m_jobsInQueueLookup.end()) && (foundInQueue.value() == rcJob))
        {
            foundInQueue = m_jobsInQueueLookup.erase(foundInQueue);
        }

        int jobIndex = m_jobs.lastIndexOf(rcJob);
        m_jobsInFlight.remove(rcJob);

        // If the job completed, remove it from the list and delete it
        if (rcJob->GetState() == RCJob::completed)
        {
            beginRemoveRows(QModelIndex(), jobIndex, jobIndex);
            m_jobs.removeAt(jobIndex);
            endRemoveRows();

            EraseFailedJobs(rcJob->GetElementID());
            rcJob->deleteLater();
        }
        else
        {
            m_jobsFailedLookup.insert(rcJob->GetElementID(), rcJob);
            Q_EMIT dataChanged(index(jobIndex, 0, QModelIndex()), index(jobIndex, 0, QModelIndex()));
        }
    }

    bool RCJobListModel::isInFlight(const AssetProcessor::QueueElementID& check) const
    {
        for (auto rcJob : m_jobsInFlight)
        {
            if (check == rcJob->GetElementID())
            {
                return true;
            }
        }
        return false;
    }

    bool RCJobListModel::isInQueue(const AssetProcessor::QueueElementID& check) const
    {
        return m_jobsInQueueLookup.contains(check);
    }

    void RCJobListModel::PerformHeuristicSearch(const QString& searchTerm, const QString& platform, QSet<QueueElementID>& found)
    {
        for (const auto& rcJob : m_jobs)
        {
            if (rcJob->GetPlatform() != platform)
            {
                continue;
            }
            const QString& input = rcJob->GetInputFileRelativePath();
            if (input.endsWith(searchTerm, Qt::CaseInsensitive))
            {
                AZ_TracePrintf(AssetProcessor::DebugChannel, "Job Queue: Heuristic search found exact match (%s,%s,%s).\n", rcJob->GetInputFileAbsolutePath().toUtf8().data(), rcJob->GetPlatform().toUtf8().data(), rcJob->GetJobKey().toUtf8().data());
                found.insert(QueueElementID(input, platform, rcJob->GetJobKey()));
            }
        }

        if (!found.isEmpty())
        {
            return;
        }

        // broaden the heuristic.  Try without extensions - that is ignore everything after the dot.
        // if there are dashes, ignore them also
        int dotIndex = searchTerm.lastIndexOf('.');
        QStringRef searchTermWithNoExtension = searchTerm.midRef(0, dotIndex);

        if (dotIndex != -1)
        {
            for (const auto& rcJob : m_jobs)
            {
                if (rcJob->GetPlatform() != platform)
                {
                    continue;
                }
                const QString& input = rcJob->GetInputFileRelativePath();
                dotIndex = input.lastIndexOf('.');
                if (dotIndex != -1)
                {
                    QStringRef testref = input.midRef(0, dotIndex);
                    if (testref.endsWith(searchTermWithNoExtension, Qt::CaseInsensitive))
                    {
                        AZ_TracePrintf(AssetProcessor::DebugChannel, "Job Queue: Heuristic search found broad match (%s,%s,%s).\n", rcJob->GetInputFileAbsolutePath().toUtf8().data(), rcJob->GetPlatform().toUtf8().data(), rcJob->GetJobKey().toUtf8().data());
                        found.insert(QueueElementID(input, platform, rcJob->GetJobKey()));
                    }
                }
            }
        }

        if (!found.isEmpty())
        {
            return;
        }

        // broaden the heuristic further.  Eliminate anything after the last underscore int he file name
        // (so blahblah_diff.dds just becomes blahblah) and then allow anything which has that file somewhere in it.)

        int slashIndex = searchTerm.lastIndexOf('/');
        int dashIndex = searchTerm.lastIndexOf('_');
        if ((dashIndex != -1) && (slashIndex == -1) || (dashIndex > slashIndex))
        {
            searchTermWithNoExtension = searchTermWithNoExtension.mid(0, dashIndex);
        }
        for (const auto& rcJob : m_jobs)
        {
            if (rcJob->GetPlatform() != platform)
            {
                continue;
            }
            const QString& input = rcJob->GetInputFileRelativePath();
            if (input.contains(searchTermWithNoExtension, Qt::CaseInsensitive)) //notice here that we use simply CONTAINS instead of endswith - this can potentially be very broad!
            {
                AZ_TracePrintf(AssetProcessor::DebugChannel, "Job Queue: Heuristic search found ultra-broad match (%s,%s,%s).\n", rcJob->GetInputFileAbsolutePath().toUtf8().data(), rcJob->GetPlatform().toUtf8().data(), rcJob->GetJobKey().toUtf8().data());
                found.insert(QueueElementID(input, platform, rcJob->GetJobKey()));
            }
        }
    }
}// namespace AssetProcessor

#include <native/resourcecompiler/rcjoblistmodel.moc>
