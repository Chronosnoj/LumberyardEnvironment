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
#include <StaticDataManager.h>
#include <CSVStaticData.h>
#include <StaticDataInterface.h>
#include <StaticDataTransferManager.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/std/parallel/lock.h>
#include <AzCore/IO/SystemFile.h>
#include <IStaticDataMonitor.h>

#include <aws/core/utils/HashingUtils.h>

#include <sstream>
#include <string>

#define CSV_TAG ".csv"

namespace CloudCanvas
{
    namespace StaticData
    {

        StaticDataManager::StaticDataManager()
        {
            m_transferManager = AZStd::make_shared<StaticDataTransferManager>(this);

            LoadAll();

            StaticDataRequestBus::Handler::BusConnect();
        }

        StaticDataManager::~StaticDataManager()
        {
            StaticDataRequestBus::Handler::BusDisconnect();
        }

        bool StaticDataManager::LoadAll()
        {

            if (m_monitor)
            {
                m_monitor->RemoveAll();
            }

            {
                AZStd::lock_guard<AZStd::mutex> thisLock(GetDataMutex());
                m_data.clear();
            }

            LoadDirectoryDataType(csvDir, CSV_TAG, StaticDataType::CSV);
 
            return true;
        }

        bool StaticDataManager::ReloadTagType(const char* tagName)
        {
            return ReloadType(tagName);
        }

        bool StaticDataManager::AddMonitoredBucket(const char* tagName)
        {
            if (m_transferManager)
            {
                return m_transferManager->AddBucket(tagName);
            }
            return false;
        }

        bool StaticDataManager::RemoveMonitoredBucket(const char* tagName)
        {
            if (m_transferManager)
            {
                return m_transferManager->RemoveBucket(tagName);
            }
            return false;
        }

        bool StaticDataManager::RequestBucket(const char* bucketName)
        {
            if (m_transferManager)
            {
                return m_transferManager->RequestList(bucketName);
            }
            return false;
        }

        bool StaticDataManager::RequestAllBuckets()
        {
            if (m_transferManager)
            {
                m_transferManager->RequestAllLists();
                return true;
            }
            return false;
        }

        bool StaticDataManager::SetUpdateFrequency(int timerValue)
        {
            if (m_transferManager)
            {
                return m_transferManager->SetUpdateTimer(timerValue);
            }
            return false;
        }

        bool StaticDataManager::ReloadType(const char* tagName)
        {
            // Currently this just reloads everything
            LoadAll();

            return true;
        }

        StaticDataInterfacePtr StaticDataManager::GetDataType(const char* tagName) const
        {
            AZStd::lock_guard<AZStd::mutex> thisLock(GetDataMutex());

            auto dataIter = m_data.find(tagName);
            if (dataIter != m_data.end())
            {
                return dataIter->second;
            }
            return StaticDataInterfacePtr{};
        }

        StaticDataTypeList StaticDataManager::GetDataTypeList() const
        {
            AZStd::lock_guard<AZStd::mutex> thisLock(GetDataMutex());

            StaticDataTypeList returnList;

            for (auto thisType : m_data)
            {
                returnList.push_back(thisType.first);
            }
            return returnList;
        }

        bool StaticDataManager::GetIntValue(const char* tagName, const char* structName, const char* fieldName, int& returnValue) const
        {
            return GetIntValueInternal(tagName, structName, fieldName, returnValue);
        }

        bool StaticDataManager::GetIntValueInternal(const char* tagName, const char* structName, const char* fieldName, int& returnValue) const
        {
            StaticDataInterfacePtr thisBuffer = GetDataType(tagName);
            if (thisBuffer)
            {
                return thisBuffer->GetIntValue(structName, fieldName, returnValue);
            }
            return false;
        }

        bool StaticDataManager::GetDoubleValue(const char* tagName, const char* structName, const char* fieldName, double& returnValue) const
        {
            return GetDoubleValueInternal(tagName, structName, fieldName, returnValue);
        }

        bool StaticDataManager::GetDoubleValueInternal(const char* tagName, const char* structName, const char* fieldName, double& returnValue) const
        {
            StaticDataInterfacePtr thisBuffer = GetDataType(tagName);
            if (thisBuffer)
            {
                return thisBuffer->GetDoubleValue(structName, fieldName, returnValue);
            }
            return false;
        }

        bool StaticDataManager::GetStrValue(const char* tagName, const char* structName, const char* fieldName, StringReturnType& returnStr) const
        {
            return GetStrValueInternal(tagName, structName, fieldName, returnStr);
        }

        bool StaticDataManager::GetStrValueInternal(const char* tagName, const char* structName, const char* fieldName, StringReturnType& returnStr) const
        {
            StaticDataInterfacePtr thisBuffer = GetDataType(tagName);
            if (thisBuffer)
            {
                return thisBuffer->GetStrValue(structName, fieldName, returnStr);
            }
            return false;
        }

        StaticDataManager::StaticDataInterfacePtrInternal StaticDataManager::CreateInterface(StaticDataType dataType, const char* initData, const char* tagName)
        {
            switch (dataType)
            {
            case StaticDataType::CSV:
            {
                StaticDataInterfacePtrInternal thisInterface = AZStd::make_shared<CSVStaticData>();
                thisInterface->LoadData(initData);
                SetInterface(tagName, thisInterface);
                return thisInterface;
            }
            break;
            }
            return StaticDataInterfacePtrInternal{};
        }

        void StaticDataManager::SetInterface(const char* tagName, StaticDataInterfacePtrInternal someInterface)
        {
            {
                AZStd::lock_guard<AZStd::mutex> thisLock(GetDataMutex());
                m_data[tagName] = someInterface;
            }
            EBUS_EVENT(DataUpdateBus, TypeReloaded, AZStd::string{ tagName });
        }

        void StaticDataManager::RemoveInterface(const char* tagName)
        {
            SetInterface(tagName, StaticDataInterfacePtrInternal{});
        }

        AZStd::string StaticDataManager::CalculateMD5(const char* relativeFile) 
        {
            AZStd::string sanitizedString = ResolveAndSanitize(relativeFile);

            AZStd::string returnStr("\"");

            // MD5 calculation requires 16 bytes in the dest string
            static const int stringLength = 16;
            unsigned char md5Hash[stringLength];
            char charHash[3] = { 0, 0, 0 };

            if (gEnv->pCryPak->ComputeMD5(sanitizedString.c_str(), md5Hash))
            {
                for (int i = 0; i < stringLength; ++i)
                {
                    sprintf_s(charHash, "%02x", md5Hash[i]);
                    returnStr += charHash;
                }
            }

            returnStr += "\"";
            return returnStr;
        }


        void StaticDataManager::LoadRelativeFile(const char* relativeFile)
        {
            AZStd::string sanitizedString = ResolveAndSanitize(relativeFile);
            // Currently our file names act as our data types, but are stripped of their extensions

            AZStd::string tagStr(GetTagFromFile(sanitizedString.c_str()));

            AZ::IO::HandleType readHandle = gEnv->pCryPak->FOpen(sanitizedString.c_str(), "rt");

            if (readHandle != AZ::IO::InvalidHandle)
            {
                size_t fileSize = gEnv->pCryPak->FGetSize(readHandle);
                if (fileSize > 0)
                {
                    if (sanitizedString.length() && m_monitor)
                    {
                        m_monitor->AddPath(sanitizedString, true);
                    }

                    AZStd::string fileBuf;
                    fileBuf.resize(fileSize);

                    size_t read = gEnv->pCryPak->FRead(fileBuf.data(), fileSize, readHandle);

                    CreateInterface(GetTypeFromFile(relativeFile), fileBuf.data(), tagStr.c_str());
                }
                gEnv->pCryPak->FClose(readHandle);
            }
            else
            {
                RemoveInterface(tagStr.c_str());
            }
        }

        void StaticDataManager::LoadDirectoryDataType(const char* dirName, const char* extensionType, StaticDataType dataType)
        {
            AZStd::string sanitizedString = ResolveAndSanitize(dirName);

            if (!sanitizedString.length())
            {
                return;
            }

            if (m_monitor)
            {
                m_monitor->AddPath(sanitizedString, false);
            }

            AddExtensionType(extensionType, dataType);
            AddExtensionForDirectory(sanitizedString.c_str(), extensionType);

            StaticDataFileSet dataSet;
            GetFilesForExtension(sanitizedString.c_str(), extensionType, dataSet);

            for (auto thisFile : dataSet)
            {
                LoadRelativeFile(thisFile.c_str());
            }
        }

        void StaticDataManager::GetFilesForExtension(const char* dirName, const char* extensionType, StaticDataFileSet& addSet) const
        {
            AZStd::string sanitizedString = ResolveAndSanitize(dirName);
            if (!sanitizedString.length())
            {
                return;
            }

            if (gEnv->pCryPak)
            {

                _finddata_t fd;
                intptr_t handle = gEnv->pCryPak->FindFirst((sanitizedString + "*" + extensionType).c_str(), &fd);

                if (handle < 0)
                {
                    return;
                }

                do
                {

                    addSet.insert(sanitizedString + fd.name);

                } while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);
                gEnv->pCryPak->FindClose(handle);
            }
        }

        AZStd::string StaticDataManager::ResolveAndSanitize(const char* dirName) const
        {
            char resolvedGameFolder[AZ_MAX_PATH_LEN] = { 0 };
            if (!gEnv->pFileIO->ResolvePath(dirName, resolvedGameFolder, AZ_MAX_PATH_LEN))
            {
                return{};
            }
            AZStd::string sanitizedString;
            sanitizedString = m_monitor ? m_monitor->GetSanitizedName(resolvedGameFolder) : resolvedGameFolder;

            return sanitizedString;
        }

        StaticDataFileSet StaticDataManager::GetFilesForDirectory(const char* dirName) const
        {
            StaticDataFileSet fileSet;

            AZStd::string sanitizedString = ResolveAndSanitize(dirName);
            if (!sanitizedString.length())
            {
                return fileSet;
            }

            StaticDataExtensionList extensionList = GetExtensionsForDirectory(sanitizedString.c_str());

            for (auto fileExtension : extensionList)
            {
                GetFilesForExtension(sanitizedString.c_str(), fileExtension.c_str(), fileSet);
            }
            return fileSet;
        }

        void StaticDataManager::AddExtensionType(const char* extensionStr, StaticDataType dataType)
        {
            m_extensionToTypeMap[GetExtensionFromFile(extensionStr)] = dataType;
        }

        void StaticDataManager::AddExtensionForDirectory(const char* dirName, const char* extensionName)
        {
            auto entryIter = m_directoryToExtensionMap.find(dirName);
            if (entryIter == m_directoryToExtensionMap.end())
            {
                m_directoryToExtensionMap[dirName] = StaticDataExtensionList{extensionName};
            }
            else
            {

                for (auto thisEntry : entryIter->second)
                {
                    if (thisEntry == extensionName)
                    {
                        return;
                    }
                }
                entryIter->second.push_back(extensionName);
            }
        }
        StaticDataExtensionList StaticDataManager::GetExtensionsForDirectory(const char* dirName) const
        {
            auto entryIter = m_directoryToExtensionMap.find(dirName);
            if (entryIter == m_directoryToExtensionMap.end())
            {
                return StaticDataExtensionList{ };
            }

            return entryIter->second;
        }

        StaticDataManager::StaticDataType StaticDataManager::GetTypeFromExtension(const AZStd::string& extensionStr) const
        {
            auto findIter = m_extensionToTypeMap.find(extensionStr);
            if (findIter != m_extensionToTypeMap.end())
            {
                return findIter->second;
            }
            return StaticDataType::NONE;
        }
        AZStd::string StaticDataManager::GetExtensionFromFile(const char* fileName) const
        {
            AZStd::string fileStr(fileName);
            AZStd::size_t findPos = fileStr.find_last_of('.');

            if (findPos == AZStd::string::npos)
            {
                return{};
            }
            return fileStr.substr(findPos + 1);
        }
        StaticDataManager::StaticDataType StaticDataManager::GetTypeFromFile(const char* fileStr) const
        {
            return GetTypeFromExtension(GetExtensionFromFile(fileStr).c_str());
        }
        StaticDataTagType StaticDataManager::GetTagFromFile(const char* fileName) const
        {
            AZStd::string fileStr(fileName);
            AZStd::size_t findPos = fileStr.find_last_of('\\');

            if (findPos != AZStd::string::npos)
            {
                fileStr = fileStr.substr(findPos + 1);
            }
            findPos = fileStr.find_last_of('/');

            if (findPos != AZStd::string::npos)
            {
                fileStr = fileStr.substr(findPos + 1);
            }
            findPos = fileStr.find_last_of('.');
            if (findPos != AZStd::string::npos)
            {
                fileStr = fileStr.substr(0, findPos);
            }
            return fileStr;
        }
    }
}