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

#include <AzCore/std/containers/vector.h>
#include <AzCore/Component/Component.h>
#include <LmbrCentral/Scripting/TagComponentBus.h>


namespace LmbrCentral
{
    class EditorTagComponent;
    /**
     * Tag Component
     * Simple component that tags an entity with a list of filters or descriptors
     */
    class TagComponent
        : public AZ::Component
        , public TagGlobalRequestBus::MultiHandler
        , public TagComponentRequestBus::Handler
    {
    public:
        AZ_COMPONENT(TagComponent, "{0F16A377-EAA0-47D2-8472-9EAAA680B169}");

        ~TagComponent() override = default;

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

    private:
        //////////////////////////////////////////////////////////////////////////
        /// EditorTagComponent will call this
        friend EditorTagComponent;
        void EditorSetTags(const EditorTags& editorTagList);

        //////////////////////////////////////////////////////////////////////////
        // TagGlobalRequestBus::MultiHandler
        const AZ::EntityId RequestTaggedEntities() override { return GetEntityId(); }

        //////////////////////////////////////////////////////////////////////////
        // TagComponentRequestBusRequestBus::Handler
        bool HasTag(const Tag&) override;
        void AddTag(const Tag&) override;
        void AddTags(const Tags&) override;
        void RemoveTag(const Tag&) override;
        void RemoveTags(const Tags&) override;
        const Tags& GetTags() override { return m_tags; }

        //////////////////////////////////////////////////////////////////////////
        // Component descriptor

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("TagService"));
        }

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("TagService"));
        }


        Tags m_tags = Tags();
    };
} // namespace LmbrCentral
