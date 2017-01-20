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
#include <QStringList>
#include <QFile>
#include <QRunnable>
#include <QCoreApplication>
#include <QFileInfo>
#include <QPair>
#include <QMutexLocker>

#include <AzCore/Serialization/Utils.h>
#include <AzCore/Casting/lossy_cast.h>

#include <AzFramework/Asset/AssetProcessorMessages.h>
#include <AzFramework/Asset/AssetRegistry.h>

#include <AssetBuilderSDK/AssetBuilderBusses.h>

#include "native/AssetManager/assetProcessorManager.h"
#include "native/utilities/PlatformConfiguration.h"
#include "native/utilities/AssetUtils.h"
#include "native/utilities/assetUtilEBusHelper.h"
#include "native/AssetManager/AssetProcessingStateData.h"
#include "native/AssetDatabase/AssetDatabase.h"
#include "native/AssetManager/assetScanFolderInfo.h"
#include <AzCore/std/utils.h>
#include "native/utilities/ByteArrayStream.h"
#include "native/resourcecompiler/RCBuilder.h"
#include <AzToolsFramework/Asset/AssetProcessorMessages.h>

#include <AzCore/IO/FileIO.h>


namespace AssetProcessor
{
    const unsigned int FAILED_FINGER_PRINT = 1;

    bool ConvertDatabaseProductPathToAssetId(QString dbPath, QString& assetId)
    {
        QString gameName = AssetUtilities::ComputeGameName();
        bool result = false;
        int gameNameIndex = dbPath.indexOf(gameName, 0, Qt::CaseInsensitive);
        if (gameNameIndex != -1)
        {
            //we will now remove the gameName and the native separator to get the assetId
            dbPath.remove(0, gameNameIndex + gameName.length() + 1);   // adding one for the native separator also
            result = true;
        }
        else
        {
            //we will just remove the platform and the native separator to get the assetId
            int separatorIndex = dbPath.indexOf("/");
            if (separatorIndex != -1)
            {
                dbPath.remove(0, separatorIndex + 1);   // adding one for the native separator
                result = true;
            }
        }

        assetId = dbPath;

        return result;
    }

    AssetProcessorManager::AssetProcessorManager(AssetProcessor::PlatformConfiguration* config, QObject* parent)
        : QObject(parent)
        , m_platformConfig(config)
    {

#if !defined(FORCE_PROXY_MODE)
        // The current way of working on mac is to share a cache folder from a PC
        // this makes it so you cannot write the cache (since the PC has it locked, remotely)
        // in proxy-only mode, there will be a problem if we try to connect multiple authorative writes to the same
        // database file, so in this situation, we don't do anything of the sort.

        m_stateData = AZStd::shared_ptr<DatabaseConnection>(aznew DatabaseConnection());
        AssetProcessingStateData* legacyData = new AssetProcessingStateData(this);

        if ((legacyData->DataExists()) && (!m_stateData->DataExists()))
        {
            // perform migration
            legacyData->LoadData();
            legacyData->MigrateToDatabase(*m_stateData);
        }
        else
        {
            // all other outcomes are an upgrade or initialization of the database
            m_stateData->OpenDatabase();
        }
        delete legacyData;
        
        m_highestJobId = m_stateData->GetHighestJobID();
        if (m_highestJobId < 0)
        {
            m_highestJobId = 0;
        }

        
#endif // FORCE_PROXY_MODE
       // cache this up front.  Note that it can fail here, and will retry later.
        InitializeCacheRoot();

        //Compute the platform flag that will be send to all the builders
        for (int idx = 0; idx < m_platformConfig->PlatformCount(); idx++)
        {
            QString platform = m_platformConfig->PlatformAt(idx);
            m_platformFlags = m_platformFlags | AssetUtilities::ComputePlatformFlag(platform);
        }

        m_commandLinePlatformsList = AssetUtilities::ReadPlatformsFromCommandLine();
        AssetProcessor::ProcessingJobInfoBus::Handler::BusConnect();
    }

    AssetProcessorManager::~AssetProcessorManager()
    {
        AssetProcessor::ProcessingJobInfoBus::Handler::BusDisconnect();
    }

    template <class R>
    inline bool AssetProcessorManager::Recv(unsigned int connId, QByteArray payload, R& request)
    {
        bool readFromStream = AZ::Utils::LoadObjectFromBufferInPlace(payload.data(), payload.size(), request);
        AZ_Assert(readFromStream, "AssetProcessorManager::Recv: Could not deserialize from stream (type=%u)", request.GetMessageType());
        return readFromStream;
    }

    bool AssetProcessorManager::InitializeCacheRoot()
    {
        if (AssetUtilities::ComputeProjectCacheRoot(m_cacheRootDir))
        {
            m_normalizedCacheRootPath = AssetUtilities::NormalizeDirectoryPath(m_cacheRootDir.absolutePath());
            return !m_normalizedCacheRootPath.isEmpty();
        }

        return false;
    }

    void AssetProcessorManager::CleanSourceFolder(QString sourceFile, QStringList productList)
    {
        QFileInfo fileInfo(sourceFile);
        bool isDDSPresent = AssetUtilities::CheckProductsExtension(productList, "dds");
        if (isDDSPresent && QString::compare(fileInfo.completeSuffix(), "dds", Qt::CaseInsensitive) != 0)
        {
            QDir dir(fileInfo.absoluteDir());
            //input file extension is not dds but one of the products is a dds file
            for (int idx = 0; idx < productList.size(); idx++)
            {
                QFileInfo productFile(productList.at(idx));
                QString productFileName = productFile.fileName();
                QString fileToDelete = dir.filePath(productFileName);
                QFile file(fileToDelete);
                if (file.exists())
                {
                    if (!file.remove())
                    {
                        AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Unable to delete file: %s.\n", fileToDelete.toUtf8().data());
                    }
                }
            }
        }
    }

    void AssetProcessorManager::PopulateSourceFileDependency(JobDetails& jobDetails)
    {
        QString fullPathToFile(jobDetails.m_jobEntry.m_absolutePathToFile);
        for (int idx = 0; idx < m_platformConfig->MetaDataFileTypesCount(); idx++)
        {
            QPair<QString, QString> metaDataFileType = m_platformConfig->GetMetaDataFileTypeAt(idx);

            if (!metaDataFileType.second.isEmpty() && !fullPathToFile.endsWith(metaDataFileType.second, Qt::CaseInsensitive))
            {
                continue;
            }

            QString metaDataFileName;
            QDir engineRoot;
            AssetUtilities::ComputeEngineRoot(engineRoot);
            QString gameName = AssetUtilities::ComputeGameName();
            QString fullMetaPath = engineRoot.filePath(gameName + "/" + metaDataFileType.first);

            if (QFile::exists(fullMetaPath))
            {
                metaDataFileName = fullMetaPath;
            }
            else
            {
                if (metaDataFileType.second.isEmpty())
                {
                    // ADD the metadata file extension to the end of the filename
                    metaDataFileName = fullPathToFile + "." + metaDataFileType.first;
                }
                else
                {
                    // REPLACE the file's extension with the metadata file extension.
                    QFileInfo fileInfo = QFileInfo(jobDetails.m_jobEntry.m_absolutePathToFile);
                    metaDataFileName = fileInfo.path() + '/' + fileInfo.completeBaseName() + "." + metaDataFileType.first;
                }
            }
            AssetBuilderSDK::SourceFileDependency sourceFileDependency(metaDataFileName.toUtf8().data());
            jobDetails.m_sourceFileDependencyList.push_back(sourceFileDependency);
        }
    }

    void AssetProcessorManager::OnAssetScannerStatusChange(AssetProcessor::AssetScanningStatus status)
    {
        if (status == AssetProcessor::AssetScanningStatus::Started)
        {
            // Ensure that the source file list is populated before
            // we assess any files before a scan
            m_SourceFilesInDatabase.clear();
            m_stateData->GetSourceFileNames(m_SourceFilesInDatabase);
            // unfortunately, there was a time when legacy data had lower case names, and to be compatible, we must do this
            // comparison in that realm:
            QSet<QString> lowered;
            lowered.reserve(m_SourceFilesInDatabase.size());
            for (QString source : m_SourceFilesInDatabase)
            {
                lowered.insert(source.toLower());
            }
            lowered.swap(m_SourceFilesInDatabase);
            
            m_isCurrentlyScanning = true;
            return;
        }
        else if ((status == AssetProcessor::AssetScanningStatus::Completed) || (status == AssetProcessor::AssetScanningStatus::Stopped))
        {
            m_isCurrentlyScanning = false;
            // we cannot invoke this immediately - the scanner might be done, but we aren't actually ready until we've processed all remaining messages:
            QMetaObject::invokeMethod(this, "CheckMissingFiles", Qt::QueuedConnection);
        }
    }


    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // JOB STATUS REQUEST HANDLING
    void AssetProcessorManager::OnJobStatusChanged(JobEntry jobEntry, JobStatus status)
    {
        using namespace AzToolsFramework::AssetSystem;
        if (status == JobStatus::Queued)
        {
            // freshly queued files start out queued.
            JobInfo& jobInfo = m_nameToJobInfoMap.insert_key(jobEntry.m_relativePathToFile.toLower().toUtf8().constData()).first->second;
            jobInfo.m_platform = jobEntry.m_platform.toUtf8().constData();
            jobInfo.m_builderGuid = jobEntry.m_builderUUID;
            jobInfo.m_jobId = jobEntry.m_jobId;
            jobInfo.m_sourceFile = jobEntry.m_relativePathToFile.toUtf8().constData();
            jobInfo.m_jobKey = jobEntry.m_jobKey.toUtf8().constData();
            jobInfo.m_status = status;

            JobInfo& otherInfo = m_jobIdToJobInfoMap.insert_key(jobEntry.m_jobId).first->second;
            otherInfo = jobInfo;
        }
        else
        {
            if (status == JobStatus::InProgress)
            {
                m_jobIdToJobInfoMap[jobEntry.m_jobId].m_status = JobStatus::InProgress;
            }
            else
            {
                m_jobIdToJobInfoMap.erase(jobEntry.m_jobId);
            }

            // set the first one you find that matches to in progress.
            auto result = m_nameToJobInfoMap.equal_range(jobEntry.m_relativePathToFile.toLower().toUtf8().data());
            for (auto iter = result.first; iter != result.second; ++iter)
            {
                JobInfo& jobInfo = iter->second;
                if (jobInfo.m_jobId == jobEntry.m_jobId)
                {
                    if (status == JobStatus::InProgress)
                    {
                        jobInfo.m_status = status;
                        return;
                    }
                    else
                    {
                        // if its any other status, then we remove it from our map since we store it in the real persistent db when it goes to "failed" or "completed"
                        m_nameToJobInfoMap.erase(iter);

                        return;
                    }
                }
            }
        }
    }

    //! A network request came in asking, for a given input asset, what the status is of any jobs related to that request
    void AssetProcessorManager::ProcessGetAssetJobRequest(NetworkRequestID requestId, BaseAssetProcessorMessage* message, bool fencingFailed)
    {
        using AzToolsFramework::AssetSystem::AssetJobsInfoRequest;
        using AzToolsFramework::AssetSystem::AssetJobsInfoResponse;

        AssetJobsInfoRequest* request = azrtti_cast<AssetJobsInfoRequest*>(message);

        if (!request)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessGetAssetJobRequest: Message is not of type %d.Incoming message type is %d.\n", AssetJobsInfoRequest::MessageType(), message->GetMessageType());
            return;
        }
        AzToolsFramework::AssetSystem::AssetSystemJobRequest::JobInfoContainer jobList;

        // Check the NameToJobInfoMap for this source asset

        QString normalizedInputAssetPath = AssetUtilities::NormalizeFilePath(request->m_assetPath.c_str());

        if (QFileInfo(normalizedInputAssetPath).isAbsolute())
        {
            QString scanFolderName;
            QString relativePathToFile;
            if (!m_platformConfig->ConvertToRelativePath(normalizedInputAssetPath, relativePathToFile, scanFolderName))
            {
                // its not in any watch folder.
                AZ_Error("AssetProcessorManager", false, "AssetProcessorManager: input path is not in any watch folder");
                AssetJobsInfoResponse response(jobList, false);
                EBUS_EVENT_ID(requestId.first, AssetProcessor::ConnectionBus, Send, requestId.second, response);
            }

            normalizedInputAssetPath = relativePathToFile;
        }

        AZStd::string loweredPath(normalizedInputAssetPath.toLower().toUtf8().constData());

        auto result = m_nameToJobInfoMap.equal_range(loweredPath);
        for (auto iter = result.first; iter != result.second; ++iter)
        {
            JobInfo jobInfo = iter->second;
            jobList.push_back(jobInfo);
        }

        auto jobFn = [&](JobInfo& job) -> bool
            {
                jobList.push_back();
                jobList.back() = std::move(job);
                return true;
            };

        m_stateData->GetJobInfosForSource(normalizedInputAssetPath.toUtf8().constData(), jobFn);

        AssetJobsInfoResponse response(jobList, true);
        EBUS_EVENT_ID(requestId.first, AssetProcessor::ConnectionBus, Send, requestId.second, response);
    }

    //! A network request came in, Given a JOB ID (from the above Job Request), asking for the actual log for that job.
    void AssetProcessorManager::ProcessGetAssetJobLogRequest(NetworkRequestID requestId, BaseAssetProcessorMessage* message, bool fencingFailed)
    {
        AssetJobLogRequest* request = azrtti_cast<AssetJobLogRequest*>(message);

        if (!request)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessGetAssetJobLogRequest: Message is not of type %d.Incoming message type is %d.\n", AssetJobLogRequest::MessageType(), message->GetMessageType());
            return;
        }
        AssetJobLogResponse response;
        ProcessGetAssetJobLogRequest(*request, response);
        EBUS_EVENT_ID(requestId.first, AssetProcessor::ConnectionBus, Send, requestId.second, response);
    }
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void AssetProcessorManager::ProcessGetAssetJobLogRequest(const AssetJobLogRequest& request, AssetJobLogResponse& response)
    {
        // get the job infos by that job id.
        using namespace AzToolsFramework::AssetSystem;
        JobInfo jobResult;

        bool foundJob = false;
        auto foundElement = m_jobIdToJobInfoMap.find(request.m_jobId);
        if (foundElement != m_jobIdToJobInfoMap.end())
        {
            foundJob = true;
            jobResult = foundElement->second;
        }
        else
        {
            auto jobFn = [&](JobInfo& job) -> bool
            {
                foundJob = true;
                jobResult = std::move(job);
                return false; // we only care about the first one.
            };

            m_stateData->GetJobInfosForJobLogPK(request.m_jobId, jobFn);
        }

        if (!foundJob)
        {
            AZ_TracePrintf("AssetProcessorManager", "Error: AssetProcessorManager: Failed to find the job for a request.");
            response = AssetJobLogResponse("Error: AssetProcessorManager: Failed to find the job for a request.", false);
            return;
        }

        if (jobResult.m_status == JobStatus::Failed_InvalidSourceNameExceedsMaxLimit)
        {
            response = AssetJobLogResponse(AZStd::string::format("Warn: Source file name exceeds the maximum length allowed (%d).", AP_MAX_PATH_LEN).c_str(), true);
            return;
        }

        // read the actual log file if available
        AZStd::string logFileFolder = AssetUtilities::ComputeJobLogFolder();
        AZStd::string logFile = logFileFolder + "/" + AssetUtilities::ComputeJobLogFileName(jobResult);

        AZ::IO::HandleType handle = AZ::IO::InvalidHandle;
        AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
        if (!fileIO)
        {
            AZ_TracePrintf("AssetProcessorManager", "Error: AssetProcessorManager: FileIO is unavailable", logFile.c_str());
            AssetJobLogResponse response("FileIO is unavailable", false);
            return;
        }
        if (!fileIO->Open(logFile.c_str(), AZ::IO::OpenMode::ModeRead | AZ::IO::OpenMode::ModeBinary, handle))
        {
            AZ_TracePrintf("AssetProcessorManager", "Error: AssetProcessorManager: Failed to find the log file %s for a request.", logFile.c_str());
            response = AssetJobLogResponse(AZStd::string::format("Error: No log file found for the given log (%s)", logFile.c_str()).c_str(), false);
            return;
        }

        AZ::u64 actualSize = 0;
        fileIO->Size(handle, actualSize);

        if (actualSize == 0)
        {
            AZ_TracePrintf("AssetProcessorManager", "Error: AssetProcessorManager: Log File %s is empty.", logFile.c_str());
            response = AssetJobLogResponse(AZStd::string::format("Error: Log is empty (%s)", logFile.c_str()).c_str(), false);
            fileIO->Close(handle);
            return;
        }

        response.m_jobLog.resize(actualSize + 1);
        fileIO->Read(handle, response.m_jobLog.data(), actualSize);
        fileIO->Close(handle);
        response.m_isSuccess = true;
    }

    void AssetProcessorManager::CheckMissingFiles()
    {
        if (!m_activeFiles.isEmpty())
        {
            // not ready yet, we have not drained the queue.
            QTimer::singleShot(10, this, SLOT(CheckMissingFiles()));
            return;
        }

        if (m_isCurrentlyScanning)
        {
            return;
        }

        for (const QString& file : m_SourceFilesInDatabase)
        {
            //We can use any of the ScanFolder here,it is only used to create a fake full path
            //so that we can feed it to CheckAsset function which requires a full path
            ScanFolderInfo scanFolderInfo = m_platformConfig->GetScanFolderAt(0);
            QDir dir(scanFolderInfo.ScanPath());
            QString fullFileName = dir.filePath(file);
            CheckAsset(FileEntry(fullFileName, true));
        }

        m_SourceFilesInDatabase.clear();

        QMetaObject::invokeMethod(this, "CheckForIdle", Qt::QueuedConnection);
    }


    QString AssetProcessorManager::GuessProductNameInDatabase(QString path, QString platform)
    {
        QString productName;
        QString inputName;
        QString platformName;
        QString jobDescription;
        QString gameName = AssetUtilities::ComputeGameName();

        productName = AssetUtilities::NormalizeAndRemoveAlias(path);
        //first check whether the input path is actually a productName
        if (m_stateData->GetSourceFromProduct(productName, inputName, platformName, jobDescription))
        {
            return productName;
        }
        else
        {
            //if we are here than prepend $PLATFORM/$GAME/ to the path and try again
            productName = platform + "/" + gameName + "/" + productName;
        }

        if (!m_stateData->GetSourceFromProduct(productName, inputName, platformName, jobDescription))
        {
            //if we are here it means that the asset database does not know about this product,
            //we will now remove the gameName and try again ,so now the path will only have $PLATFORM/ in front of it
            int gameNameIndex = productName.indexOf(gameName, 0, Qt::CaseInsensitive);

            if (gameNameIndex != -1)
            {
                //we will now remove the gameName and the native separator
                productName.remove(gameNameIndex, gameName.length() + 1);// adding one for the native separator
            }

            //Search the database for this product
            if (!m_stateData->GetSourceFromProduct(productName, inputName, platformName, jobDescription))
            {
                //return empty string if the database still does not have any idea about the product
                productName = QString();
            }
        }
        return productName.toLower();
    }

#ifdef UNIT_TEST
    void AssetProcessorManager::AddEntriesInDatabaseForUnitTest(QString sourceName, QString platform, QString jobDescription, QStringList productList)
    {
        m_stateData->SetFingerprint(sourceName, platform, jobDescription, 99999999);
        m_stateData->SetProductsForSource(sourceName, platform, jobDescription, productList);
    }

#endif //ifdef UNIT_TEST

    void AssetProcessorManager::QuitRequested()
    {
        m_quitRequested = true;
        m_filesToExamine.clear();
        Q_EMIT ReadyToQuit(this);
    }

    void AssetProcessorManager::ProcessGetAssetID(NetworkRequestID requestId, BaseAssetProcessorMessage* message, bool fencingFailed)
    {
        using AzToolsFramework::AssetSystem::GetAssetIdRequest;
        using AzToolsFramework::AssetSystem::GetAssetIdResponse;

        GetAssetIdRequest* request = azrtti_cast<GetAssetIdRequest*>(message);

        if (!request)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessGetAssetID: Message is not of type %d.Incoming message type is %d.\n", GetAssetIdRequest::MessageType(), message->GetMessageType());
            return;
        }

        QString inputAssetPath = request->m_assetPath.c_str();
        QString normalizedInputAssetPath = AssetUtilities::NormalizeFilePath(inputAssetPath);

        QString assetId;
        int resultCode = 0;
        QDir inputPath(normalizedInputAssetPath);

        AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessGetAssetID: %s...", inputAssetPath.toUtf8().data());

        if (inputPath.isRelative())
        {
            //if the path coming in is already a relative path,we just send it back
            assetId = inputAssetPath;
            resultCode = 1;
        }
        else
        {
            QDir cacheRoot;
            AssetUtilities::ComputeProjectCacheRoot(cacheRoot);
            QString normalizedCacheRoot = AssetUtilities::NormalizeFilePath(cacheRoot.path());

            bool inCacheFolder = normalizedInputAssetPath.startsWith(normalizedCacheRoot, Qt::CaseInsensitive);
            if (inCacheFolder)
            {
                // The path send by the game/editor contains the cache root so we try to find the asset id
                // from the asset database
                normalizedInputAssetPath.remove(0, normalizedCacheRoot.length() + 1); // adding 1 for the native separator
                // In this case we will try to compute the asset id from the product path
                // Now after removing the cache root,normalizedInputAssetPath can either be $Platform/$Game/xxx/yyy or something like $Platform/zzz
                // and the corresponding assetId have to be either xxx/yyy or zzz

                resultCode = ConvertDatabaseProductPathToAssetId(normalizedInputAssetPath, assetId);
            }
            else
            {
                // If we are here it means its a source file, first see whether there is any overriding file and than try to find products
                QString scanFolder;
                QString relativeName;
                if (m_platformConfig->ConvertToRelativePath(normalizedInputAssetPath, relativeName, scanFolder))
                {
                    QString overridingFile = m_platformConfig->GetOverridingFile(relativeName, scanFolder);

                    if (overridingFile.isEmpty())
                    {
                        // no overriding file found
                        overridingFile = normalizedInputAssetPath;
                    }
                    else
                    {
                        overridingFile = AssetUtilities::NormalizeFilePath(overridingFile);
                    }

                    if (m_platformConfig->ConvertToRelativePath(overridingFile, relativeName, scanFolder))
                    {
                        // Since all the platforms have the same jobDescription, we are using pc to get jobdescription and products info from the database
                        QStringList jobDescriptions;
                        if (m_stateData->GetJobDescriptionsForSource(relativeName, "pc", jobDescriptions))
                        {
                            QStringList productList;
                            if (m_stateData->GetProductsForSource(relativeName, "pc", jobDescriptions[0], productList))
                            {
                                resultCode = ConvertDatabaseProductPathToAssetId(productList[0], assetId);
                            }
                        }
                        else
                        {
                            assetId = relativeName;
                            resultCode = 1;
                        }
                    }
                }
            }
        }

        if (!resultCode)
        {
            // if we fail to determine the assetId we will send back the original path
            AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessGetAssetID no result, returning original %s...", inputAssetPath.toUtf8().data());
            assetId = inputAssetPath;
        }
#if !defined(UNIT_TEST)
        GetAssetIdResponse response(resultCode, assetId.toUtf8().data());
        EBUS_EVENT_ID(requestId.first, AssetProcessor::ConnectionBus, Send, requestId.second, response);
#else
        Q_EMIT SendUnitTestInfo(assetId, resultCode);
#endif
    }

    void AssetProcessorManager::ProcessGetFullAssetPath(NetworkRequestID requestId, BaseAssetProcessorMessage* message, QString platform, bool fencingFailed)
    {
        using AzFramework::AssetSystem::GetAssetPathRequest;
        using AzFramework::AssetSystem::GetAssetPathResponse;

        GetAssetPathRequest* request = azrtti_cast<GetAssetPathRequest*>(message);

        if (!request)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessGetFullAssetPath: Message is not of type %d. Incoming message type is %d.\n", GetAssetPathRequest::MessageType(), message->GetMessageType());
            return;
        }


        QString assetPath = request->m_assetId.c_str();
        QString normalisedAssetPath = AssetUtilities::NormalizeFilePath(assetPath);
        int resultCode = 0;
        QString fullAssetPath;

        QDir inputPath(normalisedAssetPath);

        if (inputPath.isAbsolute())
        {
            bool inCacheFolder = false;
            QDir cacheRoot;
            AssetUtilities::ComputeProjectCacheRoot(cacheRoot);
            QString normalizedCacheRoot = AssetUtilities::NormalizeFilePath(cacheRoot.path());
            //Check to see whether the path contains the cache root
            inCacheFolder = normalisedAssetPath.startsWith(normalizedCacheRoot, Qt::CaseInsensitive);

            if (!inCacheFolder)
            {
                //if the path coming in is already a fullpath and does not contain the cache root, than we will just send it back
                fullAssetPath = assetPath;
                resultCode = 1;
            }
            else
            {
                // The path send by the game/editor contains the cache root ,try to find the productName from it
                normalisedAssetPath.remove(0, normalizedCacheRoot.length() + 1); // adding 1 for the native separator
            }
        }

        if (!resultCode)
        {
            //remove aliases if present
            normalisedAssetPath = AssetUtilities::NormalizeAndRemoveAlias(normalisedAssetPath);

            //We should have the assetid now, we can now find the full asset path
            QString productName = GuessProductNameInDatabase(normalisedAssetPath, platform);
            if (!productName.isEmpty())
            {
                QString sourceName;
                QString platformName;
                QString inputName;
                QString jobDescription;
                //Now find the input name for the path,if we are here this should always return true since we were able to find the productName before
                if (m_stateData->GetSourceFromProduct(productName, inputName, platformName, jobDescription))
                {
                    //Once we have the found the inputname we will try finding the full path
                    fullAssetPath = m_platformConfig->FindFirstMatchingFile(inputName);
                    if (!fullAssetPath.isEmpty())
                    {
                        resultCode = 1;
                    }
                }
            }
            else
            {
                // if we are not able to guess the product name than maybe the asset path is an input name
                fullAssetPath = m_platformConfig->FindFirstMatchingFile(normalisedAssetPath);
                if (!fullAssetPath.isEmpty())
                {
                    resultCode = 1;
                }
            }
        }

        if (!resultCode)
        {
            // if we fail than send back the original path
            fullAssetPath = assetPath;
        }

#if !defined(UNIT_TEST)
        GetAssetPathResponse response(resultCode, fullAssetPath.toUtf8().data());
        EBUS_EVENT_ID(requestId.first, AssetProcessor::ConnectionBus, Send, requestId.second, response);
#else
        Q_EMIT SendUnitTestInfo(fullAssetPath, resultCode);
#endif
    }

    //! This request comes in and is expected to do whatever heuristic is required in order to determine if an asset actually exists in the database.
    void AssetProcessorManager::OnRequestAssetExists(NetworkRequestID groupID, QString platform, QString searchTerm)
    {
        QString productName = GuessProductNameInDatabase(searchTerm, platform);
        Q_EMIT SendAssetExistsResponse(groupID, !productName.isEmpty());
    }

    void AssetProcessorManager::RequestReady(NetworkRequestID networkRequestId, BaseAssetProcessorMessage* message, QString platform, bool fencingFailed)
    {
        using namespace AzFramework::AssetSystem;
        using namespace AzToolsFramework::AssetSystem;

        if (message->GetMessageType() == GetAssetPathRequest::MessageType())
        {
            ProcessGetFullAssetPath(networkRequestId, message, platform, fencingFailed);
        }
        else if (message->GetMessageType() == GetAssetIdRequest::MessageType())
        {
            ProcessGetAssetID(networkRequestId, message, fencingFailed);
        }
        else if (message->GetMessageType() == AssetJobsInfoRequest::MessageType())
        {
            ProcessGetAssetJobRequest(networkRequestId, message, fencingFailed);
        }
        else if (message->GetMessageType() == AssetJobLogRequest::MessageType())
        {
            ProcessGetAssetJobLogRequest(networkRequestId, message, fencingFailed);
        }

        delete message;
    }

    void AssetProcessorManager::AssetFailed(AssetProcessor::JobEntry jobEntry)
    {
        if (m_quitRequested)
        {
            return;
        }
        // wipe the times so that it will try again next time.
        // note:  Leave the prior successful products where they are, though.

        // We have to include a finger print in the database for this job, otherwise when assets change that affect this failed
        // job, the failed assets won't get rescanned and won't be in the database and therefore won't get reprocessed.
        // And don't use 0, because SetFingerprint specialcases 0 to early out and not update the database
        m_stateData->SetFingerprint(jobEntry.m_relativePathToFile, jobEntry.m_platform, jobEntry.m_jobKey, FAILED_FINGER_PRINT);
        if (jobEntry.m_absolutePathToFile.length() < AP_MAX_PATH_LEN)
        {
            m_stateData->SetJobLogForSource(jobEntry.m_jobId, jobEntry.m_relativePathToFile.toUtf8().constData(), jobEntry.m_platform.toUtf8().constData(), jobEntry.m_builderUUID, jobEntry.m_jobKey.toUtf8().constData(), JobStatus::Failed);
        }
        else
        {
            m_stateData->SetJobLogForSource(jobEntry.m_jobId, jobEntry.m_relativePathToFile.toUtf8().constData(), jobEntry.m_platform.toUtf8().constData(), jobEntry.m_builderUUID, jobEntry.m_jobKey.toUtf8().constData(), JobStatus::Failed_InvalidSourceNameExceedsMaxLimit);
        }
        
        AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Asset Failed processing: %s on %s.\n", jobEntry.m_relativePathToFile.toUtf8().constData(), jobEntry.m_platform.toUtf8().constData());
#if defined(BATCH_MODE) && !defined(UNIT_TEST)
        AZ_Warning(AssetProcessor::ConsoleChannel, false, "Failed: %s failed to process for the platform %s", jobEntry.m_relativePathToFile.toUtf8().constData(), jobEntry.m_platform.toUtf8().constData());
#else
        Q_EMIT InputAssetFailed(jobEntry.m_absolutePathToFile, jobEntry.m_platform);
#endif

        OnJobStatusChanged(jobEntry, JobStatus::Failed);

        // we know that things have changed at this point; ensure that we check for idle after we've finished processing all of our assets
        // and don't rely on the file watcher to check again.
        // If we rely on the file watcher only, it might fire before the ProductChanged signal has been responded to and the
        // Asset Catalog may not realize that things are dirty by that point.
        QMetaObject::invokeMethod(this, "CheckForIdle", Qt::QueuedConnection);
    }

    void AssetProcessorManager::AssetProcessed_Impl()
    {
        m_processedQueued = false;
        if (m_quitRequested || m_assetProcessedList.empty())
        {
            return;
        }
        // don't actually consume processed assets until we've finished draining the active file queue, or else the database fights for io with the copy jobs.
        if (!m_activeFiles.empty())
        {
            m_processedQueued = true;
            QTimer::singleShot(10, this, SLOT(AssetProcessed_Impl()));
            return;
        }

        for (AssetProcessedEntry& entry : m_assetProcessedList)
        {
            // update products / delete no longer relevant products
            // note that the cache stores products WITH the name of the platform in it so you don't have to do anything to those
            // strings to process them.

            QStringList priorProducts;
            // ToDo:  Add BuilderUuid here once we do the JobKey feature.
            m_stateData->GetProductsForSource(entry.m_entry.m_relativePathToFile, entry.m_entry.m_platform, entry.m_entry.m_jobKey, priorProducts);

            AssetUtilities::NormalizeFilePaths(priorProducts);
            QStringList productList;
            for (const AssetBuilderSDK::JobProduct& product : entry.m_response.m_outputProducts)
            {
                productList.append(AssetUtilities::NormalizeFilePath(QString(product.m_productFileName.c_str())));
            }

            // prior products, if present will be in the form "<platform>/SamplesProject/blah/blah...", convert our new products to the same thing
            // by removing the cache root
            for (int newProductIdx = 0; newProductIdx < productList.size(); ++newProductIdx)
            {
                // cut off everything including slash after the asset root:
                if (!productList[newProductIdx].startsWith(m_normalizedCacheRootPath, Qt::CaseInsensitive))
                {
                    AZ_TracePrintf(AssetProcessor::ConsoleChannel, "AssetProcessed(\" << %s << \", \" << %s << \" ... ) cache file \"  %s << \" does not appear to be within the cache!.\n",
                        entry.m_entry.m_relativePathToFile.toUtf8().data(),
                        entry.m_entry.m_platform.toUtf8().data(),
                        productList[newProductIdx].toUtf8().data());
                    continue;
                }
                productList[newProductIdx] = m_cacheRootDir.relativeFilePath(productList[newProductIdx]).toLower();
            }

            QSet<QString> priorProductSet = QSet<QString>::fromList(priorProducts);
            QSet<QString> newProductSet = QSet<QString>::fromList(productList);

            // subtract the new ones from the prior ones, to get the remainder.
            priorProductSet.subtract(newProductSet);
            // whats left is the stuff that was there before, but no longer is present in the new set.
            // we need to delete those files.
            for (const QString& productFileToDelete : priorProductSet)
            {
                QString fullNameToDelete = m_cacheRootDir.absoluteFilePath(productFileToDelete);
                QString relativePath(productFileToDelete);
                relativePath = relativePath.right(relativePath.length() - relativePath.indexOf('/') - 1); // remove PLATFORM and an extra slash
                relativePath = relativePath.right(relativePath.length() - relativePath.indexOf('/') - 1); // remove GAMENAME and an extra slash
                if (!QFile::exists(fullNameToDelete))
                {
                    AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Was expecting to delete %s ... but it already appears to be gone. \n", fullNameToDelete.toUtf8().data());
                    // we still need to tell everyone that its gone:
                    Q_EMIT ProductRemoved(relativePath, entry.m_entry.m_platform); // we notify that we are aware of a missing product either way.
                }
                else if (!QFile::remove(fullNameToDelete))
                {
                    AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Was unable to delete file %s will retry next time...\n", fullNameToDelete.toUtf8().data());
                    continue; // do not update database
                }
                else
                {
                    AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Deleting file %s because the recompiled input file no longer emitted that product.\n", fullNameToDelete.toUtf8().data());
                    Q_EMIT ProductRemoved(relativePath, entry.m_entry.m_platform); // we notify that we are aware of a missing product either way.
                }

                // if the folder is empty we may as well clean that too.
                CleanEmptyFoldersForFile(fullNameToDelete, m_normalizedCacheRootPath);
            }

            // no problems!  Update the database.
            AZ_TracePrintf(AssetProcessor::DebugChannel, "AssetProcessed [ok] (\"%s\", \"%s\", \"%s\" ) \n",
                entry.m_entry.m_relativePathToFile.toUtf8().data(),
                entry.m_entry.m_platform.toUtf8().data(),
                entry.m_entry.m_jobKey.toUtf8().data());
            m_stateData->SetFingerprint(entry.m_entry.m_relativePathToFile, entry.m_entry.m_platform, entry.m_entry.m_jobKey, entry.m_entry.m_computedFingerprint);
            m_stateData->SetProductsForSource(entry.m_entry.m_relativePathToFile, entry.m_entry.m_platform, entry.m_entry.m_jobKey, productList);
            m_stateData->SetJobLogForSource(entry.m_entry.m_jobId, entry.m_entry.m_relativePathToFile.toUtf8().constData(), entry.m_entry.m_platform.toUtf8().constData(), entry.m_entry.m_builderUUID, entry.m_entry.m_jobKey.toUtf8().constData(), JobStatus::Completed);
            //we can now clean dds that exists in the source folder
            CleanSourceFolder(entry.m_entry.m_absolutePathToFile, productList);

            // notify the system about products:
            for (const QString& product : productList)
            {
                // the system only wants to know the relative path though, so remove PLATFORM and GAMENAME
                // it starts out in this format
                // <platform>/<gamename>/<relative_asset_path> and we want instead just <relative_asset_path>
                QString chopped(product);
                chopped = chopped.right(chopped.length() - chopped.indexOf('/') - 1); // also consume the extra slash - remove PLATFORM
                chopped = chopped.right(chopped.length() - chopped.indexOf('/') - 1); // also consume the extra slash - remove GAMENAME
                Q_EMIT ProductChanged(chopped, entry.m_entry.m_platform);

                AddKnownFoldersRecursivelyForFile(m_cacheRootDir.absoluteFilePath(product), m_cacheRootDir.absolutePath());
            }
#if defined(BATCH_MODE) && !defined(UNIT_TEST)
            AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Processed: %s successfully processed for the platform %s.\n", entry.m_inputFile.toUtf8().data(), entry.m_platform.toUtf8().data());
#else
            // notify the system about inputs:
            Q_EMIT InputAssetProcessed(entry.m_entry.m_absolutePathToFile, entry.m_entry.m_platform);
            OnJobStatusChanged(entry.m_entry, JobStatus::Completed);

#endif
        }

        m_assetProcessedList.clear();
        // we know that things have changed at this point; ensure that we check for idle after we've finished processing all of our assets
        // and don't rely on the file watcher to check again.
        // If we rely on the file watcher only, it might fire before the ProductChanged signal has been responded to and the
        // Asset Catalog may not realize that things are dirty by that point.

        QMetaObject::invokeMethod(this, "CheckForIdle", Qt::QueuedConnection);
    }

    void AssetProcessorManager::AssetProcessed(JobEntry jobEntry, AssetBuilderSDK::ProcessJobResponse response)
    {
        if (m_quitRequested)
        {
            return;
        }

        m_assetProcessedList.push_back(AssetProcessedEntry(jobEntry, response));

        if (!m_processedQueued)
        {
            QMetaObject::invokeMethod(this, "AssetProcessed_Impl", Qt::QueuedConnection);
            m_processedQueued = true;
        }
    }

    void AssetProcessorManager::CheckAsset(const FileEntry& source)
    {
        // when this function is triggered, it means that a file appeared because it was modified or added or deleted,
        // and the grace period has elapsed.
        // this is the first point at which we MIGHT be interested in a file.
        // to avoid flooding threads we queue these up for later checking.

        AZ_TracePrintf(AssetProcessor::DebugChannel, "CheckAsset: %s %s\n", source.m_fileName.toUtf8().data(), source.m_isDelete ? "true" : "false");

        QString normalizedFilePath = AssetUtilities::NormalizeFilePath(source.m_fileName);

        if (m_platformConfig->IsFileExcluded(normalizedFilePath))
        {
            return;
        }

        // if metadata file change, pretend the actual file changed
        // the fingerprint will be different anyway since metadata file is folded in
        QString lowerName = normalizedFilePath.toLower();

        for (int idx = 0; idx < m_platformConfig->MetaDataFileTypesCount(); idx++)
        {
            QPair<QString, QString> metaInfo = m_platformConfig->GetMetaDataFileTypeAt(idx);
            QString originalName = normalizedFilePath;

            if (normalizedFilePath.endsWith("." + metaInfo.first, Qt::CaseInsensitive))
            {
                //its a meta file.  What was the original?

                normalizedFilePath = normalizedFilePath.left(normalizedFilePath.length() - (metaInfo.first.length() + 1));
                if (!metaInfo.second.isEmpty())
                {
                    // its not empty - replace the meta file with the original extension
                    normalizedFilePath += ".";
                    normalizedFilePath += metaInfo.second;
                }

                // we need the actual casing of the source file
                // but the metafile might have different casing... Qt will fail to get the -actual- casing of the source file, which we need.  It uses string ops internally.
                // so we have to work around this by using the Dir that the file is in:

                QFileInfo newInfo(normalizedFilePath);
                QStringList searchPattern;
                searchPattern << newInfo.fileName();

                QStringList actualCasing = newInfo.absoluteDir().entryList(searchPattern, QDir::Files);

                if (actualCasing.empty())
                {
                    QString warning = QCoreApplication::translate("Warning", "Warning:  Metadata file (%1) missing source file (%2)\n").arg(originalName).arg(normalizedFilePath);
                    AZ_TracePrintf(AssetProcessor::ConsoleChannel, warning.toUtf8().constData());
                    return;
                }

                // the casing might be different, too, so retrieve the actual case of the actual source file here:
                normalizedFilePath = newInfo.absoluteDir().absoluteFilePath(actualCasing[0]);
                lowerName = normalizedFilePath.toLower();
                break;
            }
        }
        // even if the entry already exists,
        // overwrite the entry here, so if you modify, then delete it, its the latest action thats always on the list.

        m_filesToExamine[lowerName] = FileEntry(normalizedFilePath, source.m_isDelete);
        m_AssetProcessorIsBusy = true;

        if (!m_queuedExamination)
        {
            m_queuedExamination = true;
            QTimer::singleShot(0, this, SLOT(ProcessFilesToExamineQueue()));
        }
    }

    void AssetProcessorManager::CheckDeletedFileInCache(QString normalizedPath)
    {
        // this might be interesting, but only if its a known product!
        // the dictionary in statedata stores only the relative path, not the platform.
        // which means right now we have, for example
        // d:/game/root/Cache/SamplesProject/IOS/SamplesProject/textures/favorite.tga
        //
        // ^^^^^^^^^^^^  engine root
        //
        // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^ cache root
        //
        // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ platform root
        {
            QMutexLocker locker(&m_processingJobMutex);
            auto found = m_processingProductInfoList.find(normalizedPath.toLower().toUtf8().data());
            if (found != m_processingProductInfoList.end())
            {
                // if we get here because we just deleted a product file before we copy/move the new product file
                // than its totally safe to ignore this deletion.
                return;
            }
        }
        if (QFile::exists(normalizedPath))
        {
            // this is actually okay - it may have been temporarily deleted because it was in the process of being compiled.
            return;
        }

        QString cacheRootRemoved = m_cacheRootDir.relativeFilePath(normalizedPath);

        // remainingName is now like "ios/SamplesProject/textures/favorite.tga"
        QString sourceName;
        QString sourcePlatform;
        QString sourceJobDescription;
        if (m_stateData->GetSourceFromProduct(cacheRootRemoved, sourceName, sourcePlatform, sourceJobDescription))
        {
            // sourceName should be like "textures/favorite.tga"
            // sourcePlatform should be like 'ios'

            // clear it and then queue reprocess on its parent:
            m_stateData->SetFingerprint(sourceName, sourcePlatform, sourceJobDescription, FAILED_FINGER_PRINT);

            // pretend that its source changed.  Add it to the things to keep watching so that in case MORE products change
            // we don't start processing untill all have been deleted
            AssessFileInternal(m_platformConfig->FindFirstMatchingFile(sourceName), false);

            // removing PLATFORM and GAMENAME
            // currently <platform>/<gamename>/<relative_asset_path>
            QString relativePath(cacheRootRemoved);
            relativePath = relativePath.right(relativePath.length() - relativePath.indexOf('/') - 1); // also consume the extra slash - remove PLATFORM
            relativePath = relativePath.right(relativePath.length() - relativePath.indexOf('/') - 1); // also consume the extra slash - remove GAMENAME

            Q_EMIT ProductRemoved(relativePath, sourcePlatform);
        }
    }

    bool AssetProcessorManager::DeleteProducts(QString relativePathToFile, QString platformName, QString jobDescription, QStringList& products)
    {
        bool successfullyRemoved = true;
        // delete the products.
        // products always have names like "pc/SamplesProject/textures/blah.tga" and do include platform roots!
        // this means the actual full path is something like
        // [cache root] / [platform] / [product name]

        for (int productIdx = 0; productIdx < products.size(); ++productIdx)
        {
            QString fullFilePath = m_cacheRootDir.absoluteFilePath(products[productIdx]);
            QString relativePath(products[productIdx]);
            relativePath = relativePath.right(relativePath.length() - relativePath.indexOf('/') - 1); // also consume the extra slash - remove PLATFORM
            relativePath = relativePath.right(relativePath.length() - relativePath.indexOf('/') - 1); // also consume the extra slash - remove GAMENAME
            if (QFile::exists(fullFilePath))
            {
                AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Deleting file %s because either its source file %s was removed or the builder did not emitted this job.\n", fullFilePath.toUtf8().data(), relativePathToFile.toUtf8().data());
                successfullyRemoved &= QFile::remove(fullFilePath);

                if (successfullyRemoved)
                {
                    m_stateData->ClearProducts(relativePathToFile, platformName, jobDescription);
                }
            }
            else
            {
                AZ_TracePrintf(AssetProcessor::ConsoleChannel, "An expected product %s was not present.\n", fullFilePath.toUtf8().data());
            }
            Q_EMIT ProductRemoved(relativePath, platformName); // we notify that we are aware of a missing product either way.
            // if the folder is empty we may as well clean that too.
            CleanEmptyFoldersForFile(fullFilePath, m_normalizedCacheRootPath);
        }

        return successfullyRemoved;
    }

    void AssetProcessorManager::CheckDeletedInputAsset(QString normalizedPath, QString relativePathToFile)
    {
        // getting here means an input asset has been deleted
        // and no overrides exist for it.
        // we must delete its products.

        // Check if this file causes any file types to be re-evaluated
        CheckMetaDataRealFiles(relativePathToFile);

        QStringList products; // accumulate all products

        for (int platformIndex = 0; platformIndex < m_platformConfig->PlatformCount(); ++platformIndex)
        {
            bool productDeletionFailed = false;
            QString platformName = m_platformConfig->PlatformAt(platformIndex);
            QStringList jobDescriptions;
            if (m_stateData->GetJobDescriptionsForSource(relativePathToFile, platformName, jobDescriptions))
            {
                QString jobDescription;
                for (int idx = 0; idx < jobDescriptions.size(); idx++)
                {
                    products.clear();
                    jobDescription = jobDescriptions.at(idx);

                    // ToDo:  Add BuilderUuid here once we do the JobKey feature.
                    if (m_stateData->GetProductsForSource(relativePathToFile, platformName, jobDescription, products))
                    {
                        if (!DeleteProducts(relativePathToFile, platformName, jobDescription, products))
                        {
                            // try again in a while.  Achieve this by recycling the item back into the queue as if it
                            // had been deleted again.
                            CheckAsset(FileEntry(normalizedPath, true));
                            AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Delete failed. Will retry!. \n");
                            productDeletionFailed = true;
                        }
                    }
                    // even with no products, still need to clear the fingerprint:
                    if (!productDeletionFailed)
                    {
                        m_stateData->ClearFingerprint(relativePathToFile, platformName, jobDescription);
                    }
                }
            }
        }
    }

    void AssetProcessorManager::AddKnownFoldersRecursivelyForFile(const QString& file, const QString& root)
    {
        QString normalizedRoot = AssetUtilities::NormalizeFilePath(root);

        // also track parent folders up to the specified root.
        QString parentFolderName = QFileInfo(file).absolutePath();
        QString normalizedParentFolder = AssetUtilities::NormalizeFilePath(parentFolderName);

        if (!normalizedParentFolder.startsWith(normalizedRoot, Qt::CaseInsensitive))
        {
            return; // not interested in folders not in the root.
        }

        while (normalizedParentFolder.compare(normalizedRoot, Qt::CaseInsensitive) != 0)
        {
            m_knownFolders.insert(normalizedParentFolder.toLower());
            int pos = normalizedParentFolder.lastIndexOf(QChar('/'));
            if (pos >= 0)
            {
                normalizedParentFolder = normalizedParentFolder.left(pos);
            }
            else
            {
                break; // no more slashes
            }
        }
    }

    void AssetProcessorManager::CheckMissingJobs(const AZStd::vector<JobDetails>& jobsThisTime)
    {
        // Check to see if jobs were emitted last time by this builder, but are no longer being emitted this time - in which case we must eliminate old products.
        // whats going to be in the database is fingerprints for each job last time
        // this function is called once per source file, so in the array of jobsThisTime,
        // the relative path will always be hte same.

        if (jobsThisTime.empty())
        {
            return;
        }

        QString relativePath = jobsThisTime.front().m_jobEntry.m_relativePathToFile;

        // find all fingerprints from last time
        AZStd::vector<JobInfo> jobsFromLastTime;

        auto jobCallback = [&](JobInfo& job) -> bool
            {
                jobsFromLastTime.push_back(AZStd::move(job));
                return true;
            };
        m_stateData->GetJobInfosForSource(relativePath.toUtf8().data(), jobCallback);
        // so now we have jobsFromLastTime and jobsThisTime.  Whats in last time that is no longer being emitted now?

        if (jobsFromLastTime.empty())
        {
            return;
        }

        for (int oldJobIdx = azlossy_cast<int>(jobsFromLastTime.size()) - 1; oldJobIdx >= 0; --oldJobIdx)
        {
            const JobInfo& oldJobInfo = jobsFromLastTime[oldJobIdx];
            // did we find it this time?
            bool foundIt = false;
            for (const JobDetails& newJobInfo : jobsThisTime)
            {
                // the relative path is insensitive because some legacy data didn't have the correct case.  
                if ((newJobInfo.m_jobEntry.m_builderUUID == oldJobInfo.m_builderGuid) &&
                    (QString::compare(newJobInfo.m_jobEntry.m_platform, oldJobInfo.m_platform.c_str()) == 0) &&
                    (QString::compare(newJobInfo.m_jobEntry.m_jobKey, oldJobInfo.m_jobKey.c_str()) == 0) &&
                    (QString::compare(newJobInfo.m_jobEntry.m_relativePathToFile, oldJobInfo.m_sourceFile.c_str(), Qt::CaseInsensitive) == 0)
                    )
                {
                    foundIt = true;
                    break;
                }
            }

            if (foundIt)
            {
                jobsFromLastTime.erase(jobsFromLastTime.begin() + oldJobIdx);
            }
        }

        // at this point, we contain only the jobs that are left over from last time and not found this time.
        for (const JobInfo& oldJobInfo : jobsFromLastTime)
        {
            char tempBuffer[128];
            oldJobInfo.m_builderGuid.ToString(tempBuffer, AZ_ARRAY_SIZE(tempBuffer));

            QStringList products;
            // ToDo:  Add BuilderUuid here once we do the JobKey feature.
            if (m_stateData->GetProductsForSource(relativePath, oldJobInfo.m_platform.c_str(), oldJobInfo.m_jobKey.c_str(), products))
            {
                AZ_TracePrintf(DebugChannel, "Removing products for job (%s, %s, %s, %s) since it is no longer being emitted by its builder.",
                    oldJobInfo.m_sourceFile.c_str(),
                    oldJobInfo.m_platform.c_str(),
                    oldJobInfo.m_jobKey.c_str(),
                    tempBuffer);
                DeleteProducts(relativePath, oldJobInfo.m_platform.c_str(), oldJobInfo.m_jobKey.c_str(), products);
                m_stateData->ClearFingerprint(relativePath, oldJobInfo.m_platform.c_str(), oldJobInfo.m_jobKey.c_str());
            }
        }
    }

    // clean all folders that are empty until you get to the root, or until you get to one that isn't empty.
    void AssetProcessorManager::CleanEmptyFoldersForFile(const QString& file, const QString& root)
    {
        QString normalizedRoot = AssetUtilities::NormalizeFilePath(root);

        // also track parent folders up to the specified root.
        QString parentFolderName = QFileInfo(file).absolutePath();
        QString normalizedParentFolder = AssetUtilities::NormalizeFilePath(parentFolderName);
        QDir parentDir(parentFolderName);

        // keep walking up the tree until we either run out of folders or hit the root.
        while ((normalizedParentFolder.compare(normalizedRoot, Qt::CaseInsensitive) != 0) && (parentDir.exists()))
        {
            if (parentDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).empty())
            {
                if (!parentDir.rmdir(normalizedParentFolder))
                {
                    break; // if we fail to remove for any reason we don't push our luck.
                }
            }
            if (!parentDir.cdUp())
            {
                break;
            }
            normalizedParentFolder = AssetUtilities::NormalizeFilePath(parentDir.absolutePath());
        }
    }

    void AssetProcessorManager::CheckModifiedInputAsset(QString normalizedPath, QString relativePathToFile)
    {
        // a potential input file was modified or added.  We always pass these through our filters and potentially build it.
        // before we know what to do, we need to figure out if it matches some filter we care about.

        // note that if we get here during runtime, we've already eliminated overrides
        // so this is the actual file of importance.

        // check regexes.
        // get list of recognizers which match
        // for each platform in the recognizer:
        //    check the fingerprint and queue if appropriate!
        //    also queue if products missing.

        // Check if this file causes any file types to be re-evaluated
        CheckMetaDataRealFiles(relativePathToFile);

        const ScanFolderInfo* scanFolder = m_platformConfig->GetScanFolderForFile(normalizedPath);
        if (!scanFolder)
        {
            // not in a folder we care about, either...
            AZ_TracePrintf(AssetProcessor::DebugChannel, "CheckModifiedInputAsset: no scan folder found.\n ");
            return;
        }

        // keep track of its parent folders so that if a folder disappears or is renamed, and we get the notification that this has occurred
        // we will know that it *was* a folder before now (otherwise we'd have no idea)
        AddKnownFoldersRecursivelyForFile(normalizedPath, scanFolder->ScanPath());

        AssetProcessor::BuilderInfoList builderInfoList;
        EBUS_EVENT(AssetProcessor::AssetBuilderInfoBus, GetMatchingBuildersInfo, normalizedPath.toUtf8().data(), builderInfoList);

        if (builderInfoList.empty())
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "No builders found for '%s'.\n", normalizedPath.toUtf8().data());
        }
        else
        {
            ProcessBuilders(normalizedPath, relativePathToFile, scanFolder, builderInfoList);
        }
    }

    bool AssetProcessorManager::AnalyzeJob(JobDetails& jobDetails, const ScanFolderInfo* scanFolder, bool& sentSourceFileChangedMessage)
    {
        // This function checks to see whether we need to process an asset or not, it returns true if we need to process it and false otherwise
        // It processes an asset if either there is a fingerprint mismatch between the computed and the last known fingerprint or if products are missing
        bool shouldProcessAsset = false;

        // First thing it checks is the computed fingerprint with its last known fingerprint in the database, if there is a mismatch than we need to process it
        unsigned int previousFingerprint = m_stateData->GetFingerprint(jobDetails.m_jobEntry.m_relativePathToFile, jobDetails.m_jobEntry.m_platform, jobDetails.m_jobEntry.m_jobKey);
        if (previousFingerprint != jobDetails.m_jobEntry.m_computedFingerprint)
        {
            // fingerprint for this plat does not match
            // queue a job to process it by spawning a copy
            AZ_TracePrintf(AssetProcessor::DebugChannel, "AnalyzeJob: Fingerprint Mismatch on Platform: %s.\n", jobDetails.m_jobEntry.m_platform.toUtf8().data());
            shouldProcessAsset = true;
            if (!sentSourceFileChangedMessage)
            {
                QString sourceFile(jobDetails.m_jobEntry.m_relativePathToFile);
                if (!scanFolder->OutputPrefix().isEmpty())
                {
                    sourceFile = sourceFile.right(sourceFile.length() - sourceFile.indexOf('/') - 1); // adding one for separator
                }
                using AzToolsFramework::AssetSystem::SourceFileNotificationMessage;
                SourceFileNotificationMessage message(AZ::OSString(sourceFile.toUtf8().data()), AZ::OSString(scanFolder->ScanPath().toUtf8().data()), SourceFileNotificationMessage::FileChanged);
                EBUS_EVENT(AssetProcessor::ConnectionBus, Send, 0, message);
                sentSourceFileChangedMessage = true;
            }
        }
        else
        {
            // If the fingerprint hasn't changed, we won't process it.. unless...is it missing a product.
            QStringList productList;

            // ToDo:  Add BuilderUuid here once we do the JobKey feature.
            if (m_stateData->GetProductsForSource(jobDetails.m_jobEntry.m_relativePathToFile, jobDetails.m_jobEntry.m_platform, jobDetails.m_jobEntry.m_jobKey, productList))
            {
                for (int productIdx = 0; productIdx < productList.size(); ++productIdx)
                {
                    QString fullFilePath = m_cacheRootDir.absoluteFilePath(productList[productIdx]);
                    if (!QFile::exists(fullFilePath))
                    {
                        AZ_TracePrintf(AssetProcessor::DebugChannel, "CheckModifiedInputAsset: Missing Products: %s on platform : %s\n", jobDetails.m_jobEntry.m_absolutePathToFile.toUtf8().data(), productList[productIdx].toUtf8().data(), jobDetails.m_jobEntry.m_platform.toUtf8().data());
                        shouldProcessAsset = true;
                    }
                    else
                    {
                        QString absoluteCacheRoot = m_cacheRootDir.absolutePath();
                        AddKnownFoldersRecursivelyForFile(fullFilePath, absoluteCacheRoot);
                    }
                }
            }
        }

        if (!shouldProcessAsset)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "CheckModifiedInputAsset: UpToDate: not processing on %s.\n", jobDetails.m_jobEntry.m_platform.toUtf8().data());
            return false;
        }
        else
        {
            // we need to actually run the processor on this asset unless its already in the queue.
            AZ_TracePrintf(AssetProcessor::DebugChannel, "CheckModifiedInputAsset: Needs processing on %s.\n", jobDetails.m_jobEntry.m_platform.toUtf8().data());

            QString pathRel = QString("/") + QFileInfo(jobDetails.m_jobEntry.m_relativePathToFile).path();

            if (pathRel == "/.")
            {
                // if its in the current folder, avoid using ./ or /.
                pathRel = QString();
            }

            if (scanFolder->IsRoot())
            {
                // stuff which is found in the root continues to go to the root, rather than GAMENAME folder...
                jobDetails.m_destinationPath = m_cacheRootDir.absoluteFilePath(jobDetails.m_jobEntry.m_platform + pathRel);
            }
            else
            {
                jobDetails.m_destinationPath = m_cacheRootDir.absoluteFilePath(jobDetails.m_jobEntry.m_platform + "/" + AssetUtilities::ComputeGameName() + pathRel);
            }
        }

        return true;
    }

    void AssetProcessorManager::CheckDeletedCacheFolder(QString normalizedPath)
    {
        QDir checkDir(normalizedPath);
        if (checkDir.exists())
        {
            // this is possible because it could have been moved back by the time we get here, in which case, we take no action.
            return;
        }

        // going to need to iterate on all files there, recursively, in order to emit them as having been deleted.
        // note that we don't scan here.  We use the asset database.
        QString cacheRootRemoved = m_cacheRootDir.relativeFilePath(normalizedPath);

        QSet<QString> outputList;
        m_stateData->GetMatchingProductFiles(cacheRootRemoved, outputList);

        for (const QString& value : outputList)
        {
            QString fileFound = m_cacheRootDir.absoluteFilePath(value);
            if (!QFile::exists(fileFound))
            {
                AssessDeletedFile(fileFound);
            }
        }

        m_knownFolders.remove(normalizedPath.toLower());
    }

    void AssetProcessorManager::CheckDeletedInputFolder(QString normalizedPath, QString relativePath, QString scanFolderPath)
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "CheckDeletedInputFolder...");
        // we deleted a folder that is somewhere that is a watched input folder.

        QDir checkDir(normalizedPath);
        if (checkDir.exists())
        {
            // this is possible because it could have been moved back by the time we get here, in which case, we take no action.
            return;
        }

        QSet<QString> outputList;
        m_stateData->GetMatchingSourceFiles(relativePath, outputList);

        AZ_TracePrintf(AssetProcessor::DebugChannel, "CheckDeletedInputFolder: %i matching files.", outputList.count());

        QDir scanFolder(scanFolderPath);
        for (const QString& value : outputList)
        {
            // reconstruct full path:
            QString finalPath = scanFolder.absoluteFilePath(value);
            if (!QFile::exists(finalPath))
            {
                AssessDeletedFile(finalPath);
            }
        }

        m_knownFolders.remove(normalizedPath.toLower());
    }

    namespace
    {
        void ScanFolderInternal(QString inputFolderPath, QStringList& outputs)
        {
            QDir inputFolder(inputFolderPath);
            QFileInfoList entries = inputFolder.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Files);

            for (const QFileInfo& entry : entries)
            {
                if (entry.isDir())
                {
                    //Entry is a directory
                    ScanFolderInternal(entry.absoluteFilePath(), outputs);
                }
                else
                {
                    //Entry is a file
                    outputs.push_back(entry.absoluteFilePath());
                }
            }
        }
    }

    void AssetProcessorManager::CheckMetaDataRealFiles(QString relativePath)
    {
        if (!m_platformConfig->IsMetaDataTypeRealFile(relativePath))
        {
            return;
        }

        QStringList extensions;
        for (int idx = 0; idx < m_platformConfig->MetaDataFileTypesCount(); idx++)
        {
            const auto& metaExt = m_platformConfig->GetMetaDataFileTypeAt(idx);
            if (!metaExt.second.isEmpty() && QString::compare(metaExt.first, relativePath, Qt::CaseInsensitive) == 0)
            {
                extensions.push_back(metaExt.second);
            }
        }

        QSet<QString> filesToProcess;
        for (const QString& ext : extensions)
        {
            m_stateData->GetSourceWithExtension(ext, filesToProcess);
        }

        QString fullAssetPath;
        for (const QString& file : filesToProcess)
        {
            fullAssetPath = m_platformConfig->FindFirstMatchingFile(file);
            if (!fullAssetPath.isEmpty())
            {
                AssessFileInternal(fullAssetPath, false);
            }
        }
    }

    void AssetProcessorManager::CheckCreatedInputFolder(QString normalizedPath)
    {
        // this could have happened because its a directory rename
        QDir checkDir(normalizedPath);
        if (!checkDir.exists())
        {
            // this is possible because it could have been moved back by the time we get here.
            // find all assets that are products that have this as their normalized path and then indicate that they are all deleted.
            return;
        }

        // we actually need to scan this folder, without invoking the whole asset scanner:

        const AssetProcessor::ScanFolderInfo* info = m_platformConfig->GetScanFolderForFile(normalizedPath);
        if (!info)
        {
            return; // early out, its nothing we care about.
        }

        QStringList files;
        ScanFolderInternal(normalizedPath, files);

        for (const QString fileEntry : files)
        {
            AssessModifiedFile(fileEntry);
        }
    }

    void AssetProcessorManager::ProcessFilesToExamineQueue()
    {
        // it is assumed that files entering this function are already normalized
        // that is, the path is normalized
        // and only has forward slashes.

        if (!m_platformConfig)
        {
            // this cannot be recovered from
            qFatal("Platform config is missing, we cannot continue.");
            return;
        }

        if ((m_normalizedCacheRootPath.isEmpty()) && (!InitializeCacheRoot()))
        {
            AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Cannot examine the queue yet - cache root is not ready!\n ");
            m_queuedExamination = true;
            QTimer::singleShot(250, this, SLOT(ProcessFilesToExamineQueue()));
            return;
        }

        if (m_isCurrentlyScanning)
        {
            // if we're currently scanning, then don't start processing yet, its not worth the IO thrashing.
            m_queuedExamination = true;
            QTimer::singleShot(250, this, SLOT(ProcessFilesToExamineQueue()));
            return;
        }


        FileExamineContainer swapped;
        m_filesToExamine.swap(swapped); // makes it okay to call CheckAsset(...)

        m_queuedExamination = false;
        for (const FileEntry& examineFile : swapped)
        {
            if (m_quitRequested)
            {
                return;
            }
            // examination occurs here.
            // first, is it an input or is it in the cache folder?
            QString normalizedPath = examineFile.m_fileName;

            AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessFilesToExamineQueue: %s delete: %s.\n", normalizedPath.toUtf8().data(), examineFile.m_isDelete ? "true" : "false");

            // debug-only check to make sure our assumption about normalization is correct.
            Q_ASSERT(normalizedPath == AssetUtilities::NormalizeFilePath(normalizedPath));

            // if its in the cache root then its an cache file:
            bool isCacheFile = normalizedPath.startsWith(m_normalizedCacheRootPath, Qt::CaseInsensitive);

            // strip the engine off it so that its a "normalized asset path" with appropriate slashes and such:
            if (isCacheFile)
            {
                if (normalizedPath.length() >= AP_MAX_PATH_LEN)
                {
                    // if we are here it means that we have found a cache file whose filepath is greater than the maximum path length allowed
                    continue;
                }
                // its a cached file.
                // we only care about deleted cached files.
                if (examineFile.m_isDelete)
                {
                    if (normalizedPath.endsWith(QString(FENCE_FILE_EXTENSION), Qt::CaseInsensitive))
                    {
                        QFileInfo fileInfo(normalizedPath);
                        if (fileInfo.isDir())
                        {
                            continue;
                        }
                        // its a fence file, now computing fenceId from it 
                        int startPos = normalizedPath.lastIndexOf("~");
                        int endPos = normalizedPath.lastIndexOf(".");
                        QString fenceIdString = normalizedPath.mid(startPos + 1, endPos - startPos - 1);
                        bool isNumber = false;
                        int fenceId = fenceIdString.toInt(&isNumber);
                        if (isNumber)
                        {
                            Q_EMIT FenceFileDetected(fenceId);
                        }
                        else
                        {
                            AZ_TracePrintf(AssetProcessor::DebugChannel, "AssetProcessor: Unable to compute fenceId from fenceFile name %s.\n", normalizedPath.toUtf8().data());
                        }
                        continue;
                    }
                    if (m_knownFolders.contains(normalizedPath.toLower()))
                    {
                        CheckDeletedCacheFolder(normalizedPath);
                    }
                    else
                    {
                        AZ_TracePrintf(AssetProcessor::DebugChannel, "Cached file deleted\n");
                        CheckDeletedFileInCache(normalizedPath);
                    }
                }
                else
                {
                    // a file was added or modified to the cache.
                    // we only care about the renames of folders, so cache folders here:
                    QFileInfo fileInfo(normalizedPath);
                    if (!fileInfo.isDir())
                    {
                        // keep track of its containing folder.
                        AddKnownFoldersRecursivelyForFile(normalizedPath, m_cacheRootDir.absolutePath());
                    }
                }
            }
            else
            {
                if (m_platformConfig->IsFileExcluded(normalizedPath))
                {
                    //One of the excluded file,continue
                    continue;
                }

                // its an input file.  check which scan folder it belongs to
                QString scanFolderName;
                QString relativePathToFile;
                if (!m_platformConfig->ConvertToRelativePath(normalizedPath, relativePathToFile, scanFolderName))
                {
                    AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessFilesToExamineQueue: Unable to find the relative path.\n");
                    continue;
                }

                if (normalizedPath.length() >= AP_MAX_PATH_LEN)
                {
                    // if we are here it means that we have found a source file whose filepath is greater than the maximum path length allowed
                    AZ_TracePrintf(AssetProcessor::ConsoleChannel, "ProcessFilesToExamineQueue: %s filepath length %d exceeds the maximum path length (%d) allowed.\n", normalizedPath.toUtf8().data(), normalizedPath.length(), AP_MAX_PATH_LEN);
                    JobDetails job;
                    for (int idx = 0; idx < m_platformConfig->PlatformCount(); idx++)
                    {
                        QString platform = m_platformConfig->PlatformAt(idx);
                        job.m_jobEntry = JobEntry(++m_highestJobId, normalizedPath, relativePathToFile, 0, platform, "");
                        Q_EMIT AssetToProcess(job);// forwarding this job to rccontroller to fail it
                    }
                    continue;
                }

                if (examineFile.m_isDelete)
                {
                    // if its a delete for a known input folder, we handle it differently.
                    if (m_knownFolders.contains(normalizedPath.toLower()))
                    {
                        CheckDeletedInputFolder(normalizedPath, relativePathToFile, scanFolderName);
                        continue;
                    }
                }
                else
                {
                    // a file was added to the source folder.
                    QFileInfo fileInfo(normalizedPath);
                    if (!fileInfo.isDir())
                    {
                        // keep track of its containing folder.
                        m_knownFolders.insert(AssetUtilities::NormalizeFilePath(fileInfo.absolutePath()).toLower());
                    }

                    if (fileInfo.isDir())
                    {
                        // if its a dir, we need to keep track of that too (no need to go recursive here since we will encounter this via a scan).
                        m_knownFolders.insert(normalizedPath.toLower());
                        // we actually need to scan this folder now...
                        CheckCreatedInputFolder(normalizedPath);
                        continue;
                    }
                }

                // is it being overridden by a higher priority file?
                QString overrider;
                if (examineFile.m_isDelete)
                {
                    // if we delete it, check if its revealed by an underlying file:
                    overrider = m_platformConfig->FindFirstMatchingFile(relativePathToFile);

                    // if the overrider is the same file, it means that a file was deleted, then reappeared.
                    // if that happened there will be a message in the notification queue for that file reappearing, there
                    // is no need to add a double here.
                    if (overrider.compare(normalizedPath, Qt::CaseInsensitive) == 0)
                    {
                        overrider.clear();
                    }
                }
                else
                {
                    overrider = m_platformConfig->GetOverridingFile(relativePathToFile, scanFolderName);
                }

                if (!overrider.isEmpty())
                {
                    // this file is being overridden by an earlier file.
                    // ignore us, and pretend the other file changed:
                    AZ_TracePrintf(AssetProcessor::DebugChannel, "File overridden by %s.\n", overrider.toUtf8().data());
                    CheckAsset(FileEntry(overrider, false));
                    continue;
                }

                // its an input file or a file we don't care about...
                // note that if the file now exists, we have to treat it as an input asset even if it came in as a delete.
                if (examineFile.m_isDelete && !QFile::exists(examineFile.m_fileName))
                {
                    AZ_TracePrintf(AssetProcessor::DebugChannel, "Input was deleted and no overrider was found.\n");
                    const AssetProcessor::ScanFolderInfo* scanFolderInfo = m_platformConfig->GetScanFolderForFile(normalizedPath);
                    QString sourceFile(relativePathToFile);
                    if (!scanFolderInfo->OutputPrefix().isEmpty())
                    {
                        sourceFile = sourceFile.right(sourceFile.length() - sourceFile.indexOf('/') - 1); // adding one for separator
                    }

                    using AzToolsFramework::AssetSystem::SourceFileNotificationMessage;
                    SourceFileNotificationMessage message(AZ::OSString(sourceFile.toUtf8().data()), AZ::OSString(scanFolderInfo->ScanPath().toUtf8().data()), SourceFileNotificationMessage::FileRemoved);
                    EBUS_EVENT(AssetProcessor::ConnectionBus, Send, 0, message);
                    CheckDeletedInputAsset(normalizedPath, relativePathToFile);
                }
                else
                {
                    AZ_TracePrintf(AssetProcessor::DebugChannel, "Input is modified or is overriding something.\n");
                    CheckModifiedInputAsset(normalizedPath, relativePathToFile);
                }
            }
        }

        // instead of checking here, we place a message at the end of the queue.
        // this is because there may be additional scan or other results waiting in the queue
        // an example would be where the scanner found additional "copy" jobs waiting in the queue for finalization
        QMetaObject::invokeMethod(this, "CheckForIdle", Qt::QueuedConnection);
    }

    void AssetProcessorManager::CheckForIdle()
    {
        if (IsIdle())
        {
            if (!m_hasProcessedCriticalAssets)
            {
                // only once, when we finish startup
                m_stateData->VacuumAndAnalyze();
                m_hasProcessedCriticalAssets = true;
            }

            if (!m_quitRequested && m_AssetProcessorIsBusy)
            {
                m_AssetProcessorIsBusy = false;
                Q_EMIT OnBecameIdle();
            }
        }
    }

    // ----------------------------------------------------
    // ------------- File change Queue --------------------
    // ----------------------------------------------------
    void AssetProcessorManager::AssessFileInternal(QString filePath, bool isDelete)
    {
        QString normalizedFilePath = AssetUtilities::NormalizeFilePath(filePath);
        if (m_platformConfig->IsFileExcluded(normalizedFilePath))
        {
            return;
        }

        AZ_TracePrintf(AssetProcessor::DebugChannel, "AssesFileInternal: %s %s\n", normalizedFilePath.toUtf8().data(), isDelete ? "true" : "false");

        // this function is the raw function that gets called from the file monitor
        // whenever an asset has been modified or added (not deleted)
        // it should place the asset on a grace period list and not considered until changes stop happening to it.
        // note that file Paths come in raw, full absolute paths.

        if (!m_SourceFilesInDatabase.empty())
        {
            QString relativeFileName;
            QString scanFolder;
            // filepath here are full filepath,but the QSet contains relative filepaths
            //therefore we convert to relativeFilename for checking
            if (m_platformConfig->ConvertToRelativePath(normalizedFilePath, relativeFileName, scanFolder))
            {
                // the database source files are always cased, do not lowercase this.
                QString lowered = relativeFileName.toLower();
                m_SourceFilesInDatabase.remove(lowered);
            }
        }

        FileEntry newEntry(normalizedFilePath, isDelete);

        if (m_alreadyActiveFiles.find(filePath) != m_alreadyActiveFiles.end())
        {
            auto entryIt = std::find_if(m_activeFiles.begin(), m_activeFiles.end(), [&normalizedFilePath](const FileEntry& entry)
                    {
                        return entry.m_fileName == normalizedFilePath;
                    });

            if (entryIt != m_activeFiles.end())
            {
                m_activeFiles.erase(entryIt);
            }
        }

        m_AssetProcessorIsBusy = true;
        m_activeFiles.push_back(newEntry);
        m_alreadyActiveFiles.insert(filePath);

        ScheduleNextUpdate(); // eventually, CheckAsset() will result.
    }


    void AssetProcessorManager::AssessAddedFile(QString filePath)
    {
        if (filePath.startsWith(m_normalizedCacheRootPath, Qt::CaseInsensitive))
        {
            // modifies/adds to the cache are irrelevant.  Deletions are all we care about
            return;
        }

        AssessFileInternal(filePath, false);
    }

    void AssetProcessorManager::AssessModifiedFile(QString filePath)
    {
        // we don't care about modified folders at this time.
        // you'll get a "folder modified" whenever a file in a folder is removed or added or modified
        // but you'll also get the actual file modify itself.
        if (!QFileInfo(filePath).isDir())
        {
            // we also don't care if you modify files in the cache, only deletions matter.
            if (!filePath.startsWith(m_normalizedCacheRootPath, Qt::CaseInsensitive))
            {
                AssessFileInternal(filePath, false);
            }
            
        }
        
    }

    void AssetProcessorManager::AssessDeletedFile(QString filePath)
    {
        AssessFileInternal(filePath, true);
    }

    void AssetProcessorManager::ScheduleNextUpdate()
    {
        if (m_activeFiles.size() > 0)
        {
            DispatchFileChange();
        }
        else
        {
            QMetaObject::invokeMethod(this, "CheckForIdle", Qt::QueuedConnection);
        }
    }

    void AssetProcessorManager::DispatchFileChange()
    {
        Q_ASSERT(m_activeFiles.size() > 0);

        if (m_quitRequested)
        {
            return;
        }

        // This was added because we found out that the consumer was not able to keep up, which led to the app taking forever to shut down
        // we want to make sure that our queue has at least this many to eat in a single gulp, so it remains busy, but we cannot let this number grow too large
        // or else it never returns to the main message pump and thus takes a while to realize that quit has been signalled.
        // if the processing thread ever runs dry, then this needs to be increased.
        int maxPerIteration = 500;

        // Burn through all pending files
        const FileEntry* firstEntry = &m_activeFiles.front();
        while (m_filesToExamine.size() < maxPerIteration)
        {
            m_alreadyActiveFiles.remove(firstEntry->m_fileName);
            CheckAsset(*firstEntry);
            m_activeFiles.pop_front();

            if (m_activeFiles.size() == 0)
            {
                break;
            }
            firstEntry = &m_activeFiles.front();
        }

        QMetaObject::invokeMethod(this, "ScheduleNextUpdate", Qt::QueuedConnection);
    }


    bool AssetProcessorManager::IsIdle()
    {
        if ((!m_queuedExamination) && (m_filesToExamine.isEmpty()) && (m_activeFiles.isEmpty()))
        {
            return true;
        }

        return false;
    }

    bool AssetProcessorManager::HasProcessedCriticalAssets() const
    {
        return m_hasProcessedCriticalAssets;
    }

    void AssetProcessorManager::ProcessBuilders(QString normalizedPath, QString relativePathToFile, const ScanFolderInfo* scanFolder, const AssetProcessor::BuilderInfoList& builderInfoList)
    {
        bool sentSourceFileChangedMessage = false;

        // Go through the builder list in 2 phases:
        //  1.  Collect all the jobs
        //  2.  Dispatch all the jobs

        AZStd::vector<JobDetails> jobsToAnalyze;

        // Phase 1 : Collect all the jobs and responses
        for (const AssetBuilderSDK::AssetBuilderDesc& builderInfo : builderInfoList)
        {
            QString builderVersionString = QString::number(builderInfo.m_version);
            const AssetBuilderSDK::CreateJobsRequest createJobsRequest(builderInfo.m_busId, relativePathToFile.toUtf8().data(), scanFolder->ScanPath().toUtf8().data(), m_platformFlags);
            AssetBuilderSDK::CreateJobsResponse createJobsResponse;

            // Send job request to the builder
            builderInfo.m_createJobFunction(createJobsRequest, createJobsResponse);

            if (createJobsResponse.m_result == AssetBuilderSDK::CreateJobsResultCode::Failed)
            {
                AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Error processing file %s.\n", normalizedPath.toUtf8().data());
                continue;
            }
            else if (createJobsResponse.m_result == AssetBuilderSDK::CreateJobsResultCode::ShuttingDown)
            {
                return;
            }
            else
            {
                for (AssetBuilderSDK::JobDescriptor& jobDescriptor : createJobsResponse.m_createJobOutputs)
                {
                    QString platformString = AssetUtilities::ComputePlatformName(jobDescriptor.m_platform);
                    JobDetails newJob;
                    newJob.m_assetBuilderDesc = builderInfo;
                    newJob.m_critical = jobDescriptor.m_critical;
                    newJob.m_extraInformationForFingerprinting = builderVersionString + QString(jobDescriptor.m_additionalFingerprintInfo.c_str());
                    newJob.m_jobEntry = JobEntry(0, normalizedPath, relativePathToFile, builderInfo.m_busId, platformString, jobDescriptor.m_jobKey.c_str());
                    newJob.m_jobEntry.m_checkExclusiveLock = jobDescriptor.m_checkExclusiveLock;
                    newJob.m_jobParam = AZStd::move(jobDescriptor.m_jobParameters);
                    newJob.m_priority = jobDescriptor.m_priority;
                    newJob.m_watchFolder = scanFolder->ScanPath();

                    PopulateSourceFileDependency(newJob);
                    // note that until analysis completes, the jobId is not set and neither is the destination path.
                    jobsToAnalyze.push_back(AZStd::move(newJob));
                }
            }
        }

        // Phase 2 : Process all the jobs and responses
        CheckMissingJobs(jobsToAnalyze);

        for (JobDetails& newJob : jobsToAnalyze)
        {
            // Check the current builder jobs with the previous ones in the database
            newJob.m_jobEntry.m_computedFingerprint = AssetUtilities::GenerateFingerprint(newJob);
            if (newJob.m_jobEntry.m_computedFingerprint == 0)
            {
                // unable to fingerprint this file.
                AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessBuilders: Unable to fingerprint for platform: %s.\n", newJob.m_jobEntry.m_platform.toUtf8().data());
                continue;
            }

            if (!m_commandLinePlatformsList.isEmpty() && !m_commandLinePlatformsList.contains(newJob.m_jobEntry.m_platform, Qt::CaseInsensitive))
            {
                // if the platform of this job does not match any of the platforms mentioned in the command line than continue  
                continue;
            }
            // Check to see whether we need to process this asset
            if (AnalyzeJob(newJob, scanFolder, sentSourceFileChangedMessage))
            {
                newJob.m_jobEntry.m_jobId = ++m_highestJobId;
                Q_EMIT AssetToProcess(newJob);
            }
        }
    }

    void AssetProcessorManager::ComputeNumberOfPendingWorkUnits(int& result)
    {
        result = m_activeFiles.size() + m_filesToExamine.size();
    }

    void AssetProcessorManager::BeginIgnoringCacheFileDelete(const AZStd::string productPath)
    {
        QMutexLocker locker(&m_processingJobMutex);
        m_processingProductInfoList.insert(productPath);
    }

    void AssetProcessorManager::StopIgnoringCacheFileDelete(const AZStd::string productPath, bool queueAgainForProcessing)
    {
        QMutexLocker locker(&m_processingJobMutex);

        m_processingProductInfoList.erase(productPath);
        if (queueAgainForProcessing)
        {
            AssessDeletedFile(QString(productPath.c_str()));
            return;
        }
    }

} // namespace AssetProcessor

// ----------------------------------------------------
// ----------- End File change Queue ------------------
// ----------------------------------------------------


#include <native/AssetManager/AssetProcessorManager.moc>