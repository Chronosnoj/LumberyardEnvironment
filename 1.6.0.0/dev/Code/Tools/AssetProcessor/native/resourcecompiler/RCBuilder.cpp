/*
* All or portions of this file Copyright(c) Amazon.com, Inc.or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution(the "License").All use of this software is governed by the License,
*or, if provided, by the license below or the license accompanying this file.Do not
* remove or modify any license notices.This file is distributed on an "AS IS" BASIS,
*WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include "RCBuilder.h"

#include <QElapsedTimer>
#include <QCoreApplication>

#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/IO/SystemFile.h>

#include <AzFramework/StringFunc/StringFunc.h>

#include <AzToolsFramework/Process/ProcessCommunicator.h>
#include <AzToolsFramework/Process/ProcessWatcher.h>

#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <AssetBuilderSDK/AssetBuilderBusses.h>

#include "native/resourcecompiler/rccontroller.h"

#include "native/utilities/assetUtils.h"
#include "native/utilities/AssetBuilderInfo.h"

#define LEGACY_RC_RELATIVE_PATH "/rc/rc.exe"    // Location of the legacy RC compiler relative to the BinXX folder the asset processor resides in

namespace AssetProcessor
{
#if defined(AZ_PLATFORM_WINDOWS)
    // remove the above IFDEF as soon as the AzToolsFramework ProcessCommunicator functions on OSX, along with the rest of the similar IFDEFs in this file.

    //! CommunicatorTracePrinter listens to stderr and stdout of a running process and writes its output to the AZ_Trace system
    //! Importantly, it does not do any blocking operations.
    class CommunicatorTracePrinter
    {
    public:
        CommunicatorTracePrinter(AzToolsFramework::ProcessCommunicator* communicator)
            : m_communicator(communicator)
        {
            m_stringBeingConcatenated.reserve(1024);
        }

        ~CommunicatorTracePrinter()
        {
            WriteCurrentString();
        }

        // call this periodically to drain the buffers and write them.
        void Pump()
        {
            if (m_communicator->IsValid())
            {
                // Don't call readOutput unless there is output or else it will block...
                while (m_communicator->PeekOutput())
                {
                    AZ::u32 readSize = m_communicator->ReadOutput(m_streamBuffer, AZ_ARRAY_SIZE(m_streamBuffer));
                    ParseDataBuffer(readSize);

                }
                while (m_communicator->PeekError())
                {
                    AZ::u32 readSize = m_communicator->ReadError(m_streamBuffer, AZ_ARRAY_SIZE(m_streamBuffer));
                    ParseDataBuffer(readSize);
                }

            }
        }

        // drains the buffer into the string thats being built, then traces the string when it hits a newline.
        void ParseDataBuffer(AZ::u32 readSize)
        {
            if (readSize > AZ_ARRAY_SIZE(m_streamBuffer))
            {
                AZ_ErrorOnce("ERROR", false, "Programmer bug:  Read size is overflowing in traceprintf communicator.");
                return;
            }
            for (size_t pos = 0; pos < readSize; ++pos)
            {
                if ((m_streamBuffer[pos] == '\n') || (m_streamBuffer[pos] == '\r'))
                {
                    WriteCurrentString();
                }
                else
                {
                    m_stringBeingConcatenated.push_back(m_streamBuffer[pos]);
                }
            }
        }
        
        void WriteCurrentString()
        {
            if (!m_stringBeingConcatenated.empty())
            {
                AZ_TracePrintf("RC Builder", "%s", m_stringBeingConcatenated.c_str());
            }
            m_stringBeingConcatenated.clear();
        }

        
    private :
        AzToolsFramework::ProcessCommunicator* m_communicator = nullptr;
        char m_streamBuffer[128];
        AZStd::string m_stringBeingConcatenated;
    };
   
#endif // AZ_PLATFORM_WINDOWS

    // don't make this too high, its basically how slowly the app responds to a job finishing.
    // this basically puts a hard cap on how many RC jobs can execute per second, since at 10ms per job (minimum), with 8 cores, thats a max
    // of 800 jobs per second that can possibly run.  However, the actual time it takes to launch RC.EXE is far, far longer than 10ms, so this is not a bad number for now...
    const int NativeLegacyRCCompiler::s_maxSleepTime = 10;

    // You have up to 60 minutes to finish processing an asset.
    // This was increased from 10 to account for PVRTC compression
    // taking up to an hour for large normal map textures, and should
    // be reduced again once we move to the ASTC compression format, or
    // find another solution to reduce processing times to be reasonable.
    const unsigned int NativeLegacyRCCompiler::s_jobMaximumWaitTime = 1000 * 60 * 60;

    NativeLegacyRCCompiler::Result::Result(int exitCode, bool crashed, const QString& outputDir)
        : m_exitCode(exitCode)
        , m_crashed(crashed)
        , m_outputDir(outputDir)
    {
    }

    NativeLegacyRCCompiler::NativeLegacyRCCompiler()
        : m_resourceCompilerInitialized(false)
        , m_systemRoot()
        , m_rcExecutableFullPath()
        , m_requestedQuit(false)
    {
    }

    bool NativeLegacyRCCompiler::Initialize(const QString& systemRoot, const QString& rcExecutableFullPath)
    {
        // QFile::exists(normalizedPath)
        if (!QDir(systemRoot).exists())
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, QString("Cannot locate system root dir %1").arg(systemRoot).toUtf8().data());
            return false;
        }
#if defined(AZ_PLATFORM_WINDOWS)

        if (!AZ::IO::SystemFile::Exists(rcExecutableFullPath.toUtf8().data()))
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, QString("Invalid executable path '%1'").arg(rcExecutableFullPath).toUtf8().data());
            return false;
        }
        this->m_systemRoot = systemRoot;
        this->m_rcExecutableFullPath = rcExecutableFullPath;
        this->m_resourceCompilerInitialized = true;
        return true;
#else // AZ_PLATFORM_WINDOWS
        AZ_TracePrintf(AssetProcessor::DebugChannel, "There is no implementation for how to compile assets on this platform");
        return false;
#endif // AZ_PLATFORM_WINDOWS
    }


    bool NativeLegacyRCCompiler::Execute(const QString& inputFile, const QString& platform, const QString& params, const QString& dest, NativeLegacyRCCompiler::Result& result) const
    {
#if defined(AZ_PLATFORM_WINDOWS)
        if (!this->m_resourceCompilerInitialized)
        {
            result.m_exitCode = JobExitCode_RCCouldNotBeLaunched;
            result.m_crashed = false;
            AZ_Warning("RC Builder", false, "RC Compiler has not been initialized before use.");
            return false;
        }

        // build the command line:
        QString commandString = NativeLegacyRCCompiler::BuildCommand(inputFile, platform, params, dest);

        AzToolsFramework::ProcessLauncher::ProcessLaunchInfo processLaunchInfo;

        // while it might be tempting to set the executable in processLaunchInfo.m_processExecutableString, it turns out that RC.EXE
        // won't work if you do that becuase it assumes the first command line param is the exe name, which is not the case if you do it that way...
        processLaunchInfo.m_commandlineParameters = (QString("\"") + this->m_rcExecutableFullPath + QString("\" ") + commandString).toUtf8().data();
        processLaunchInfo.m_showWindow = false;
        processLaunchInfo.m_workingDirectory = m_systemRoot.absolutePath().toUtf8().data();
        processLaunchInfo.m_processPriority = AzToolsFramework::PROCESSPRIORITY_IDLE;

        AZ_TracePrintf("RC Builder", "Executing RC.EXE: '%s' ...\n", processLaunchInfo.m_commandlineParameters.c_str());
        AZ_TracePrintf("Rc Builder", "Executing RC.EXE with working directory: '%s' ...\n", processLaunchInfo.m_workingDirectory.c_str());

        AzToolsFramework::ProcessWatcher* watcher = AzToolsFramework::ProcessWatcher::LaunchProcess(processLaunchInfo, AzToolsFramework::COMMUNICATOR_TYPE_STDINOUT);

        if (!watcher)
        {
            result.m_exitCode = JobExitCode_RCCouldNotBeLaunched;
            result.m_crashed = false;
            AZ_Error("RC Builder", false, "RC failed to execute\n");

            return false;
        }

        QElapsedTimer ticker;
        ticker.start();

        // it created the process, wait for it to exit:
        bool finishedOK = false;
        {
            CommunicatorTracePrinter tracer(watcher->GetCommunicator()); // allow this to go out of scope...
            while ((!m_requestedQuit) && (!finishedOK))
            {
                AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(NativeLegacyRCCompiler::s_maxSleepTime));

                tracer.Pump();
            
                if (ticker.elapsed() > s_jobMaximumWaitTime)
                {
                    break;
                }

                AZ::u32 exitCode = 0;
                if (!watcher->IsProcessRunning(&exitCode))
                {
                    finishedOK = true; // we either cant wait for it, or it finished.
                    result.m_exitCode = exitCode;
                    result.m_crashed = (exitCode == 100) || (exitCode == 101); // these indicate fatal errors.
                    break;
                }
            }

            tracer.Pump(); // empty whats left if possible.
        }

        if (!finishedOK)
        {
            if (watcher->IsProcessRunning())
            {
                watcher->TerminateProcess(0xFFFFFFFF);
            }

            if (!this->m_requestedQuit)
            {
                AZ_Error("RC Builder", false, "RC failed to complete within the maximum allowed time and was terminated. please see %s/rc_log.log for details", result.m_outputDir.toUtf8().data());
            }
            else
            {
                AZ_Warning("RC Builder", false, "RC terminated because the application is shutting down.");
                result.m_exitCode = JobExitCode_JobCancelled;
            }
            result.m_crashed = false;
        }
        AZ_TracePrintf("RC Builder", "RC.EXE execution has ended\n");

        delete watcher;
       
        return finishedOK;

#else // AZ_PLATFORM_WINDOWS
        result.m_exitCode = JobExitCode_RCCouldNotBeLaunched;
        result.m_crashed = false;
        AZ_Error("RC Builder", false, "There is no implementation for how to compile assets via RC on this platform");
        return false;
#endif // AZ_PLATFORM_WINDOWS
    }

    QString NativeLegacyRCCompiler::BuildCommand(const QString& inputFile, const QString& platform, const QString& params, const QString& dest)
    {
        QString cmdLine;
        if (!dest.isEmpty())
        {
            QDir engineRoot;
            AssetUtilities::ComputeEngineRoot(engineRoot);
            QString gameRoot = engineRoot.absoluteFilePath(AssetUtilities::ComputeGameName());
            cmdLine = QString("\"%1\" /p=%2 %3 /unattended /threads=1 /gameroot=\"%4\" /targetroot=\"%5\" /logprefix=\"%5/\"").arg(inputFile).arg(platform).arg(params).arg(gameRoot).arg(dest);
        }
        else
        {
            cmdLine = QString("\"%1\" /p=%2 %3 /threads=1").arg(inputFile).arg(platform).arg(params);
        }
        return cmdLine;
    }

    void NativeLegacyRCCompiler::RequestQuit()
    {
        this->m_requestedQuit = true;
    }

    InternalAssetRecognizer::InternalAssetRecognizer(const AssetRecognizer& src, std::function<bool(const AssetPlatformSpec&)> filter)
        : AssetRecognizer(src.m_name, src.m_testLockSource, src.m_priority, src.m_patternMatcher, src.m_version)
    {
        for (auto iterSrcPlatformSpec = src.m_platformSpecs.begin();
             iterSrcPlatformSpec != src.m_platformSpecs.end();
             iterSrcPlatformSpec++)
        {
            if (filter(*iterSrcPlatformSpec))
            {
                int convertedSrcPlatformFlag = AssetUtilities::ComputePlatformFlag(iterSrcPlatformSpec.key());
                this->m_platformSpecsByPlatform[convertedSrcPlatformFlag] = *iterSrcPlatformSpec;
            }
        }
        this->m_paramID = CalculateCRC();
    }

    AZ::u32 InternalAssetRecognizer::CalculateCRC() const
    {
        AZ::Crc32 crc;
        crc.Add(m_name.toUtf8().data());
        crc.Add(const_cast<void*>(static_cast<const void*>(&m_testLockSource)), sizeof(bool));
        crc.Add(const_cast<void*>(static_cast<const void*>(&m_priority)), sizeof(int));
        crc.Add(m_patternMatcher.GetBuilderPattern().m_pattern.c_str());
        crc.Add(const_cast<void*>(static_cast<const void*>(&m_patternMatcher.GetBuilderPattern().m_type)), sizeof(size_t));
        return static_cast<AZ::u32>(crc);
    }

    InternalRecognizerBasedBuilder::InternalRecognizerBasedBuilder(const AZStd::string& name,
        const AZ::Uuid& builderUuid,
        InternalAssetRecognizer::PlatformFilter platformFilter)
        : m_isShuttingDown(false)
        , m_platformFilter(platformFilter)
        , m_builderUuid(builderUuid)
        , m_name(name)
    {
        AssetBuilderSDK::AssetBuilderCommandBus::Handler::BusConnect(m_builderUuid);
    }

    InternalRecognizerBasedBuilder::~InternalRecognizerBasedBuilder()
    {
        AssetBuilderSDK::AssetBuilderCommandBus::Handler::BusDisconnect(m_builderUuid);
        for (auto assetRecognizer : m_assetRecognizerDictionary)
        {
            delete assetRecognizer;
        }
    }

    void InternalRecognizerBasedBuilder::ShutDown()
    {
        m_isShuttingDown = true;
    }

    bool InternalRecognizerBasedBuilder::Initialize(const RecognizerConfiguration& recognizerConfig)
    {
        InitializeAssetRecognizers(recognizerConfig.GetAssetRecognizerContainer());
        InitializeExcludeRecognizers(recognizerConfig.GetExcludeAssetRecognizerContainer());
        return true;
    }

    void InternalRecognizerBasedBuilder::UnInitialize()
    {
        EBUS_EVENT(AssetBuilderRegistrationBus, UnRegisterBuilderDescriptor, this->m_builderUuid);
    }

    void InternalRecognizerBasedBuilder::InitializeAssetRecognizers(const RecognizerContainer& assetRecognizers)
    {
        AZStd::vector<AssetBuilderSDK::AssetBuilderPattern>  builderPatterns;

        for (const AssetRecognizer& assetRecognizer : assetRecognizers)
        {
            // Only accept recognizers that match the current platform filter
            InternalAssetRecognizer* internalAssetRecognizer = new InternalAssetRecognizer(assetRecognizer, m_platformFilter);
            if (internalAssetRecognizer->m_platformSpecsByPlatform.size() == 0)
            {
                delete internalAssetRecognizer;
                AZ_Warning(AssetProcessor::DebugChannel, "Skipping recognizer %s, no platforms supported\n", assetRecognizer.m_name.toUtf8().data());
                continue;
            }

            // Ignore duplicate recognizers
            if (m_assetRecognizerDictionary.contains(internalAssetRecognizer->m_paramID))
            {
                delete internalAssetRecognizer;
                AZ_Warning(AssetProcessor::ConsoleChannel, false, "Ignoring duplicate asset recognizer in configuration: %s\n", internalAssetRecognizer->m_name.toUtf8().data());
                continue;
            }

            // Register the recognizer
            builderPatterns.push_back(assetRecognizer.m_patternMatcher.GetBuilderPattern());
            m_assetRecognizerDictionary[internalAssetRecognizer->m_paramID] = internalAssetRecognizer;
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Registering %s as a %s\n", internalAssetRecognizer->m_name.toUtf8().data(), this->GetBuilderName().c_str());
        }

        // Register the builder desc
        AssetBuilderSDK::AssetBuilderDesc builderDesc = CreateBuilderDesc(builderPatterns);
        EBUS_EVENT(AssetBuilderSDK::AssetBuilderBus, RegisterBuilderInformation, builderDesc);
    }

    void InternalRecognizerBasedBuilder::InitializeExcludeRecognizers(const ExcludeRecognizerContainer& excludeRecognizers)
    {
        for (const ExcludeAssetRecognizer& excludeRecognizer : excludeRecognizers)
        {
            if (!m_excludeAssetRecognizers.contains(excludeRecognizer.m_name))
            {
                m_excludeAssetRecognizers[excludeRecognizer.m_name] = excludeRecognizer;
            }
            else
            {
                AZ_Warning(AssetProcessor::ConsoleChannel, false, "Ignoring duplicate asset exclude recognizer in configuration: %s\n", excludeRecognizer.m_name.toUtf8().data());
            }
        }
    }

    bool InternalRecognizerBasedBuilder::GetMatchingRecognizers(int platformFlags, const QString fileName, InternalRecognizerPointerContainer& output) const
    {
        bool foundAny = false;
        if (IsFileExcluded(fileName))
        {
            //if the file is excluded than return false;
            return false;
        }
        for (const InternalAssetRecognizer* recognizer : m_assetRecognizerDictionary)
        {
            if (recognizer->m_patternMatcher.MatchesPath(fileName))
            {
                bool matchPlatform = false;
                // found a match, now match the platform
                for (auto iterPlatformKeys = recognizer->m_platformSpecsByPlatform.keyBegin();
                     iterPlatformKeys != recognizer->m_platformSpecsByPlatform.keyEnd();
                     iterPlatformKeys++)
                {
                    if (platformFlags & (*iterPlatformKeys))
                    {
                        matchPlatform = true;
                        break;
                    }
                }
                if (matchPlatform)
                {
                    output.push_back(recognizer);
                }
                foundAny = true;
            }
        }
        return foundAny;
    }

    bool InternalRecognizerBasedBuilder::IsFileExcluded(const QString& fileName) const
    {
        for (const ExcludeAssetRecognizer& excludeRecognizer : m_excludeAssetRecognizers)
        {
            if (excludeRecognizer.m_patternMatcher.MatchesPath(fileName))
            {
                return true;
            }
        }
        return false;
    }

    AZ::Uuid InternalRecognizerBasedBuilder::GetUuid() const
    {
        return this->m_builderUuid;
    }

    const AZStd::string& InternalRecognizerBasedBuilder::GetBuilderName() const
    {
        return this->m_name;
    }

    AZ::Uuid InternalRCBuilder::GetUUID()
    {
        return AZ::Uuid::CreateString("0BBFC8C1-9137-4404-BD94-64C0364EFBFB");
    }

    const char* InternalRCBuilder::GetName()
    {
        return "Internal RC Builder";
    }

    // The filter callback for RC Recognizer Platforms
    InternalAssetRecognizer::PlatformFilter InternalRCBuilder::s_platformFilter =
        [](const AssetPlatformSpec& srcPlatSpec)
        {
            // Internal RC Builders are RC rules that do NOT use 'copy' as the param
            return (QString::compare(srcPlatSpec.m_extraRCParams, "copy",
                        Qt::CaseInsensitive) != 0);
        };

    InternalRCBuilder::InternalRCBuilder()
        : InternalRecognizerBasedBuilder(
            GetName(),
            GetUUID(),
            InternalRCBuilder::s_platformFilter)
        , m_legacyRCCompiler()
    {
    }

    InternalRCBuilder::~InternalRCBuilder()
    {
    }

    bool InternalRCBuilder::Initialize(const RecognizerConfiguration& recognizerConfig)
    {
        if (!InternalRecognizerBasedBuilder::Initialize(recognizerConfig))
        {
            return false;
        }

        QDir systemRoot;
        bool computeRootResult = AssetUtilities::ComputeEngineRoot(systemRoot);
        AZ_Assert(computeRootResult, "AssetUtilities::ComputeEngineRoot failed");

        QString rcExecutableFullPath = QCoreApplication::applicationDirPath() + "/rc/rc.exe";

        if (!m_legacyRCCompiler.Initialize(systemRoot.canonicalPath(), rcExecutableFullPath))
        {
            AssetBuilderSDK::BuilderLog(InternalRCBuilder::GetUUID(), "Unable to find rc.exe from the engine root (%1).", rcExecutableFullPath.toUtf8().data());
            false;
        }
        return true;
    }

    void InternalRCBuilder::CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response)
    {
        if (m_isShuttingDown)
        {
            response.m_result = AssetBuilderSDK::CreateJobsResultCode::ShuttingDown;
            return;
        }
        response.m_result = AssetBuilderSDK::CreateJobsResultCode::Failed;
        AZStd::string fullPath;
        if (!AzFramework::StringFunc::Path::ConstructFull(request.m_watchFolder.c_str(), request.m_sourceFile.c_str(), fullPath, true))
        {
            AssetBuilderSDK::BuilderLog(InternalRCBuilder::GetUUID(), "Cannot construct a full path from folder '%s' and file '%s'", request.m_watchFolder.c_str(), request.m_sourceFile.c_str());
            return;
        }

        QString normalizedPath = QString(fullPath.c_str());

        // Locate recognizers that match the file
        InternalRecognizerPointerContainer  recognizers;
        if (!InternalRCBuilder::GetMatchingRecognizers(request.m_platformFlags, request.m_sourceFile.c_str(), recognizers))
        {
            AssetBuilderSDK::BuilderLog(InternalRCBuilder::GetUUID(), "Cannot find recognizer for %s.", request.m_sourceFile.c_str());
            return;
        }

        // First pass
        for (const InternalAssetRecognizer* recognizer : recognizers)
        {
            // Iterate through the platform specific specs and apply the ones that match the platform flag
            for (auto iterPlatformSpec = recognizer->m_platformSpecsByPlatform.cbegin();
                 iterPlatformSpec != recognizer->m_platformSpecsByPlatform.cend();
                 iterPlatformSpec++)
            {
                if (iterPlatformSpec.key() & request.m_platformFlags)
                {
                    AssetBuilderSDK::JobDescriptor descriptor;
                    descriptor.m_jobKey = recognizer->m_name.toUtf8().data();
                    descriptor.m_platform = iterPlatformSpec.key();
                    descriptor.m_priority = recognizer->m_priority;
                    descriptor.m_checkExclusiveLock = recognizer->m_testLockSource;

                    QString extraInformationForFingerprinting;
                    extraInformationForFingerprinting.append(iterPlatformSpec->m_extraRCParams);
                    extraInformationForFingerprinting.append(recognizer->m_version);
                    descriptor.m_additionalFingerprintInfo = AZStd::string(extraInformationForFingerprinting.toUtf8().data());

                    // Job Parameter Value can be any arbitrary string since we are relying on the key to lookup
                    // the parameter in the process job
                    descriptor.m_jobParameters[recognizer->m_paramID] = descriptor.m_jobKey;

                    response.m_createJobOutputs.push_back(descriptor);
                    response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
                }
            }
        }
    }

    AssetBuilderSDK::AssetBuilderDesc InternalRCBuilder::CreateBuilderDesc(const AZStd::vector<AssetBuilderSDK::AssetBuilderPattern>& builderPatterns)
    {
        AssetBuilderSDK::AssetBuilderDesc   builderDesc;
        builderDesc.m_name = this->m_name;
        builderDesc.m_patterns = builderPatterns;
        builderDesc.m_busId = this->m_builderUuid;
        builderDesc.m_createJobFunction = AZStd::bind(&InternalRCBuilder::CreateJobs, this, AZStd::placeholders::_1, AZStd::placeholders::_2);
        builderDesc.m_processJobFunction = AZStd::bind(&InternalRCBuilder::ProcessJob, this, AZStd::placeholders::_1, AZStd::placeholders::_2);
        return builderDesc;
    }

    void InternalRCBuilder::ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response)
    {
        if (m_isShuttingDown)
        {
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            return;
        }
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;

        if (request.m_jobDescription.m_jobParameters.empty())
        {
            AZ_TracePrintf(AssetProcessor::ConsoleChannel,
                "Job request for for %s in builder %s missing job parameters.",
                request.m_sourceFile.c_str(),
                this->GetBuilderName().c_str());
            return;
        }

        for (auto jobParam = request.m_jobDescription.m_jobParameters.begin();
             jobParam != request.m_jobDescription.m_jobParameters.end();
             jobParam++)
        {
            if (this->m_assetRecognizerDictionary.find(jobParam->first) == this->m_assetRecognizerDictionary.end())
            {
                AZ_TracePrintf(AssetProcessor::ConsoleChannel,
                    "Job request for for %s in builder %s has invalid job parameter (%ld).",
                    request.m_sourceFile.c_str(),
                    this->GetBuilderName().c_str(),
                    jobParam->first);
                continue;
            }
            InternalAssetRecognizer* assetRecognizer = this->m_assetRecognizerDictionary[jobParam->first];
            if (!assetRecognizer->m_platformSpecsByPlatform.contains(request.m_jobDescription.m_platform))
            {
                // Skip due to platform restrictions
                continue;
            }

            // Process this job
            QString inputFile = QString(request.m_fullPath.c_str());
            QString platform = AssetUtilities::ComputePlatformName(request.m_jobDescription.m_platform);
            QString rcParam = assetRecognizer->m_platformSpecsByPlatform[request.m_jobDescription.m_platform].m_extraRCParams;
            QString dest = request.m_tempDirPath.c_str();
            NativeLegacyRCCompiler::Result  rcResult;

            if ((!this->m_legacyRCCompiler.Execute(inputFile, platform, rcParam, dest, rcResult))||(rcResult.m_exitCode !=  0))
            {
                AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Job ID %d Failed with exit code %d", AssetProcessor::GetThreadLocalJobId(), rcResult.m_exitCode);
                response.m_resultCode = rcResult.m_crashed ? AssetBuilderSDK::ProcessJobResult_Crashed : AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }

            // Get all of the files from the dest folder
            QDir            workingDir(dest);
            QFileInfoList   originalFiles(workingDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files));
            QFileInfoList   filteredFiles;

            // Filter out the log files and add to the result products
            for (const auto& file : originalFiles)
            {
                QString outputFilename = file.fileName();
                if (MatchTempFileToSkip(outputFilename))
                {
                    continue;
                }
                response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(AZStd::string(file.absoluteFilePath().toUtf8().data())));
            }
            // its fine for RC to decide there are no outputs.  The only factor is what its exit code is.
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        }
    }

    void InternalRCBuilder::ShutDown()
    {
        InternalRecognizerBasedBuilder::ShutDown();
        this->m_legacyRCCompiler.RequestQuit();
    }

    bool InternalRCBuilder::MatchTempFileToSkip(const QString& outputFilename)
    {
        // List of specific files to skip
        static const char* s_fileNamesToSkip[] = {
            "rc_createdfiles.txt",
            "rc_log.log",
            "rc_log_warnings.log",
            "rc_log_errors.log"
        };

        for (const char* filenameToSkip : s_fileNamesToSkip)
        {
            if (QString::compare(outputFilename, filenameToSkip, Qt::CaseInsensitive) == 0)
            {
                return true;
            }
        }

        // List of specific file name patters to skip
        static const QString s_filePatternsToSkip[] = {
            QString(".*\\.\\$.*"),
            QString("log.*\\.txt")
        };

        for (const QString& patternsToSkip : s_filePatternsToSkip)
        {
            QRegExp skipRegex(patternsToSkip, Qt::CaseInsensitive, QRegExp::RegExp);
            if (skipRegex.exactMatch(outputFilename))
            {
                return true;
            }
        }

        return false;
    }


    AZ::Uuid InternalCopyBuilder::GetUUID()
    {
        return AZ::Uuid::CreateString("31B74BFD-7046-47AC-A7DA-7D5167E9B2F8");
    }

    const char* InternalCopyBuilder::GetName()
    {
        return "Internal Copy Builder";
    }

    // The filter callback for RC Recognizer Platforms
    InternalAssetRecognizer::PlatformFilter InternalCopyBuilder::s_platformFilter =
        [](const AssetPlatformSpec& srcPlatSpec)
        {
            // Internal RC Builders are RC rules that do use 'copy' as the param
            return (QString::compare(srcPlatSpec.m_extraRCParams, "copy",
                        Qt::CaseInsensitive) == 0);
        };

    InternalCopyBuilder::InternalCopyBuilder()
        : InternalRecognizerBasedBuilder(
            GetName(),
            GetUUID(),
            InternalCopyBuilder::s_platformFilter)
    {
    }

    InternalCopyBuilder::~InternalCopyBuilder()
    {
    }

    AssetBuilderSDK::AssetBuilderDesc InternalCopyBuilder::CreateBuilderDesc(const AZStd::vector<AssetBuilderSDK::AssetBuilderPattern>& builderPatterns)
    {
        AssetBuilderSDK::AssetBuilderDesc   builderDesc;

        builderDesc.m_name = this->m_name;
        builderDesc.m_patterns = builderPatterns;
        builderDesc.m_busId = this->m_builderUuid;
        builderDesc.m_createJobFunction = AZStd::bind(&InternalCopyBuilder::CreateJobs, this, AZStd::placeholders::_1, AZStd::placeholders::_2);
        builderDesc.m_processJobFunction = AZStd::bind(&InternalCopyBuilder::ProcessJob, this, AZStd::placeholders::_1, AZStd::placeholders::_2);
        return builderDesc;
    }

    void InternalCopyBuilder::CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response)
    {
        if (m_isShuttingDown)
        {
            response.m_result = AssetBuilderSDK::CreateJobsResultCode::ShuttingDown;
            return;
        }

        response.m_result = AssetBuilderSDK::CreateJobsResultCode::Failed;

        AZStd::string fullPath;
        if (!AzFramework::StringFunc::Path::ConstructFull(request.m_watchFolder.c_str(), request.m_sourceFile.c_str(), fullPath, true))
        {
            AssetBuilderSDK::BuilderLog(GetUUID(), "Cannot construct a full path from folder '%s' and file '%s'", request.m_watchFolder.c_str(), request.m_sourceFile.c_str());
            return;
        }

        // first, is it an input or is it in the cache folder?
        QString normalizedPath = QString(fullPath.c_str());
        QString scanFolderName = QString(request.m_watchFolder.c_str());
        QString relativePathToFile = QString(request.m_sourceFile.c_str());


        // Locate recognizers that match the file
        InternalRecognizerPointerContainer  recognizers;
        if (!GetMatchingRecognizers(request.m_platformFlags, request.m_sourceFile.c_str(), recognizers))
        {
            AssetBuilderSDK::BuilderLog(GetUUID(), "Cannot find recognizer for %s.", request.m_sourceFile.c_str());
            return;
        }

        // First pass
        for (const InternalAssetRecognizer* recognizer : recognizers)
        {
            // Iterate through the platform specific specs and apply the ones that match the platform flag
            for (auto iterPlatformSpec = recognizer->m_platformSpecsByPlatform.cbegin();
                 iterPlatformSpec != recognizer->m_platformSpecsByPlatform.cend();
                 iterPlatformSpec++)
            {
                if (iterPlatformSpec.key() & request.m_platformFlags)
                {
                    AssetBuilderSDK::JobDescriptor descriptor;
                    descriptor.m_jobKey = recognizer->m_name.toUtf8().data();
                    descriptor.m_platform = iterPlatformSpec.key();
                    descriptor.m_critical = true;   // All copy jobs are critical
                    descriptor.m_checkExclusiveLock = recognizer->m_testLockSource;

                    QString extraInformationForFingerprinting;
                    extraInformationForFingerprinting.append(iterPlatformSpec->m_extraRCParams);
                    extraInformationForFingerprinting.append(recognizer->m_version);
                    descriptor.m_additionalFingerprintInfo = AZStd::string(extraInformationForFingerprinting.toUtf8().data());

                    // Job Parameter Value can be any arbitrary string since we are relying on the key to lookup
                    // the parameter in the process job
                    descriptor.m_jobParameters[recognizer->m_paramID] = descriptor.m_jobKey;
                    response.m_createJobOutputs.push_back(descriptor);
                    response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
                }
            }
        }
    }

    void InternalCopyBuilder::ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response)
    {
        if (m_isShuttingDown)
        {
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            return;
        }

        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;

        if (request.m_jobDescription.m_jobParameters.empty())
        {
            AZ_TracePrintf(AssetProcessor::ConsoleChannel,
                "Job request for for %s in builder %s missing job parameters.",
                request.m_sourceFile.c_str(),
                this->GetBuilderName().c_str());
            return;
        }

        for (auto jobParam = request.m_jobDescription.m_jobParameters.begin();
            jobParam != request.m_jobDescription.m_jobParameters.end();
             jobParam++)
        {
            if (this->m_assetRecognizerDictionary.find(jobParam->first) == this->m_assetRecognizerDictionary.end())
            {
                AZ_TracePrintf(AssetProcessor::ConsoleChannel,
                    "Job request for for %s in builder %s has invalid job parameter (%ld).",
                    request.m_sourceFile.c_str(),
                    this->GetBuilderName().c_str(),
                    jobParam->first);
                continue;
            }
            InternalAssetRecognizer* assetRecognizer = this->m_assetRecognizerDictionary[jobParam->first];

            if (assetRecognizer->m_platformSpecsByPlatform.contains(request.m_jobDescription.m_platform))
            {
                QString platformParam = (assetRecognizer->m_platformSpecsByPlatform[request.m_jobDescription.m_platform].m_extraRCParams);
                AZ_Assert((QString::compare(platformParam, "copy", Qt::CaseInsensitive) == 0), "Illegal param for copy task recognizer %s", assetRecognizer->m_name.toUtf8().data());
                response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(request.m_fullPath));
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
            }
        }
    }
}

