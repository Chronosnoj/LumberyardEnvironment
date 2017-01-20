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
#ifndef FILEWATCHER_COMPONENT_H
#define FILEWATCHER_COMPONENT_H
//////////////////////////////////////////////////////////////////////////

#include "FileWatcherAPI.h"

#include <QMap>
#include <QVector>
#include <QString>

#include <thread>

#include <AzCore/IO/SystemFile.h> // for AZ_MAX_PATH_LEN

class FileWatcher;

#if defined(_WIN32)
typedef void* HANDLE; // avoid windows.h
#endif

//////////////////////////////////////////////////////////////////////////
//! FolderRootWatch
/*! Class used for holding a point in the files system from which file changes are tracked.
 * */
class FolderRootWatch
    : public QObject
{
    Q_OBJECT

    friend class FileWatcher;
public:
    FolderRootWatch(const QString rootFolder);
    virtual ~FolderRootWatch();

public Q_SLOTS:
    bool Start();
    void Stop();

private:
    void WatchFolderLoop();

private:
    std::thread m_thread;
    QString m_root;
    QMap<int, FolderWatchBase*> m_subFolderWatchesMap;
    volatile bool m_shutdownThreadSignal;
    FileWatcher* m_fileWatcher;

#if defined(_WIN32)
    HANDLE m_directoryHandle;
    HANDLE m_ioHandle;
#endif
};

//////////////////////////////////////////////////////////////////////////
//! FileWatcher
/*! Class that handles creation and deletion of FolderRootWatches based on
 *! the given FolderWatches, and forwards file change signals to them.
 * */
class FileWatcher
    : public QObject
{
    Q_OBJECT

public:
    FileWatcher();
    virtual ~FileWatcher();

    //////////////////////////////////////////////////////////////////////////
    virtual int AddFolderWatch(FolderWatchBase* pFolderWatch);
    virtual void RemoveFolderWatch(int handle);
    //////////////////////////////////////////////////////////////////////////

Q_SIGNALS:
    void AnyFileChange(FileChangeInfo info);

private:
    int m_nextHandle;
    QVector<FolderRootWatch*> m_folderWatchRoots;
};

#endif//FILEWATCHER_COMPONENT_H

