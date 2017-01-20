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
#ifndef RCCONTROLLER_H
#define RCCONTROLLER_H

#include "RCCommon.h"

#include <QObject>
#include <QProcess>
#include <QDir>
#include <QList>
#include "native/AssetManager/AssetData.h"
#include "native/utilities/assetUtilEBusHelper.h"

#include "rcjoblistmodel.h"
#include "RCQueueSortModel.h"

#include <AzFramework/Asset/AssetProcessorMessages.h>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>

class RCcontrollerUnitTests;

namespace AssetProcessor
{
    /**
     * The RCController class controls the receiving of job requests, adding them to the model,
     * running RC and sending of responses
     */
    class RCController
        : public QObject
        , public AssetProcessorPlatformBus::Handler
    {
        friend class ::RCcontrollerUnitTests;
        Q_OBJECT
    public:
        enum CommandType
        {
            cmdUnknown = 0,
            cmdExecute,
            cmdTerminate
        };

        explicit RCController(int minJobs, int maxJobs, QObject* parent = 0);
        virtual ~RCController();

        AssetProcessor::RCJobListModel* getQueueModel();

        void startJob(AssetProcessor::RCJob* rcJob);

        int NumberOfPendingCriticalJobsPerPlatform(QString platform);

        bool IsIdle();

        bool IsPriorityCopyJob(AssetProcessor::RCJob* rcJob);
    signals:
        void fileCompiled(JobEntry entry, AssetBuilderSDK::ProcessJobResponse response);
        void fileFailed(JobEntry entry);
        void assetStatus(JobEntry jobEntry, AzFramework::AssetSystem::AssetStatus status);
        void rcError(QString error);
        void ReadyToQuit(QObject* source); //After receiving QuitRequested, you must send this when its safe
        void JobStarted(QString inputFile, QString platform);
        void JobStatusChanged(JobEntry entry, AzToolsFramework::AssetSystem::JobStatus status);
        void JobsInQueuePerPlatform(QString platform, int jobs);

        void BecameIdle();

        //! This will be signalled upon compile group creation - or failure to do so (in which case status will be unknown)
        void CompileGroupCreated(AssetProcessor::NetworkRequestID groupID, AzFramework::AssetSystem::AssetStatus status);
        //! Once a compile group has errored or finished, this will be invoked.
        void CompileGroupFinished(AssetProcessor::NetworkRequestID groupID, AzFramework::AssetSystem::AssetStatus status);

    public Q_SLOTS:
        void jobSubmitted(JobDetails details);
        void QuitRequested();

        //! This will be called in order to create a compile group and start tracking it.
        void OnRequestCompileGroup(AssetProcessor::NetworkRequestID groupID, QString platform, QString searchTerm);

        void dispatchJobs();

    private:
        void finishJob(AssetProcessor::RCJob* rcJob);


        void CheckCompileAssetsGroup(const AssetProcessor::QueueElementID& queuedElement, AssetProcessor::RCJob::JobState state);

        unsigned int m_maxJobs;

        bool m_dispatchingJobs = false;
        bool m_shuttingDown = false;

        QMap<QString, int> m_jobsCountPerPlatform;// This stores the count of jobs per platform in the RC Queue
        QMap<QString, int> m_pendingCriticalJobsPerPlatform;// This stores the count of pending critical jobs per platform in the RC Queue
        AssetProcessor::RCJobListModel m_RCJobListModel;
        AssetProcessor::RCQueueSortModel m_RCQueueSortModel;

        //! An Asset Compile Group is a set of assets that we're tracking the compilation of
        //! It consists of a whole bunch of assets and is considered to be "complete" when either one of the assets in the group fails
        //! Or all assets in the group have finished.
        class AssetCompileGroup
        {
        public:
            AssetProcessor::NetworkRequestID m_requestID;
            QSet<AssetProcessor::QueueElementID> m_groupMembers;
        };

        QList<AssetCompileGroup> m_activeCompileGroups;
    };
} // namespace AssetProcessor


#endif // RCCONTROLLER_H
