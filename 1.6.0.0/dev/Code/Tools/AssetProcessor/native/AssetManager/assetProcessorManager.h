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

#pragma once

#include <QString>
#include <QByteArray>
#include <QQueue>
#include <QVector>
#include <QHash>
#include <QDir>
#include <QSet>
#include <QMap>
#include <QPair>
#include <QMutex>

#include <AzCore/std/smart_ptr/shared_ptr.h>

#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <AssetBuilderSDK/AssetBuilderBusses.h>

#include "native/assetprocessor.h"
#include "native/AssetManager/AssetData.h"
#include "native/utilities/assetUtilEBusHelper.h"
#include "native/utilities/ThreadHelper.h"
#include "native/AssetManager/AssetCatalog.h"
#include "native/AssetDatabase/AssetDatabase.h"
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>

class FileWatcher;

namespace AzFramework
{
    namespace AssetSystem
    {
        class BaseAssetProcessorMessage;
    } // namespace AssetSystem
} // namespace AzFramework

namespace AzToolsFramework
{
    namespace AssetSystem
    {
        class AssetJobLogRequest;
        class AssetJobLogResponse;
    } // namespace AssetSystem
} // namespace AzToolsFramework

namespace AssetProcessor
{
    class AssetProcessingStateData;
    struct AssetRecognizer;
    class PlatformConfiguration;
    class ScanFolderInfo;

    //! The Asset Processor Manager is the heart of the pipeline
    //! It is what makes the critical decisions about what should and should not be processed
    //! It emits signals when jobs need to be performed and when assets are complete or have failed.
    class AssetProcessorManager
        : public QObject
        , public AssetProcessor::ProcessingJobInfoBus::Handler
    {
        using BaseAssetProcessorMessage = AzFramework::AssetSystem::BaseAssetProcessorMessage;
        using JobInfo = AzToolsFramework::AssetSystem::JobInfo;
        using JobStatus = AzToolsFramework::AssetSystem::JobStatus;
        using AssetJobLogRequest = AzToolsFramework::AssetSystem::AssetJobLogRequest;
        using AssetJobLogResponse = AzToolsFramework::AssetSystem::AssetJobLogResponse;
        Q_OBJECT

    private:
        struct FileEntry
        {
            QString m_fileName;
            bool m_isDelete = false;

            FileEntry(const QString& fileName=QString(), bool isDelete = false)
                : m_fileName(fileName)
                , m_isDelete(isDelete)
            {
            }

            FileEntry(const FileEntry& rhs)
                : m_fileName(rhs.m_fileName)
                , m_isDelete(rhs.m_isDelete)
            {
            }
        };

        struct AssetProcessedEntry
        {
            JobEntry m_entry;
            AssetBuilderSDK::ProcessJobResponse m_response;

            AssetProcessedEntry() = default;
            AssetProcessedEntry(JobEntry& entry, AssetBuilderSDK::ProcessJobResponse& response)
                : m_entry(AZStd::move(entry))
                , m_response(AZStd::move(response))

            {
            }

            AssetProcessedEntry(const AssetProcessedEntry& other) = default;
            AssetProcessedEntry(AssetProcessedEntry&& other)
                : m_entry(AZStd::move(other.m_entry))
                , m_response(AZStd::move(other.m_response))
            {
                
            }
        };

    public:
        explicit AssetProcessorManager(AssetProcessor::PlatformConfiguration* config, QObject *parent = 0);
        virtual ~AssetProcessorManager();
        bool IsIdle();
        bool HasProcessedCriticalAssets() const;

        //////////////////////////////////////////////////////////////////////////
        // ProcessingJobInfoBus::Handler overrides
        void BeginIgnoringCacheFileDelete(const AZStd::string productPath) override;
        void StopIgnoringCacheFileDelete(const AZStd::string productPath, bool queueAgainForProcessing) override;
        //////////////////////////////////////////////////////////////////////////
        AZStd::shared_ptr<DatabaseConnection> GetDatabaseConnection() const { return m_stateData; }

    Q_SIGNALS:
        void AssetToProcess(JobDetails jobDetails);

        //! Emit whenever a new asset is found or an existing asset is updated
        void ProductChanged(QString relativeAssetName, QString platform);
        //! Emit whenever we delete a file because a product is removed.
        void ProductRemoved(QString relativeAssetName, QString platform);

        // InputAssetProcessed - uses absolute asset path of input file.
        void InputAssetProcessed(QString fullAssetPath, QString platform);

        // InputAssetFailed - uses absolute asset path of input file.
        void InputAssetFailed(QString fullAssetPath, QString platform);

        void RequestInputAssetStatus(QString inputAssetPath, QString platform, QString jobDescription);
        void RequestPriorityAssetCompile(QString inputAssetPath, QString platform, QString jobDescription );

        //! OnBecameIdle is emitted when its waiting for outside stimulus
        //! This will be emitted whenever its eaten through all of its queues and is only waiting for
        //! responses back from other systems (like its waiting for responses back from the compiler)
        void OnBecameIdle();

        void ReadyToQuit(QObject* source);

        void CreateAssetsRequest( unsigned int nonce, QString name, QString platform, bool onlyExactMatch = true, bool syncRequest = false);

        void SendAssetExistsResponse(NetworkRequestID groupID, bool exists);

        void FenceFileDetected(unsigned int fenceId);

#if defined(UNIT_TEST)
        void SendUnitTestInfo(QString info, bool result);
#endif

    public Q_SLOTS:

        //! ComputeNumberOfPendingWorkUnits - meant to be called blocking - counts up the total # of things to be done
        //! this includes the elements in the active file queue as well as the files to examine queue, etc.
        void ComputeNumberOfPendingWorkUnits(int& result);
        void AssetProcessed(JobEntry jobEntry, AssetBuilderSDK::ProcessJobResponse response);
        void AssetFailed(JobEntry jobEntry);
        void AssetProcessed_Impl();
        void AssessModifiedFile(QString filePath);
        void AssessAddedFile(QString filePath);
        void AssessDeletedFile(QString filePath);
        void OnAssetScannerStatusChange(AssetProcessor::AssetScanningStatus status);
        void OnJobStatusChanged(JobEntry jobEntry, JobStatus status);

        void QuitRequested();

        // given some absolute path, please respond with its 'asset ID'.  For now, this will be a
        // string like 'textures/blah.tif' (we don't care about extensions), but eventually, this will
        // be an actual asset UUID.
        void ProcessGetAssetID(NetworkRequestID requestId, BaseAssetProcessorMessage* message, bool fencingFailed = false);

        //! A network request came in asking, for a given input asset, what the status is of any jobs related to that request
        void ProcessGetAssetJobRequest(NetworkRequestID requestId, BaseAssetProcessorMessage* message, bool fencingFailed = false);

        //! A network request came in, Given a JOB ID (from the above Job Request), asking for the actual log for that job.
        void ProcessGetAssetJobLogRequest(NetworkRequestID requestId, BaseAssetProcessorMessage* message, bool fencingFailed = false);

        // This function helps in determining the full asset path of an assetid.
        //In the future we will be sending an asset UUID to this function to request for full path.
        void ProcessGetFullAssetPath(NetworkRequestID requestId, BaseAssetProcessorMessage* message, QString platform, bool fencingFailed = false);
        
        //! This request comes in and is expected to do whatever heuristic is required in order to determine if an asset actually exists in the database.
        void OnRequestAssetExists(NetworkRequestID requestId, QString platform, QString searchTerm);

        void RequestReady(NetworkRequestID requestId, BaseAssetProcessorMessage* message, QString platform, bool fencingFailed = false);

    private Q_SLOTS:
        void ProcessFilesToExamineQueue();
        void CheckForIdle();
        void CheckMissingFiles();
        void ProcessGetAssetJobLogRequest(const AssetJobLogRequest& request, AssetJobLogResponse& response);
        void ScheduleNextUpdate();

    private:
        template <class R>
        bool Recv(unsigned int connId, QByteArray payload, R& request);

        void AssessFileInternal(QString filePath, bool isDelete);
        void CheckAsset(const FileEntry& source);
        void CheckMissingJobs(const AZStd::vector<JobDetails>& jobsThisTime);
        void CheckDeletedFileInCache(QString normalizedPath);
        void CheckDeletedInputAsset(QString relativePathToFile, QString normalizedPath);
        void CheckModifiedInputAsset(QString relativePathToFile, QString normalizedPath);
        bool AnalyzeJob(JobDetails& details, const ScanFolderInfo* scanFolder, bool& sentSourceFileChangedMessage);
        void CheckDeletedCacheFolder(QString normalizedPath);
        void CheckDeletedInputFolder(QString normalizedPath, QString relativePath, QString scanFolderPath);
        void CheckCreatedInputFolder(QString normalizedPath);
        void CheckMetaDataRealFiles(QString relativePath);
        bool DeleteProducts(QString relativePathToFile, QString platformName, QString jobDescription, QStringList& products);
        void DispatchFileChange();
        bool InitializeCacheRoot();
        void CleanSourceFolder(QString sourceFile, QStringList productList);
        void PopulateSourceFileDependency(JobDetails& jobDetails);
        
        // given a file name and a root to not go beyond, add the parent folder and its parent folders recursively
        // to the list of known folders.
        void AddKnownFoldersRecursivelyForFile(const QString& file, const QString& root);
        void CleanEmptyFoldersForFile(const QString& file, const QString& root);
        
        QString GuessProductNameInDatabase(QString path, QString platform);

        void ProcessBuilders(QString normalizedPath, QString relativePathToFile, const ScanFolderInfo* scanFolder, const AssetProcessor::BuilderInfoList& builderInfoList);
#ifdef UNIT_TEST
    public:
        void AddEntriesInDatabaseForUnitTest(QString sourceName, QString platform, QString jobDescription, QStringList productList);
#endif

    private:
        bool m_queuedExamination = false;
        unsigned int m_nextQueueID = 1;
        bool m_hasProcessedCriticalAssets = false;

        QQueue<FileEntry> m_activeFiles;
        QSet<QString> m_alreadyActiveFiles; // a simple optimization to only do the exhaustive search if we know its there.
        AZStd::vector<AssetProcessedEntry> m_assetProcessedList;
        AZStd::shared_ptr<DatabaseConnection> m_stateData;
        ThreadController<AssetCatalog>* m_assetCatalog;
        typedef QHash<QString, FileEntry> FileExamineContainer;
        FileExamineContainer m_filesToExamine; // order does not actually matter in this (yet)
        AssetProcessor::PlatformConfiguration* m_platformConfig = nullptr;
        QSet<QString> m_SourceFilesInDatabase;

        QSet<QString> m_knownFolders; // a cache of all known folder names, normalized to have forward slashes.
     
        typedef AZStd::unordered_multimap<AZStd::string, JobInfo> NameToJobInfoMap; // for when network requests come in about the name
        typedef AZStd::unordered_map<AZ::s64, JobInfo> JobIdToJobInfoMap;  // for when network requests come in about the jobid

        NameToJobInfoMap m_nameToJobInfoMap; // a map of all the jobs submitted to the rcController
        JobIdToJobInfoMap m_jobIdToJobInfoMap;

        QString m_normalizedCacheRootPath;
        QStringList m_commandLinePlatformsList;
        QDir m_cacheRootDir;
        bool m_isCurrentlyScanning = false;
        bool m_quitRequested = false;
        bool m_processedQueued = false;
        bool m_AssetProcessorIsBusy = false;
        int m_platformFlags = 0;
        AZ::s64 m_highestJobId = 0;
        QMutex m_processingJobMutex;
        AZStd::unordered_set<AZStd::string> m_processingProductInfoList;
    };
} // namespace AssetProcessor

