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
#include "rcjob.h"

#include <AzCore/Debug/TraceMessageBus.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/IO/SystemFile.h> // for max path len
#include <AzCore/std/string/string.h> // for wstring

#include <AzFramework/StringFunc/StringFunc.h>
#include <AzFramework/Logging/LogFile.h>

#include <AzToolsFramework/API/EditorAssetSystemAPI.h>

#include <AssetBuilderSDK/AssetBuilderBusses.h>
#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include "native/utilities/assetUtilEBusHelper.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QDir>
#include <QElapsedTimer>

#include "native/utilities/PlatformConfiguration.h"
#include "native/utilities/AssetUtils.h"
#include "native/assetprocessor.h"

namespace
{
    unsigned long s_jobSerial = 1;
    bool s_typesRegistered = false;
    // You have up to 60 minutes to finish processing an asset.
    // This was increased from 10 to account for PVRTC compression
    // taking up to an hour for large normal map textures, and should
    // be reduced again once we move to the ASTC compression format, or
    // find another solution to reduce processing times to be reasonable.
    const unsigned int g_jobMaximumWaitTime = 1000 * 60 * 60;

    const unsigned int g_sleepDurationForLockingAndFingerprintChecking = 100;

    bool MoveCopyFile(QString sourceFile, QString productFile, bool isCopyJob = false)
    {
        if (!isCopyJob && (AssetUtilities::MoveFileWithTimeout(sourceFile, productFile, 10)))
        {
            //We do not want to rename the file if it is a copy job
            return true;
        }
        else if (AssetUtilities::CopyFileWithTimeout(sourceFile, productFile, 10))
        {
            // try to copy instead
            return true;
        }
        AZ_TracePrintf(AssetBuilderSDK::ErrorWindow, "Failed to move OR copy file from Source directory: %s  to Destination Directory: %s", sourceFile.toUtf8().data(), productFile.toUtf8().data());
        return false;
    }
}

using namespace AssetProcessor;

namespace RCJobInternal
{
    //! JobLogTraceListener listens for job messages
    class JobLogTraceListener
        : public AZ::Debug::TraceMessageBus::Handler
    {
    public:

        JobLogTraceListener(AzToolsFramework::AssetSystem::JobInfo& jobInfo)
        {
            m_jobInfo = AZStd::move(jobInfo);
            AZ::Debug::TraceMessageBus::Handler::BusConnect();
        }
        ~JobLogTraceListener()
        {
            BusDisconnect();
        }

        //////////////////////////////////////////////////////////////////////////
        // AZ::Debug::TraceMessagesBus - we actually ignore all outputs except those for our ID.

        bool OnAssert(const char* message) override
        {
            if (AssetProcessor::GetThreadLocalJobId() == m_jobInfo.m_jobId)
            {
                AppendLog(AzFramework::LogFile::SEV_ASSERT, "ASSERT", message);
                return true;
            }

            return false;
        }
        bool OnException(const char* message) override
        {
            if (AssetProcessor::GetThreadLocalJobId() == m_jobInfo.m_jobId)
            {
                AppendLog(AzFramework::LogFile::SEV_EXCEPTION, "EXCEPTION", message);
                return true;
            }

            return false;
        }
        bool OnError(const char* window, const char* message) override
        {
            if (AssetProcessor::GetThreadLocalJobId() == m_jobInfo.m_jobId)
            {
                AppendLog(AzFramework::LogFile::SEV_ERROR, window, message);
                return true;
            }

            return false;
        }
        bool OnWarning(const char* window, const char* message) override
        {
            if (AssetProcessor::GetThreadLocalJobId() == m_jobInfo.m_jobId)
            {
                AppendLog(AzFramework::LogFile::SEV_WARNING, window, message);
                return true;
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////

        bool OnPrintf(const char* window, const char* message)
        {
            if (AssetProcessor::GetThreadLocalJobId() == m_jobInfo.m_jobId)
            {
                if (azstrnicmp(window, "debug", 5) == 0)
                {
                    AppendLog(AzFramework::LogFile::SEV_DEBUG, window, message);
                }
                else
                {
                    AppendLog(AzFramework::LogFile::SEV_NORMAL, window, message);
                }

                return true;
            }


            return false;
        }
        //////////////////////////////////////////////////////////////////////////

    private:
        AZStd::unique_ptr<AzFramework::LogFile> m_logFile;
        AzToolsFramework::AssetSystem::JobInfo m_jobInfo;
        // using m_isLogging bool to prevent an infinite loop which can happen if an error/warning happens when trying to create an invalid logFile,
        // because it will cause the appendLog function to be called again, which will again try to create that log file.
        bool m_isLogging = false;

        void AppendLog(AzFramework::LogFile::SeverityLevel severity, const char* window, const char* message)
        {
            if (m_isLogging)
            {
                return;
            }

            m_isLogging = true;

            if (!m_logFile)
            {
                AZStd::string fullPath = AssetUtilities::ComputeJobLogFolder() + "/" + AssetUtilities::ComputeJobLogFileName(m_jobInfo);
                m_logFile = new AzFramework::LogFile(fullPath.c_str());
            }
            m_logFile->AppendLog(severity, window, message);
            m_isLogging = false;
        }
    };
}

bool Params::IsValidParams() const
{
    return (!m_finalOutputDir.isEmpty());
}

bool RCParams::IsValidParams() const
{
    return (
        (!m_rcExe.isEmpty()) &&
        (!m_rootDir.isEmpty()) &&
        (!m_inputFile.isEmpty()) &&
        Params::IsValidParams()
        );
}

namespace AssetProcessor
{
    RCJob::RCJob(QObject* parent)
        : QObject(parent)
        , m_timeCreated(QDateTime::currentDateTime())
    {
        m_jobState = RCJob::pending;

        if (!s_typesRegistered)
        {
            qRegisterMetaType<RCParams>("RCParams");
            qRegisterMetaType<BuilderParams>("BuilderParams");
            qRegisterMetaType<JobOutputInfo>("JobOutputInfo");
            s_typesRegistered = true;
        }
    }

    RCJob::~RCJob()
    {
    }

    void RCJob::Init(JobDetails& details)
    {
        m_jobDetails = AZStd::move(details);
        m_QueueElementID = QueueElementID(GetInputFileRelativePath(), GetPlatform(), GetJobKey());
    }

    AZ::s64 RCJob::jobID() const
    {
        return m_jobDetails.m_jobEntry.m_jobId;
    }

    const JobEntry& RCJob::GetJobEntry() const
    {
        return m_jobDetails.m_jobEntry;
    }

    QDateTime RCJob::timeCreated() const
    {
        return m_timeCreated;
    }

    void RCJob::setTimeCreated(const QDateTime& timeCreated)
    {
        m_timeCreated = timeCreated;
    }

    QDateTime RCJob::timeLaunched() const
    {
        return m_timeLaunched;
    }

    void RCJob::setTimeLaunched(const QDateTime& timeLaunched)
    {
        m_timeLaunched = timeLaunched;
    }

    QDateTime RCJob::timeCompleted() const
    {
        return m_timeCompleted;
    }

    void RCJob::setTimeCompleted(const QDateTime& timeCompleted)
    {
        m_timeCompleted = timeCompleted;
    }

    unsigned int RCJob::GetOriginalFingerprint() const
    {
        return m_jobDetails.m_jobEntry.m_computedFingerprint;
    }

    void RCJob::SetOriginalFingerprint(unsigned int fingerprint)
    {
        m_jobDetails.m_jobEntry.m_computedFingerprint = fingerprint;
    }

    RCJob::JobState RCJob::GetState() const
    {
        return m_jobState;
    }

    void RCJob::SetState(const JobState& state)
    {
        m_jobState = state;
    }

    void RCJob::SetInputFileAbsolutePath(QString absolutePath)
    {
        m_jobDetails.m_jobEntry.m_absolutePathToFile = absolutePath.toUtf8().data();
    }
    void RCJob::SetCheckExclusiveLock(bool value)
    {
        m_jobDetails.m_jobEntry.m_checkExclusiveLock = value;
    }

    QString RCJob::stateDescription(const RCJob::JobState& state)
    {
        switch (state)
        {
        case RCJob::pending:
            return tr("Pending");
        case RCJob::processing:
            return tr("Processing");
        case RCJob::completed:
            return tr("Completed");
        case RCJob::crashed:
            return tr("Crashed");
        case RCJob::terminated:
            return tr("Terminated");
        case RCJob::failed:
            return tr("Failed");
        }
        return QString();
    }

    QString RCJob::GetInputFileAbsolutePath() const
    {
        return m_jobDetails.m_jobEntry.m_absolutePathToFile;
    }

    QString RCJob::GetInputFileRelativePath() const
    {
        return m_jobDetails.m_jobEntry.m_relativePathToFile;
    }

    QString RCJob::GetFinalOutputPath() const
    {
        return m_jobDetails.m_destinationPath;
    }

    QString RCJob::GetPlatform() const
    {
        return m_jobDetails.m_jobEntry.m_platform;
    }

    AssetBuilderSDK::ProcessJobResponse& RCJob::GetProcessJobResponse()
    {
        return m_processJobResponse;
    }

    void RCJob::PopulateProcessJobRequest(AssetBuilderSDK::ProcessJobRequest& processJobRequest)
    {
        processJobRequest.m_jobDescription.m_critical = IsCritical();
        processJobRequest.m_jobDescription.m_additionalFingerprintInfo = m_jobDetails.m_extraInformationForFingerprinting.toUtf8().data();
        processJobRequest.m_jobDescription.m_jobKey = GetJobKey().toUtf8().data();
        processJobRequest.m_jobDescription.m_jobParameters = AZStd::move(m_jobDetails.m_jobParam);
        processJobRequest.m_jobDescription.m_platform = AssetUtilities::ComputePlatformFlag(GetPlatform());
        processJobRequest.m_jobDescription.m_priority = GetPriority();

        processJobRequest.m_builderId = GetBuilderUUID();
        processJobRequest.m_sourceFile = GetInputFileRelativePath().toUtf8().data();
        processJobRequest.m_watchFolder = GetWatchFolder().toUtf8().data();
        processJobRequest.m_fullPath = GetInputFileAbsolutePath().toUtf8().data();
    }

    QString RCJob::GetJobKey() const
    {
        return m_jobDetails.m_jobEntry.m_jobKey;
    }

    QString RCJob::GetWatchFolder() const
    {
        return m_jobDetails.m_watchFolder;
    }

    AZ::Uuid RCJob::GetBuilderUUID() const
    {
        return m_jobDetails.m_jobEntry.m_builderUUID;
    }

    bool RCJob::IsCritical() const
    {
        return m_jobDetails.m_critical;
    }

    int RCJob::GetPriority() const
    {
        return m_jobDetails.m_priority;
    }

    void RCJob::start()
    {
        AssetProcessor::QuitListener listener;
        listener.BusConnect();
        RCParams rc(this);
        BuilderParams builderParams(this);

        //Create the process job request
        AssetBuilderSDK::ProcessJobRequest processJobRequest;
        PopulateProcessJobRequest(processJobRequest);

        builderParams.m_processJobRequest = processJobRequest;
        builderParams.m_finalOutputDir = GetFinalOutputPath();
        builderParams.m_assetBuilderDesc = m_jobDetails.m_assetBuilderDesc;

        // when the job finishes, record the results and emit finished()
        connect(this, &RCJob::jobFinished, this, [this](AssetBuilderSDK::ProcessJobResponse result)
        {
            m_processJobResponse = AZStd::move(result);
            switch (m_processJobResponse.m_resultCode)
            {
                case AssetBuilderSDK::ProcessJobResult_Crashed:
                    {
                        SetState(crashed);
                    }
                    break;
                case AssetBuilderSDK::ProcessJobResult_Success:
                    {
                        SetState(completed);
                    }
                    break;
                default:
                    {
                        SetState(failed);
                    }
                    break;
                }
            Q_EMIT finished();
        });

        if (!listener.WasQuitRequested())
        {
            QtConcurrent::run(&RCJob::executeBuilderCommand, builderParams);
        }
        else
        {
            AZ_TracePrintf(AssetBuilderSDK::ErrorWindow, "Job cancelled due to quit being requested.");
            SetState(terminated);
            Q_EMIT finished();
        }
    }

    void RCJob::executeBuilderCommand(BuilderParams builderParams)
    {
        // Setting job id for logging purposes

        AssetProcessor::SetThreadLocalJobId(builderParams.m_rcJob->jobID());

        // listen for the user quitting (CTRL-C or otherwise)
        AssetProcessor::QuitListener listener;
        listener.BusConnect();
        QElapsedTimer ticker;
        ticker.start();
        AssetBuilderSDK::ProcessJobResponse result;
        // Lock and unlock the source file to ensure it is not still open by another process.
        // This prevents premature processing of some source files that are opened for writing, but are zero bytes for longer than the modification threshhold
        QString inputFile(builderParams.m_rcJob->GetJobEntry().m_absolutePathToFile);
        if (builderParams.m_rcJob->GetJobEntry().m_checkExclusiveLock && QFile::exists(inputFile))
        {
            // We will only continue once we get exclusive lock on the source file
            while (!AssetUtilities::CheckCanLock(inputFile))
            {
                QThread::msleep(g_sleepDurationForLockingAndFingerprintChecking);
                if (listener.WasQuitRequested() || (ticker.elapsed() > g_jobMaximumWaitTime))
                {
                    result.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
                    Q_EMIT builderParams.m_rcJob->jobFinished(result);
                    return;
                }
            }
        }

        unsigned int fingerprint = AssetUtilities::GenerateFingerprint(builderParams.m_rcJob->m_jobDetails);
        while (fingerprint != builderParams.m_rcJob->GetOriginalFingerprint())
        {
            // We will only continue once the fingerprint of the file stops changing
            builderParams.m_rcJob->SetOriginalFingerprint(fingerprint);
            QThread::msleep(g_sleepDurationForLockingAndFingerprintChecking);
            if (listener.WasQuitRequested() || (ticker.elapsed() > g_jobMaximumWaitTime))
            {
                result.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
                Q_EMIT builderParams.m_rcJob->jobFinished(result);
                return;
            }
        }

        Q_EMIT builderParams.m_rcJob->BeginWork();
        // We will actually start working on the job after this point and even if RcController gets the same job again, we will put it in the queue for processing
        builderParams.m_rcJob->DoWork(result, builderParams, listener);
        Q_EMIT builderParams.m_rcJob->jobFinished(result);
    }


    void RCJob::DoWork(AssetBuilderSDK::ProcessJobResponse& result, BuilderParams& builderParams, AssetProcessor::QuitListener& listener)
    {
        result.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed; // failed by default

        // each thread has its own random number generator which needs to be initialized.
        //We are seeding it for TemporaryDir which we are using later.
        qsrand(static_cast<qint64>(QTime::currentTime().msecsSinceStartOfDay()) ^ reinterpret_cast<ptrdiff_t>(QThread::currentThreadId()));
        // create a temporary directory for Builder to work in.
        // lets make it as a subdir of a known temp dir

        QString tempPath = QDir::tempPath();
        QString workFolder = AssetUtilities::CreateTempWorkspace();

        if (workFolder.isEmpty())
        {
            AZ_TracePrintf(AssetBuilderSDK::ErrorWindow, "Could not create temporary directory for Builder at (%s)!", tempPath.toUtf8().data());
            result.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        builderParams.m_processJobRequest.m_tempDirPath = AZStd::string(workFolder.toUtf8().data());


        AzToolsFramework::AssetSystem::JobInfo jobInfo;
        jobInfo.m_jobId = AssetProcessor::GetThreadLocalJobId();
        jobInfo.m_builderGuid = builderParams.m_assetBuilderDesc.m_busId;
        jobInfo.m_jobKey = builderParams.m_processJobRequest.m_jobDescription.m_jobKey;
        jobInfo.m_platform = AssetUtilities::ComputePlatformName(builderParams.m_processJobRequest.m_jobDescription.m_platform).toUtf8().data();
        jobInfo.m_sourceFile = builderParams.m_processJobRequest.m_sourceFile;
        jobInfo.m_status = AzToolsFramework::AssetSystem::JobStatus::InProgress;

        RCJobInternal::JobLogTraceListener jobLogTraceListener(jobInfo);

        QString sourceFullPath(builderParams.m_processJobRequest.m_fullPath.c_str());
        if (sourceFullPath.length() >= AP_MAX_PATH_LEN)
        {
            AZ_TracePrintf(AssetBuilderSDK::WarningWindow, "Warning: Source Asset: %s filepath length %d exceeds the maximum path length (%d) allowed.\n", sourceFullPath.toUtf8().data(), sourceFullPath.length(), AP_MAX_PATH_LEN);
            result.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
        }
        else
        {
            // sending process job command to the builder
            builderParams.m_assetBuilderDesc.m_processJobFunction(builderParams.m_processJobRequest, result);
        }

        bool shouldRemoveTempFolder = true;

        switch (result.m_resultCode)
        {
        case AssetBuilderSDK::ProcessJobResult_Success:
            if (!copyCompiledAssets(builderParams, result))
            {
                result.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                shouldRemoveTempFolder = false;
            }
            break;

        case AssetBuilderSDK::ProcessJobResult_Crashed:
            AZ_TracePrintf(AssetBuilderSDK::ErrorWindow, "Builder indicated that its process crashed!");
            break;

        case AssetBuilderSDK::ProcessJobResult_Cancelled:
            AZ_TracePrintf(AssetBuilderSDK::ErrorWindow, "Builder indicates that the job was cancelled.");
            break;
        case AssetBuilderSDK::ProcessJobResult_Failed:
            AZ_TracePrintf(AssetBuilderSDK::ErrorWindow, "Builder indicated that the job has failed.");
            shouldRemoveTempFolder = false;
            break;
        }

        if ((shouldRemoveTempFolder) || (listener.WasQuitRequested()))
        {
            QDir workingDir(QString(builderParams.m_processJobRequest.m_tempDirPath.c_str()));
            workingDir.removeRecursively();
        }

        // Setting the job id back to zero for error detection
        AssetProcessor::SetThreadLocalJobId(0);
        listener.BusDisconnect();
    }

    bool RCJob::copyCompiledAssets(BuilderParams& params, AssetBuilderSDK::ProcessJobResponse& response)
    {
        QDir            outputDirectory(params.m_finalOutputDir.toLower());
        QString         tempFolder = params.m_processJobRequest.m_tempDirPath.c_str();
        QDir            tempDir(tempFolder);

        // if outputDirectory does not exist then create it
        if (!outputDirectory.exists())
        {
            if (!outputDirectory.mkpath("."))
            {
                AZ_TracePrintf(AssetBuilderSDK::ErrorWindow, "Failed to create output directory: %s\n", outputDirectory.absolutePath().toUtf8().data());
                return false;
            }
        }

        for (AssetBuilderSDK::JobProduct& product : response.m_outputProducts)
        {
            // each Output Product communicated by the builder will either be
            // * a relative path, which means we assume its relative to the temp folder, and we attempt to move the file
            // * an absolute path in the temp folder, and we attempt to move also
            // * an absolute path outside the temp folder, in which we assume you'd like to just copy a file somewhere.

            QString outputProduct = QString(product.m_productFileName.c_str()); // could be a relative path.
            QFileInfo fileInfo(outputProduct);

            if (fileInfo.isRelative())
            {
                // we assume that its relative to the TEMP folder.
                fileInfo = QFileInfo(tempDir.absoluteFilePath(outputProduct));
            }

            QString absolutePathOfSource = fileInfo.absoluteFilePath();
            QString outputFilename = fileInfo.fileName();
            QString productFile = outputDirectory.filePath(outputFilename);

            productFile = productFile.toLower(); // make it all case-insensitive lowercase

            if (productFile.length() >= AP_MAX_PATH_LEN)
            {
                AZ_TracePrintf(AssetBuilderSDK::WarningWindow, "Warning: Product %s path length (%d) exceeds the max path length (%d) allowed.\n", productFile.toUtf8().data(), productFile.length(), AP_MAX_PATH_LEN);
                return false;
            }

            bool isCopyJob = !(absolutePathOfSource.startsWith(tempFolder, Qt::CaseInsensitive));
            EBUS_EVENT(AssetProcessor::ProcessingJobInfoBus, BeginIgnoringCacheFileDelete, productFile.toUtf8().data());
            if (!MoveCopyFile(absolutePathOfSource, productFile, isCopyJob)) // this has its own traceprintf for failure
            {
                EBUS_EVENT(AssetProcessor::ProcessingJobInfoBus, StopIgnoringCacheFileDelete, productFile.toUtf8().data(), true);
                return false;
            }

            EBUS_EVENT(AssetProcessor::ProcessingJobInfoBus, StopIgnoringCacheFileDelete, productFile.toUtf8().data(), false);

            //we now ensure that the file is writable
            if ((QFile::exists(productFile)) && (!AssetUtilities::MakeFileWritable(productFile)))
            {
                AZ_TracePrintf(AssetBuilderSDK::WarningWindow, "Unable to change permission for the file: %s.\n", productFile.toUtf8().data());
            }
            // replace the product file name in the result with the actual output path to the file.
            product.m_productFileName = productFile.toUtf8().data();
        }

        return true;
    }
} // namespace AssetProcessor

#include <native/resourcecompiler/rcjob.moc>

