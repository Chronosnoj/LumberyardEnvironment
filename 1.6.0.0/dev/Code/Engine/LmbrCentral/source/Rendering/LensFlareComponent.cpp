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

#include <AzCore/Math/Vector3.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/EBus/ScriptBinder.h>

#include "LensFlareComponent.h"

namespace LmbrCentral
{
    //////////////////////////////////////////////////////////////////////////
    AZ_SCRIPTABLE_EBUS(
        LensFlareComponentRequestBus,
        LensFlareComponentRequestBus,
        "{20A017B5-9333-45C0-9210-C88F826C2AF9}",
        "{888FD52D-B790-4003-B155-12AEEB503A9A}",
        AZ_SCRIPTABLE_EBUS_EVENT(SetLensFlareState, LensFlareComponentRequests::State)
        AZ_SCRIPTABLE_EBUS_EVENT(TurnOnLensFlare)
        AZ_SCRIPTABLE_EBUS_EVENT(TurnOffLensFlare)
        AZ_SCRIPTABLE_EBUS_EVENT(ToggleLensFlare)
        )

    AZ_SCRIPTABLE_EBUS(
        LensFlareComponentNotificationBus,
        LensFlareComponentNotificationBus,
        "{2BD8326D-04DC-45AF-B60F-D3341AC59F01}",
        "{285A6AB7-82CF-472F-8C3B-1659A59213FF}",
        AZ_SCRIPTABLE_EBUS_EVENT(LensFlareTurnedOn)
        AZ_SCRIPTABLE_EBUS_EVENT(LensFlareTurnedOff)
        )

    void LensFlareComponent::Reflect(AZ::ReflectContext* context)
    {
        LensFlareConfiguration::Reflect(context);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<LensFlareComponent, AZ::Component>()->
                Version(1)->
                Field("LensFlareConfiguration", &LensFlareComponent::m_configuration);
        }

        AZ::ScriptContext* script = azrtti_cast<AZ::ScriptContext*>(context);
        if (script)
        {
            ScriptableEBus_LensFlareComponentRequestBus::Reflect(script);
            ScriptableEBus_LensFlareComponentNotificationBus::Reflect(script);
        }
    }

    void LensFlareConfiguration::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<LensFlareConfiguration>()->
                Version(2)->
                Field("Visible", &LensFlareConfiguration::m_visible)->
                Field("LensFlare", &LensFlareConfiguration::m_lensFlare)->

                Field("MinimumSpec", &LensFlareConfiguration::m_minSpec)->
                Field("LensFlareFrustumAngle", &LensFlareConfiguration::m_lensFlareFrustumAngle)->
                Field("Size", &LensFlareConfiguration::m_size)->
                Field("AttachToSun", &LensFlareConfiguration::m_attachToSun)->
                Field("AffectsThisAreaOnly", &LensFlareConfiguration::m_affectsThisAreaOnly)->
                Field("IgnoreVisAreas", &LensFlareConfiguration::m_ignoreVisAreas)->
                Field("IndoorOnly", &LensFlareConfiguration::m_indoorOnly)->
                Field("OnInitially", &LensFlareConfiguration::m_onInitially)->
                Field("ViewDistanceMultiplier", &LensFlareConfiguration::m_viewDistMultiplier)->

                Field("Tint", &LensFlareConfiguration::m_tint)->
                Field("TintAlpha", &LensFlareConfiguration::m_tintAlpha)->
                Field("Brightness", &LensFlareConfiguration::m_brightness)->

                Field("SyncAnimWithLight", &LensFlareConfiguration::m_syncAnimWithLight)->
                Field("LightEntity", &LensFlareConfiguration::m_lightEntity)->
                Field("AnimIndex", &LensFlareConfiguration::m_animIndex)->
                Field("AnimSpeed", &LensFlareConfiguration::m_animSpeed)->
                Field("AnimPhase", &LensFlareConfiguration::m_animPhase)->

                Field("SyncedAnimIndex", &LensFlareConfiguration::m_syncAnimIndex)->
                Field("SyncedAnimSpeed", &LensFlareConfiguration::m_syncAnimSpeed)->
                Field("SyncedAnimPhase", &LensFlareConfiguration::m_syncAnimPhase)
                ;
        }
    }

    void LensFlareComponent::TurnOnLensFlare()
    {
        bool success = m_light.TurnOn();
        if (success)
        {
            EBUS_EVENT_ID(GetEntityId(), LensFlareComponentNotificationBus, LensFlareTurnedOn);
        }
    }

    void LensFlareComponent::TurnOffLensFlare()
    {
        bool success = m_light.TurnOff();
        if (success)
        {
            EBUS_EVENT_ID(GetEntityId(), LensFlareComponentNotificationBus, LensFlareTurnedOff);
        }
    }

    void LensFlareComponent::ToggleLensFlare()
    {
        if (m_light.IsOn())
        {
            TurnOffLensFlare();
        }
        else
        {
            TurnOnLensFlare();
        }
    }

    void LensFlareComponent::SetLensFlareState(State state)
    {
        switch (state)
        {
        case LensFlareComponentRequests::State::On:
        {
            TurnOnLensFlare();
        }
        break;

        case LensFlareComponentRequests::State::Off:
        {
            TurnOffLensFlare();
        }
        break;
        }
    }

    IRenderNode* LensFlareComponent::GetRenderNode()
    {
        return m_light.GetRenderNode();
    }

    float LensFlareComponent::GetRenderNodeRequestBusOrder() const
    {
        return s_renderNodeRequestBusOrder;
    }

    const float LensFlareComponent::s_renderNodeRequestBusOrder = 600.f;

    void LensFlareComponent::Activate()
    {
        const AZ::EntityId entityId = GetEntityId();

        m_light.SetEntity(entityId);
        m_light.CreateRenderLight(m_configuration);

        LensFlareComponentRequestBus::Handler::BusConnect(entityId);
        RenderNodeRequestBus::Handler::BusConnect(entityId);

        if (m_configuration.m_onInitially)
        {
            TurnOnLensFlare();
        }
        else
        {
            TurnOffLensFlare();
        }
    }

    void LensFlareComponent::Deactivate()
    {
        LensFlareComponentRequestBus::Handler::BusDisconnect();
        RenderNodeRequestBus::Handler::BusDisconnect();

        m_light.DestroyRenderLight();
        m_light.SetEntity(AZ::EntityId());
    }

    LensFlareConfiguration::LensFlareConfiguration()
        : m_minSpec(EngineSpec::Low)
        , m_visible(true)
        , m_onInitially(true)
        , m_tint(1.f)
        , m_brightness(1.f)
        , m_viewDistMultiplier(1.f)
        , m_affectsThisAreaOnly(1.f)
        , m_ignoreVisAreas(false)
        , m_indoorOnly(false)
        , m_animSpeed(1.f)
        , m_animPhase(0.f)
        , m_animIndex(0)
        , m_lensFlareFrustumAngle(360.f)
        , m_size(1.0f)
        , m_attachToSun(false)
        , m_syncAnimWithLight(false)
    {
    }
} // namespace LmbrCentral
