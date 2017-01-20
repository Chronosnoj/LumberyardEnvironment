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
#ifndef APPLICATIONMANAGER_H
#define APPLICATIONMANAGER_H

#include <type_traits>
#include <QObject>
#include <QList>
#include <QPair>
#include <QTimer>
#include <QDateTime>
#include <QDir>
#include "native/utilities/ThreadHelper.h"
#include "native/utilities/assetUtilEBusHelper.h"
#include <AzFramework/Application/Application.h>
#include <AssetBuilderSDK/AssetBuilderBusses.h>
#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <AzCore/std/string/string.h>
#include <QVector>
#include <QSet>
#include <QLibrary>

class FolderWatchCallbackEx;
class QCoreApplication;

namespace AZ
{
    class Entity;
}

struct ApplicationDependencyInfo;

//This global function is required, if we want to use uuid as a key in a QSet
uint qHash(const AZ::Uuid& key, uint seed = 0);

//! This class allows you to register any number of objects to it
//! and when quit is requested, it will send a signal "QuitRequested()" to the registered object.
//! (You must implement this slot in your object!)
//! It will then expect each of those objects to send it the "ReadyToQuit(QObject*)" message when its ready
//! once every object is ready, qApp() will be told to quit.
//! the QObject parameter is the object that was originally registered and serves as the handle.
//! if your registered object is destroyed, it will automatically remove it from the list for you, no need to
//! unregister.

class ApplicationManager
    : public QObject
{
    Q_OBJECT
public:
    //! This enum is used by the BeforeRun method  and is useful in deciding whether we can run the application
    //! or whether we need to exit the application either because of an error or because we are restarting
    enum BeforeRunStatus
    {
        Status_Success = 0,
        Status_Restarting,
        Status_Failure,
    };
    explicit ApplicationManager(int argc, char** argv, QObject* parent = 0);
    virtual ~ApplicationManager();
    //! Prepares all the prerequisite needed for the main application functionality
    //! For eg Starts the AZ Framework,Activates logging ,Initialize Qt etc
    //! This method can return the following states success,failure and restarting.The latter two will cause the application to exit.
    virtual ApplicationManager::BeforeRunStatus BeforeRun();
    //! This method actually runs the main functionality of the application ,if BeforeRun method succeeds
    virtual bool Run() = 0;
    //! Returns a pointer to the QCoreApplication
    QCoreApplication* GetQtApplication();

    QDir GetSystemRoot() const;
    QString GetGameName() const;

    void RegisterComponentDescriptor(const AZ::ComponentDescriptor* descriptor);
    void UnRegisterComponentDescriptor(const AZ::ComponentDescriptor* descriptor);

public Q_SLOTS:
    void ReadyToQuit(QObject* source);
    void QuitRequested();
    void ObjectDestroyed(QObject* source);
    void Restart();

private Q_SLOTS:
    void CheckQuit();
    void CheckForUpdate();

protected:
    //! Deactivate all your class member objects in this method
    virtual void Destroy() = 0;
    //! Prepares Qt Directories,Install Qt Translator etc
    virtual bool Activate();
    //! Override this method to create either QApplication or QCoreApplication
    virtual void CreateQtApplication() = 0;

    void RegisterObjectForQuit(QObject* source);
    bool NeedRestart() const;
    QString ProcessName() const;
    void addRunningThread(AssetProcessor::ThreadWorker* thread);
    char** GetCommandArguments() const;
    int CommandLineArgumentsCount() const;
    QCoreApplication* m_qApp;
    template<class BuilderClass>
    void RegisterInternalBuilder(const QString& builderName);

private:
    void InstallTranslators();
    void PopulateApplicationDependencies();
    bool StartAZFramework();
    // QuitPair - Object pointer and "is ready" boolean pair.
    typedef QPair<QObject*, bool> QuitPair;
    QList<QuitPair> m_objectsToNotify;
    bool m_duringShutdown = false;
    QList<ApplicationDependencyInfo*> m_appDependencies;
    QList<QString> m_filesOfInterest;
    QList<AssetProcessor::ThreadWorker*> m_runningThreads;
    QTimer m_updateTimer;
    bool m_needRestart = false;
    bool m_queuedCheckQuit = false;
    QString m_processName;
    QDir m_systemRoot;
    QString m_gameName;
    AZ::Entity* m_entity = nullptr;
    AzFramework::Application m_frameworkApp;
protected:
    int m_argc;
    char** m_argv;
};

///This class stores all the information of files that
/// we need to monitor for relaunching assetprocessor
struct ApplicationDependencyInfo
{
    QString m_fileName;
    QDateTime m_timestamp;
    bool m_isModified;
    bool m_stillUpdating;
    bool m_wasPresentEver; // files that were never present in the first place are not relevant

    ApplicationDependencyInfo(QString fileName, QDateTime timestamp, bool modified = false, bool stillUpdating = false, bool wasPresentEver = false)
        : m_fileName(fileName)
        , m_timestamp(timestamp)
        , m_isModified(modified)
        , m_stillUpdating(stillUpdating)
        , m_wasPresentEver(wasPresentEver)
    {
    }

public:
    QString FileName() const;
    void SetFileName(const QString& FileName);
    QDateTime Timestamp() const;
    void SetTimestamp(const QDateTime& Timestamp);
    bool IsModified() const;
    void SetIsModified(bool IsModified);
    bool StillUpdating() const;
    void SetStillUpdating(bool StillUpdating);
    bool WasPresentEver() const;
    void SetWasPresentEver();
};

#endif //APPLICATIONMANAGER_H
