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

#include <stdint.h>
#include <AzCore/Memory/Memory.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/utils.h>
#include <SceneAPI/SceneCore/SceneCoreConfiguration.h>
#include <SceneAPI/SceneCore/Containers/Views/View.h>
#include <SceneAPI/SceneCore/Containers/Views/ConvertIterator.h>
#include <SceneAPI/SceneCore/DataTypes/IManifestObject.h>

namespace AZ
{
    class SerializeContext;
    namespace IO
    {
        class GenericStream;
    }

    namespace SceneAPI
    {
        namespace Containers
        {
            // Scene manifests hold arbitrary meta data about a scene in a dictionary-like fashion.
            //      This can include data such as export rules.
            class SceneManifest
            {
                friend class SceneManifestContainer;
            public:
                AZ_CLASS_ALLOCATOR_DECL

                SCENE_CORE_API static AZStd::shared_ptr<const DataTypes::IManifestObject> SceneManifestConstDataConverter(
                    const AZStd::shared_ptr<DataTypes::IManifestObject>& value);

                using Index = size_t;
                static const Index s_invalidIndex = static_cast<Index>(-1);

                using StringHasher = AZStd::hash<AZStd::string>;
                using StringHash = size_t;
                using NameLookup = AZStd::unordered_multimap<StringHash, Index>;

                using StorageHash = const DataTypes::IManifestObject *;
                using StorageLookup = AZStd::unordered_map<StorageHash, Index>;

                using NameStorageType = AZStd::string;
                using NameStorage = AZStd::vector<NameStorageType>;
                using NameStorageConstData = Views::View<NameStorage::const_iterator>;

                using ValueStorageType = AZStd::shared_ptr<DataTypes::IManifestObject>;
                using ValueStorage = AZStd::vector<ValueStorageType>;
                using ValueStorageData = Views::View<ValueStorage::const_iterator>;

                using ValueStorageConstDataIteratorWrapper = Views::ConvertIterator<ValueStorage::const_iterator,
                            decltype(SceneManifestConstDataConverter(AZStd::shared_ptr<DataTypes::IManifestObject>()))>;
                using ValueStorageConstData = Views::View<ValueStorageConstDataIteratorWrapper>;

                using ReflectionElement = AZStd::pair<AZStd::string, AZStd::shared_ptr<SceneAPI::DataTypes::IManifestObject> >;

                SCENE_CORE_API void Clear();
                inline bool IsEmpty() const;

                inline bool AddEntry(const char* name, const AZStd::shared_ptr<DataTypes::IManifestObject>& value);
                inline bool AddEntry(const AZStd::string& name, const AZStd::shared_ptr<DataTypes::IManifestObject>& value);
                inline bool AddEntry(AZStd::string&& name, const AZStd::shared_ptr<DataTypes::IManifestObject>& value);
                inline bool AddEntry(const char* name, AZStd::shared_ptr<DataTypes::IManifestObject>&& value);
                inline bool AddEntry(const AZStd::string& name, AZStd::shared_ptr<DataTypes::IManifestObject>&& value);
                SCENE_CORE_API bool AddEntry(AZStd::string&& name, AZStd::shared_ptr<DataTypes::IManifestObject>&& value);
                inline bool RemoveEntry(const char* name);
                SCENE_CORE_API bool RemoveEntry(const AZStd::string& name);

                inline bool HasEntry(const char* name) const;
                inline bool HasEntry(const AZStd::string& name) const;

                inline bool RenameEntry(const char* oldName, const char* newName);
                inline bool RenameEntry(const char* oldName, const AZStd::string& newName);
                inline bool RenameEntry(const char* oldName, AZStd::string&& newName);
                inline bool RenameEntry(const AZStd::string& oldName, const char* newName);
                inline bool RenameEntry(const AZStd::string& oldName, const AZStd::string& newName);
                SCENE_CORE_API bool RenameEntry(const AZStd::string& oldName, AZStd::string&& newName);

                SCENE_CORE_API AZStd::shared_ptr<DataTypes::IManifestObject> FindValue(const char* name);
                SCENE_CORE_API AZStd::shared_ptr<DataTypes::IManifestObject> FindValue(const AZStd::string& name);
                SCENE_CORE_API AZStd::shared_ptr<const DataTypes::IManifestObject> FindValue(const char* name) const;
                SCENE_CORE_API AZStd::shared_ptr<const DataTypes::IManifestObject> FindValue(const AZStd::string& name) const;

                // Finds the name belonging to the manifest object. A nullptr or invalid object will return an empty name;
                SCENE_CORE_API const AZStd::string& FindName(const DataTypes::IManifestObject* const value) const;

                inline size_t GetEntryCount() const;
                SCENE_CORE_API const AZStd::string& GetName(Index index) const;
                inline AZStd::shared_ptr<DataTypes::IManifestObject> GetValue(Index index);
                inline AZStd::shared_ptr<const DataTypes::IManifestObject> GetValue(Index index) const;
                SCENE_CORE_API Index FindIndex(const char* name) const;
                SCENE_CORE_API Index FindIndex(const AZStd::string& name) const;
                // Finds the index of the given manifest object. A nullptr or invalid object will return s_invalidIndex.
                SCENE_CORE_API Index FindIndex(const DataTypes::IManifestObject* const value) const;

                inline NameStorageConstData GetNameStorage() const;
                inline ValueStorageData GetValueStorage();
                inline ValueStorageConstData GetValueStorage() const;

                // If SceneManifest if part of a class normal reflection will work as expected, but if it's directly serialized
                //      additional arguments for the common serialization functions are required. This function is utility uses
                //      the correct arguments to load the SceneManifest from a stream in-place.
                SCENE_CORE_API bool LoadFromStream(IO::GenericStream& stream, SerializeContext* context = nullptr);
                // If SceneManifest if part of a class normal reflection will work as expected, but if it's directly serialized
                //      additional arguments for the common serialization functions are required. This function is utility uses
                //      the correct arguments to save the SceneManifest to a stream.
                SCENE_CORE_API bool SaveToStream(IO::GenericStream& stream, SerializeContext* context = nullptr) const;

            private:
                SCENE_CORE_API NameLookup::iterator FindNameIterator(const AZStd::string& name);
                SCENE_CORE_API NameLookup::iterator FindNameIterator(StringHash hash, const AZStd::string& name);
                SCENE_CORE_API NameLookup::const_iterator FindNameIterator(const AZStd::string& name) const;
                SCENE_CORE_API NameLookup::const_iterator FindNameIterator(StringHash hash, const AZStd::string& name) const;

                NameLookup m_nameLookup;
                StorageLookup m_storageLookup;
                NameStorage m_names;
                ValueStorage m_values;

                // During serialization a temporary value is needed by the serializer to fill in. Since there's no direct
                //      storage object to return an address of, use a placeholder instead. Allocating one on the heap for
                //      every value would be expensive and static place holders can become problematic when multiple manifests
                //      are serialized at the same time.
                ReflectionElement m_reflectionPlaceholder;
            };
        } // Containers
    } // SceneAPI
} // AZ

#include <SceneAPI/SceneCore/Containers/SceneManifest.inl>