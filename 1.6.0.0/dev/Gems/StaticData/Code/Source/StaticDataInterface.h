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

#include <AzCore/std/string/string.h>
#include <AzCore/EBus/EBus.h>

namespace CloudCanvas
{
    namespace StaticData
    {
        using StringReturnType = AZStd::string;

        class DataUpdateGroup : public AZ::EBusTraits
        {
        public:
            virtual void TypeReloaded(const AZStd::string& reloadType) = 0;

            virtual ~DataUpdateGroup() {}
        };

        typedef AZ::EBus<DataUpdateGroup> DataUpdateBus;

        class StaticDataInterface
        {
        public:
            virtual ~StaticDataInterface() {}

            virtual bool GetIntValue(const char* structName, const char* fieldName, int& returnValue) const = 0;
            virtual bool GetStrValue(const char* structName, const char* fieldName, StringReturnType& returnStr) const = 0;
            virtual bool GetDoubleValue(const char* structName, const char* fieldName, double& returnValue) const = 0;

            friend class StaticDataManager;

        protected:
            virtual bool LoadData(const char* dataBuffer) = 0;
        private:

        };

    }
}
