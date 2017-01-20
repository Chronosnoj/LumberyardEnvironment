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
#pragma once

#include <StaticDataInterface.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/parallel/mutex.h>

#include <StaticData/StaticDataBus.h>

namespace CloudCanvas
{
    namespace StaticData
    {
        class StaticDataTransferManager;
        class IStaticDataMonitor;

        using StaticDataInterfacePtr = AZStd::shared_ptr<const StaticDataInterface>;
        using StaticDataMapType = AZStd::unordered_map<StaticDataTagType, StaticDataInterfacePtr>;
        using StaticDataExtensionList = AZStd::vector<StaticDataExtension>;

        static const char* csvDir = "@assets@\\staticdata\\csv\\";

        class StaticDataManager
            : public StaticDataRequestBus::Handler
        {
        public:

            StaticDataManager();
            ~StaticDataManager() override;

            bool GetIntValue(const char* tagName, const char* structName, const char* fieldName, int& returnValue) const override;
            bool GetStrValue(const char* tagName, const char* structName, const char* fieldName, StringReturnType& returnStr) const override;
            bool GetDoubleValue(const char* tagName, const char* structName, const char* fieldName, double& returnValue) const override;

            bool ReloadTagType(const char* tagName) override;
            void LoadRelativeFile(const char* relativeFile) override;

            bool AddMonitoredBucket(const char* bucketName) override;
            bool RemoveMonitoredBucket(const char* bucketName) override;

            bool RequestBucket(const char* bucketName) override;
            bool RequestAllBuckets() override;
            bool SetUpdateFrequency(int timerValue) override;

            StaticDataTypeList GetDataTypeList() const override;

            enum class StaticDataType
            {
                NONE = 0,
                CSV = 1,
            };

            StaticDataInterfacePtr GetDataType(const char* tagName) const;
            AZStd::mutex& GetDataMutex() const { return m_dataMutex; }

            StaticDataType GetTypeFromExtension(const AZStd::string& extensionStr) const;
            AZStd::string GetExtensionFromFile(const char* fileName) const;
            StaticDataType GetTypeFromFile(const char* fileName) const;
            StaticDataTagType GetTagFromFile(const char* fileName) const;

            AZStd::string ResolveAndSanitize(const char* dirName) const;

            AZStd::string CalculateMD5(const char* relativeFile);

            AZStd::string GetEditorGameDir() const { return m_editorGameDir; }

            friend class IStaticDataMonitor;

        private:
            using StaticDataInterfacePtrInternal = AZStd::shared_ptr<StaticDataInterface>;

            bool GetIntValueInternal(const char* tagName, const char* structName, const char* fieldName, int& returnValue) const;
            bool GetStrValueInternal(const char* tagName, const char* structName, const char* fieldName, StringReturnType& returnStr) const;
            bool GetDoubleValueInternal(const char* tagName, const char* structName, const char* fieldName, double& returnValue) const;

            bool ReloadType(const char* tagName);

            void LoadDirectoryDataType(const char* dirName, const char* fileExtension, StaticDataType dataType);

            bool LoadAll();

            void AddExtensionType(const char* extensionStr, StaticDataType dataType);

            StaticDataInterfacePtrInternal CreateInterface(StaticDataType dataType, const char* initData, const char* tagName);
            void RemoveInterface(const char* tagName);
            void SetInterface(const char* tagName, StaticDataInterfacePtrInternal someInterface);

            StaticDataExtensionList GetExtensionsForDirectory(const char* dirName) const;
            void AddExtensionForDirectory(const char* dirName, const char* extensionName);

            void GetFilesForExtension(const char* dirName, const char* extensionName, StaticDataFileSet& addSet) const;
            StaticDataFileSet GetFilesForDirectory(const char* dirName) const override;

            void SetMonitor(IStaticDataMonitor* newMonitor) override { m_monitor = newMonitor; }

            void SetEditorGameDir(const char* dirName) override { m_editorGameDir = dirName; }

            StaticDataMapType m_data;

            AZStd::unordered_map<StaticDataTagType, StaticDataType> m_extensionToTypeMap;
            AZStd::unordered_map<AZStd::string, StaticDataExtensionList> m_directoryToExtensionMap;

            AZStd::shared_ptr<StaticDataTransferManager> m_transferManager;
            mutable AZStd::mutex m_dataMutex;

            IStaticDataMonitor* m_monitor{ nullptr };

            AZStd::string m_editorGameDir;
        };
    }
}