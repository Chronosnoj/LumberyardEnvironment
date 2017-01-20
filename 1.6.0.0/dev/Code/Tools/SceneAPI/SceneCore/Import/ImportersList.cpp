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

#include <SceneAPI/SceneCore/Import/ImportersList.h>
#include <algorithm>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Import
        {
            void MakeLowerCase(std::string& inString)
            {
                std::transform(inString.begin(), inString.end(), inString.begin(), ::tolower);
            }

            bool ImportersList::RegisterImporter(const char* fileExtension, std::shared_ptr<IImporter> importer)
            {
                return RegisterImporter(std::string(fileExtension), importer);
            }

            bool ImportersList::RegisterImporter(const std::string& fileExtension, std::shared_ptr<IImporter> importer)
            {
                if (fileExtension.empty() || importer == nullptr)
                {
                    return false;
                }

                std::string lowerCaseFileExtension = fileExtension;
                MakeLowerCase(lowerCaseFileExtension);

                StringHash hash = StringHasher()(lowerCaseFileExtension);
                auto it = m_importers.find(hash);
                if (it == m_importers.end())
                {
                    m_importers[hash] = importer;
                    m_names.push_back(std::move(lowerCaseFileExtension));
                    return true;
                }
                else
                {
                    return false;
                }
            }

            bool ImportersList::RegisterImporter(std::string&& fileExtension, std::shared_ptr<IImporter> importer)
            {
                if (fileExtension.empty() || importer == nullptr)
                {
                    return false;
                }

                std::string lowerCaseFileExtension = fileExtension;
                MakeLowerCase(lowerCaseFileExtension);

                StringHash hash = StringHasher()(lowerCaseFileExtension);
                auto it = m_importers.find(hash);
                if (it == m_importers.end())
                {
                    m_importers[hash] = importer;
                    m_names.push_back(std::move(lowerCaseFileExtension));
                    return true;
                }
                else
                {
                    return false;
                }
            }

            size_t ImportersList::GetExtensionCount() const
            {
                return m_names.size();
            }

            const std::string& ImportersList::GetExtension(size_t index) const
            {
                return m_names[index];
            }

            std::shared_ptr<IImporter> ImportersList::FindImporter(const char* fileExtension)
            {
                std::string lowerCaseFileExtension = fileExtension;
                MakeLowerCase(lowerCaseFileExtension);

                auto it = m_importers.find(StringHasher()(lowerCaseFileExtension));
                return it != m_importers.end() ? (*it).second : nullptr;
            }

            std::shared_ptr<IImporter> ImportersList::FindImporter(const std::string& fileExtension)
            {
                std::string lowerCaseFileExtension = fileExtension;
                MakeLowerCase(lowerCaseFileExtension);

                auto it = m_importers.find(StringHasher()(lowerCaseFileExtension));
                return it != m_importers.end() ? (*it).second : nullptr;
            }

            std::shared_ptr<const IImporter> ImportersList::FindImporter(const char* fileExtension) const
            {
                std::string lowerCaseFileExtension = fileExtension;
                MakeLowerCase(lowerCaseFileExtension);

                auto it = m_importers.find(StringHasher()(lowerCaseFileExtension));
                return it != m_importers.end() ? (*it).second : nullptr;
            }

            std::shared_ptr<const IImporter> ImportersList::FindImporter(const std::string& fileExtension) const
            {
                std::string lowerCaseFileExtension = fileExtension;
                MakeLowerCase(lowerCaseFileExtension);

                auto it = m_importers.find(StringHasher()(lowerCaseFileExtension));
                return it != m_importers.end() ? (*it).second : nullptr;
            }
        } // Import
    } // SceneAPI
} // AZ