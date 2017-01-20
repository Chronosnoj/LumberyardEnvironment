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

#include <vector>
#include <unordered_map>
#include <SceneAPI/SceneCore/Import/IImportersList.h>
#include <SceneAPI/SceneCore/SceneCoreConfiguration.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Import
        {
            class ImportersList
                : public IImportersList
            {
            public:
                SCENE_CORE_API bool RegisterImporter(const char* fileExtension, std::shared_ptr<IImporter> importer) override;
                SCENE_CORE_API bool RegisterImporter(const std::string& fileExtension, std::shared_ptr<IImporter> importer) override;
                SCENE_CORE_API bool RegisterImporter(std::string&& fileExtension, std::shared_ptr<IImporter> importer) override;

                SCENE_CORE_API size_t GetExtensionCount() const override;
                SCENE_CORE_API const std::string& GetExtension(size_t index) const override;

                SCENE_CORE_API std::shared_ptr<IImporter> FindImporter(const char* fileExtension) override;
                SCENE_CORE_API std::shared_ptr<IImporter> FindImporter(const std::string& fileExtension) override;
                SCENE_CORE_API std::shared_ptr<const IImporter> FindImporter(const char* fileExtension) const override;
                SCENE_CORE_API std::shared_ptr<const IImporter> FindImporter(const std::string& fileExtension) const override;

            private:
                using StringHasher = std::hash<std::string>;
                using StringHash = size_t;

                std::vector<std::string> m_names;
                std::unordered_map<StringHash, std::shared_ptr<IImporter> > m_importers;
            };
        } // Import
    } // SceneAPI
} // AZ