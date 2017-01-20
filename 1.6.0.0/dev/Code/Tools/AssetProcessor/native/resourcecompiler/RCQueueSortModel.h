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
#ifndef ASSETPROCESSOR_RCQUEUESORTMODEL_H
#define ASSETPROCESSOR_RCQUEUESORTMODEL_H

#include <QSortFilterProxyModel>
#include <QSet>
#include <QString>


#include "native/utilities/AssetUtilEBusHelper.h"

namespace AssetProcessor
{
    class QueueElementID;
    class RCJobListModel;
    class RCJob;

    //! This sort and filtering proxy model attaches to the raw RC job list
    //! And presents it in the optimal order for processing rather than display.
    //! The current desired order is
    //!  * Critical (currently Copy) jobs for currently connected platforms
    //!  * Jobs in Sync Compile Requests for currently connected platforms (with most recent requests first)
    //!  * Jobs in Async Compile Lists for currently connected platforms
    //!  * Remaining jobs in currently connected platforms, in priority order
    //!  (The same, repeated, for unconnected platforms).
    class RCQueueSortModel
        : public QSortFilterProxyModel
        , protected AssetProcessorPlatformBus::Handler
    {
        Q_OBJECT
    public:
        explicit RCQueueSortModel(QObject* parent = 0);

        void AttachToModel(RCJobListModel* target);
        RCJob* GetNextPendingJob();

        void AddCompileRequest(const AssetProcessor::QueueElementID& target, bool sync);
        void RemoveCompileRequest(const AssetProcessor::QueueElementID& target, bool sync);

        // implement QSortFilteRProxyModel:
        bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;
        bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

    protected:

        QList<AssetProcessor::QueueElementID> m_activeSyncCompileRequests; // contains duplicates, on purpose so that it can be ordered from oldest to newest.
        QList<AssetProcessor::QueueElementID> m_activeAsyncCompileRequests;
        QSet<QString> m_currentlyConnectedPlatforms;
        bool m_dirtyNeedsResort = false; // instead of constantly resorting, we resort only when someone wants to pull an element from us

        // ---------------------------------------------------------
        // AssetProcessorPlatformBus::Handler
        void AssetProcessorPlatformConnected(const AZStd::string platform) override;
        void AssetProcessorPlatformDisconnected(const AZStd::string platform) override;
        // -----------

        RCJobListModel* m_sourceModel;
    private Q_SLOTS:

        void ProcessPlatformChangeMessage(QString platformName, bool connected);
    };
} // namespace AssetProcessor

#endif //ASSETPROCESSOR_RCQUEUESORTMODEL_H