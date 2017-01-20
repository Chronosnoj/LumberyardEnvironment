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

#include <algorithm>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <SceneAPI/SceneCore/Containers/SceneManifest.h>
#include <SceneAPI/SceneCore/Containers/SceneManifestReflection.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            AZ_CLASS_ALLOCATOR_IMPL(SceneManifest, AZ::SystemAllocator, 0)
            
            void SceneManifest::Clear()
            {
                m_nameLookup.clear();
                m_storageLookup.clear();
                m_names.clear();
                m_values.clear();
            }

            bool SceneManifest::AddEntry(AZStd::string&& name, AZStd::shared_ptr<DataTypes::IManifestObject>&& value)
            {
                size_t stringHash = StringHasher()(name);
                auto itName = FindNameIterator(stringHash, name);
                if (itName != m_nameLookup.end())
                {
                    return false;
                }

                auto itValue = m_storageLookup.find(value.get());
                if (itValue != m_storageLookup.end())
                {
                    return false;
                }

                Index index = aznumeric_caster(m_values.size());
                m_nameLookup.insert(NameLookup::value_type(stringHash, index));
                m_storageLookup[value.get()] = index;
                m_names.push_back(AZStd::move(name));
                m_values.push_back(AZStd::move(value));

                AZ_Assert(m_values.size() == m_names.size(), "SceneManifest name and value tables have gone out of lockstep (%i vs %i)", m_values.size(), m_names.size());
                AZ_Assert(m_values.size() == m_nameLookup.size(), "SceneManifest name and lookup tables have gone out of lockstep (%i vs %i)", m_values.size(), m_nameLookup.size());
                AZ_Assert(m_values.size() == m_storageLookup.size(), "SceneManifest name and lookup tables have gone out of lockstep (%i vs %i)", m_values.size(), m_storageLookup.size());
                return true;
            }

            bool SceneManifest::RemoveEntry(const AZStd::string& name)
            {
                auto nameLookupIt = FindNameIterator(name);
                if (nameLookupIt == m_nameLookup.end())
                {
                    return false;
                }

                Index index = (*nameLookupIt).second;

                auto storageLookupIt = m_storageLookup.find(m_values[index].get());
                if (storageLookupIt == m_storageLookup.end())
                {
                    AZ_Assert(false, "Manifest entry '%s' has no value storage lookup. Tables have gone out of sync.", name.c_str());
                    return false;
                }

                m_nameLookup.erase(nameLookupIt);
                for (auto& it : m_nameLookup)
                {
                    AZ_Assert(it.second != index, "Multiple names are referencing the same value.");
                    if (it.second > index)
                    {
                        it.second--;
                    }
                }

                m_storageLookup.erase(storageLookupIt);
                for (auto& it : m_storageLookup)
                {
                    AZ_Assert(it.second != index, "Multiple names are referencing the same value.");
                    if (it.second > index)
                    {
                        it.second--;
                    }
                }

                m_names.erase(m_names.begin() + index);
                m_values.erase(m_values.begin() + index);

                return true;
            }

            bool SceneManifest::RenameEntry(const AZStd::string& oldName, AZStd::string&& newName)
            {
                auto it = FindNameIterator(oldName);
                if (it == m_nameLookup.end())
                {
                    return false;
                }

                StringHash newNameHash = StringHasher()(newName);
                if (FindNameIterator(newNameHash, newName) != m_nameLookup.end())
                {
                    return false;
                }

                Index index = (*it).second;
                m_nameLookup.erase(it);
                m_names[index] = std::move(newName);
                m_nameLookup.insert(NameLookup::value_type(newNameHash, index));

                return true;
            }

            AZStd::shared_ptr<DataTypes::IManifestObject> SceneManifest::FindValue(const char* name)
            {
                if (!name)
                {
                    return AZStd::shared_ptr<DataTypes::IManifestObject>();
                }
                auto it = FindNameIterator(name);
                return it != m_nameLookup.end() ? m_values[(*it).second] : AZStd::shared_ptr<DataTypes::IManifestObject>();
            }

            AZStd::shared_ptr<DataTypes::IManifestObject> SceneManifest::FindValue(const AZStd::string& name)
            {
                auto it = FindNameIterator(name);
                return it != m_nameLookup.end() ? m_values[(*it).second] : AZStd::shared_ptr<DataTypes::IManifestObject>();
            }

            AZStd::shared_ptr<const DataTypes::IManifestObject> SceneManifest::FindValue(const char* name) const
            {
                if (!name)
                {
                    return AZStd::shared_ptr<DataTypes::IManifestObject>();
                }
                auto it = FindNameIterator(name);
                return it != m_nameLookup.end() ? m_values[(*it).second] : AZStd::shared_ptr<DataTypes::IManifestObject>();
            }

            AZStd::shared_ptr<const DataTypes::IManifestObject> SceneManifest::FindValue(const AZStd::string& name) const
            {
                auto it = FindNameIterator(name);
                return it != m_nameLookup.end() ? m_values[(*it).second] : AZStd::shared_ptr<DataTypes::IManifestObject>();
            }

            const AZStd::string& SceneManifest::FindName(const DataTypes::IManifestObject* const value) const
            {
                auto it = m_storageLookup.find(value);
                if (it != m_storageLookup.end())
                {
                    return m_names[(*it).second];
                }
                else
                {
                    static AZStd::string emptyName;
                    return emptyName;
                }
            }

            const AZStd::string& SceneManifest::GetName(Index index) const
            {
                if (index < m_names.size())
                {
                    return m_names[index];
                }
                else
                {
                    static AZStd::string emptyName;
                    return emptyName;
                }
            }

            SceneManifest::Index SceneManifest::FindIndex(const char* name) const
            {
                if (!name)
                {
                    return s_invalidIndex;
                }
                auto it = FindNameIterator(name);
                return it != m_nameLookup.end() ? (*it).second : s_invalidIndex;
            }

            SceneManifest::Index SceneManifest::FindIndex(const AZStd::string& name) const
            {
                auto it = FindNameIterator(name);
                return it != m_nameLookup.end() ? (*it).second : s_invalidIndex;
            }

            SceneManifest::Index SceneManifest::FindIndex(const DataTypes::IManifestObject* const value) const
            {
                auto it = m_storageLookup.find(value);
                return it != m_storageLookup.end() ? (*it).second : s_invalidIndex;
            }

            bool SceneManifest::LoadFromStream(IO::GenericStream& stream, SerializeContext* context)
            {
                if (!context)
                {
                    EBUS_EVENT_RESULT(context, ComponentApplicationBus, GetSerializeContext);
                    AZ_Assert(context, "No serialize context");
                }

                // Merging of manifests probably introduces more problems than it would solve. In particular there's a high risk of names
                //      colliding. If merging is every required a dedicated function would be preferable that would allow some measure of
                //      control over the merging behavior. To avoid merging manifests, always clear out the old manifest.
                Clear();

                bool result = ObjectStream::LoadBlocking(&stream, *context, ObjectStream::ClassReadyCB(), ObjectStream::FilterDescriptor(), 
                    // During loading this function is potentially called multiple times. Depending on whether instance and/or classData
                    //      is set, that variable needs to set the relevant pointer.
                    [this](void** instance, const SerializeContext::ClassData** classData, const Uuid& classId, SerializeContext* context)
                    {
                        (void)context;

                        if (classId == SerializeGenericTypeInfo<SceneManifest>::GetClassTypeId())
                        {
                            if (instance)
                            {
                                *instance = this;
                            }
                            if (classData)
                            {
                                *classData = SerializeGenericTypeInfo<SceneManifest>::GetGenericInfo()->GetClassData();
                            }
                        }
                    });

                if (!result)
                {
                    Clear();
                }
                return result;
            }
            
            bool SceneManifest::SaveToStream(IO::GenericStream& stream, SerializeContext* context) const
            {
                return Utils::SaveObjectToStream(stream, ObjectStream::ST_XML, this, context,
                    SerializeGenericTypeInfo<SceneManifest>::GetGenericInfo()->GetClassData());
            }

            SceneManifest::NameLookup::iterator SceneManifest::FindNameIterator(const AZStd::string& name)
            {
                StringHash hash = StringHasher()(name);
                return FindNameIterator(hash, name);
            }

            SceneManifest::NameLookup::iterator SceneManifest::FindNameIterator(StringHash hash, const AZStd::string& name)
            {
                auto range = m_nameLookup.equal_range(hash);
                // Always check the name, even if there's only one entry as the hash can be a clash with
                //      the single entry.
                for (auto it = range.first; it != range.second; ++it)
                {
                    if (m_names[it->second] == name)
                    {
                        return it;
                    }
                }

                return m_nameLookup.end();
            }

            SceneManifest::NameLookup::const_iterator SceneManifest::FindNameIterator(const AZStd::string& name) const
            {
                StringHash hash = StringHasher()(name);
                return FindNameIterator(hash, name);
            }

            SceneManifest::NameLookup::const_iterator SceneManifest::FindNameIterator(StringHash hash, const AZStd::string& name) const
            {
                auto range = m_nameLookup.equal_range(hash);
                // Always check the name, even if there's only one entry as the hash can be a clash with
                //      the single entry.
                for (auto it = range.first; it != range.second; ++it)
                {
                    if (m_names[it->second] == name)
                    {
                        return it;
                    }
                }

                return m_nameLookup.end();
            }

            AZStd::shared_ptr<const DataTypes::IManifestObject> SceneManifest::SceneManifestConstDataConverter(
                const AZStd::shared_ptr<DataTypes::IManifestObject>& value)
            {
                return AZStd::shared_ptr<const DataTypes::IManifestObject>(value);
            }
        } // Containers
    } // SceneAPI
} // AZ
