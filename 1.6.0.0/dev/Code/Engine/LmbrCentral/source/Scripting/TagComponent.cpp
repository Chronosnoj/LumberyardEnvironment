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
#include "Precompiled.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/EBus/ScriptBinder.h>
#include "TagComponent.h"

namespace LmbrCentral
{

    //=========================================================================
    // Script bus
    //=========================================================================
    static const AZ::EntityId s_invalidEntityId;
    
    AZ_SCRIPTABLE_EBUS(
        TagComponentRequestBus,
        TagComponentRequestBus,
        "{CFF0A1D4-8900-4C11-94AB-FB3CA2582FC5}",
        "{05477DFF-EE03-4D58-8A49-A0AC486DC8E3}",
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(bool, false, HasTag, const Tag&)
        AZ_SCRIPTABLE_EBUS_EVENT(AddTag, const Tag&)
        AZ_SCRIPTABLE_EBUS_EVENT(RemoveTag, const Tag&)
        )

    AZ_SCRIPTABLE_EBUS(
        TagGlobalRequestBus,
        TagGlobalRequestBus,
        "{CFF0A1D4-8900-4C11-94AB-FB3CA2582FC5}",
        "{05477DFF-EE03-4D58-8A49-A0AC486DC8E3}",
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(const AZ::EntityId, s_invalidEntityId, RequestTaggedEntities)
        )

    AZ_SCRIPTABLE_EBUS(
        TagComponentNotificationsBus,
        TagComponentNotificationsBus,
        "{03D704A2-90E1-4095-81C4-7BD3EA1BAC11}",
        "{7AEDC591-41AB-4E3B-87D2-03346154279D}",
        AZ_SCRIPTABLE_EBUS_EVENT(OnTagAdded, const Tag&)
        AZ_SCRIPTABLE_EBUS_EVENT(OnTagRemoved, const Tag&)
        )
    //=========================================================================
    // Component Descriptor
    //=========================================================================
    void TagComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context);
        if (serialize)
        {
            serialize->Class<TagComponent, AZ::Component>()
                ->Version(1)
                ->Field("Tags", &TagComponent::m_tags);
        }

        AZ::ScriptContext* script = azrtti_cast<AZ::ScriptContext*>(context);
        if (script)
        {
            ScriptableEBus_TagComponentRequestBus::Reflect(script);
            ScriptableEBus_TagGlobalRequestBus::Reflect(script);
        }
    }

    //=========================================================================
    // AZ::Component
    //=========================================================================
    void TagComponent::Activate()
    {
        for (const Tag& tag : m_tags)
        {
            TagGlobalRequestBus::MultiHandler::BusConnect(tag);
        }
        TagComponentRequestBus::Handler::BusConnect(GetEntityId());
    }

    void TagComponent::Deactivate()
    { 
        TagComponentRequestBus::Handler::BusDisconnect();
        for (const Tag& tag : m_tags)
        {
            TagGlobalRequestBus::MultiHandler::BusDisconnect(tag);
        }
    }

    //=========================================================================
    // EditorTagComponent friend will call this
    //=========================================================================
    void TagComponent::EditorSetTags(const EditorTags& editorTagList)
    {
        for (const auto& tag : editorTagList)
        {
            m_tags.insert(Tag(tag.c_str()));
        }
    }

    //=========================================================================
    // TagRequestBus
    //=========================================================================
    bool TagComponent::HasTag(const Tag& tag)
    {
        return m_tags.find(tag) != m_tags.end();
    }

    void TagComponent::AddTag(const Tag& tag)
    {
        if (m_tags.insert(tag).second)
        {
            EBUS_EVENT_ID(GetEntityId(), TagComponentNotificationsBus, OnTagAdded, tag);
            TagGlobalRequestBus::MultiHandler::BusConnect(tag);
        }
    }

    void TagComponent::AddTags(const Tags& tags)
    {
        for (const Tag& tag : tags)
        {
            AddTag(tag);
        }
    }

    void TagComponent::RemoveTag(const Tag& tag)
    {
        if (m_tags.erase(tag) > 0)
        {
            EBUS_EVENT_ID(GetEntityId(), TagComponentNotificationsBus, OnTagRemoved, tag);
            TagGlobalRequestBus::MultiHandler::BusDisconnect(tag);
        }
    }

    void TagComponent::RemoveTags(const Tags& tags)
    {
        for (const Tag& tag : tags)
        {
            RemoveTag(tag);
        }
    }

} // namespace LmbrCentral
