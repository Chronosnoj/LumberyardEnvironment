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
#include "DecalComponent.h"
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <MathConversion.h>
#include <I3DEngine.h>
#include <AzCore/EBus/ScriptBinder.h>

namespace LmbrCentral
{
    AZ_SCRIPTABLE_EBUS(
        DecalComponentRequestBus,
        DecalComponentRequestBus,
        "{A5E29679-736A-4473-A2AC-D70D45738535}",
        "{46DB611A-5A8B-4C77-8E04-312F9F362B91}",
        AZ_SCRIPTABLE_EBUS_EVENT(SetVisibility, bool)
        AZ_SCRIPTABLE_EBUS_EVENT(Show)
        AZ_SCRIPTABLE_EBUS_EVENT(Hide)
        )


    void DecalConfiguration::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<DecalConfiguration>()->
                Version(2, &VersionConverter)->
                Field("Visible", &DecalConfiguration::m_visible)->
                Field("ProjectionType", &DecalConfiguration::m_projectionType)->
                Field("Material", &DecalConfiguration::m_material)->
                Field("SortPriority", &DecalConfiguration::m_sortPriority)->
                Field("Depth", &DecalConfiguration::m_depth)->
                Field("Offset", &DecalConfiguration::m_position)->
                Field("Opacity", &DecalConfiguration::m_opacity)->
                Field("Deferred", &DecalConfiguration::m_deferred)->
                Field("DeferredString", &DecalConfiguration::m_deferredString)->
                Field("Max View Distance", &DecalConfiguration::m_maxViewDist)->
                Field("View Distance Multiplier", &DecalConfiguration::m_viewDistanceMultiplier)->
                Field("Min Spec", &DecalConfiguration::m_minSpec)
            ;
        }

        AZ::ScriptContext* script = azrtti_cast<AZ::ScriptContext*>(context);
        if (script)
        {
            ScriptableEBus_DecalComponentRequestBus::Reflect(script);
        }
    }

    // Private Static 
    bool DecalConfiguration::VersionConverter(AZ::SerializeContext& context,
        AZ::SerializeContext::DataElementNode& classElement)
    {
        // conversion from version 1:
        // - Remove Normal
        if (classElement.GetVersion() <= 1)
        {
            int normalIndex = classElement.FindElement(AZ_CRC("Normal"));

            classElement.RemoveElement(normalIndex);
        }

        return true;
    }

    //////////////////////////////////////////////////////////////////////////

    void DecalComponent::Reflect(AZ::ReflectContext* context)
    {
        DecalConfiguration::Reflect(context);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<DecalComponent, AZ::Component>()->
                Version(1)->
                Field("DecalConfiguration", &DecalComponent::m_configuration);
            ;
        }
    }

    //////////////////////////////////////////////////////////////////////////

    DecalComponent::DecalComponent()
        : m_decalRenderNode(nullptr)
    {}

    void DecalComponent::Activate()
    {
        AZ::Transform transform = AZ::Transform::CreateIdentity();
        EBUS_EVENT_ID_RESULT(transform, GetEntityId(), AZ::TransformBus, GetWorldTM);

        SDecalProperties decalProperties = m_configuration.GetDecalProperties(transform);

        m_decalRenderNode = static_cast<IDecalRenderNode*>(gEnv->p3DEngine->CreateRenderNode(eERType_Decal));
        if (m_decalRenderNode)
        {
            m_decalRenderNode->SetDecalProperties(decalProperties);
            m_decalRenderNode->SetMinSpec(static_cast<int>(decalProperties.m_minSpec));
            m_decalRenderNode->SetMatrix(AZTransformToLYTransform(transform));
            m_decalRenderNode->SetViewDistanceMultiplier(m_configuration.m_viewDistanceMultiplier);

            const int configSpec = gEnv->pSystem->GetConfigSpec(true);
            if (!m_configuration.m_visible || configSpec <= static_cast<uint32_t>(m_configuration.m_minSpec))
            {
                Hide();
            }
        }

        DecalComponentRequestBus::Handler::BusConnect(GetEntityId());
        RenderNodeRequestBus::Handler::BusConnect(GetEntityId());
    }

    void DecalComponent::Deactivate()
    {
        DecalComponentRequestBus::Handler::BusDisconnect();
        RenderNodeRequestBus::Handler::BusDisconnect();

        if (m_decalRenderNode)
        {
            gEnv->p3DEngine->DeleteRenderNode(m_decalRenderNode);
            m_decalRenderNode = nullptr;
        }
    }

    void DecalComponent::Show()
    {
        if (!m_decalRenderNode)
        {
            return;
        }

        m_decalRenderNode->Hide(false);
    }

    void DecalComponent::Hide()
    {
        if (!m_decalRenderNode)
        {
            return;
        }

        m_decalRenderNode->Hide(true);
    }

    void DecalComponent::SetVisibility(bool show)
    {
        if (show)
        {
            Show();
        }
        else
        {
            Hide();
        }
    }

    IRenderNode* DecalComponent::GetRenderNode()
    {
        return m_decalRenderNode;
    }

    float DecalComponent::GetRenderNodeRequestBusOrder() const
    {
        return s_renderNodeRequestBusOrder;
    }

    const float DecalComponent::s_renderNodeRequestBusOrder = 900.f;
} // namespace LmbrCentral
