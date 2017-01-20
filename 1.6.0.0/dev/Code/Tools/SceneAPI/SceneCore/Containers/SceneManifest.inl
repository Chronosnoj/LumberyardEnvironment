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

#include <AzCore/Debug/Trace.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            bool SceneManifest::IsEmpty() const
            {
                // Any of the containers would do as they should be in-sync with each other, so pick one randomly.
                AZ_Assert(m_names.empty() == m_nameLookup.empty(), "SceneManifest name and name-lookup table have gone out of lockstep.");
                AZ_Assert(m_names.empty() == m_storageLookup.empty(), "SceneManifest name and storage-lookup table have gone out of lockstep.");
                AZ_Assert(m_names.empty() == m_values.empty(), "SceneManifest name and values have gone out of lockstep.");
                return m_names.empty();
            }

            bool SceneManifest::AddEntry(const char* name, const AZStd::shared_ptr<DataTypes::IManifestObject>& value)
            {
                return name ? AddEntry(AZStd::string(name), AZStd::shared_ptr<DataTypes::IManifestObject>(value)) : false;
            }

            bool SceneManifest::AddEntry(const AZStd::string& name, const AZStd::shared_ptr<DataTypes::IManifestObject>& value)
            {
                return AddEntry(AZStd::string(name), AZStd::shared_ptr<DataTypes::IManifestObject>(value));
            }

            bool SceneManifest::AddEntry(AZStd::string&& name, const AZStd::shared_ptr<DataTypes::IManifestObject>& value)
            {
                return AddEntry(name, AZStd::shared_ptr<DataTypes::IManifestObject>(value));
            }

            bool SceneManifest::AddEntry(const char* name, AZStd::shared_ptr<DataTypes::IManifestObject>&& value)
            {
                return name ? AddEntry(AZStd::string(name), AZStd::move(value)) : false;
            }

            bool SceneManifest::AddEntry(const AZStd::string& name, AZStd::shared_ptr<DataTypes::IManifestObject>&& value)
            {
                return AddEntry(AZStd::string(name), AZStd::move(value));
            }

            bool SceneManifest::RemoveEntry(const char* name)
            {
                return name ? RemoveEntry(AZStd::string(name)) : false;
            }

            bool SceneManifest::HasEntry(const char* name) const
            {
                return name ? FindNameIterator(name) != m_nameLookup.end() : false;
            }

            bool SceneManifest::HasEntry(const AZStd::string& name) const
            {
                return FindNameIterator(name) != m_nameLookup.end();
            }

            bool SceneManifest::RenameEntry(const char* oldName, const char* newName)
            {
                return (oldName && newName) ? RenameEntry(AZStd::string(oldName), AZStd::string(newName)) : false;
            }

            bool SceneManifest::RenameEntry(const char* oldName, const AZStd::string& newName)
            {
                return oldName ? RenameEntry(AZStd::string(oldName), AZStd::string(newName)) : false;
            }

            bool SceneManifest::RenameEntry(const char* oldName, AZStd::string&& newName)
            {
                return oldName ? RenameEntry(AZStd::string(oldName), AZStd::move(newName)) : false;
            }

            bool SceneManifest::RenameEntry(const AZStd::string& oldName, const char* newName)
            {
                return newName ? RenameEntry(oldName, AZStd::string(newName)) : false;
            }

            bool SceneManifest::RenameEntry(const AZStd::string& oldName, const AZStd::string& newName)
            {
                return RenameEntry(oldName, AZStd::string(newName));
            }

            size_t SceneManifest::GetEntryCount() const
            {
                // Any of the containers would do as they should be in-sync with each other, so pick one randomly.
                AZ_Assert(m_names.size() == m_nameLookup.size(),
                    "SceneManifest name and name-lookup table have gone out of lockstep. (%i vs. %i)", m_names.size(), m_nameLookup.size());
                AZ_Assert(m_names.size() == m_storageLookup.size(),
                    "SceneManifest name and storage-lookup table have gone out of lockstep. (%i vs. %i)", m_names.size(), m_storageLookup.size());
                AZ_Assert(m_names.size() == m_values.size(),
                    "SceneManifest name and values have gone out of lockstep. (%i vs. %i)", m_names.size(), m_values.size());
                return m_names.size();
            }

            AZStd::shared_ptr<DataTypes::IManifestObject> SceneManifest::GetValue(Index index)
            {
                return index < m_values.size() ? m_values[index] : AZStd::shared_ptr<DataTypes::IManifestObject>();
            }

            AZStd::shared_ptr<const DataTypes::IManifestObject> SceneManifest::GetValue(Index index) const
            {
                return index < m_values.size() ? m_values[index] : AZStd::shared_ptr<const DataTypes::IManifestObject>();
            }

            SceneManifest::NameStorageConstData SceneManifest::GetNameStorage() const
            {
                return NameStorageConstData(m_names.begin(), m_names.end());
            }

            SceneManifest::ValueStorageData SceneManifest::GetValueStorage()
            {
                return ValueStorageData(m_values.begin(), m_values.end());
            }

            SceneManifest::ValueStorageConstData SceneManifest::GetValueStorage() const
            {
                return ValueStorageConstData(
                    Views::MakeConvertIterator(m_values.cbegin(), SceneManifestConstDataConverter),
                    Views::MakeConvertIterator(m_values.cend(), SceneManifestConstDataConverter));
            }
        } // Containers
    } // SceneAPI
} // AZ