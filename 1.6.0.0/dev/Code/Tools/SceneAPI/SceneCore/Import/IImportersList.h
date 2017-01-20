#pragma once

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

#include <memory>
#include <string>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Import
        {
            class IImporter;

            // Interface for creating importer lists. The main purpose is to be able to
            //      register multiple file extensions with the same importer and then
            //      during processing find the appropriate importer for a file extension.
            class IImportersList
            {
            public:
                virtual ~IImportersList() = 0;

                virtual bool RegisterImporter(const char* fileExtension, std::shared_ptr<IImporter> importer) = 0;
                virtual bool RegisterImporter(const std::string& fileExtension, std::shared_ptr<IImporter> importer) = 0;
                virtual bool RegisterImporter(std::string&& fileExtension, std::shared_ptr<IImporter> importer) = 0;

                virtual size_t GetExtensionCount() const = 0;
                virtual const std::string& GetExtension(size_t index) const = 0;

                virtual std::shared_ptr<IImporter> FindImporter(const char* fileExtension) = 0;
                virtual std::shared_ptr<IImporter> FindImporter(const std::string& fileExtension) = 0;
                virtual std::shared_ptr<const IImporter> FindImporter(const char* fileExtension) const = 0;
                virtual std::shared_ptr<const IImporter> FindImporter(const std::string& fileExtension) const = 0;
            };

            inline IImportersList::~IImportersList()
            {
            }
        } // Import
    } // SceneAPI
} // AZ