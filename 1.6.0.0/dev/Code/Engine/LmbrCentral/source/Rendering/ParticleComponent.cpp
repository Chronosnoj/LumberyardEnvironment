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
#include "ParticleComponent.h"

#include <IParticles.h>
#include <ParticleParams.h>

#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/EBus/ScriptBinder.h>
#include <AzCore/Asset/AssetDatabaseBus.h>

#include <MathConversion.h>
#include <AzFramework/StringFunc/StringFunc.h>


namespace LmbrCentral
{
    AZ_SCRIPTABLE_EBUS(
        ParticleComponentRequestBus,
        ParticleComponentRequestBus,
        "{D126923F-9FAF-4580-825C-DC26FC39201E}",
        "{71A0B1DD-3713-4DAC-9395-880D31221F57}",
        AZ_SCRIPTABLE_EBUS_EVENT(SetVisibility, bool)
        AZ_SCRIPTABLE_EBUS_EVENT(Show)
        AZ_SCRIPTABLE_EBUS_EVENT(Hide)
        )

    //////////////////////////////////////////////////////////////////////////
    void ParticleComponent::Reflect(AZ::ReflectContext* context)
    {
        ParticleEmitterSettings::Reflect(context);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<ParticleComponent, AZ::Component>()->
                Version(1)->
                Field("Particle", &ParticleComponent::m_settings)
            ;
        }

        AZ::ScriptContext* script = azrtti_cast<AZ::ScriptContext*>(context);
        if (script)
        {
            ScriptableEBus_ParticleComponentRequestBus::Reflect(script);
        }
    }

    //////////////////////////////////////////////////////////////////////////

    SpawnParams ParticleEmitterSettings::GetPropertiesAsSpawnParams() const
    {
        SpawnParams params;
        params.eAttachType = m_attachType;
        params.eAttachForm = m_attachForm;
        params.bCountPerUnit = m_countPerUnit;
        params.bEnableAudio = m_enableAudio;
        params.bRegisterByBBox = m_registerByBBox;
        params.fCountScale = m_countScale;
        params.fSizeScale = m_sizeScale;
        params.fSpeedScale = m_speedScale;
        params.fTimeScale = m_timeScale;
        params.fPulsePeriod = m_pulsePeriod;
        params.fStrength = m_strength;
        params.sAudioRTPC = m_audioRTPC.c_str();
        return params;
    }

    void ParticleEmitterSettings::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<ParticleEmitterSettings>()->
                Version(2, &VersionConverter)->
                
                //Particle
                Field("Visible", &ParticleEmitterSettings::m_visible)->
                
                //General Settings
                Field("Attach Type", &ParticleEmitterSettings::m_attachType)->
                Field("Emitter Shape", &ParticleEmitterSettings::m_attachForm)->
                Field("Geometry", &ParticleEmitterSettings::m_geometryAsset)->

                //Spawn Properties
                Field("Color", &ParticleEmitterSettings::m_color)->
                Field("Count Per Unit", &ParticleEmitterSettings::m_countPerUnit)->                
                Field("Prime", &ParticleEmitterSettings::m_prime)->
                Field("Particle Count Scale", &ParticleEmitterSettings::m_countScale)->
                Field("Time Scale", &ParticleEmitterSettings::m_timeScale)->
                Field("Pulse Period", &ParticleEmitterSettings::m_pulsePeriod)->
                Field("Position Offset", &ParticleEmitterSettings::m_positionOffset)->
                Field("Random Offset", &ParticleEmitterSettings::m_randomOffset)->
                Field("Particle Size Scale", &ParticleEmitterSettings::m_sizeScale)->
                Field("Size X", &ParticleEmitterSettings::m_sizeX)->
                Field("Size Y", &ParticleEmitterSettings::m_sizeY)->
                Field("Size Random X", &ParticleEmitterSettings::m_sizeRandomX)->
                Field("Size Random Y", &ParticleEmitterSettings::m_sizeRandomY)->
                Field("Init Angles", &ParticleEmitterSettings::m_initAngles)->                
                Field("Rotation Rate X", &ParticleEmitterSettings::m_rotationRateX)->
                Field("Rotation Rate Y", &ParticleEmitterSettings::m_rotationRateY)->
                Field("Rotation Rate Z", &ParticleEmitterSettings::m_rotationRateZ)->
                Field("Rotation Rate Random X", &ParticleEmitterSettings::m_rotationRateRandomX)->
                Field("Rotation Rate Random Y", &ParticleEmitterSettings::m_rotationRateRandomY)->
                Field("Rotation Rate Random Z", &ParticleEmitterSettings::m_rotationRateRandomZ)->
                Field("Rotation Random Angles", &ParticleEmitterSettings::m_rotationRandomAngles)->
                Field("Speed Scale", &ParticleEmitterSettings::m_speedScale)->
                Field("Strength", &ParticleEmitterSettings::m_strength)->
                Field("Ignore Rotation", &ParticleEmitterSettings::m_ignoreRotation)->
                Field("Not Attached", &ParticleEmitterSettings::m_notAttached)->
                Field("Register by Bounding Box", &ParticleEmitterSettings::m_registerByBBox)->

                //Audio
                Field("Enable Audio", &ParticleEmitterSettings::m_enableAudio)->                
                Field("Audio RTPC", &ParticleEmitterSettings::m_audioRTPC)->         

                Field("SelectedEmitter", &ParticleEmitterSettings::m_selectedEmitter)
            ;
        }
    }

    // Private Static 
    bool ParticleEmitterSettings::VersionConverter(AZ::SerializeContext& context,
        AZ::SerializeContext::DataElementNode& classElement)
    {
        // conversion from version 1:
        // - Need to rename Emitter Object Type to Attach Type
        // - Need to rename Emission Speed to Speed Scale
        if (classElement.GetVersion() <= 1)
        {
            int emitterIndex = classElement.FindElement(AZ_CRC("Emitter Object Type"));
            int speedIndex = classElement.FindElement(AZ_CRC("Emission Speed"));

            if (emitterIndex <= -1 || speedIndex <= -1)
            {
                return false;
            }

            //Invert hidden and rename to visible
            AZ::SerializeContext::DataElementNode& emitter = classElement.GetSubElement(emitterIndex);
            emitter.SetName("Attach Type");

            //Invert outdoor only and rename to indoor only
            AZ::SerializeContext::DataElementNode& speedScale = classElement.GetSubElement(speedIndex);
            speedScale.SetName("Speed Scale");
        }

        // conversion from version 2:
        // - Need to remove Visible
        if (classElement.GetVersion() <= 2)
        {
            int visibleIndex = classElement.FindElement(AZ_CRC("Visible"));

            classElement.RemoveElement(visibleIndex);
        }

        return true;
    }

    //////////////////////////////////////////////////////////////////////////

    void ParticleComponent::Init()
    {
    }

    void ParticleComponent::Activate()
    {
        ParticleComponentRequestBus::Handler::BusConnect(m_entity->GetId());
        RenderNodeRequestBus::Handler::BusConnect(m_entity->GetId());

        m_emitter.AttachToEntity(m_entity->GetId());
        m_emitter.Set(m_settings.m_selectedEmitter, m_settings);
    }

    void ParticleComponent::Deactivate()
    {
        ParticleComponentRequestBus::Handler::BusDisconnect();
        RenderNodeRequestBus::Handler::BusDisconnect();

        m_emitter.Clear();
    }

    IRenderNode* ParticleComponent::GetRenderNode()
    {
        return m_emitter.GetRenderNode();
    }

    float ParticleComponent::GetRenderNodeRequestBusOrder() const
    {
        return s_renderNodeRequestBusOrder;
    }

    const float ParticleComponent::s_renderNodeRequestBusOrder = 800.f;

    //////////////////////////////////////////////////////////////////////////

    ParticleEmitter::ParticleEmitter()
    {
    }

    void ParticleEmitter::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        if (m_emitter)
        {
            m_emitter->SetMatrix(AZTransformToLYTransform(world));
        }
    }

    void ParticleEmitter::AttachToEntity(AZ::EntityId id)
    {
        m_attachedToEntityId = id;
    }

    void ParticleEmitter::Set(const AZStd::string& emitterName, const ParticleEmitterSettings& settings)
    {
        if (emitterName.empty())
        {
            return;
        }

        m_effect = gEnv->pParticleManager->FindEffect(emitterName.c_str());

        // Given a valid effect, we'll spawn a new emitter
        if (m_effect)
        {
            // Apply the spawn params and entity transformation.
            SpawnEmitter(settings);
        }
        else
        {
            AZ_Warning("Particle Component", "Could not find particle emitter: %s", emitterName.c_str());
        }
    }

    void ParticleEmitter::SpawnEmitter(const ParticleEmitterSettings& settings)
    {
        AZ_Assert(m_effect, "Cannot spawn an emitter without an effect");
        if (!m_effect)
        {
            return;
        }

        // If we already have an emitter, remove it to spawn a new one
        if (m_emitter)
        {
            m_emitter->Kill();
            m_emitter = nullptr;
        }

        uint32 emmitterFlags = 0;
        if (settings.m_ignoreRotation)
        {
            emmitterFlags |= ePEF_IgnoreRotation;
        }
        else
        {
            emmitterFlags &= ~ePEF_IgnoreRotation;
        }

        if (settings.m_notAttached)
        {
            emmitterFlags |= ePEF_NotAttached;

            if (AZ::TransformNotificationBus::Handler::BusIsConnectedId(m_attachedToEntityId))
            {
                AZ::TransformNotificationBus::Handler::BusDisconnect(m_attachedToEntityId);
            }
        }
        else
        {
            emmitterFlags &= ~ePEF_NotAttached;

            if (!AZ::TransformNotificationBus::Handler::BusIsConnectedId(m_attachedToEntityId))
            {
                AZ::TransformNotificationBus::Handler::BusConnect(m_attachedToEntityId);
            }
        }

        // Spawn the particle emitter
        AZ::Transform parentTransform = AZ::Transform::CreateIdentity();
        EBUS_EVENT_ID_RESULT(parentTransform, m_attachedToEntityId, AZ::TransformBus, GetWorldTM);

        SpawnParams spawnParams = settings.GetPropertiesAsSpawnParams();
        m_emitter = m_effect->Spawn(QuatTS(AZTransformToLYTransform(parentTransform)), emmitterFlags, &spawnParams);

        if (settings.m_prime)
        {
            m_emitter->Prime();
        }

        if (settings.m_visible)
        {
            EBUS_EVENT_ID(m_attachedToEntityId, ParticleComponentRequestBus, SetVisibility, true);
        }
        else
        {
            EBUS_EVENT_ID(m_attachedToEntityId, ParticleComponentRequestBus, SetVisibility, false);
        }

        // Apply the entity's transformation.
        OnTransformChanged(AZ::Transform::CreateIdentity(), parentTransform);
    }

    void ParticleEmitter::SetVisibility(bool visible)
    {
        if (m_emitter)
        {
            m_emitter->Hide(!visible);
        }
    }

    void ParticleEmitter::Show()
    {
        SetVisibility(true);
    }

    void ParticleEmitter::Hide()
    {
        SetVisibility(false);
    }

    void ParticleEmitter::Clear()
    {
        if (m_emitter)
        {
            m_emitter->Activate(false);
            m_emitter->SetEntity(nullptr, 0);
        }

        m_emitter = nullptr;
        m_effect = nullptr;
    }

    bool ParticleEmitter::IsCreated() const
    {
        return (m_emitter != nullptr);
    }

    IRenderNode* ParticleEmitter::GetRenderNode()
    {
        return m_emitter;
    }
} // namespace LmbrCentral
