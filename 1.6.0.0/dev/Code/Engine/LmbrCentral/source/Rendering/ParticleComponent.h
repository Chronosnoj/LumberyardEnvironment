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

#include <AzCore/Math/Crc.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/EntityBus.h>

#include <AzFramework/Asset/SimpleAsset.h>

#include <IParticles.h>
#include <Cry_Geo.h>

#include <LmbrCentral/Rendering/ParticleComponentBus.h>
#include <LmbrCentral/Rendering/RenderNodeBus.h>
#include <LmbrCentral/Rendering/MaterialAsset.h>
#include <LmbrCentral/Rendering/MeshAsset.h>

namespace LmbrCentral
{
    /*!
    *  Particle emitter settings, these are used by both the in-game and editor components.
    */
    class ParticleEmitterSettings
    {
    public:

        AZ_TYPE_INFO(ParticleEmitterSettings, "{A1E34557-30DB-4716-B4CE-39D52A113D0C}")

            ParticleEmitterSettings()
            : m_color(1.0, 1.0, 1.0f)
            , m_positionOffset(0.0f, 0.0f, 0.0f)
            , m_randomOffset(0.0f, 0.0f, 0.0f)
            , m_attachType(EGeomType::GeomType_None)
            , m_attachForm(EGeomForm::GeomForm_Surface)
            , m_sizeX(1.0f)
            , m_sizeY(1.0f)
            , m_sizeRandomX(0.0f)
            , m_sizeRandomY(0.0f)
            , m_rotationRateX(0.0f)
            , m_rotationRateY(0.0f)
            , m_rotationRateZ(0.0f)
            , m_rotationRateRandomX(0.0f)
            , m_rotationRateRandomY(0.0f)
            , m_rotationRateRandomZ(0.0f)
            , m_rotationRandomAngles(0.0f, 0.0f, 0.0f)
            , m_countPerUnit(false)
            , m_enableAudio(false)
            , m_registerByBBox(false)
            , m_prime(false)
            , m_ignoreRotation(false)
            , m_notAttached(false)
            , m_visible(true)
            , m_countScale(1.f)
            , m_sizeScale(1.f)
            , m_speedScale(1.f)
            , m_timeScale(1.f)
            , m_pulsePeriod(0.f)
            , m_strength(-1.f)
        {}

        AZ::Vector3                 m_color;                //!< An additional tint color
        AZ::Vector3                 m_positionOffset;       //!< Spawn point offset
        AZ::Vector3                 m_randomOffset;         //!< Max random position offset
        EGeomType                   m_attachType;           //!< What type of object particles emitted from.
        EGeomForm                   m_attachForm;           //!< What aspect of shape emitted from.
        float                       m_sizeX;                //!< Scaling on X
        float                       m_sizeY;                //!< Scaling on Y
        float                       m_sizeRandomX;          //!< Random Scaling on X
        float                       m_sizeRandomY;          //!< Random Scaling on Y
        AZ::Vector3                 m_initAngles;           //!< Starting rotation
        float                       m_rotationRateX;        //!< Rotation rate along the X axis
        float                       m_rotationRateY;        //!< Rotation rate along the Y axis
        float                       m_rotationRateZ;        //!< Rotation rate along the Z axis
        float                       m_rotationRateRandomX;  //!< Rotation rate randomness along the X axis
        float                       m_rotationRateRandomY;  //!< Rotation rate randomness along the Y axis
        float                       m_rotationRateRandomZ;  //!< Rotation rate randomness along the Z axis
        AZ::Vector3                 m_rotationRandomAngles; //!< Rotation random angles
        bool                        m_countPerUnit;         //!< Multiply particle count also by geometry extent (length/area/volume).
        bool                        m_enableAudio;          //!< Used by particle effect instances to indicate whether audio should be updated or not.
        bool                        m_registerByBBox;       //!< Use the Bounding Box instead of Position to Register in VisArea
        bool                        m_prime;                //!< Set emitter as though it's been running indefinitely.
        bool                        m_ignoreRotation;       //!< The entity rotation is ignored.
        bool                        m_notAttached;          //!< The entity's position is ignore (i.e. emitter does not follow its entity).
        bool                        m_visible;              //!< Controls the emitter's visibility.
        float                       m_countScale;           //!< Multiple for particle count (on top of m_countPerUnit if set).
        float                       m_sizeScale;            //!< Multiple for all effect sizes.
        float                       m_speedScale;           //!< Multiple for particle emission speed.
        float                       m_timeScale;            //!< Multiple for emitter time evolution.
        float                       m_pulsePeriod;          //!< How often to restart emitter.
        float                       m_strength;             //!< Controls parameter strength curves.
        AZStd::string               m_audioRTPC;            //!< Indicates what audio RTPC this particle effect instance drives.
        AZ::Data::Asset<StaticMeshAsset>  m_geometryAsset;  //!< Mesh asset if the particle is supposed to spawn geometry other than planes


        //! Name of the particle emitter to use.
        AZStd::string m_selectedEmitter;

        //! Constructs a SpawnParams struct from the component's properties.
        SpawnParams GetPropertiesAsSpawnParams() const;

        static void Reflect(AZ::ReflectContext* context);

    private:

        static bool VersionConverter(AZ::SerializeContext& context,
            AZ::SerializeContext::DataElementNode& classElement);
    };

    //////////////////////////////////////////////////////////////////////////

    /*!
    *  Particle emitter, this is a wrapper around IParticleEmitter that provides access to an effect's emitter from a specified particle library.
    */
    class ParticleEmitter
        : public AZ::TransformNotificationBus::Handler
    {
    public:

        ParticleEmitter();

        //////////////////////////////////////////////////////////////////////////
        // TransformNotificationBus
        //! Called when the local transform of the entity has changed. Local transform update always implies world transform change too.
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;
        //////////////////////////////////////////////////////////////////////////

        //! Attaches the component's entity to this emitter.
        void AttachToEntity(AZ::EntityId id);

        //! Will find and load the effect associated with the provided emitter name and apply the ParticleEmitterSettings
        void Set(const AZStd::string& emmitterName, const ParticleEmitterSettings& settings);

        //! Will spawn an emitter with the specified settings, if an emitter exists it will destroy it first.
        void SpawnEmitter(const ParticleEmitterSettings& settings);

        //! Removes the effect and the emitter.
        void Clear();

        //! Marks this emitter as visible
        void Show();

        //! Hides this emitter
        void Hide();

        //! Allows the user to specify the visibility of the emitter.
        void SetVisibility(bool visible);

        //! True if the emitter has been created.
        bool IsCreated() const;

        IRenderNode* GetRenderNode();

    protected:

        AZ::EntityId m_attachedToEntityId;

        _smart_ptr<IParticleEffect> m_effect;
        _smart_ptr<IParticleEmitter> m_emitter;
    };

    //////////////////////////////////////////////////////////////////////////

    /*!
    * In-game particle component.
    */
    class ParticleComponent
        : public AZ::Component
        , public ParticleComponentRequestBus::Handler
        , public RenderNodeRequestBus::Handler
    {
    public:

        friend class EditorParticleComponent;

        AZ_COMPONENT(ParticleComponent, "{65BC817A-ABF6-440F-AD4F-581C40F92795}")

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // ParticleComponentRequestBus
        void Show() override { m_emitter.Show(); }
        void Hide() override { m_emitter.Hide(); }
        void SetVisibility(bool visible) override { m_emitter.SetVisibility(visible); }
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // RenderNodeRequestBus implementation
        IRenderNode* GetRenderNode() override;
        float GetRenderNodeRequestBusOrder() const override;
        static const float s_renderNodeRequestBusOrder;
        //////////////////////////////////////////////////////////////////////////

    private:

        ParticleEmitter m_emitter;
        ParticleEmitterSettings m_settings;

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("ParticleService"));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("TransformService"));
        }

        static void Reflect(AZ::ReflectContext* context);
    };
} // namespace LmbrCentral
