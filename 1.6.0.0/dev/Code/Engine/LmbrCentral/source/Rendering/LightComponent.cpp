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

#include "LightInstance.h"
#include "LightComponent.h"

namespace LmbrCentral
{
    //////////////////////////////////////////////////////////////////////////
    AZ_SCRIPTABLE_EBUS(
        LightComponentRequestBus,
        LightComponentRequestBus,
        "{46CFE806-006D-4D04-B682-E74D6D8BBED1}",
        "{AF979B11-7C6D-402B-A4B6-029FE5056DDE}",
        AZ_SCRIPTABLE_EBUS_EVENT(SetLightState, LightComponentRequests::State)
        AZ_SCRIPTABLE_EBUS_EVENT(TurnOnLight)
        AZ_SCRIPTABLE_EBUS_EVENT(TurnOffLight)
        AZ_SCRIPTABLE_EBUS_EVENT(ToggleLight)
        )

    AZ_SCRIPTABLE_EBUS(
        LightComponentNotificationBus,
        LightComponentNotificationBus,
        "{7777A2CA-7461-4664-AB61-9D42542215BD}",
        "{969C5B17-10D1-41DB-8123-6664FA64B4E9}",
        AZ_SCRIPTABLE_EBUS_EVENT(LightTurnedOn)
        AZ_SCRIPTABLE_EBUS_EVENT(LightTurnedOff)
        )

    void LightComponent::Reflect(AZ::ReflectContext* context)
    {
        LightConfiguration::Reflect(context);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<LightComponent, AZ::Component>()->
                Version(1)->
                Field("LightConfiguration", &LightComponent::m_configuration);
        }

        AZ::ScriptContext* script = azrtti_cast<AZ::ScriptContext*>(context);
        if (script)
        {
            ScriptableEBus_LightComponentRequestBus::Reflect(script);
            ScriptableEBus_LightComponentNotificationBus::Reflect(script);
        }
    }

    void LightConfiguration::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<LightConfiguration>()->
                Version(4, &VersionConverter)->
                Field("LightType", &LightConfiguration::m_lightType)->
                Field("Visible", &LightConfiguration::m_visible)->
                Field("OnInitially", &LightConfiguration::m_onInitially)->
                Field("Color", &LightConfiguration::m_color)->
                Field("DiffuseMultiplier", &LightConfiguration::m_diffuseMultiplier)->
                Field("SpecMultiplier", &LightConfiguration::m_specMultiplier)->
                Field("Ambient", &LightConfiguration::m_ambient)->
                Field("PointMaxDistance", &LightConfiguration::m_pointMaxDistance)->
                Field("PointAttenuationBulbSize", &LightConfiguration::m_pointAttenuationBulbSize)->
                Field("AreaWidth", &LightConfiguration::m_areaWidth)->
                Field("AreaHeight", &LightConfiguration::m_areaHeight)->
                Field("AreaMaxDistance", &LightConfiguration::m_areaMaxDistance)->
                Field("ProjectorDistance", &LightConfiguration::m_projectorRange)->
                Field("ProjectorAttenuationBulbSize", &LightConfiguration::m_projectorAttenuationBulbSize)->
                Field("ProjectorFOV", &LightConfiguration::m_projectorFOV)->
                Field("ProjectorNearPlane", &LightConfiguration::m_projectorNearPlane)->
                Field("ProjectorTexture", &LightConfiguration::m_projectorTexture)->
                Field("Area X,Y,Z", &LightConfiguration::m_probeArea)->
                Field("SortPriority", &LightConfiguration::m_probeSortPriority)->
                Field("CubemapResolution", &LightConfiguration::m_probeCubemapResolution)->
                Field("CubemapTexture", &LightConfiguration::m_probeCubemap)->
                Field("ViewDistanceMultiplier", &LightConfiguration::m_viewDistMultiplier)->
                Field("MinimumSpec", &LightConfiguration::m_minSpec)->
                Field("CastShadowsSpec", &LightConfiguration::m_castShadowsSpec)->
                Field("IgnoreVisAreas", &LightConfiguration::m_ignoreVisAreas)->
                Field("IndoorOnly", &LightConfiguration::m_indoorOnly)->
                Field("AffectsThisAreaOnly", &LightConfiguration::m_affectsThisAreaOnly)->
                Field("VolumetricFogOnly", &LightConfiguration::m_volumetricFogOnly)->
                Field("VolumetricFog", &LightConfiguration::m_volumetricFog)->
                Field("Deferred", &LightConfiguration::m_deferred)->
                Field("ShadowBias", &LightConfiguration::m_shadowBias)->
                Field("ShadowResScale", &LightConfiguration::m_shadowResScale)->
                Field("ShadowSlopeBias", &LightConfiguration::m_shadowSlopeBias)->
                Field("ShadowUpdateMinRadius", &LightConfiguration::m_shadowUpdateMinRadius)->
                Field("ShadowUpdateRatio", &LightConfiguration::m_shadowUpdateRatio)->
                Field("AnimIndex", &LightConfiguration::m_animIndex)->
                Field("AnimSpeed", &LightConfiguration::m_animSpeed)->
                Field("AnimPhase", &LightConfiguration::m_animPhase);
        }
    }

    // Private Static 
    bool LightConfiguration::VersionConverter(AZ::SerializeContext& context,
        AZ::SerializeContext::DataElementNode& classElement)
    {
        // conversion from version 1:
        // - Need to rename OmniRadius to MaxDistance
        if (classElement.GetVersion() <= 1)
        {
            int radiusIndex = classElement.FindElement(AZ_CRC("OmniRadius"));

            if (radiusIndex <= 1)
            {
                return false;
            }

            AZ::SerializeContext::DataElementNode& radius = classElement.GetSubElement(radiusIndex);
            radius.SetName("MaxDistance");
        }

        // conversion from version 2:
        // - Replace AreaSize with AreaWidth and AreaHeights
        if (classElement.GetVersion() <= 2)
        {
            int areaIndex = classElement.FindElement(AZ_CRC("AreaSize"));

            if (areaIndex <= 1)
            {
                return false;
            }

            //Get the size
            AZ::SerializeContext::DataElementNode& area = classElement.GetSubElement(areaIndex);
            AZ::Vector3 size;

            area.GetData<AZ::Vector3>(size);

            //Create width and height
            int areaWidthIndex = classElement.AddElement<float>(context, "AreaWidth");
            int areaHeightIndex = classElement.AddElement<float>(context, "AreaHeight");

            AZ::SerializeContext::DataElementNode& areaWidth = classElement.GetSubElement(areaWidthIndex);
            AZ::SerializeContext::DataElementNode& areaHeight = classElement.GetSubElement(areaHeightIndex);

            //Set width and height data to x and y from the old size
            areaWidth.SetData<float>(context, size.GetX());
            areaHeight.SetData<float>(context, size.GetY());
        }

        // conversion from version 3:
        // - MaxDistance turned into PointMaxDistance and AreaMaxDistance
        // - AttenuationBulbSize turned into PointAttenuationBulbSize and ProjectorAttenuationBulbSize
        // - Apply old area size to new probe area
        if (classElement.GetVersion() <= 3)
        {
            int maxDistanceIndex = classElement.FindElement(AZ_CRC("MaxDistance"));
            int attenBulbIndex = classElement.FindElement(AZ_CRC("AttenuationBulbSize"));
            int areaWidthIndex = classElement.FindElement(AZ_CRC("AreaWidth"));
            int areaHeightIndex = classElement.FindElement(AZ_CRC("AreaHeight"));
            int areaXYZIndex = classElement.FindElement(AZ_CRC("Area X,Y,Z"));

            if (maxDistanceIndex <= 1 || attenBulbIndex <= 1 || areaWidthIndex <= 1 || areaHeightIndex <= 1 || areaXYZIndex <= 1)
            {
                return false;
            }

            //Get some values
            AZ::SerializeContext::DataElementNode& maxDistance = classElement.GetSubElement(maxDistanceIndex);
            AZ::SerializeContext::DataElementNode& attenBulbSize = classElement.GetSubElement(attenBulbIndex);
            AZ::SerializeContext::DataElementNode& areaWidth = classElement.GetSubElement(areaWidthIndex);
            AZ::SerializeContext::DataElementNode& areaHeight = classElement.GetSubElement(attenBulbIndex);
            AZ::SerializeContext::DataElementNode& areaXYZ = classElement.GetSubElement(areaXYZIndex);

            float maxDistVal, attenBulbVal, areaWidthVal, areaHeightVal;

            maxDistance.GetData<float>(maxDistVal);
            attenBulbSize.GetData<float>(attenBulbVal);
            areaWidth.GetData<float>(areaWidthVal);
            areaHeight.GetData<float>(areaHeightVal);

            //Create AreaMaxDistance, PointMaxDistance, PointAttenuationBulbSize and ProjectorAttenuationBulbSize
            int areaMaxDistIndex = classElement.AddElement<float>(context, "AreaMaxDistance");
            int pointMaxDistIndex = classElement.AddElement<float>(context, "PointMaxDistance");
            int pointAttenBulbIndex = classElement.AddElement<float>(context, "PointAttenuationBulbSize");
            int projectorAttenBulbIndex = classElement.AddElement<float>(context, "ProjectorAttenuationBulbSize");

            AZ::SerializeContext::DataElementNode& areaMaxDist = classElement.GetSubElement(areaMaxDistIndex);
            AZ::SerializeContext::DataElementNode& pointMaxDist = classElement.GetSubElement(pointMaxDistIndex);
            AZ::SerializeContext::DataElementNode& pointAttenBulb = classElement.GetSubElement(pointAttenBulbIndex);
            AZ::SerializeContext::DataElementNode& projectorAttenBulb = classElement.GetSubElement(projectorAttenBulbIndex);

            //Set width and height data to x and y from the old size
            areaMaxDist.SetData<float>(context, maxDistVal);
            pointMaxDist.SetData<float>(context, maxDistVal);
            pointAttenBulb.SetData<float>(context, attenBulbVal);
            projectorAttenBulb.SetData<float>(context, attenBulbVal);

            //Set new area XYZ values based on existing area values
            areaXYZ.SetData<AZ::Vector3>(context, AZ::Vector3(maxDistVal, areaWidthVal, areaHeightVal));

            //Remove old MaxDistance and AttenuationBulbSize
            classElement.RemoveElement(maxDistanceIndex);
            classElement.RemoveElement(attenBulbIndex);
        }

        return true;
    }

    void LightComponent::TurnOnLight()
    {
        bool success = m_light.TurnOn();
        if (success)
        {
            EBUS_EVENT_ID(GetEntityId(), LightComponentNotificationBus, LightTurnedOn);
        }
    }

    void LightComponent::TurnOffLight()
    {
        bool success = m_light.TurnOff();
        if (success)
        {
            EBUS_EVENT_ID(GetEntityId(), LightComponentNotificationBus, LightTurnedOff);
        }
    }

    void LightComponent::ToggleLight()
    {
        if (m_light.IsOn())
        {
            TurnOffLight();
        }
        else
        {
            TurnOnLight();
        }
    }

    void LightComponent::SetLightState(State state)
    {
        switch (state)
        {
        case LightComponentRequests::State::On:
        {
            TurnOnLight();
        }
        break;

        case LightComponentRequests::State::Off:
        {
            TurnOffLight();
        }
        break;
        }
    }

    IRenderNode* LightComponent::GetRenderNode()
    {
        return m_light.GetRenderNode();
    }

    float LightComponent::GetRenderNodeRequestBusOrder() const
    {
        return s_renderNodeRequestBusOrder;
    }

    const float LightComponent::s_renderNodeRequestBusOrder = 500.f;

    LightComponent::LightComponent()
    {
    }

    LightComponent::~LightComponent()
    {
    }

    void LightComponent::Init()
    {
    }

    void LightComponent::Activate()
    {
        const AZ::EntityId entityId = GetEntityId();

        m_light.SetEntity(entityId);
        m_light.CreateRenderLight(m_configuration);

        LightComponentRequestBus::Handler::BusConnect(entityId);
        RenderNodeRequestBus::Handler::BusConnect(entityId);

        if (m_configuration.m_onInitially)
        {
            m_light.TurnOn();
        }
        else
        {
            m_light.TurnOff();
        }
    }

    void LightComponent::Deactivate()
    {
        LightComponentRequestBus::Handler::BusDisconnect();
        RenderNodeRequestBus::Handler::BusDisconnect();
        m_light.DestroyRenderLight();
        m_light.SetEntity(AZ::EntityId());
    }

    LightConfiguration::LightConfiguration()
        : m_lightType(LightType::Point)
        , m_visible(true)
        , m_onInitially(true)
        , m_pointMaxDistance(2.f)
        , m_pointAttenuationBulbSize(0.05f)
        , m_areaMaxDistance(2.f)
        , m_areaWidth(5.f)
        , m_areaHeight(5.f)
        , m_projectorAttenuationBulbSize(0.05f)
        , m_projectorRange(5.f)
        , m_projectorFOV(90.f)
        , m_projectorNearPlane(0)
        , m_probeSortPriority(0)
        , m_probeArea(5.0f, 5.0f, 5.0f)
        , m_probeCubemapResolution(ResolutionSetting::ResDefault)
        , m_minSpec(EngineSpec::Low)
        , m_viewDistMultiplier(1.f)
        , m_castShadowsSpec(EngineSpec::Never)
        , m_color(1.f, 1.f, 1.f)
        , m_diffuseMultiplier(1.f)
        , m_specMultiplier(1.f)
        , m_affectsThisAreaOnly(true)
        , m_ignoreVisAreas(false)
        , m_volumetricFog(true)
        , m_volumetricFogOnly(false)
        , m_indoorOnly(false)
        , m_ambient(false)
        , m_deferred(true)
        , m_animIndex(0)
        , m_animSpeed(1.f)
        , m_animPhase(0.f)
        , m_shadowBias(1.f)
        , m_shadowSlopeBias(1.f)
        , m_shadowResScale(1.f)
        , m_shadowUpdateMinRadius(10.f)
        , m_shadowUpdateRatio(1.f)
    {
    }
} // namespace LmbrCentral
