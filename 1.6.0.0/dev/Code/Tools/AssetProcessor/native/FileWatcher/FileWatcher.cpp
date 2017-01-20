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
#include "FileWatcher.h"

#if defined(_WIN32)
#include <Windows.h>
#endif

//////////////////////////////////////////////////////////////////////////////
/// FolderWatchRoot
FolderRootWatch::FolderRootWatch(const QString rootFolder)
    : m_root(rootFolder)
    , m_shutdownThreadSignal(false)
{
#if defined(_WIN32)
    m_directoryHandle = nullptr;
    m_ioHandle = nullptr;
#endif
}

FolderRootWatch::~FolderRootWatch()
{
    Stop();
}

bool FolderRootWatch::Start()
{
#if defined(_WIN32)
    m_directoryHandle = ::CreateFile(m_root.toStdWString().data(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);

    if (m_directoryHandle != INVALID_HANDLE_VALUE)
    {
        m_ioHandle = ::CreateIoCompletionPort(m_directoryHandle, nullptr, 1, 0);
        if (m_ioHandle != INVALID_HANDLE_VALUE)
        {
            m_shutdownThreadSignal = false;
            m_thread = std::thread(std::bind(&FolderRootWatch::WatchFolderLoop, this));
            return true;
        }
    }
#endif
    return false;
}

void FolderRootWatch::Stop()
{
    m_shutdownThreadSignal = true;
#if defined(_WIN32)
    CloseHandle(m_ioHandle);
    m_ioHandle = nullptr;
#endif

    if (m_thread.joinable())
    {
        m_thread.join(); // wait for the thread to finish
        m_thread = std::thread(); //destroy
    }
#if defined(_WIN32)
    CloseHandle(m_directoryHandle);
    m_directoryHandle = nullptr;
#endif
}

void FolderRootWatch::WatchFolderLoop()
{
#if defined(_WIN32)
    FILE_NOTIFY_INFORMATION aFileNotifyInformationList[50000];
    QString path;
    OVERLAPPED aOverlapped;
    LPOVERLAPPED pOverlapped;
    DWORD dwByteCount;
    ULONG_PTR ulKey;

    while (!m_shutdownThreadSignal)
    {
        ::memset(aFileNotifyInformationList, 0, sizeof(aFileNotifyInformationList));
        ::memset(&aOverlapped, 0, sizeof(aOverlapped));

        if (::ReadDirectoryChangesW(m_directoryHandle, aFileNotifyInformationList, sizeof(aFileNotifyInformationList), true, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_FILE_NAME, nullptr, &aOverlapped, nullptr))
        {
            //wait for up to a second for I/O to signal
            dwByteCount = 0;
            if (::GetQueuedCompletionStatus(m_ioHandle, &dwByteCount, &ulKey, &pOverlapped, INFINITE))
            {
                //if we are signaled to shutdown bypass
                if (!m_shutdownThreadSignal && ulKey)
                {
                    if (dwByteCount)
                    {
                        int offset = 0;
                        FILE_NOTIFY_INFORMATION* pFileNotifyInformation = aFileNotifyInformationList;
                        do
                        {
                            pFileNotifyInformation = (FILE_NOTIFY_INFORMATION*)((char*)pFileNotifyInformation + offset);

                            path.clear();
                            path.append(m_root);
                            path.append(QString::fromWCharArray(pFileNotifyInformation->FileName, pFileNotifyInformation->FileNameLength / 2));

                            FileChangeInfo info;
                            info.m_filePath = QDir::toNativeSeparators(QDir::cleanPath(path));

                            switch (pFileNotifyInformation->Action)
                            {
                            case FILE_ACTION_ADDED:
                            case FILE_ACTION_RENAMED_NEW_NAME:
                                info.m_action = FileAction::FileAction_Added;
                                break;
                            case FILE_ACTION_REMOVED:
                            case FILE_ACTION_RENAMED_OLD_NAME:
                                info.m_action = FileAction::FileAction_Removed;
                                break;
                            case FILE_ACTION_MODIFIED:
                                info.m_action = FileAction::FileAction_Modified;
                                break;
                            }

                            if (info.m_action != FileAction::FileAction_None)
                            {
                                bool invoked = QMetaObject::invokeMethod(m_fileWatcher, "AnyFileChange", Qt::QueuedConnection, Q_ARG(FileChangeInfo, info));
                                Q_ASSERT(invoked);
                            }

                            offset = pFileNotifyInformation->NextEntryOffset;
                        } while (offset);
                    }
                }
            }
        }
    }
#endif
}

//////////////////////////////////////////////////////////////////////////
/// FileWatcher
FileWatcher::FileWatcher()
    : m_nextHandle(0)
{
    qRegisterMetaType<FileChangeInfo>("FileChangeInfo");
}

FileWatcher::~FileWatcher()
{
}

int FileWatcher::AddFolderWatch(FolderWatchBase* pFolderWatch)
{
    if (!pFolderWatch)
    {
        return -1;
    }

    FolderRootWatch* pFolderRootWatch = nullptr;

    //see if this a sub folder of an already watched root
    for (auto rootsIter = m_folderWatchRoots.begin(); !pFolderRootWatch && rootsIter != m_folderWatchRoots.end(); ++rootsIter)
    {
        if (FolderWatchBase::IsSubfolder(pFolderWatch->m_folder, (*rootsIter)->m_root))
        {
            pFolderRootWatch = *rootsIter;
        }
    }

    bool bCreatedNewRoot = false;
    //if its not a sub folder
    if (!pFolderRootWatch)
    {
        //create a new root and start listening for changes
        pFolderRootWatch = new FolderRootWatch(pFolderWatch->m_folder);

        //make sure the folder watcher(s) get deleted before this
        pFolderRootWatch->setParent(this);
        bCreatedNewRoot = true;
    }

    pFolderRootWatch->m_fileWatcher = this;
    QObject::connect(this, &FileWatcher::AnyFileChange, pFolderWatch, &FolderWatchBase::OnAnyFileChange);

    if (bCreatedNewRoot)
    {
        pFolderRootWatch->Start();

        //since we created a new root, see if the new root is a super folder
        //of other roots, if it is then then fold those roots into the new super root
        for (auto rootsIter = m_folderWatchRoots.begin(); rootsIter != m_folderWatchRoots.end(); )
        {
            if (FolderWatchBase::IsSubfolder((*rootsIter)->m_root, pFolderWatch->m_folder))
            {
                //union the sub folder map over to the new root
                pFolderRootWatch->m_subFolderWatchesMap.unite((*rootsIter)->m_subFolderWatchesMap);

                //clear the old root sub folders map so they don't get deleted when we
                //delete the old root as they are now pointed to by the new root
                (*rootsIter)->m_subFolderWatchesMap.clear();

                //delete the empty old root, deleting a root will call Stop()
                //automatically which kills the thread
                delete *rootsIter;

                //remove the old root pointer form the watched list
                rootsIter = m_folderWatchRoots.erase(rootsIter);
            }
            else
            {
                ++rootsIter;
            }
        }

        //add the new root to the watched roots
        m_folderWatchRoots.push_back(pFolderRootWatch);
    }

    //add to the root
    pFolderRootWatch->m_subFolderWatchesMap.insert(m_nextHandle, pFolderWatch);

    m_nextHandle++;

    return m_nextHandle - 1;
}

void FileWatcher::RemoveFolderWatch(int handle)
{
    for (auto rootsIter = m_folderWatchRoots.begin(); rootsIter != m_folderWatchRoots.end(); )
    {
        //find an element by the handle
        auto foundIter = (*rootsIter)->m_subFolderWatchesMap.find(handle);
        if (foundIter != (*rootsIter)->m_subFolderWatchesMap.end())
        {
            //remove the element
            (*rootsIter)->m_subFolderWatchesMap.erase(foundIter);

            //we removed a folder watch, if it's empty then there is no reason to keep watching it.
            if ((*rootsIter)->m_subFolderWatchesMap.empty())
            {
                delete(*rootsIter);
                rootsIter = m_folderWatchRoots.erase(rootsIter);
            }
            else
            {
                ++rootsIter;
            }
        }
        else
        {
            ++rootsIter;
        }
    }
}

#include <native/FileWatcher/FileWatcher.moc>
#include <native/FileWatcher/FileWatcherAPI.moc>
