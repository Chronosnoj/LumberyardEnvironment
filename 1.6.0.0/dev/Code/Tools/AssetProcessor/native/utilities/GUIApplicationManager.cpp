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
#include "native/utilities/GUIApplicationManager.h"
#include "native/utilities/AssetUtils.h"
#include "native/AssetManager/assetProcessorManager.h"
#include "native/connection/connectionManager.h"
#include "native/utilities/IniConfiguration.h"
#include "native/utilities/ApplicationServer.h"
#include "native/resourcecompiler/rccontroller.h"
#include "native/FileServer/fileServer.h"
#include "native/AssetManager/assetScanner.h"
#include "native/shadercompiler/shadercompilerManager.h"
#include "native/shadercompiler/shadercompilerModel.h"
#include "native/AssetManager/AssetRequestHandler.h"
#include "native/utilities/ByteArrayStream.h"
#include <functional>
#include <QProcess>
#include <QThread>
#include <QApplication>
#include <QStringList>
#include <QStyleFactory>
#include "native/ui/MainWindow.h"

#include <QSystemTrayIcon>
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <AzToolsFramework/UI/UICore/StylesheetPreprocessor.hxx>
#include <AzCore/base.h>
#include <AzCore/debug/trace.h>
#include <AzToolsFramework/Asset/AssetProcessorMessages.h>

#define STYLE_SHEET_VARIABLES_PATH_DARK "Editor/Styles/EditorStylesheetVariables_Dark.json"
#define STYLE_SHEET_VARIABLES_PATH_LIGHT "Editor/Styles/EditorStylesheetVariables_Light.json"
#define GLOBAL_STYLE_SHEET_PATH "Editor/Styles/EditorStylesheet.qss"
#define ASSET_PROCESSOR_STYLE_SHEET_PATH "Editor/Styles/AssetProcessor.qss"

using namespace AssetProcessor;

namespace
{
#ifdef AZ_PLATFORM_WINDOWS
    typedef QPair<QString, QString> InputOutputFilePair;
    const int NUMBER_OF_RETRY = 5;

    bool CopyFile(QString sourceFile, QString destFile)
    {
        QFile::remove(destFile);
        return QFile::copy(sourceFile, destFile);
    }

    bool CopyFileWithRetry(InputOutputFilePair pair)
    {
        bool copiedFile = false;
        int counter = NUMBER_OF_RETRY;
        while (!copiedFile && counter > 0)
        {
            QString outputFile(pair.second);
            copiedFile = CopyFile(pair.first, outputFile);
            counter--;
            if (!copiedFile)
            {
                //sleep only if it fails
                QThread::msleep(1000);
            }
            else
            {
                if (!AssetUtilities::MakeFileWritable(outputFile))
                {
                    AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Unable to change permission for the file %s.\n", outputFile.toUtf8().data());
                }
            }
        }
        if (!copiedFile)
        {
            return false;
        }

        return true;
    }
    // Renamed the executable to $name_tmp.exe and re-launch it so that it can be updated
    // from source control without causing locking issues
    bool RenameAndRelaunch(QStringList args)
    {
        QString appDir;
        QString file;
        AssetUtilities::ComputeApplicationInformation(appDir, file);
        QDir appDirectory(appDir);
        QString applicationName = appDirectory.filePath(file);
        QFileInfo moduleFileInfo(applicationName);
        QDir binaryDir = moduleFileInfo.absoluteDir();
        QList<InputOutputFilePair> appDependencies;
        QString newApplicationName = moduleFileInfo.canonicalFilePath();
        // strip extension
        newApplicationName = newApplicationName.left(newApplicationName.lastIndexOf(moduleFileInfo.suffix()) - 1);
        newApplicationName += "_tmp";
        // add extension back
        newApplicationName += "." + moduleFileInfo.suffix();

        appDependencies.push_back(InputOutputFilePair(applicationName, newApplicationName));


        bool launched = false;
        int counter = NUMBER_OF_RETRY;
        while (!launched && counter >= 0)
        {
            // do not try launching the application the first time
            if (counter != NUMBER_OF_RETRY)
            {
                launched = QProcess::startDetached(newApplicationName, args);

                if (!launched)
                {
                    //sleep only if it fails
                    QThread::msleep(1000);
                }
                else
                {
                    return true;
                }
            }
            //go through all the dependencies and try to copy them
            for (int idx = 0; idx < appDependencies.size(); idx++)
            {
                if (!CopyFileWithRetry(appDependencies[idx]))
                {
                    AZ_Warning(AssetProcessor::ConsoleChannel, false, "Failed to copy %s to %s.\n", appDependencies[idx].first.toUtf8().data(), appDependencies[idx].second.toUtf8().data());
                }
            }
            counter--;
        }

        if (launched)
        {
            return true;
        }
        else
        {
            AZ_Warning(AssetProcessor::ConsoleChannel, false,  "Failed to launch tmp AssetProcessor.\n");
            return false;
        }
    }
#endif //AZ_PLATFORM_WINDOWS
    QString LoadStyleSheet(QDir rootDir, QString styleSheetPath, bool darkSkin)
    {
        QFile styleSheetVariablesFile;
        if (darkSkin)
        {
            styleSheetVariablesFile.setFileName(rootDir.filePath(STYLE_SHEET_VARIABLES_PATH_DARK));
        }
        else
        {
            styleSheetVariablesFile.setFileName(rootDir.filePath(STYLE_SHEET_VARIABLES_PATH_LIGHT));
        }

        AzToolsFramework::StylesheetPreprocessor styleSheetProcessor(nullptr);
        if (styleSheetVariablesFile.open(QFile::ReadOnly))
        {
            styleSheetProcessor.ReadVariables(styleSheetVariablesFile.readAll());
        }

        QFile styleSheetFile(rootDir.filePath(styleSheetPath));
        if (styleSheetFile.open(QFile::ReadOnly))
        {
            return styleSheetProcessor.ProcessStyleSheet(styleSheetFile.readAll());
        }

        return QString();
    }
}


GUIApplicationManager::GUIApplicationManager(int argc, char** argv, QObject* parent)
    : BatchApplicationManager(argc, argv, parent)
{
}


GUIApplicationManager::~GUIApplicationManager()
{
    Destroy();
}



ApplicationManager::BeforeRunStatus GUIApplicationManager::BeforeRun()
{
    ApplicationManager::BeforeRunStatus status = BatchApplicationManager::BeforeRun();
    if (status != ApplicationManager::BeforeRunStatus::Status_Success)
    {
        return status;
    }

#if defined(AZ_PLATFORM_WINDOWS)
    //Check whether restart required
    if (RestartRequired())
    {
        int argc = CommandLineArgumentsCount();
        char** argv = GetCommandArguments();
        QStringList args;
        for (int idx = 0; idx < argc; idx++)
        {
            QString arg = argv[idx];
            args.append(arg);
        }

        if (RenameAndRelaunch(args))
        {
            return ApplicationManager::BeforeRunStatus::Status_Restarting;
        }
        else
        {
            return ApplicationManager::BeforeRunStatus::Status_Failure;
        }
    }
#endif //AZ_PLATFORM_WINDOWS
    AssetProcessor::MessageInfoBus::Handler::BusConnect();
    AssetUtilities::UpdateBranchToken();

    QDir devRoot;
    AssetUtilities::ComputeEngineRoot(devRoot);
    QString bootstrapPath = devRoot.filePath("bootstrap.cfg");
    m_fileWatcher.addPath(bootstrapPath);

    QObject::connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &GUIApplicationManager::FileChanged);

    return ApplicationManager::BeforeRunStatus::Status_Success;
}

void GUIApplicationManager::Destroy()
{
    AssetProcessor::MessageInfoBus::Handler::BusDisconnect();
    BatchApplicationManager::Destroy();

    delete m_assetRequestHandler;
    m_assetRequestHandler = nullptr;

    DestroyConnectionManager();
    DestroyIniConfiguration();
    DestroyApplicationServer();
    DestroyFileServer();
    DestroyShaderCompilerManager();
    DestroyShaderCompilerModel();
}


bool GUIApplicationManager::Run()
{
    if (!Activate())
    {
        return false;
    }

    QDir systemRoot = GetSystemRoot();

    QDir::addSearchPath("STYLESHEETIMAGES", systemRoot.filePath("Editor/Styles/StyleSheetImages"));

    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QString appStyleSheet = LoadStyleSheet(systemRoot, GLOBAL_STYLE_SHEET_PATH, true);
    qApp->setStyleSheet(appStyleSheet);

    MainWindow* mainWindow = new MainWindow(this);
    QString windowStyleSheet = LoadStyleSheet(systemRoot, ASSET_PROCESSOR_STYLE_SHEET_PATH, true);
    mainWindow->setStyleSheet(windowStyleSheet);

    bool startHidden = QApplication::arguments().contains("--start-hidden", Qt::CaseInsensitive);

    if (!startHidden)
    {
        mainWindow->show();
    }

    QAction* quitAction = new QAction(QObject::tr("Quit"), mainWindow);
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    mainWindow->addAction(quitAction);
    mainWindow->connect(quitAction, SIGNAL(triggered()), this, SLOT(QuitRequested()));

    QAction* refreshAction = new QAction(QObject::tr("Refresh Stylesheet"), mainWindow);
    refreshAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_R));
    mainWindow->addAction(refreshAction);
    mainWindow->connect(refreshAction, &QAction::triggered, [systemRoot, mainWindow]()
    {
        QString appStyleSheet = LoadStyleSheet(systemRoot, GLOBAL_STYLE_SHEET_PATH, true);
        qApp->setStyleSheet(appStyleSheet);

        QString windowStyleSheet = LoadStyleSheet(systemRoot, ASSET_PROCESSOR_STYLE_SHEET_PATH, true);
        mainWindow->setStyleSheet(windowStyleSheet);
    });

    QObject::connect(this, &GUIApplicationManager::ToggleDisplay, mainWindow, &MainWindow::ToggleWindow);

    QSystemTrayIcon* trayIcon = nullptr;

    if (QSystemTrayIcon::isSystemTrayAvailable())
    {
        QAction* showAction = new QAction(QObject::tr("Show"), mainWindow);
        QObject::connect(showAction, SIGNAL(triggered()), mainWindow, SLOT(show()));
        QAction* hideAction = new QAction(QObject::tr("Hide"), mainWindow);
        QObject::connect(hideAction, SIGNAL(triggered()), mainWindow, SLOT(hide()));

        QMenu* trayIconMenu = new QMenu();
        trayIconMenu->addAction(showAction);
        trayIconMenu->addAction(hideAction);
        trayIconMenu->addSeparator();
        trayIconMenu->addAction(quitAction);

        trayIcon = new QSystemTrayIcon(mainWindow);
        trayIcon->setContextMenu(trayIconMenu);
        trayIcon->setToolTip(QObject::tr("Asset Processor"));
        trayIcon->setIcon(QIcon(":/AssetProcessor.png"));
        trayIcon->show();
        QObject::connect(trayIcon, &QSystemTrayIcon::activated, mainWindow, [&](QSystemTrayIcon::ActivationReason reason)
        {
            if (reason == QSystemTrayIcon::DoubleClick)
            {
                mainWindow->setVisible(!mainWindow->isVisible());
            }
        });

        if (startHidden)
        {
            trayIcon->showMessage(
                QCoreApplication::translate("Tray Icon", "Lumberyard Asset Processor has started"),
                QCoreApplication::translate("Tray Icon", "The Lumberyard Asset Processor monitors raw project assets and converts those assets into runtime-ready data."),
                QSystemTrayIcon::Information, 3000);
        }
    }

    qApp->setQuitOnLastWindowClosed(false);
    int resultCode =  qApp->exec(); // this blocks until the last window is closed.

    if (trayIcon)
    {
        trayIcon->hide();
        delete trayIcon;
    }

    if (NeedRestart())
    {
        bool launched = QProcess::startDetached(ProcessName(), QCoreApplication::arguments());
        if (!launched)
        {
            QMessageBox::critical(nullptr,
                QCoreApplication::translate("application", "Unable to launch Asset Processor"),
                QCoreApplication::translate("application", "Unable to launch Asset Processor"));

            return false;
        }
    }

    delete mainWindow;

    Destroy();

    return !resultCode;
}

void GUIApplicationManager::NegotiationFailed()
{
    QString message = QCoreApplication::translate("error", "An attempt to connect to the game or editor has failed. The game or editor appears to be running from a different folder. Please restart the asset processor from the correct branch.");
    QMetaObject::invokeMethod(this, "ShowMessageBox", Qt::QueuedConnection, Q_ARG(QString, QString("Negotiation Failed")), Q_ARG(QString, message));
}

void GUIApplicationManager::ProxyConnectFailed()
{
    QString message = QCoreApplication::translate("error", "Proxy Connect Disabled!\n\rPlease make sure that the Proxy IP does not loop back to this same Asset Processor.");
    QMetaObject::invokeMethod(this, "ShowMessageBox", Qt::QueuedConnection, Q_ARG(QString, QString("Proxy Connection Failed")), Q_ARG(QString, message));
}

void GUIApplicationManager::ShowMessageBox(QString title,  QString msg)
{
    if (!m_messageBoxIsVisible)
    {
        // Only show the message box if it is not visible
        m_messageBoxIsVisible = true;
        QMessageBox msgBox;
        msgBox.setWindowTitle(title);
        msgBox.setText(msg);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.exec();
        m_messageBoxIsVisible = false;
    }
}

bool GUIApplicationManager::Activate()
{
    //activate the base stuff.
    if (!BatchApplicationManager::Activate())
    {
        return false;
    }
    InitIniConfiguration();
    bool appInited = InitApplicationServer();
    if (!appInited)
    {
        return false;
    }
    InitFileServer();
    InitShaderCompilerModel();
    InitShaderCompilerManager();
    InitConnectionManager();

    RegisterObjectForQuit(m_connectionManager);
    RegisterObjectForQuit(m_applicationServer);

    InitAssetRequestHandler();

#if !defined(FORCE_PROXY_MODE)
    GetAssetScanner()->StartScan();
#endif
    return true;
}

void GUIApplicationManager::CreateQtApplication()
{
    // Qt actually modifies the argc and argv, you must pass the real ones in as ref so it can.
    m_qApp = new QApplication(m_argc, m_argv);
}

void GUIApplicationManager::FileChanged(QString path)
{
    QDir devRoot = ApplicationManager::GetSystemRoot();
    QString bootstrapPath = devRoot.filePath("bootstrap.cfg");
    if (QString::compare(AssetUtilities::NormalizeFilePath(path), bootstrapPath, Qt::CaseInsensitive) == 0)
    {
        //Check and update the game token if the bootstrap file get modified
        if (!AssetUtilities::UpdateBranchToken())
        {
            QMetaObject::invokeMethod(this, "FileChanged", Qt::QueuedConnection, Q_ARG(QString, path));
            return; // try again later
        }
        //if the bootstrap file changed,checked whether the game name changed
        QString gameName = AssetUtilities::ReadGameNameFromBootstrap();
        if (QString::compare(gameName, ApplicationManager::GetGameName(), Qt::CaseInsensitive) != 0)
        {
            //The gamename have changed ,should restart the assetProcessor
            QMetaObject::invokeMethod(this, "Restart", Qt::QueuedConnection);
        }
    }
}

bool GUIApplicationManager::RestartRequired()
{
    QString appDir;
    QString file;
    AssetUtilities::ComputeApplicationInformation(appDir, file);
    QFileInfo moduleFileInfo(file);
    QString baseName = moduleFileInfo.baseName();

    if (QCoreApplication::arguments().contains("--norestart", Qt::CaseInsensitive))
    {
        return false;
    }

    if (baseName.endsWith("_tmp"))
    {
        return false;
    }

    // Do not restart if the debugger is attached
    if (AZ::Debug::Trace::IsDebuggerPresent())
    {
        return false;
    }
    return true;
}

void GUIApplicationManager::InitConnectionManager()
{
    using namespace std::placeholders;
    using namespace AzFramework::AssetSystem;
    using namespace AzToolsFramework::AssetSystem;

    m_connectionManager = new ConnectionManager(GetPlatformConfiguration(), m_applicationServer->getServerListeningPort());

    QObject* connectionAndChangeMessagesThreadContext = this;

    // AssetProcessor Manager related stuff

    // The AssetCatalog has to be rebuilt on connection, so we force the asset changed messages to be serialized on the GUIApplicationManager's thread
    // so that later, when we get a connection, we can trigger an asset catalog saved, and be sure that the asset changed messages won't be passed on
    // until the AssetCatalog's saved signal has been processed
    QObject::connect(GetAssetCatalog(), &AssetCatalog::ProductChangeRegistered, connectionAndChangeMessagesThreadContext,
        [this](AssetProcessor::ProductInfo productInfo)
    {
        AssetNotificationMessage message(productInfo.m_assetInfo.m_relativePath, AssetNotificationMessage::AssetChanged);
        message.m_sizeBytes = productInfo.m_assetInfo.m_sizeBytes;
        message.m_assetId = productInfo.m_assetId;
        EBUS_EVENT(AssetProcessor::ConnectionBus, SendPerPlatform, 0, message, productInfo.m_platform);
    }
        , Qt::QueuedConnection);

    QObject::connect(GetAssetCatalog(), &AssetCatalog::ProductRemoveRegistered, connectionAndChangeMessagesThreadContext,
        [this](AssetProcessor::ProductInfo productInfo)
    {
        AssetNotificationMessage message(productInfo.m_assetInfo.m_relativePath, AssetNotificationMessage::AssetRemoved);
        message.m_assetId = productInfo.m_assetId;
        EBUS_EVENT(AssetProcessor::ConnectionBus, SendPerPlatform, 0, message, productInfo.m_platform);
    }
        , Qt::QueuedConnection);

    QObject::connect(GetAssetProcessorManager(), &AssetProcessorManager::InputAssetProcessed,
        [this](QString absolutePath, QString platform)
    {
        m_connectionManager->SendMessageToPlatform(platform, AssetUtilities::ComputeCRC32Lowercase("AssetProcessorManager::InputAssetProcessed"), 0, absolutePath.toUtf8());
    }
        );

    QObject::connect(GetAssetProcessorManager(), &AssetProcessorManager::InputAssetFailed,
        [this](QString absolutePath, QString platform)
    {
        m_connectionManager->SendMessageToPlatform(platform, AssetUtilities::ComputeCRC32Lowercase("AssetProcessorManager::InputAssetFailed"), 0, absolutePath.toUtf8());
    }
        );

    //Application manager related stuff

    // The AssetCatalog has to be rebuilt on connection, so we force the incoming connection messages to be serialized as they connect to the GUIApplicationManager
    QObject::connect(m_applicationServer, &ApplicationServer::newIncomingConnection, connectionAndChangeMessagesThreadContext,
        [this](qintptr connection)
        {
            // if we're not a proxy, we need to make sure our registry is up to date:
            if (!m_connectionManager->ProxyConnect())
            {
                m_connectionsAwaitingAssetCatalogSave.ref();
                int registrySaveVersion = GetAssetCatalog()->SaveRegistry();
                m_queuedConnections[registrySaveVersion] = connection;
            }

            QMetaObject::invokeMethod(m_connectionManager, "NewConnection", Qt::QueuedConnection, Q_ARG(qintptr, connection));
        }
        , Qt::QueuedConnection);

    QObject::connect(GetAssetCatalog(), &AssetCatalog::Saved, connectionAndChangeMessagesThreadContext,
        [this](int assetCatalogVersion)
        {
            if (m_queuedConnections.contains(assetCatalogVersion))
            {
                // tell the connection manager to send messages to this connection now
                m_queuedConnections.take(assetCatalogVersion);
                m_connectionsAwaitingAssetCatalogSave.deref();
            }
        }
        , Qt::QueuedConnection);

    //File Server related
    m_connectionManager->RegisterService(FileOpenRequest::MessageType(), std::bind(&FileServer::ProcessOpenRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileCloseRequest::MessageType(), std::bind(&FileServer::ProcessCloseRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileReadRequest::MessageType(), std::bind(&FileServer::ProcessReadRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileWriteRequest::MessageType(), std::bind(&FileServer::ProcessWriteRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileSeekRequest::MessageType(), std::bind(&FileServer::ProcessSeekRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileTellRequest::MessageType(), std::bind(&FileServer::ProcessTellRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileIsReadOnlyRequest::MessageType(), std::bind(&FileServer::ProcessIsReadOnlyRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(PathIsDirectoryRequest::MessageType(), std::bind(&FileServer::ProcessIsDirectoryRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileSizeRequest::MessageType(), std::bind(&FileServer::ProcessSizeRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileModTimeRequest::MessageType(), std::bind(&FileServer::ProcessModificationTimeRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileExistsRequest::MessageType(), std::bind(&FileServer::ProcessExistsRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileFlushRequest::MessageType(), std::bind(&FileServer::ProcessFlushRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(PathCreateRequest::MessageType(), std::bind(&FileServer::ProcessCreatePathRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(PathDestroyRequest::MessageType(), std::bind(&FileServer::ProcessDestroyPathRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileRemoveRequest::MessageType(), std::bind(&FileServer::ProcessRemoveRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileCopyRequest::MessageType(), std::bind(&FileServer::ProcessCopyRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileRenameRequest::MessageType(), std::bind(&FileServer::ProcessRenameRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FindFilesRequest::MessageType(), std::bind(&FileServer::ProcessFindFileNamesRequest, m_fileServer, _1, _2, _3, _4));

    QObject::connect(m_connectionManager, SIGNAL(connectionAdded(uint, Connection*)), m_fileServer, SLOT(ConnectionAdded(unsigned int, Connection*)));
    QObject::connect(m_connectionManager, SIGNAL(ConnectionDisconnected(unsigned int)), m_fileServer, SLOT(ConnectionRemoved(unsigned int)));
    QObject::connect(m_iniConfiguration, &IniConfiguration::ProxyInfoChanged, m_connectionManager, &ConnectionManager::SetProxyInformation);

    QObject::connect(m_fileServer, SIGNAL(AddBytesReceived(unsigned int, qint64, bool)), m_connectionManager, SLOT(AddBytesReceived(unsigned int, qint64, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddBytesSent(unsigned int, qint64, bool)), m_connectionManager, SLOT(AddBytesSent(unsigned int, qint64, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddBytesRead(unsigned int, qint64, bool)), m_connectionManager, SLOT(AddBytesRead(unsigned int, qint64, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddBytesWritten(unsigned int, qint64, bool)), m_connectionManager, SLOT(AddBytesWritten(unsigned int, qint64, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddOpenRequest(unsigned int, bool)), m_connectionManager, SLOT(AddOpenRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddCloseRequest(unsigned int, bool)), m_connectionManager, SLOT(AddCloseRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddOpened(unsigned int, bool)), m_connectionManager, SLOT(AddOpened(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddClosed(unsigned int, bool)), m_connectionManager, SLOT(AddClosed(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddReadRequest(unsigned int, bool)), m_connectionManager, SLOT(AddReadRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddWriteRequest(unsigned int, bool)), m_connectionManager, SLOT(AddWriteRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddTellRequest(unsigned int, bool)), m_connectionManager, SLOT(AddTellRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddSeekRequest(unsigned int, bool)), m_connectionManager, SLOT(AddSeekRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddIsReadOnlyRequest(unsigned int, bool)), m_connectionManager, SLOT(AddIsReadOnlyRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddIsDirectoryRequest(unsigned int, bool)), m_connectionManager, SLOT(AddIsDirectoryRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddSizeRequest(unsigned int, bool)), m_connectionManager, SLOT(AddSizeRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddModificationTimeRequest(unsigned int, bool)), m_connectionManager, SLOT(AddModificationTimeRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddExistsRequest(unsigned int, bool)), m_connectionManager, SLOT(AddExistsRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddFlushRequest(unsigned int, bool)), m_connectionManager, SLOT(AddFlushRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddCreatePathRequest(unsigned int, bool)), m_connectionManager, SLOT(AddCreatePathRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddDestroyPathRequest(unsigned int, bool)), m_connectionManager, SLOT(AddDestroyPathRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddRemoveRequest(unsigned int, bool)), m_connectionManager, SLOT(AddRemoveRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddCopyRequest(unsigned int, bool)), m_connectionManager, SLOT(AddCopyRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddRenameRequest(unsigned int, bool)), m_connectionManager, SLOT(AddRenameRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddFindFileNamesRequest(unsigned int, bool)), m_connectionManager, SLOT(AddFindFileNamesRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(UpdateConnectionMetrics()), m_connectionManager, SLOT(UpdateConnectionMetrics()));

    m_connectionManager->ReadProxyServerInformation();

    //Shader compiler stuff
    m_connectionManager->RegisterService(AssetUtilities::ComputeCRC32Lowercase("ShaderCompilerProxyRequest"), std::bind(&ShaderCompilerManager::process, m_shaderCompilerManager, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
    QObject::connect(m_shaderCompilerManager, SIGNAL(sendErrorMessageFromShaderJob(QString, QString, QString, QString)), m_shaderCompilerModel, SLOT(addShaderErrorInfoEntry(QString, QString, QString, QString)));

    //RcController related stuff
    QObject::connect(GetRCController(), &RCController::JobStatusChanged, GetAssetProcessorManager(), &AssetProcessorManager::OnJobStatusChanged);

#if !defined(FORCE_PROXY_MODE)
    QObject::connect(GetRCController(), &RCController::JobStarted,
        [this](QString inputFile, QString platform)
    {
        AssetNotificationMessage message(inputFile.toUtf8().constData(), AssetNotificationMessage::JobStarted);
        EBUS_EVENT(AssetProcessor::ConnectionBus, SendPerPlatform, 0, message, platform);
    }
        );

    QObject::connect(GetRCController(), &RCController::fileCompiled,
        [this](AssetProcessor::JobEntry entry, AssetBuilderSDK::ProcessJobResponse /*response*/)
    {
        AssetNotificationMessage message(entry.m_relativePathToFile.toUtf8().constData(), AssetNotificationMessage::JobCompleted);
        EBUS_EVENT(AssetProcessor::ConnectionBus, SendPerPlatform, 0, message, entry.m_platform);
    }
        );

    QObject::connect(GetRCController(), &RCController::fileFailed,
        [this](AssetProcessor::JobEntry entry)
    {
        AssetNotificationMessage message(entry.m_relativePathToFile.toUtf8().constData(), AssetNotificationMessage::JobFailed);
        EBUS_EVENT(AssetProcessor::ConnectionBus, SendPerPlatform, 0, message, entry.m_platform);
    }
        );

    QObject::connect(GetRCController(), &RCController::JobsInQueuePerPlatform,
        [this](QString platform, int count)
    {
        AssetNotificationMessage message(QByteArray::number(count).constData(), AssetNotificationMessage::JobCount);
        EBUS_EVENT(AssetProcessor::ConnectionBus, SendPerPlatform, 0, message, platform);
    }
        );
#endif //FORCE_PROXY_MODE

    m_connectionManager->RegisterService(AssetUtilities::ComputeCRC32Lowercase("AssetProcessorManager::Toggle"),
        std::bind([this](unsigned int connId, unsigned int type, unsigned int serial, QByteArray payload)
    {
        (void) connId;
        (void) type;
        (void) serial;
        (void) payload;

        Q_EMIT ToggleDisplay();
    }, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)
        );

    //You can get Asset Processor Current State
    using AzFramework::AssetSystem::RequestAssetProcessorStatus;
    auto GetState = [this](unsigned int connId, unsigned int, unsigned int serial, QByteArray payload, QString)
        {
            RequestAssetProcessorStatus requestAssetProcessorMessage;

            if (AssetProcessor::UnpackMessage(payload, requestAssetProcessorMessage))
            {
                bool allAssetCatalogSavesComplete = (m_connectionsAwaitingAssetCatalogSave.loadAcquire() == 0);

                bool status = false;
                //check whether the scan is complete,the asset processor manager initial processing is complete and
                //the number of copy jobs are zero

                int numberOfPendingJobs = GetRCController()->NumberOfPendingCriticalJobsPerPlatform(requestAssetProcessorMessage.m_platform.c_str());
                    status = (GetAssetScanner()->status() == AssetProcessor::AssetScanningStatus::Completed)
                        && GetAssetProcessorManager()->HasProcessedCriticalAssets()
                        && allAssetCatalogSavesComplete
                        && (!numberOfPendingJobs);

                int pendingResult = 0;
                QMetaObject::invokeMethod(GetAssetProcessorManager(), "ComputeNumberOfPendingWorkUnits", Qt::BlockingQueuedConnection, Q_ARG(int&, pendingResult));

                ResponseAssetProcessorStatus responseAssetProcessorMessage;
                responseAssetProcessorMessage.m_isAssetProcessorReady = status;
                responseAssetProcessorMessage.m_numberOfPendingJobs = numberOfPendingJobs + pendingResult;
                EBUS_EVENT_ID(connId, AssetProcessor::ConnectionBus, Send, serial, responseAssetProcessorMessage);
            }
        };
    // connect the network messages to the Request handler:
    m_connectionManager->RegisterService(RequestAssetProcessorStatus::MessageType(), GetState);
}

void GUIApplicationManager::DestroyConnectionManager()
{
    if (m_connectionManager)
    {
        delete m_connectionManager;
        m_connectionManager = nullptr;
    }
}

void GUIApplicationManager::InitIniConfiguration()
{
    m_iniConfiguration = new IniConfiguration();
    m_iniConfiguration->readINIConfigFile();
    m_iniConfiguration->parseCommandLine();
}

void GUIApplicationManager::DestroyIniConfiguration()
{
    if (m_iniConfiguration)
    {
        delete m_iniConfiguration;
        m_iniConfiguration = nullptr;
    }
}

bool GUIApplicationManager::InitApplicationServer()
{
    m_applicationServer = new ApplicationServer();
    return m_applicationServer->startListening();
}

void GUIApplicationManager::DestroyApplicationServer()
{
    if (m_applicationServer)
    {
        delete m_applicationServer;
        m_applicationServer = nullptr;
    }
}

void GUIApplicationManager::InitFileServer()
{
    m_fileServer = new FileServer();
    m_fileServer->SetSystemRoot(GetSystemRoot());
}

void GUIApplicationManager::DestroyFileServer()
{
    if (m_fileServer)
    {
        delete m_fileServer;
        m_fileServer = nullptr;
    }
}

void GUIApplicationManager::InitShaderCompilerManager()
{
    m_shaderCompilerManager = new ShaderCompilerManager();
}

void GUIApplicationManager::DestroyShaderCompilerManager()
{
    if (m_shaderCompilerManager)
    {
        delete m_shaderCompilerManager;
        m_shaderCompilerManager = nullptr;
    }
}

void GUIApplicationManager::InitShaderCompilerModel()
{
    m_shaderCompilerModel = new ShaderCompilerModel();
}

void GUIApplicationManager::DestroyShaderCompilerModel()
{
    if (m_shaderCompilerModel)
    {
        delete m_shaderCompilerModel;
        m_shaderCompilerModel = nullptr;
    }
}

ConnectionManager* GUIApplicationManager::GetConnectionManager() const
{
    return m_connectionManager;
}
IniConfiguration* GUIApplicationManager::GetIniConfiguration() const
{
    return m_iniConfiguration;
}
ApplicationServer* GUIApplicationManager::GetApplicationServer() const
{
    return m_applicationServer;
}
FileServer* GUIApplicationManager::GetFileServer() const
{
    return m_fileServer;
}
ShaderCompilerManager* GUIApplicationManager::GetShaderCompilerManager() const
{
    return m_shaderCompilerManager;
}
ShaderCompilerModel* GUIApplicationManager::GetShaderCompilerModel() const
{
    return m_shaderCompilerModel;
}

void GUIApplicationManager::InitAssetRequestHandler()
{
    using namespace std::placeholders;
    using namespace AzFramework::AssetSystem;
    using namespace AzToolsFramework::AssetSystem;

    m_assetRequestHandler = new AssetRequestHandler();

    m_assetRequestHandler->RegisterRequestHandler(GetAssetIdRequest::MessageType(), GetAssetProcessorManager());
    m_assetRequestHandler->RegisterRequestHandler(GetAssetPathRequest::MessageType(), GetAssetProcessorManager());
    m_assetRequestHandler->RegisterRequestHandler(AssetJobsInfoRequest::MessageType(), GetAssetProcessorManager());
    m_assetRequestHandler->RegisterRequestHandler(AssetJobLogRequest::MessageType(), GetAssetProcessorManager());


    auto ProcessGetAssetID = [&](unsigned int connId, unsigned int type, unsigned int serial, QByteArray payload, QString platform)
    {
        QMetaObject::invokeMethod(m_assetRequestHandler, "OnNewIncomingRequest", Qt::QueuedConnection, Q_ARG(unsigned int, connId), Q_ARG(unsigned int, serial), Q_ARG(QByteArray, payload), Q_ARG(QString, platform));
    };
    m_connectionManager->RegisterService(GetAssetIdRequest::MessageType(), std::bind(ProcessGetAssetID, _1, _2, _3, _4, _5));

    auto ProcessGetAssetPath = [&](unsigned int connId, unsigned int type, unsigned int serial, QByteArray payload, QString platform)
    {
        QMetaObject::invokeMethod(m_assetRequestHandler, "OnNewIncomingRequest", Qt::QueuedConnection, Q_ARG(unsigned int, connId), Q_ARG(unsigned int, serial), Q_ARG(QByteArray, payload), Q_ARG(QString, platform));
    };
    m_connectionManager->RegisterService(GetAssetPathRequest::MessageType(), std::bind(ProcessGetAssetPath, _1, _2, _3, _4, _5));

    auto ProcessGetJobRequest = [&](unsigned int connId, unsigned int type, unsigned int serial, QByteArray payload, QString platform)
    {
        QMetaObject::invokeMethod(m_assetRequestHandler, "OnNewIncomingRequest", Qt::QueuedConnection, Q_ARG(unsigned int, connId), Q_ARG(unsigned int, serial), Q_ARG(QByteArray, payload), Q_ARG(QString, platform));
    };
    m_connectionManager->RegisterService(AssetJobsInfoRequest::MessageType(), std::bind(ProcessGetJobRequest, _1, _2, _3, _4, _5));

    auto ProcessGetJobLogRequest = [&](unsigned int connId, unsigned int type, unsigned int serial, QByteArray payload, QString platform)
    {
        QMetaObject::invokeMethod(m_assetRequestHandler, "OnNewIncomingRequest", Qt::QueuedConnection, Q_ARG(unsigned int, connId), Q_ARG(unsigned int, serial), Q_ARG(QByteArray, payload), Q_ARG(QString, platform));
    };
    m_connectionManager->RegisterService(AssetJobLogRequest::MessageType(), std::bind(ProcessGetJobLogRequest, _1, _2, _3, _4, _5));

    // connect the "Does asset exist?" loop to each other:
    QObject::connect(m_assetRequestHandler, &AssetRequestHandler::RequestAssetExists, GetAssetProcessorManager(), &AssetProcessorManager::OnRequestAssetExists);
    QObject::connect(GetAssetProcessorManager(), &AssetProcessorManager::SendAssetExistsResponse, m_assetRequestHandler, &AssetRequestHandler::OnRequestAssetExistsResponse);
    QObject::connect(GetAssetProcessorManager(), &AssetProcessorManager::OnBecameIdle, this, 
        [this]()
        {
            // When we've completed work (i.e. no more pending jobs), update the on-disk asset catalog.
            GetAssetCatalog()->SaveRegistry();
        });

    // connect the network messages to the Request handler:
    m_connectionManager->RegisterService(RequestAssetStatus::MessageType(),
        std::bind([&](unsigned int connId, unsigned int serial, QByteArray payload, QString platform)
    {
        QMetaObject::invokeMethod(m_assetRequestHandler, "OnNewIncomingRequest", Qt::QueuedConnection, Q_ARG(unsigned int, connId), Q_ARG(unsigned int, serial), Q_ARG(QByteArray, payload), Q_ARG(QString, platform));
    },
        std::placeholders::_1, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5)
        );

    QObject::connect(GetAssetProcessorManager(), &AssetProcessorManager::FenceFileDetected, m_assetRequestHandler, &AssetRequestHandler::OnFenceFileDetected);
#if !defined(FORCE_PROXY_MODE)
    // connect the Asset Request Handler to RC:
    QObject::connect(m_assetRequestHandler, &AssetRequestHandler::RequestCompileGroup, GetRCController(), &RCController::OnRequestCompileGroup);
    QObject::connect(GetRCController(), &RCController::CompileGroupCreated, m_assetRequestHandler, &AssetRequestHandler::OnCompileGroupCreated);
    QObject::connect(GetRCController(), &RCController::CompileGroupFinished, m_assetRequestHandler, &AssetRequestHandler::OnCompileGroupFinished);
#endif
}

#include <native/utilities/GUIApplicationManager.moc>
