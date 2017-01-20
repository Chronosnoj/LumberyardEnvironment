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

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            // SceneManifestContainer

            const char* SceneManifestContainer::GetDefaultElementName()
            {
                return "entry";
            }

            u32 SceneManifestContainer::GetDefaultElementNameCrc()
            {
                return AZ_CRC("entry");
            }

            const SerializeContext::ClassElement* SceneManifestContainer::GetElement(u32 elementNameCrc) const
            {
                return elementNameCrc == m_classElement.m_nameCrc ? &m_classElement : nullptr;
            }

            size_t SceneManifestContainer::Size(void* instance) const
            {
                return reinterpret_cast<SceneManifest*>(instance)->GetEntryCount();
            }

            bool SceneManifestContainer::IsStableElements() const
            {
                return true;
            }

            bool SceneManifestContainer::IsFixedSize() const
            {
                return false;
            }

            bool SceneManifestContainer::IsSmartPointer() const
            {
                return false;
            }

            bool SceneManifestContainer::CanAccessElementsByIndex() const
            {
                return false;
            }

            void* SceneManifestContainer::ReserveElement(void* instance, const SerializeContext::ClassElement* classElement)
            {
                SceneManifest* manifest = reinterpret_cast<SceneManifest*>(instance);
                return &manifest->m_reflectionPlaceholder;
            }

            void* SceneManifestContainer::GetElementByIndex(void* instance, const SerializeContext::ClassElement* classElement, size_t index)
            {
                (void)instance;
                (void)classElement;
                (void)index;
                return nullptr;
            }

            void SceneManifestContainer::StoreElement(void* instance, void* element)
            {
                SceneManifest* manifest = reinterpret_cast<SceneManifest*>(instance);
                SceneManifest::ReflectionElement* entry = reinterpret_cast<SceneManifest::ReflectionElement*>(element);
                manifest->AddEntry(AZStd::move(entry->first), AZStd::move(entry->second));
            }

            bool SceneManifestContainer::RemoveElement(void* instance, const void* element, SerializeContext* deletePointerDataContext)
            {
                (void)deletePointerDataContext;
                SceneManifest* manifest = reinterpret_cast<SceneManifest*>(instance);
                const SceneManifest::ReflectionElement* entry = reinterpret_cast<const SceneManifest::ReflectionElement*>(element);

                return manifest->RemoveEntry(entry->first);
            }

            void SceneManifestContainer::ClearElements(void* instance, SerializeContext* deletePointerDataContext)
            {
                (void)deletePointerDataContext;
                SceneManifest* manifest = reinterpret_cast<SceneManifest*>(instance);
                manifest->Clear();
            }


            // SceneManifestClassInfo
            SceneManifestClassInfo::SceneManifestClassInfo()
            {
                m_classData = SerializeContext::ClassData::Create<SceneManifestContainer>(
                        "SceneManifest", "{9274AD17-3212-4651-9F3B-7DCCB080E467}",
                        SceneAPI::Containers::SceneManifestFactory::GetInstance(), nullptr, &m_containerStorage);
            }

            SerializeContext::ClassData* SceneManifestClassInfo::GetClassData()
            {
                return &m_classData;
            }

            size_t SceneManifestClassInfo::GetNumTemplatedArguments()
            {
                return 0;
            }

            const Uuid& SceneManifestClassInfo::GetTemplatedTypeId(size_t element)
            {
                (void)element;
                return AZ::AzTypeInfo<SceneManifest::ReflectionElement>().Uuid();
            }
        } // Containers
    } // SceneAPI

    GenericClassInfo* SerializeGenericTypeInfo<SceneAPI::Containers::SceneManifest>::GetGenericInfo()
    {
        return SceneAPI::Containers::SceneManifestClassInfo::Instance();
    }

    const Uuid& SerializeGenericTypeInfo<SceneAPI::Containers::SceneManifest>::GetClassTypeId()
    {
        return SceneAPI::Containers::SceneManifestClassInfo::Instance()->GetClassData()->m_typeId;
    }

    const char* AzTypeInfo<SceneAPI::Containers::SceneManifest>::Name()
    {
        return SceneAPI::Containers::SceneManifestClassInfo::Instance()->GetClassData()->m_name;
    }

    const AZ::Uuid& AzTypeInfo<SceneAPI::Containers::SceneManifest>::Uuid()
    {
        return SceneAPI::Containers::SceneManifestClassInfo::Instance()->GetClassData()->m_typeId;
    }
} // AZ