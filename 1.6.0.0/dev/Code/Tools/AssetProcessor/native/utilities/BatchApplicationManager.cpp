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
#include "BatchApplicationManager.h"

#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/smart_ptr/make_shared.h>

#include "native/utilities/assetUtils.h"
#include "native/AssetManager/assetProcessorManager.h"
#include "native/utilities/PlatformConfiguration.h"
#include "native/FileWatcher/FileWatcherAPI.h"
#include "native/AssetManager/assetScanFolderInfo.h"
#include "native/resourcecompiler/rccontroller.h"
#include "native/AssetManager/assetScanner.h"
#include <AzFramework/Asset/AssetProcessorMessages.h>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>

#include <QTextStream>
#include <QCoreApplication>

#include <AzToolsFramework/Asset/AssetProcessorMessages.h>

#if defined(AZ_PLATFORM_WINDOWS) && defined(BATCH_MODE)
namespace BatchApplicationManagerPrivate
{
    BatchApplicationManager* g_appManager = nullptr;
    BOOL WINAPI CtrlHandlerRoutine(DWORD dwCtrlType)
    {
        (void)dwCtrlType;
        AZ_Printf("AssetProcessor", "Asset Processor Batch Processing Interrupted. Quitting.\n");
        QMetaObject::invokeMethod(g_appManager, "QuitRequested", Qt::QueuedConnection);
        return TRUE;
    }
}
#endif  //#if defined(AZ_PLATFORM_WINDOWS) && defined(BATCH_MODE)

#ifdef UNIT_TEST
#include "native/connection/connectionManager.h"
#include "native/utilities/UnitTests.h"
#endif

BatchApplicationManager::BatchApplicationManager(int argc, char** argv, QObject* parent)
    : ApplicationManager(argc, argv, parent)
    , m_currentExternalAssetBuilder(nullptr)
{
}


BatchApplicationManager::~BatchApplicationManager()
{

    for (const AZStd::shared_ptr<AssetProcessor::InternalRecognizerBasedBuilder> internalBuilder : m_internalBuilders)
    {
        internalBuilder->UnInitialize();
    }

    for (AssetProcessor::ExternalModuleAssetBuilderInfo* externalAssetBuilderInfo : this->m_externalAssetBuilders)
    {
        externalAssetBuilderInfo->UnInitialize();
        delete externalAssetBuilderInfo;
    }

    Destroy();
}

AssetProcessor::RCController* BatchApplicationManager::GetRCController() const
{
    return m_rcController;
}

int BatchApplicationManager::ProcessedAssetCount() const
{
    return m_processedAssetCount;
}
int BatchApplicationManager::FailedAssetsCount() const
{
    return m_failedAssetsCount;
}

void BatchApplicationManager::ResetProcessedAssetCount()
{
    m_processedAssetCount = 0;
}

void BatchApplicationManager::ResetFailedAssetCount()
{
    m_failedAssetsCount = 0;
}


AssetProcessor::AssetScanner* BatchApplicationManager::GetAssetScanner() const
{
    return m_assetScanner;
}

AssetProcessor::AssetProcessorManager* BatchApplicationManager::GetAssetProcessorManager() const
{
    return m_assetProcessorManager;
}

AssetProcessor::PlatformConfiguration* BatchApplicationManager::GetPlatformConfiguration() const
{
    return m_platformConfiguration;
}

void BatchApplicationManager::InitAssetProcessorManager()
{
    AssetProcessor::ThreadController<AssetProcessor::AssetProcessorManager>* assetProcessorHelper = new AssetProcessor::ThreadController<AssetProcessor::AssetProcessorManager>();

    addRunningThread(assetProcessorHelper);
    m_assetProcessorManager = assetProcessorHelper->initialize(
            [this, &assetProcessorHelper]()
    {
        return new AssetProcessor::AssetProcessorManager(m_platformConfiguration, assetProcessorHelper);
    });
}

void BatchApplicationManager::InitAssetCatalog()
{
    using namespace AssetProcessor;
    ThreadController<AssetCatalog>* assetCatalogHelper = new ThreadController<AssetCatalog>();

    addRunningThread(assetCatalogHelper);
    m_assetCatalog = assetCatalogHelper->initialize(
            [this, &assetCatalogHelper]()
    {
        QStringList platforms;
        for (int idx = 0, end = m_platformConfiguration->PlatformCount(); idx < end; ++idx)
        {
            platforms.push_back(m_platformConfiguration->PlatformAt(idx));
        }

        AssetProcessor::AssetCatalog* catalog = new AssetCatalog(assetCatalogHelper, platforms, m_assetProcessorManager->GetDatabaseConnection());
        connect(m_assetProcessorManager, &AssetProcessorManager::ProductChanged, catalog, &AssetCatalog::OnProductChanged);
        connect(m_assetProcessorManager, &AssetProcessorManager::ProductRemoved, catalog, &AssetCatalog::OnProductRemoved);
        return catalog;
    });
}

void BatchApplicationManager::InitRCController()
{
    m_rcController = new AssetProcessor::RCController(m_platformConfiguration->GetMinJobs(), m_platformConfiguration->GetMaxJobs());

    QObject::connect(m_assetProcessorManager, &AssetProcessor::AssetProcessorManager::AssetToProcess, m_rcController, &AssetProcessor::RCController::jobSubmitted);
    QObject::connect(m_rcController, &AssetProcessor::RCController::fileCompiled, m_assetProcessorManager, &AssetProcessor::AssetProcessorManager::AssetProcessed, Qt::UniqueConnection);
    QObject::connect(m_rcController, &AssetProcessor::RCController::fileFailed, m_assetProcessorManager, &AssetProcessor::AssetProcessorManager::AssetFailed);
}

void BatchApplicationManager::DestroyRCController()
{
    if (m_rcController)
    {
        delete m_rcController;
        m_rcController = nullptr;
    }
}

void BatchApplicationManager::InitAssetScanner()
{
    m_assetScanner = new AssetProcessor::AssetScanner(m_platformConfiguration);
    QObject::connect(m_assetScanner, SIGNAL(AssetScanningStatusChanged(AssetProcessor::AssetScanningStatus)), m_assetProcessorManager, SLOT(OnAssetScannerStatusChange(AssetProcessor::AssetScanningStatus)));
    QObject::connect(m_assetScanner, SIGNAL(FileOfInterestFound(QString)), m_assetProcessorManager, SLOT(AssessModifiedFile(QString)));
}

void BatchApplicationManager::DestroyAssetScanner()
{
    if (m_assetScanner)
    {
        delete m_assetScanner;
        m_assetScanner = nullptr;
    }
}

void BatchApplicationManager::InitPlatformConfiguration()
{
    m_platformConfiguration = new AssetProcessor::PlatformConfiguration();
    QStringList configFiles = {
        GetSystemRoot().absoluteFilePath("AssetProcessorPlatformConfig.ini"),
        GetSystemRoot().absoluteFilePath(GetGameName() + "/AssetProcessorGamePlatformConfig.ini")
    };

    // Read platforms first
    for (const QString& configFile : configFiles)
    {
        m_platformConfiguration->ReadPlatformsFromConfigFile(configFile);
    }

    // Then read recognizers (which depend on platforms)
    for (const QString& configFile : configFiles)
    {
        m_platformConfiguration->ReadRecognizersFromConfigFile(configFile);
    }

    // Then read metadata (which depends on scan folders)
    for (const QString& configFile : configFiles)
    {
        m_platformConfiguration->ReadMetaDataFromConfigFile(configFile);
    }

    // Read in Gems for the active game
    m_platformConfiguration->ReadGemsConfigFile(GetSystemRoot().absoluteFilePath(GetGameName() + "/gems.json"));
}

void BatchApplicationManager::DestroyPlatformConfiguration()
{
    if (m_platformConfiguration)
    {
        delete m_platformConfiguration;
        m_platformConfiguration = nullptr;
    }
}

void BatchApplicationManager::InitFileMonitor()
{
    m_folderWatches.reserve(m_platformConfiguration->GetScanFolderCount());
    m_watchHandles.reserve(m_platformConfiguration->GetScanFolderCount());
    for (int folderIdx = 0; folderIdx < m_platformConfiguration->GetScanFolderCount(); ++folderIdx)
    {
        const AssetProcessor::ScanFolderInfo& info = m_platformConfiguration->GetScanFolderAt(folderIdx);

        FolderWatchCallbackEx* newFolderWatch = new FolderWatchCallbackEx(info.ScanPath(), "", info.RecurseSubFolders());
        // hook folder watcher to assess files on add/modify
        // relevant files will be sent to resource compiler
        QObject::connect(newFolderWatch, &FolderWatchCallbackEx::fileAdded, m_assetProcessorManager, &AssetProcessor::AssetProcessorManager::AssessAddedFile);
        QObject::connect(newFolderWatch, &FolderWatchCallbackEx::fileModified, m_assetProcessorManager, &AssetProcessor::AssetProcessorManager::AssessModifiedFile);
        QObject::connect(newFolderWatch, &FolderWatchCallbackEx::fileRemoved, m_assetProcessorManager, &AssetProcessor::AssetProcessorManager::AssessDeletedFile);

        m_folderWatches.push_back(AZStd::unique_ptr<FolderWatchCallbackEx>(newFolderWatch));
        m_watchHandles.push_back(m_fileWatcher.AddFolderWatch(newFolderWatch));
    }

    // also hookup monitoring for the cache (output directory)
    QDir cacheRoot;
    if (AssetUtilities::ComputeProjectCacheRoot(cacheRoot))
    {
        FolderWatchCallbackEx* newFolderWatch = new FolderWatchCallbackEx(cacheRoot.absolutePath(), "", true);

        // we only care about cache root deletions.
        QObject::connect(newFolderWatch, &FolderWatchCallbackEx::fileRemoved, m_assetProcessorManager, &AssetProcessor::AssetProcessorManager::AssessDeletedFile);

        m_folderWatches.push_back(AZStd::unique_ptr<FolderWatchCallbackEx>(newFolderWatch));
        m_watchHandles.push_back(m_fileWatcher.AddFolderWatch(newFolderWatch));
    }
}

void BatchApplicationManager::DestroyFileMonitor()
{
    for (int watchHandle : m_watchHandles)
    {
        m_fileWatcher.RemoveFolderWatch(watchHandle);
    }
    m_folderWatches.resize(0);
}


ApplicationManager::BeforeRunStatus BatchApplicationManager::BeforeRun()
{
    ApplicationManager::BeforeRun();

    //Register all QMetatypes here
    qRegisterMetaType<AzFramework::AssetSystem::AssetStatus>("AzFramework::AssetSystem::AssetStatus");
    qRegisterMetaType<AzFramework::AssetSystem::AssetStatus>("AssetStatus");

    qRegisterMetaType<FileChangeInfo>("FileChangeInfo");

    qRegisterMetaType<AssetProcessor::AssetScanningStatus>("AssetScanningStatus");

    qRegisterMetaType<AssetProcessor::NetworkRequestID>("NetworkRequestID");

    qRegisterMetaType<AssetProcessor::JobEntry>("JobEntry");

    qRegisterMetaType<AssetBuilderSDK::ProcessJobResponse>("ProcessJobResponse");

    qRegisterMetaType<AzToolsFramework::AssetSystem::JobStatus>("AzToolsFramework::AssetSystem::JobStatus");
    qRegisterMetaType<AzToolsFramework::AssetSystem::JobStatus>("JobStatus");

    qRegisterMetaType<AssetProcessor::JobDetails>("JobDetails");
    qRegisterMetaType<AssetProcessor::ProductInfo>("ProductInfo");
    qRegisterMetaType<AssetProcessor::ProductInfo>("AssetProcessor::ProductInfo");

    qRegisterMetaType<AzToolsFramework::AssetSystem::AssetJobLogRequest>("AzToolsFramework::AssetSystem::AssetJobLogRequest");
    qRegisterMetaType<AzToolsFramework::AssetSystem::AssetJobLogRequest>("AssetJobLogRequest");

    qRegisterMetaType<AzToolsFramework::AssetSystem::AssetJobLogResponse>("AzToolsFramework::AssetSystem::AssetJobLogResponse");
    qRegisterMetaType<AzToolsFramework::AssetSystem::AssetJobLogResponse>("AssetJobLogResponse");

    qRegisterMetaType<AzFramework::AssetSystem::BaseAssetProcessorMessage*>("AzFramework::AssetSystem::BaseAssetProcessorMessage*");
    qRegisterMetaType<AzFramework::AssetSystem::BaseAssetProcessorMessage*>("BaseAssetProcessorMessage*");

    AssetBuilderSDK::AssetBuilderBus::Handler::BusConnect();
    AssetProcessor::AssetBuilderRegistrationBus::Handler::BusConnect();
    AssetProcessor::AssetBuilderInfoBus::Handler::BusConnect();

    return ApplicationManager::BeforeRunStatus::Status_Success;
}

void BatchApplicationManager::Destroy()
{
#if defined(AZ_PLATFORM_WINDOWS) && defined(BATCH_MODE)
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)BatchApplicationManagerPrivate::CtrlHandlerRoutine, FALSE);
    BatchApplicationManagerPrivate::g_appManager = nullptr;
#endif //#if defined(AZ_PLATFORM_WINDOWS) && defined(BATCH_MODE)
#if !FORCE_PROXY_MODE
    DestroyRCController();
    DestroyAssetScanner();
    DestroyFileMonitor();
#endif
    DestroyPlatformConfiguration();
}

bool BatchApplicationManager::Run()
{
    QStringList args = QCoreApplication::arguments();
#ifdef UNIT_TEST
    for (QString arg : args)
    {
        bool runTests = false;

        if (arg.startsWith("/unittest", Qt::CaseInsensitive))
        {
            runTests = true;
        }
        else if (arg.startsWith("--unittest", Qt::CaseInsensitive))
        {
            runTests = true;
            AZ_TracePrintf(AssetProcessor::ConsoleChannel, "the --unittest command line parameter has been deprecated.  please use /unittest[=TestName,TestName,TestName...] instead.");
        }

        if (runTests)
        {
            // Before running the unit tests we are disconnecting from the AssetBuilderInfo bus as we will be making a dummy handler in the unit test
            AssetProcessor::AssetBuilderInfoBus::Handler::BusDisconnect();
            return RunUnitTests();
        }
    }
#endif
    if (!Activate())
    {
        return false;
    }
    AZ_Printf(AssetProcessor::ConsoleChannel, "Asset Processor Batch Processing Started.\n");
    qApp->exec();

    AZ_Printf(AssetProcessor::ConsoleChannel, "Number of Assets Successfully Processed: %d.\n", ProcessedAssetCount());
    AZ_Printf(AssetProcessor::ConsoleChannel, "Number of Assets Failed to Process: %d.\n", FailedAssetsCount());
    AZ_Printf(AssetProcessor::ConsoleChannel, "Asset Processor Batch Processing Completed.\n");

    Destroy();

    if (FailedAssetsCount() == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void BatchApplicationManager::CheckForIdle()
{
    if ((m_rcController->IsIdle()) && (m_assetProcessorManager->IsIdle()))
    {
        QuitRequested();
    }
}

#ifdef UNIT_TEST
bool BatchApplicationManager::RunUnitTests()
{
    if (!ApplicationManager::Activate())
    {
        return false;
    }
    AssetProcessor::PlatformConfiguration config;
    ConnectionManager connectionManager(&config);
    RegisterObjectForQuit(&connectionManager);
    UnitTestWorker unitTestWorker;
    int testResult = 1;
    QObject::connect(&unitTestWorker, &UnitTestWorker::UnitTestCompleted, [&](int result)
    {
        testResult = result;
        QuitRequested();
    });

    unitTestWorker.Process();


    qApp->exec();

    if (!testResult)
    {
        AZ_TracePrintf(AssetProcessor::ConsoleChannel, "All Unit Tests passed.\n");
    }
    else
    {
        AZ_TracePrintf(AssetProcessor::ConsoleChannel, "WARNING: Unit Tests Failed.\n");
    }
    return !testResult ? true : false;
}
#endif

// IMPLEMENTATION OF -------------- AzToolsFramework::AssetDatabase::AssetDatabaseRequests::Bus::Listener
void BatchApplicationManager::GetDatabaseLocation(AZStd::string& result)
{
    QDir cacheRoot;
    if (!AssetUtilities::ComputeProjectCacheRoot(cacheRoot))
    {
        result = "assetdb.sqlite";
    }

    result = cacheRoot.absoluteFilePath("assetdb.sqlite").toUtf8().data();
}

// ------------------------------------------------------------

bool BatchApplicationManager::Activate()
{
    // since we're not doing unit tests, we can go ahead and let bus queries know where the real DB is.
    AzToolsFramework::AssetDatabase::AssetDatabaseRequests::Bus::Handler::BusConnect();

    if (!ApplicationManager::Activate())
    {
        return false;
    }

    InitPlatformConfiguration();
    InitAssetProcessorManager();
#if !FORCE_PROXY_MODE
    InitializeInternalBuilders();
#endif
    if (!InitializeExternalBuilders())
    {
        AZ_Error("AssetProcessor", false, "AssetProcessor is closing. Failed to initialize and load all the external builders. Please see log for more information.\n");
        return false;
    }

#if !FORCE_PROXY_MODE
    InitAssetCatalog();
    InitFileMonitor();
    InitAssetScanner();
    InitRCController();
#endif

    //We must register all objects that need to be notified if we are shutting down before we install the ctrlhandler
    RegisterObjectForQuit(m_assetProcessorManager);
#if !defined(FORCE_PROXY_MODE)
    RegisterObjectForQuit(m_rcController);
#endif

#if defined(AZ_PLATFORM_WINDOWS) && defined(BATCH_MODE)
    BatchApplicationManagerPrivate::g_appManager = this;
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)BatchApplicationManagerPrivate::CtrlHandlerRoutine, TRUE);
#endif //defined(AZ_PLATFORM_WINDOWS) && defined(BATCH_MODE)

#if defined(BATCH_MODE)
    QObject::connect(m_rcController, &AssetProcessor::RCController::fileCompiled, m_assetProcessorManager, [this](AssetProcessor::JobEntry /*entry*/, AssetBuilderSDK::ProcessJobResponse /*response*/)
    {
        m_processedAssetCount++;
    });

    QObject::connect(m_rcController, &AssetProcessor::RCController::JobStarted, m_assetProcessorManager, [this](QString inputFile, QString platform)
    {
        QString msg = QCoreApplication::translate("Batch Mode", "Processing %1 (%2)...\n", "%1 is the name of the file, and %2 is the platform to process it for").arg(inputFile, platform);
        AZ_Printf(AssetProcessor::ConsoleChannel, msg.toUtf8().constData());
    });
    QObject::connect(m_rcController, &AssetProcessor::RCController::fileFailed, m_assetProcessorManager, [this](AssetProcessor::JobEntry entry)
    {
        m_failedAssetsCount++;

        QString msg = QCoreApplication::translate("Batch Mode", "[ERROR] - processing failed on %1 (%2)!\n", "%1 is the name of the file, and %2 is the platform to process it for").arg(entry.m_absolutePathToFile).arg(entry.m_platform);
        AZ_Printf(AssetProcessor::ConsoleChannel, msg.toUtf8().constData());
    });

    AZ_Printf(AssetProcessor::ConsoleChannel, QCoreApplication::translate("Batch Mode", "Starting to scan for changes...\n").toUtf8().constData());

    connect(m_assetProcessorManager, &AssetProcessor::AssetProcessorManager::OnBecameIdle, this, &BatchApplicationManager::CheckForIdle);
    connect(m_rcController, &AssetProcessor::RCController::BecameIdle, this, &BatchApplicationManager::CheckForIdle);

    QObject::connect(m_assetScanner, &AssetProcessor::AssetScanner::AssetScanningStatusChanged, [this](AssetProcessor::AssetScanningStatus status)
    {
        if ((status == AssetProcessor::AssetScanningStatus::Completed) || (status == AssetProcessor::AssetScanningStatus::Stopped))
        {
            AZ_Printf(AssetProcessor::ConsoleChannel, QCoreApplication::translate("Batch Mode", "Checking changes\n").toUtf8().constData());
            CheckForIdle();
        }
    });

    // only start AFTER connecting

    m_assetScanner->StartScan();


#endif
    return true;
}

void BatchApplicationManager::CreateQtApplication()
{
    m_qApp = new QCoreApplication(m_argc, m_argv);
}

void BatchApplicationManager::InitializeInternalBuilders()
{
    m_internalBuilders.push_back(AZStd::make_shared<AssetProcessor::InternalRCBuilder>());
    m_internalBuilders.push_back(AZStd::make_shared<AssetProcessor::InternalCopyBuilder>());

    for (const AZStd::shared_ptr<AssetProcessor::InternalRecognizerBasedBuilder> internalBuilder : m_internalBuilders)
    {
        internalBuilder->Initialize(*this->m_platformConfiguration);
    }
}

bool BatchApplicationManager::InitializeExternalBuilders()
{
    QString dirPath;
    QString fileName;
    AssetUtilities::ComputeApplicationInformation(dirPath, fileName);
    QDir directory(dirPath);
    QString builderDirPath = directory.filePath("Builders");
    QDir builderDir(builderDirPath);
    QDir tempBuilderDir(directory.filePath("Builders_Temp"));
    if (!AssetUtilities::CopyDirectory(builderDir, tempBuilderDir))
    {
        AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Failed to copy the builder directory %s to the destination directory %s.\n", builderDirPath.toUtf8().data(), tempBuilderDir.absolutePath().toUtf8().data());
        return false;
    }
    QStringList filter;
#if defined(AZ_PLATFORM_WINDOWS)
    filter.append("*.dll");
#elif defined(AZ_PLATFORM_LINUX)
    filter.append("*.so");
#elif defined(AZ_PLATFORM_APPLE)
    filter.append("*.dylib");
#endif
    QString exePath;
    QVector<AssetProcessor::ExternalModuleAssetBuilderInfo*>  externalBuilderList;

    QStringList fileList = tempBuilderDir.entryList(filter, QDir::Files);
    for (QString& file : fileList)
    {
        QString filePath = tempBuilderDir.filePath(file);
        bool    externalBuilderLoaded = false;
        if (QLibrary::isLibrary(filePath))
        {
            AssetProcessor::ExternalModuleAssetBuilderInfo* externalAssetBuilderInfo = new AssetProcessor::ExternalModuleAssetBuilderInfo(filePath);
            if (!externalAssetBuilderInfo->Load())
            {
                AZ_Warning(AssetProcessor::DebugChannel, false, "AssetProcessor was not able to load the library: %s\n", filePath.toUtf8().data());
                delete externalAssetBuilderInfo;
                return false;
            }

            AZ_TracePrintf(AssetProcessor::DebugChannel, "Initializing and registering builder %s\n", externalAssetBuilderInfo->GetName().toUtf8().data());

            this->m_currentExternalAssetBuilder = externalAssetBuilderInfo;

            externalAssetBuilderInfo->Initialize();

            this->m_currentExternalAssetBuilder = nullptr;

            this->m_externalAssetBuilders.push_back(externalAssetBuilderInfo);
        }
    }

    return true;
}

void BatchApplicationManager::RegisterBuilderInformation(const AssetBuilderSDK::AssetBuilderDesc& builderDesc)
{
    // Create Job Function validation
    AZ_Error(AssetProcessor::ConsoleChannel,
        !builderDesc.m_createJobFunction.empty(),
        "Create Job Function (m_createJobFunction) for %s builder is empty.\n",
        builderDesc.m_name.c_str());

    // Process Job Function validation
    AZ_Error(AssetProcessor::ConsoleChannel,
        !builderDesc.m_processJobFunction.empty(),
        "Process Job Function (m_processJobFunction) for %s builder is empty.\n",
        builderDesc.m_name.c_str());

    // This is an external builder registering, we will want to track its builder desc since it can register multiple ones
    if (m_currentExternalAssetBuilder)
    {
        m_currentExternalAssetBuilder->RegisterBuilderDesc(builderDesc.m_busId);
    }

    if (m_builderDescMap.find(builderDesc.m_busId) != m_builderDescMap.end())
    {
        AZ_Warning(AssetProcessor::DebugChannel, false, "Uuid for %s builder is already registered.\n", builderDesc.m_name.c_str());
        return;
    }
    if (m_builderNameToId.find(builderDesc.m_name) != m_builderNameToId.end())
    {
        AZ_Warning(AssetProcessor::DebugChannel, false, "Duplicate builder detected.  A builder named '%s' is already registered.\n", builderDesc.m_name.c_str());
        return;
    }

    m_builderDescMap[builderDesc.m_busId] = builderDesc;
    m_builderNameToId[builderDesc.m_name] = builderDesc.m_busId;

    for (const AssetBuilderSDK::AssetBuilderPattern& pattern : builderDesc.m_patterns)
    {
        AssetUtilities::BuilderFilePatternMatcher patternMatcher(pattern, builderDesc.m_busId);
        m_matcherBuilderPatterns.push_back(patternMatcher);
    }
}

void BatchApplicationManager::RegisterComponentDescriptor(AZ::ComponentDescriptor* descriptor)
{
    ApplicationManager::RegisterComponentDescriptor(descriptor);
    if (m_currentExternalAssetBuilder)
    {
        m_currentExternalAssetBuilder->RegisterComponentDesc(descriptor);
    }
    else
    {
        AZ_Warning(AssetProcessor::DebugChannel, false, "Component description can only be registered during component activation.\n");
    }
}

void BatchApplicationManager::BuilderLog(const AZ::Uuid& builderId, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    BuilderLogV(builderId, message, args);
    va_end(args);
}

void BatchApplicationManager::BuilderLogV(const AZ::Uuid& builderId, const char* message, va_list list)
{
    AZStd::string builderName;

    if (m_builderDescMap.find(builderId) != m_builderDescMap.end())
    {
        const AssetBuilderSDK::AssetBuilderDesc& builderDesc = m_builderDescMap[builderId];
        char messageBuffer[1024];
        azvsnprintf(messageBuffer, 1024, message, list);
        AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Builder name : %s Message : %s.\n", builderDesc.m_name.c_str(), messageBuffer);
    }
    else
    {
        // asset processor does not know about this builder id
        AZ_TracePrintf(AssetProcessor::ConsoleChannel, "AssetProcessor does not know about the builder id: %s. \n", builderId.ToString<AZStd::string>().c_str());
    }
}

void BatchApplicationManager::UnRegisterComponentDescriptor(const AZ::ComponentDescriptor* componentDescriptor)
{
    AZ_Assert(componentDescriptor, "componentDescriptor cannot be null");
    ApplicationManager::UnRegisterComponentDescriptor(componentDescriptor);
}

void BatchApplicationManager::UnRegisterBuilderDescriptor(const AZ::Uuid& builderId)
{
    if (m_builderDescMap.find(builderId) == m_builderDescMap.end())
    {
        AZ_Warning(AssetProcessor::DebugChannel, false, "Cannot unregister builder descriptor for Uuid %s, not currently registered.\n", builderId.ToString<AZStd::string>().c_str());
        return;
    }

    // Remove from the map
    AssetBuilderSDK::AssetBuilderDesc& descToUnregister = m_builderDescMap[builderId];
    descToUnregister.m_createJobFunction.clear();
    descToUnregister.m_processJobFunction.clear();
    m_builderDescMap.erase(builderId);

    // Remove the matcher build pattern
    for (auto remover = this->m_matcherBuilderPatterns.begin();
            remover != this->m_matcherBuilderPatterns.end();
            remover++)
    {
        if (remover->GetBuilderDescID() == builderId) {
            auto deleteIter = remover;
            remover++;
            this->m_matcherBuilderPatterns.erase(deleteIter);
        }
    }
}

void BatchApplicationManager::GetMatchingBuildersInfo(const AZStd::string& assetPath, AssetProcessor::BuilderInfoList& builderInfoList)
{
    AZStd::set<AZ::Uuid>  uniqueBuilderDescIDs;

    for (AssetUtilities::BuilderFilePatternMatcher& matcherPair : m_matcherBuilderPatterns)
    {
        if (uniqueBuilderDescIDs.find(matcherPair.GetBuilderDescID()) != uniqueBuilderDescIDs.end())
        {
            continue;
        }
        if (matcherPair.MatchesPath(assetPath))
        {
            const AssetBuilderSDK::AssetBuilderDesc& builderDesc = m_builderDescMap[matcherPair.GetBuilderDescID()];
            uniqueBuilderDescIDs.insert(matcherPair.GetBuilderDescID());
            builderInfoList.push_back(builderDesc);
        }
    }
}


#include <native/utilities/BatchApplicationManager.moc>
