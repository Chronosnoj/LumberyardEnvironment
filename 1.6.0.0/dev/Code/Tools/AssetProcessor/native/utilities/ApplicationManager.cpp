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
#include "native/utilities/ApplicationManager.h"
#include "native/utilities/assetUtils.h"
#include "native/utilities/ApplicationManagerAPI.h"
#include "native/assetprocessor.h"
#include "native/resourcecompiler/RCBuilder.h"
#include "native/utilities/AssetBuilderInfo.h"
#include <AzFramework/Asset/AssetProcessorMessages.h>
#include <AzToolsFramework/Asset/AssetProcessorMessages.h>
#include <AzToolsFramework/Asset/AssetSystemComponent.h>
#include <QCoreApplication>
#include <QLocale>
#include <QFileInfo>
#include <AzCore/base.h>
#include <AzCore/Component/Entity.h>
#include <AzFramework/IO/LocalFileIO.h>
#include "native/utilities/assetUtils.h"
#include <AzFramework/Logging/LoggingComponent.h>
#include <QDir>
#include <QFileInfo>
#include <QTranslator>
#include <QByteArray>
#if defined(_WIN32)
#include <windows.h>
#endif
#include <AzCore/IO/SystemFile.h> // for AZ_MAX_PATH_LEN
#include <AzCore/Casting/lossy_cast.h>
#include <string.h> // for base  strcpy
#include <QRegExp>


namespace AssetProcessor
{
    void MessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
    {
        switch (type)
        {
        case QtDebugMsg:
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Qt-Debug: %s (%s:%u, %s)\n", msg.toUtf8().constData(), context.file, context.line, context.function);
            break;
        case QtWarningMsg:
            AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Qt-Warning: %s (%s:%u, %s)\n", msg.toUtf8().constData(), context.file, context.line, context.function);
            break;
        case QtCriticalMsg:
            AZ_Warning(AssetProcessor::ConsoleChannel, false, "Qt-Critical: %s (%s:%u, %s)\n", msg.toUtf8().constData(), context.file, context.line, context.function);
            break;
        case QtFatalMsg:
            AZ_Error(AssetProcessor::ConsoleChannel, false, "Qt-Fatal: %s (%s:%u, %s)\n", msg.toUtf8().constData(), context.file, context.line, context.function);
            abort();
        }
    }

    //! we filter the main app logs to only include non-job-thread messages:
    class FilteredLogComponent : public AzFramework::LogComponent
    {
    public:
        void OutputMessage(AzFramework::LogFile::SeverityLevel severity, const char* window, const char* message) override
        {
            if (AssetProcessor::GetThreadLocalJobId() != 0)
            {
                return; // we are in a job thread
            }

            AzFramework::LogComponent::OutputMessage(severity, window, message);
        }
    };
}

uint qHash(const AZ::Uuid& key, uint seed)
{
    (void) seed;
    return azlossy_caster(AZStd::hash<AZ::Uuid>()(key));
}

ApplicationManager::ApplicationManager(int argc, char** argv, QObject* parent)
    : QObject(parent)
    , m_argc(argc)
    , m_argv(argv)
{
    qInstallMessageHandler(&AssetProcessor::MessageHandler);
}

ApplicationManager::~ApplicationManager()
{
    for (int idx = 0; idx < m_appDependencies.size(); idx++)
    {
        delete m_appDependencies[idx];
    }

    qInstallMessageHandler(nullptr);

    //deleting QCoreApplication/QApplication
    delete m_qApp;

    if (m_entity)
    {
        //Deactivate all the components
        m_entity->Deactivate();
        delete m_entity;
        m_entity = nullptr;
    }

    //Unregistering and deleting all the components
    EBUS_EVENT_ID(azrtti_typeid<AzFramework::LogComponent>(), AZ::ComponentDescriptorBus, ReleaseDescriptor);

    //Stop AZFramework
    m_frameworkApp.Stop();
}


QDir ApplicationManager::GetSystemRoot() const
{
    return m_systemRoot;
}
QString ApplicationManager::GetGameName() const
{
    return m_gameName;
}

QCoreApplication* ApplicationManager::GetQtApplication()
{
    return m_qApp;
}

char** ApplicationManager::GetCommandArguments() const
{
    return m_argv;
}

int ApplicationManager::CommandLineArgumentsCount() const
{
    return m_argc;
}

void ApplicationManager::RegisterObjectForQuit(QObject* source)
{
    Q_ASSERT(!m_duringShutdown);

    if (m_duringShutdown)
    {
        AZ_Warning(AssetProcessor::DebugChannel, false, "You may not register objects for quit during shutdown.\n");
        return;
    }

    QuitPair quitPair(source, false);
    if (!m_objectsToNotify.contains(quitPair))
    {
        m_objectsToNotify.push_back(quitPair);
        if (!connect(source, SIGNAL(ReadyToQuit(QObject*)), this, SLOT(ReadyToQuit(QObject*))))
        {
            AZ_Warning(AssetProcessor::DebugChannel, false, "ApplicationManager::RegisterObjectForQuit was passed an object of type %s which has no ReadyToQuit(QObject*) signal.\n", source->metaObject()->className());
        }
        connect(source, SIGNAL(destroyed(QObject*)), this, SLOT(ObjectDestroyed(QObject*)));
    }
}

void ApplicationManager::ObjectDestroyed(QObject* source)
{
    for (int notifyIdx = 0; notifyIdx < m_objectsToNotify.size(); ++notifyIdx)
    {
        if (m_objectsToNotify[notifyIdx].first == source)
        {
            m_objectsToNotify.erase(m_objectsToNotify.begin() + notifyIdx);
            if (m_duringShutdown)
            {
                if (!m_queuedCheckQuit)
                {
                    QTimer::singleShot(0, this, SLOT(CheckQuit()));
                    m_queuedCheckQuit = true;
                }
            }
            return;
        }
    }
}

void ApplicationManager::QuitRequested()
{
    if (m_duringShutdown)
    {
        return;
    }
    m_duringShutdown = true;

    //Inform all the builders to shutdown
    EBUS_EVENT(AssetBuilderSDK::AssetBuilderCommandBus, ShutDown);

    // the following call will invoke on the main thread of the application, since its a direct bus call.
    EBUS_EVENT(AssetProcessor::ApplicationManagerNotifications::Bus, ApplicationShutdownRequested);

    // while it may be tempting to just collapse all of this to a bus call,  Qt Objects have the advantage of being
    // able to automatically queue calls onto their own thread, and a lot of these involved objects are in fact
    // on their own threads.  So even if we used a bus call we'd ultimately still have to invoke a queued
    // call there anyway.

    for (const QuitPair& quitter : m_objectsToNotify)
    {
        if (!quitter.second)
        {
            QMetaObject::invokeMethod(quitter.first, "QuitRequested", Qt::QueuedConnection);
        }
    }
#if !defined(BATCH_MODE)
    AZ_TracePrintf(AssetProcessor::ConsoleChannel, "App quit requested %d listeners notified.\n", m_objectsToNotify.size());
#endif
    if (!m_queuedCheckQuit)
    {
        QTimer::singleShot(0, this, SLOT(CheckQuit()));
        m_queuedCheckQuit = true;
    }
}

void ApplicationManager::CheckQuit()
{
    m_queuedCheckQuit = false;
    for (const QuitPair& quitter : m_objectsToNotify)
    {
        if (!quitter.second)
        {
#if !defined(BATCH_MODE)
            AZ_TracePrintf(AssetProcessor::ConsoleChannel, "App Quit: Object of type %d is not yet ready to quit.\n", quitter.first->metaObject()->className());
#endif
            return;
        }
    }
#if !defined(BATCH_MODE)
    AZ_TracePrintf(AssetProcessor::ConsoleChannel, "App quit requested, and all objects are ready.  Quitting app.\n");
#endif
    m_duringShutdown = false;

    // We will now loop over all the running threads and destroy them
    // Any QObjects that have these ThreadWorker as parent will also be deleted
    for (int idx = 0; idx < m_runningThreads.size(); idx++)
    {
        m_runningThreads.at(idx)->Destroy();
    }
    // all good.
    qApp->quit();
}

void ApplicationManager::CheckForUpdate()
{
    bool tryRestart = false;
    int count = 0;
    for (int idx = 0; idx < m_appDependencies.size(); ++idx)
    {
        ApplicationDependencyInfo* fileDependencyInfo = m_appDependencies[idx];
        QString fileName = fileDependencyInfo->FileName();
        QFile file(fileName);
        if (file.exists())
        {
            fileDependencyInfo->SetWasPresentEver();
            count++; // keeps track of how many files we have found

            QFileInfo fileInfo(fileName);
            QDateTime fileLastModifiedTime = fileInfo.lastModified();
            bool hasTimestampChanged = (fileDependencyInfo->Timestamp() != fileLastModifiedTime);
            if (!fileDependencyInfo->IsModified())
            {
                //if file is not reported as modified earlier
                //we check to see whether it is modified now
                if (hasTimestampChanged)
                {
                    fileDependencyInfo->SetTimestamp(fileLastModifiedTime);
                    fileDependencyInfo->SetIsModified(true);
                    fileDependencyInfo->SetStillUpdating(true);
                }
            }
            else
            {
                // if the file has been reported to have modified once
                //we check to see whether it is still changing
                if (hasTimestampChanged)
                {
                    fileDependencyInfo->SetTimestamp(fileLastModifiedTime);
                    fileDependencyInfo->SetStillUpdating(true);
                }
                else
                {
                    fileDependencyInfo->SetStillUpdating(false);
                    tryRestart = true;
                }
            }
        }
        else
        {
            // if one of the files is not present or is deleted we construct a null datetime for it and
            // continue checking

            if (!fileDependencyInfo->WasPresentEver())
            {
                // if it never existed in the first place, its okay to restart!
                // however, if it suddenly appears later or it disappears after it was there, it is relevant
                // and we need to wait for it to come back.
                ++count;
            }
            else
            {
                fileDependencyInfo->SetTimestamp(QDateTime());
                fileDependencyInfo->SetIsModified(true);
                fileDependencyInfo->SetStillUpdating(true);
            }
        }
    }

    if (tryRestart && count == m_appDependencies.size())
    {
        //we only check here if one file after getting modified is not getting changed any longer
        tryRestart = false;
        count = 0;
        for (int idx = 0; idx < m_appDependencies.size(); ++idx)
        {
            ApplicationDependencyInfo* fileDependencyInfo = m_appDependencies[idx];
            if (fileDependencyInfo->IsModified() && !fileDependencyInfo->StillUpdating())
            {
                //all those files which have modified and are not changing any longer
                count++;
            }
            else if (!fileDependencyInfo->IsModified())
            {
                //files that are not modified
                count++;
            }
        }

        if (count == m_appDependencies.size())
        {
            AZ_TracePrintf(AssetProcessor::ConsoleChannel, "All dependencies accounted for.\n");

            //We will reach here only if all the modified files are not changing
            //we can now stop the timer and request exit
            Restart();
        }
    }
}

void ApplicationManager::PopulateApplicationDependencies()
{
    QString currentDir(QCoreApplication::applicationDirPath());
    QDir dir(currentDir);
    QString applicationPath = dir.filePath("AssetProcessor.exe");

    m_filesOfInterest.push_back(applicationPath);
    m_processName = applicationPath;

    QString builderDirPath = dir.filePath("Builders");
    QDir builderDir(builderDirPath);
    QStringList filter;
#if defined(AZ_PLATFORM_WINDOWS)
    filter.append("*.dll");
#elif defined(AZ_PLATFORM_LINUX)
    filter.append("*.so");
#elif defined(AZ_PLATFORM_APPLE)
    filter.append("*.dylib");
#endif
    QStringList fileList = builderDir.entryList(filter, QDir::Files);
    for (QString& file : fileList)
    {
        m_filesOfInterest.push_back(builderDir.filePath(file));
    }

    QDir engineRoot;
    AssetUtilities::ComputeEngineRoot(engineRoot);

    QString globalConfigPath = engineRoot.filePath("AssetProcessorPlatformConfig.ini");
    m_filesOfInterest.push_back(globalConfigPath);

    QString gameName = AssetUtilities::ComputeGameName();
    QString gamePlatformConfigPath = engineRoot.filePath(gameName + "/AssetProcessorGamePlatformConfig.ini");
    m_filesOfInterest.push_back(gamePlatformConfigPath);

    // if our Gems file changes, make sure we watch that, too.
    QString gemsConfigFile = engineRoot.filePath(gameName + "/gems.json");
    if (QFile::exists(gemsConfigFile))
    {
        m_filesOfInterest.push_back(gemsConfigFile);
    }

    //find timestamps of all the files
    for (int idx = 0; idx < m_filesOfInterest.size(); idx++)
    {
        QString fileName = m_filesOfInterest.at(idx);
        QFileInfo fileInfo(fileName);
        QDateTime fileLastModifiedTime = fileInfo.lastModified();
        ApplicationDependencyInfo* applicationDependencyInfo = new ApplicationDependencyInfo(fileName, fileLastModifiedTime);
        // if some file does not exist than null datetime will be stored
        m_appDependencies.push_back(applicationDependencyInfo);
    }
}


bool ApplicationManager::StartAZFramework()
{
    m_frameworkApp.Start(AzFramework::Application::Descriptor());
    QString dir;
    QString filename;

    //Registering all the Components
    m_frameworkApp.RegisterComponentDescriptor(AzFramework::LogComponent::CreateDescriptor());
    m_frameworkApp.RegisterComponentDescriptor(AzToolsFramework::AssetSystemComponent::CreateDescriptor());

    AssetUtilities::ComputeApplicationInformation(dir, filename);
    AZ::IO::FileIOBase::GetInstance()->SetAlias("@log@", dir.toUtf8().data());
    m_entity = aznew AZ::Entity("Application Entity");
    if (m_entity)
    {
        AssetProcessor::FilteredLogComponent* logger = aznew AssetProcessor::FilteredLogComponent();
        m_entity->AddComponent(logger);
        if (logger)
        {
#if BATCH_MODE
            // Prevent files overwriting each other if you run batch at same time as GUI (unit tests, for example)
            logger->SetLogFileBaseName("AP_Batch");
#else
            logger->SetLogFileBaseName("AP_GUI");
#endif
        }

        //Activate all the components
        m_entity->Init();
        m_entity->Activate();

        return true;
    }
    else
    {
        //aznew failed
        return false;
    }
}

void ApplicationManager::addRunningThread(AssetProcessor::ThreadWorker* thread)
{
    m_runningThreads.push_back(thread);
}


ApplicationManager::BeforeRunStatus ApplicationManager::BeforeRun()
{
    if (!StartAZFramework())
    {
        return ApplicationManager::BeforeRunStatus::Status_Failure;
    }

    //Initialize and prepares Qt Directories
    if (!AssetUtilities::InitializeQtLibraries())
    {
        return ApplicationManager::BeforeRunStatus::Status_Failure;
    }

    //Setting the argument seeds used for generating random numbers
    qsrand(QTime::currentTime().msecsSinceStartOfDay());

    CreateQtApplication();

    return ApplicationManager::BeforeRunStatus::Status_Success;
}

bool ApplicationManager::Activate()
{
    if (!AssetUtilities::ComputeEngineRoot(m_systemRoot))
    {
        return false;
    }

    QString appDir;
    QString appFileName;
    AssetUtilities::ComputeApplicationInformation(appDir, appFileName);

    m_gameName = AssetUtilities::ComputeGameName(appDir);

    if (m_gameName.isEmpty())
    {
        AZ_Error(AssetProcessor::ConsoleChannel, false, "Unable to detect name of current game project.  Is bootstrap.cfg appropriately configured?");
        return false;
    }

    QString fileName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    //Only start the timer for checking for update if we are running AssetProcessor_tmp.exe
    if (fileName.endsWith("_tmp.exe"))
    {
        PopulateApplicationDependencies();
        connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(CheckForUpdate()));
        m_updateTimer.start(5000);
    }

    // the following controls what registry keys (or on mac or linux what entries in home folder) are used
    // so they should not be translated!
    qApp->setOrganizationName("Amazon");
    qApp->setOrganizationDomain("amazon.com");
    qApp->setApplicationName("Asset Processor");

    InstallTranslators();

    return true;
}


void ApplicationManager::InstallTranslators()
{
    QTranslator guiTranslator; // for autogenerated qt files.
    QStringList args = QCoreApplication::arguments();

    QString languageName = QString(":/i18n/qml_") + QLocale::system().name().toLower() + QString(".qm");
    guiTranslator.load(languageName);
    qApp->installTranslator(&guiTranslator);

    if ((args.contains("--wubbleyous", Qt::CaseInsensitive)) ||
        (args.contains("/w", Qt::CaseInsensitive)) ||
        (args.contains("-w", Qt::CaseInsensitive)))
    {
        guiTranslator.load(":/i18n/qml_ww_ww.qm");
    }
}

bool ApplicationManager::NeedRestart() const
{
    return m_needRestart;
}

void ApplicationManager::Restart()
{
    if (m_needRestart)
    {
        return;
    }
    AZ_TracePrintf(AssetProcessor::ConsoleChannel, "AssetProcessor is restarting.\n");
    m_needRestart = true;
    m_updateTimer.stop();
    QuitRequested();
}

QString ApplicationManager::ProcessName() const
{
    return m_processName;
}

void ApplicationManager::ReadyToQuit(QObject* source)
{
    if (!source)
    {
        return;
    }
#if !defined(BATCH_MODE)
    AZ_TracePrintf(AssetProcessor::ConsoleChannel, "App Quit Object of type %s indicates it is ready.\n", source->metaObject()->className());
#endif

    for (int notifyIdx = 0; notifyIdx < m_objectsToNotify.size(); ++notifyIdx)
    {
        if (m_objectsToNotify[notifyIdx].first == source)
        {
            // replace it.
            m_objectsToNotify[notifyIdx] = QuitPair(m_objectsToNotify[notifyIdx].first, true);
        }
    }
    if (!m_queuedCheckQuit)
    {
        QTimer::singleShot(0, this, SLOT(CheckQuit()));
        m_queuedCheckQuit = true;
    }
}

void ApplicationManager::RegisterComponentDescriptor(const AZ::ComponentDescriptor* descriptor)
{
    AZ_Assert(descriptor, "descriptor cannot be null");
    this->m_frameworkApp.RegisterComponentDescriptor(descriptor);
}

void ApplicationManager::UnRegisterComponentDescriptor(const AZ::ComponentDescriptor* descriptor)
{
    AZ_Assert(descriptor, "descriptor cannot be null");
    this->m_frameworkApp.UnregisterComponentDescriptor(descriptor);
}


QDateTime ApplicationDependencyInfo::Timestamp() const
{
    return m_timestamp;
}

void ApplicationDependencyInfo::SetTimestamp(const QDateTime& timestamp)
{
    m_timestamp = timestamp;
}

bool ApplicationDependencyInfo::IsModified() const
{
    return m_isModified;
}

void ApplicationDependencyInfo::SetIsModified(bool hasModified)
{
    m_isModified = hasModified;
}

bool ApplicationDependencyInfo::StillUpdating() const
{
    return m_stillUpdating;
}

void ApplicationDependencyInfo::SetStillUpdating(bool stillUpdating)
{
    m_stillUpdating = stillUpdating;
}
QString ApplicationDependencyInfo::FileName() const
{
    return m_fileName;
}

void ApplicationDependencyInfo::SetFileName(const QString& fileName)
{
    m_fileName = fileName;
}

void ApplicationDependencyInfo::SetWasPresentEver()
{
    m_wasPresentEver = true;
}

bool ApplicationDependencyInfo::WasPresentEver() const
{
    return m_wasPresentEver;
}

#include <native/utilities/ApplicationManager.moc>

