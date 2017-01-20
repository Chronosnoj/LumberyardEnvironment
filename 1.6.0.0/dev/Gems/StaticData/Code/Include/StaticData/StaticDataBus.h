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

#include <AzCore/EBus/EBus.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/containers/unordered_set.h>

namespace CloudCanvas
{
    namespace StaticData
    {
        using StringReturnType = AZStd::string;
        using StaticDataTagType = AZStd::string;
        using StaticDataExtension = AZStd::string;
        using StaticDataTypeList = AZStd::vector<StaticDataTagType>;
        using StaticDataFileSet = AZStd::unordered_set<AZStd::string>;

        class IStaticDataMonitor;

        class IStaticDataManager
        {
        public:
            virtual bool GetIntValue(const char* tagName, const char* structName, const char* fieldName, int& returnValue) const = 0;
            virtual bool GetStrValue(const char* tagName, const char* structName, const char* fieldName, StringReturnType& returnStr) const = 0;
            virtual bool GetDoubleValue(const char* tagName, const char* structName, const char* fieldName, double& returnValue) const = 0;

            virtual bool ReloadTagType(const char* tagName) = 0;
            virtual void LoadRelativeFile(const char* relativeFile) = 0;

            virtual bool AddMonitoredBucket(const char* bucketName) = 0;
            virtual bool RemoveMonitoredBucket(const char* bucketName) = 0;

            virtual bool RequestBucket(const char* bucketName) = 0;
            virtual bool RequestAllBuckets() = 0;
            virtual bool SetUpdateFrequency(int timerValue) = 0;

            virtual StaticDataTypeList GetDataTypeList() const = 0;
            virtual StaticDataFileSet GetFilesForDirectory(const char* dirName) const = 0;
            
            friend class StaticDataMonitor;
        private:
            virtual void SetMonitor(IStaticDataMonitor* newMonitor) = 0;
            virtual void SetEditorGameDir(const char* dirName) = 0;
        };

        class StaticDataRequestBusTraits
            : public AZ::EBusTraits
        {
        public:
            //////////////////////////////////////////////////////////////////////////
            // EBusTraits overrides
            static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
            static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
            //////////////////////////////////////////////////////////////////////////
        };

        using StaticDataRequestBus = AZ::EBus<IStaticDataManager, StaticDataRequestBusTraits>;
    }
}