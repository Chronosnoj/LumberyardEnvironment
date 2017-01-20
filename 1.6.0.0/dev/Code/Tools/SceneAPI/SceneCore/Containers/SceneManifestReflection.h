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

#include <AzCore/std/utils.h>
#include <AzCore/Memory/Memory.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <SceneAPI/SceneCore/SceneCoreConfiguration.h>
#include <SceneAPI/SceneCore/Containers/SceneManifest.h>
#include <SceneAPI/SceneCore/DataTypes/IManifestObject.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            class SceneManifestFactory
                : public SerializeContext::IObjectFactory
            {
            public:
                SCENE_CORE_API void* Create(const char* name) override;
                SCENE_CORE_API void Destroy(void* object) override;

                SCENE_CORE_API static SceneManifestFactory* GetInstance();
            };

            class SceneManifestContainer
                : public AZ::SerializeContext::IDataContainer
            {
            public:
                SCENE_CORE_API SceneManifestContainer();
                ~SceneManifestContainer() override = default;

                static inline const char* GetDefaultElementName();
                static inline u32 GetDefaultElementNameCrc();

                inline const SerializeContext::ClassElement* GetElement(u32 elementNameCrc) const override;
                SCENE_CORE_API void EnumElements(void* instance, const ElementCB& cb) override;

                inline size_t Size(void* instance) const override;

                inline bool IsStableElements() const override;
                inline bool IsFixedSize() const override;
                inline bool IsSmartPointer() const override;
                inline bool CanAccessElementsByIndex() const override;

                inline void* ReserveElement(void* instance, const SerializeContext::ClassElement* classElement) override;
                inline void* GetElementByIndex(void* instance, const SerializeContext::ClassElement* classElement, size_t index) override;
                inline void StoreElement(void* instance, void* element) override;
                inline bool RemoveElement(void* instance, const void* element, SerializeContext* deletePointerDataContext) override;

                SCENE_CORE_API size_t RemoveElements(void* instance, const void** elements, size_t numElements, SerializeContext* deletePointerDataContext) override;
                inline void ClearElements(void* instance, SerializeContext* deletePointerDataContext) override;

            protected:
                SerializeContext::ClassElement m_classElement;
            };

            class SceneManifestClassInfo
                : public GenericClassInfo
            {
            public:
                inline SceneManifestClassInfo();

                inline SerializeContext::ClassData* GetClassData() override;
                inline size_t GetNumTemplatedArguments() override;
                inline const Uuid& GetTemplatedTypeId(size_t element) override;

                SCENE_CORE_API static SceneManifestClassInfo* Instance();

            private:
                SceneManifestContainer m_containerStorage;
                SerializeContext::ClassData m_classData;
            };
        } // Containers
    } // SceneAPI

    template<>
    struct SerializeGenericTypeInfo<SceneAPI::Containers::SceneManifest>
    {
        inline static GenericClassInfo* GetGenericInfo();
        inline static const Uuid& GetClassTypeId();
    };

    template<>
    struct AzTypeInfo<SceneAPI::Containers::SceneManifest>
    {
        inline static const char* Name();
        inline static const AZ::Uuid& Uuid();
    };
} // AZ

#include <SceneAPI/SceneCore/Containers/SceneManifestReflection.inl>
