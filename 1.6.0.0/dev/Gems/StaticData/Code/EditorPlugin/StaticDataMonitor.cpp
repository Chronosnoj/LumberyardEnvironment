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
#include "StdAfx.h"
#include "StaticDataMonitor.h"

#include "StaticDataManager.h"

#include <StaticDataMonitor.moc>

#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>

namespace CloudCanvas
{
    namespace StaticData
    {

        StaticDataMonitor::StaticDataMonitor() :
            QObject{}
        {
            connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &StaticDataMonitor::OnFileChanged);
            connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &StaticDataMonitor::OnDirectoryChanged);

            EBUS_EVENT_RESULT(m_editorGameDir, AzToolsFramework::AssetSystemRequestBus, GetAbsoluteDevGameFolderPath);

        }

        StaticDataMonitor::~StaticDataMonitor()
        {
            SetManager(nullptr);
        }

        AZStd::string StaticDataMonitor::GetSanitizedName(const char* pathName) const
        {
            QDir dir(pathName);
            QString path(dir.absoluteFilePath(pathName));

            return path.toStdString().c_str();
        }

        void StaticDataMonitor::AddPath(const AZStd::string& inputName, bool isFile)
        {
            if (isFile)
            {
                m_monitoredFiles.insert(inputName);
            }

            bool watchResult = m_watcher.addPath(QDir::toNativeSeparators(QString(inputName.c_str())));
            if (!watchResult)
            {
                qDebug() << "Monitor failed on " << inputName.c_str();
            }
        }

        void StaticDataMonitor::RemovePath(const AZStd::string& inputName)
        {
            m_monitoredFiles.erase(inputName);

            QFileInfo fileInfo{ inputName.c_str() };

            m_watcher.removePath(inputName.c_str());
        }


        void StaticDataMonitor::OnFileChanged(const QString& fileName)
        {
            QString fromNative = QDir::fromNativeSeparators(fileName);

            if (m_manager)
            {
                m_manager->LoadRelativeFile(fromNative.toStdString().c_str());
            }
        }

        void StaticDataMonitor::OnDirectoryChanged(const QString& directoryName)
        {
            // We need to decide exactly what has changed and what to do about it.  This means first grabbing a list of the files 
            // we care about as they currently look in this directory.  Then we run through and compare against what we were already watching
            // and ignore those.  Then we potentially have a remaining list of adds and removes to process.
            QString fromNative = QDir::fromNativeSeparators(directoryName);

            if (m_manager)
            {

                StaticDataFileSet currentSet = m_manager->GetFilesForDirectory(fromNative.toStdString().c_str()); // What we should now be watching
                auto fileSet = m_monitoredFiles; // What we were previously watching

                auto thisElement = fileSet.begin(); 
                while(thisElement != fileSet.end())
                {
                    auto lastElement = thisElement;
                    ++thisElement;

                    auto currentElement = currentSet.find(lastElement->c_str());

                    if (currentElement != currentSet.end())
                    {
                        // We have a match, disregard this file
                        currentSet.erase(currentElement);
                        fileSet.erase(lastElement);
                    }
                }

                // At this point what's remaining in currentSet should be new files, and what's remaining in fileSet should be removed files
                for (auto newFile : currentSet)
                {
                    m_manager->LoadRelativeFile(newFile.c_str());
                }

                for (auto removedFile : fileSet)
                {
                    m_manager->LoadRelativeFile(removedFile.c_str());
                    RemovePath(removedFile.c_str());
                }
            }
        }

        void StaticDataMonitor::RemoveAll()
        {

            auto fileList = m_monitoredFiles;
            for (auto thisFile : fileList)
            {
                RemovePath(thisFile.c_str());
            }

            auto directoryList = m_watcher.directories();
            for (auto thisDir : directoryList)
            {
                RemovePath(thisDir.toStdString().c_str());
            }
        }

        void StaticDataMonitor::SetManager(CloudCanvas::StaticData::IStaticDataManager* newManager)
        {
            if (m_manager)
            {
                m_manager->SetMonitor(nullptr);
            }
            m_manager = newManager;
            if (m_manager)
            {
                m_manager->SetMonitor(this);
                m_manager->SetEditorGameDir(m_editorGameDir.c_str());
            }
        }
    }
}