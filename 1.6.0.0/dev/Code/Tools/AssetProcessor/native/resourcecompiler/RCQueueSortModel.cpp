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
#include <native/resourcecompiler/RCQueueSortModel.h>
#include "RCCommon.h"
#include "rcjoblistmodel.h"
#include "rcjob.h"
#include "native/assetprocessor.h"

namespace AssetProcessor
{
    RCQueueSortModel::RCQueueSortModel(QObject* parent)
        : QSortFilterProxyModel(parent)
    {
    }

    void RCQueueSortModel::AttachToModel(RCJobListModel* target)
    {
        if (target)
        {
            setDynamicSortFilter(true);
            BusConnect();
            m_sourceModel = target;

            setSourceModel(target);
            setSortRole(RCJobListModel::jobIDRole);
            sort(0);
        }
        else
        {
            BusDisconnect();
            setSourceModel(nullptr);
            m_sourceModel = nullptr;
        }
    }

    RCJob* RCQueueSortModel::GetNextPendingJob()
    {
        if (m_dirtyNeedsResort)
        {
            setDynamicSortFilter(false);
            QSortFilterProxyModel::sort(0);
            setDynamicSortFilter(true);
            m_dirtyNeedsResort = false;
        }
        for (int idx = 0; idx < rowCount(); ++idx)
        {
            QModelIndex parentIndex = mapToSource(index(idx, 0));
            RCJob* actualJob = m_sourceModel->getItem(parentIndex.row());
            if ((actualJob) && (actualJob->GetState() == RCJob::pending))
            {
                return actualJob;
            }
        }
        // there are no jobs to do.
        return nullptr;
    }

    bool RCQueueSortModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
    {
        (void)source_parent;
        RCJob* actualJob = m_sourceModel->getItem(source_row);
        if (!actualJob)
        {
            return false;
        }

        if (actualJob->GetState() != RCJob::pending)
        {
            return false;
        }

        return true;
    }

    bool RCQueueSortModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
    {
        RCJob* leftJob = m_sourceModel->getItem(left.row());
        RCJob* rightJob = m_sourceModel->getItem(right.row());

        // first thing to check is in platform.
        bool leftActive = m_currentlyConnectedPlatforms.contains(leftJob->GetPlatform());
        bool rightActive = m_currentlyConnectedPlatforms.contains(rightJob->GetPlatform());

        if (leftActive)
        {
            if (!rightActive)
            {
                return true; // left before right
            }
        }
        else if (rightActive)
        {
            return false; // right before left.
        }

        // critical jobs take priority
        if (leftJob->IsCritical())
        {
            if (!rightJob->IsCritical())
            {
                return true; // left wins.
            }
        }
        else if (rightJob->IsCritical())
        {
            return false; // right wins
        }

        // sync compile jobs take priority:
        const QueueElementID& leftID = leftJob->GetElementID();
        const QueueElementID& rightID = rightJob->GetElementID();

        int lastSyncLeft = m_activeSyncCompileRequests.lastIndexOf(leftID);
        int lastSyncRight = m_activeSyncCompileRequests.lastIndexOf(rightID);

        // this may seem backwards, but its important to understand that higher numbers for sync groups mean they are MORE RECENT requests
        // this function, even though its called lessThan(), really is asking, does LEFT come before RIGHT
        // and in the case of sync compile requests, the higher the number, the more recent the request, and thus the sooner we want to do it
        // Which means if left has a higher sync number than right, its LESS THAN right.
        if (lastSyncLeft != lastSyncRight)
        {
            return lastSyncLeft > lastSyncRight;
        }

        lastSyncLeft = m_activeAsyncCompileRequests.lastIndexOf(leftID);
        lastSyncRight = m_activeAsyncCompileRequests.lastIndexOf(rightID);

        if (lastSyncLeft != lastSyncRight)
        {
            return lastSyncLeft > lastSyncRight;
        }

        // arbitrarily, lets have PC get done first since pc-format assets are what the editor uses.
        if (leftJob->GetPlatform() != rightJob->GetPlatform())
        {
            if (leftJob->GetPlatform() == "pc")
            {
                return true; // left wins.
            }
            if (rightJob->GetPlatform() == "pc")
            {
                return false; // right wins
            }
        }

        int priorityLeft = leftJob->GetPriority();
        int priorityRight = rightJob->GetPriority();
        if ((priorityLeft >= 0) && (priorityRight >= 0))
        {
            // similar to the sync issue above, this lessThan() function needs to tell us whether left comes before right in the queue
            // and if left has a HIGHER priority than right, it does indeed come before right in the queue.
            if (priorityLeft != priorityRight)
            {
                return priorityLeft > priorityRight;
            }
        }

        // if we get all the way down here it means we're dealing with two assets which are not
        // in any compile groups, not a priority platform, not a priority type, priority platform, etc.
        // we can arrange these any way we want, but must pick at least a stable order.
        return leftJob->jobID() < rightJob->jobID();
    }

    void RCQueueSortModel::AddCompileRequest(const AssetProcessor::QueueElementID& target, bool sync)
    {
        m_dirtyNeedsResort = true;
        if (sync)
        {
            m_activeSyncCompileRequests.push_back(target);
        }
        else
        {
            m_activeAsyncCompileRequests.push_back(target);
        }
    }

    void RCQueueSortModel::RemoveCompileRequest(const AssetProcessor::QueueElementID& target, bool sync)
    {
        // removing a compile request is currently done pretty much only when a compile request completes, so does not alter the order in which compilation occurs.
        if (sync)
        {
            int lastIndex = m_activeSyncCompileRequests.lastIndexOf(target);
            if (lastIndex >= 0)
            {
                m_activeSyncCompileRequests.removeAt(lastIndex);
            }
        }
        else
        {
            int lastIndex = m_activeAsyncCompileRequests.lastIndexOf(target);
            if (lastIndex >= 0)
            {
                m_activeAsyncCompileRequests.removeAt(lastIndex);
            }
        }
    }

    void RCQueueSortModel::AssetProcessorPlatformConnected(const AZStd::string platform)
    {
        QMetaObject::invokeMethod(this, "ProcessPlatformChangeMessage", Qt::QueuedConnection, Q_ARG(QString, QString::fromUtf8(platform.c_str())), Q_ARG(bool, true));
    }

    void RCQueueSortModel::AssetProcessorPlatformDisconnected(const AZStd::string platform)
    {
        QMetaObject::invokeMethod(this, "ProcessPlatformChangeMessage", Qt::QueuedConnection, Q_ARG(QString, QString::fromUtf8(platform.c_str())), Q_ARG(bool, false));
    }

    void RCQueueSortModel::ProcessPlatformChangeMessage(QString platformName, bool connected)
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "RCQueueSortModel: Platform %s has %s.", platformName.toUtf8().data(), connected ? "connected" : "disconnected");
        m_dirtyNeedsResort = true;
        if (connected)
        {
            m_currentlyConnectedPlatforms.insert(platformName);
        }
        else
        {
            m_currentlyConnectedPlatforms.remove(platformName);
        }
    }
} // end namespace AssetProcessor

#include <native/resourcecompiler/RCQueueSortModel.moc>