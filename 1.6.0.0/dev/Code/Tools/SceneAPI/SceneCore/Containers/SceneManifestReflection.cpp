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

#include <SceneAPI/SceneCore/Containers/SceneManifestReflection.h>
#include <SceneAPI/SceneCore/Containers/Views/PairIterator.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            // SceneManifestFactory

            void* SceneManifestFactory::Create(const char* name)
            {
                (void)name;
                return new SceneManifest();
            }

            void SceneManifestFactory::Destroy(void* object)
            {
                SceneManifest* manifest = reinterpret_cast<SceneManifest*>(object);
                delete manifest;
            }

            SceneManifestFactory* SceneManifestFactory::GetInstance()
            {
                static SceneManifestFactory s_factory;
                return &s_factory;
            }


            // SceneManifestContainer

            SceneManifestContainer::SceneManifestContainer()
            {
                m_classElement.m_name = GetDefaultElementName();
                m_classElement.m_nameCrc = GetDefaultElementNameCrc();
                m_classElement.m_dataSize = sizeof(SceneManifest::ReflectionElement);
                m_classElement.m_offset = 0;
                m_classElement.m_azRtti = GetSerializeRttiHelper<SceneManifest::ReflectionElement>();
                m_classElement.m_flags = SerializeContext::ClassElement::FLG_DYNAMIC_FIELD;
                m_classElement.m_genericClassInfo = SerializeGenericTypeInfo<SceneManifest::ReflectionElement>::GetGenericInfo();
                m_classElement.m_typeId = SerializeGenericTypeInfo<SceneManifest::ReflectionElement>::GetClassTypeId();
                m_classElement.m_editData = nullptr;
            }

            void SceneManifestContainer::EnumElements(void* instance, const ElementCB& cb)
            {
                SceneManifest* manifest = reinterpret_cast<SceneManifest*>(instance);

                auto view = Views::MakePairView(manifest->GetNameStorage(), manifest->GetValueStorage());
                for (auto& it : view)
                {
                    SceneManifest::ReflectionElement entry(it.first, it.second);
                    cb(&entry, m_classElement.m_typeId, m_classElement.m_genericClassInfo ? m_classElement.m_genericClassInfo->GetClassData() : nullptr, &m_classElement);
                }
            }

            size_t SceneManifestContainer::RemoveElements(void* instance, const void** elements, size_t numElements, SerializeContext* deletePointerDataContext)
            {
                (void)deletePointerDataContext;
                SceneManifest* manifest = reinterpret_cast<SceneManifest*>(instance);

                size_t removedCount = 0;
                for (size_t i = 0; i < numElements; ++i)
                {
                    const SceneManifest::ReflectionElement* entry = reinterpret_cast<const SceneManifest::ReflectionElement*>(elements[i]);
                    if (manifest->RemoveEntry(entry->first))
                    {
                        ++removedCount;
                    }
                }
                return removedCount;
            }


            // SceneManifestClassInfo

            SceneManifestClassInfo* SceneManifestClassInfo::Instance()
            {
                static SceneManifestClassInfo s_instance;
                return &s_instance;
            }
        } // Containers
    } // SceneAPI
} // AZ