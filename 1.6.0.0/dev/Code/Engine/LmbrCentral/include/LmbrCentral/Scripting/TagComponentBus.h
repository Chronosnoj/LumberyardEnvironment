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

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/string/string.h>
#include <AzCore/base.h>

namespace LmbrCentral
{
    using Tag = AZ::Crc32;
    using Tags = AZStd::unordered_set<Tag>;
    using EditorTags = AZStd::vector<AZStd::string>;

    class TagGlobalRequests : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = Tag;

        virtual ~TagGlobalRequests() = default;

        /// Handlers will respond if they have the tag (i.e. they are listening on the tag's channel)
        /// Use AZ::EbusAggregateResults to handle more than the first responder
        virtual const AZ::EntityId RequestTaggedEntities() { return AZ::EntityId(); }
    };

    class TagComponentRequests : public AZ::ComponentBus
    {
    public:
        virtual ~TagComponentRequests() = default;

        /// Returns true if the entity has the tag
        virtual bool HasTag(const Tag&) = 0;

        /// Adds the tag to the entity if it didn't already have it
        virtual void AddTag(const Tag&) = 0;

        /// Add a list of tags to the entity if it didn't already have them
        virtual void AddTags(const Tags& tags)  { for (const Tag& tag : tags) AddTag(tag); }

        /// Removes a tag from the entity if it had it
        virtual void RemoveTag(const Tag&) = 0;

        /// Removes a list of tags from the entity if it had them
        virtual void RemoveTags(const Tags& tags) { for (const Tag& tag : tags) RemoveTag(tag); }

        /// Gets the list of tags on the entity
        virtual const Tags& GetTags() { static Tags s_emptyTags; return s_emptyTags; }
    };

    class TagComponentNotifications 
        : public AZ::ComponentBus
    {
        public:
            //! Notifies listeners about tags being added
            virtual void OnTagAdded(const Tag&) {}

            //! Notifies listeners about tags being removed
            virtual void OnTagRemoved(const Tag&) {}
    };

    using TagComponentNotificationsBus = AZ::EBus<TagComponentNotifications>;
    using TagGlobalRequestBus = AZ::EBus<TagGlobalRequests>;
    using TagComponentRequestBus = AZ::EBus<TagComponentRequests>;

} // namespace LmbrCentral
