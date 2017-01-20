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
#include <AzCore/Math/Aabb.h>
#include <AzCore/Asset/AssetCommon.h>

struct IStatObj;
struct ICharacterInstance;
struct IMaterial;

namespace LmbrCentral
{
    /*!
    * MaterialRequestBus
    * Messages serviced by the component that support materials (e.g. Mesh, Decal).
    */
    class MaterialRequests
        : public AZ::ComponentBus
    {
    public:

        virtual void SetMaterial(IMaterial*) = 0;
        virtual IMaterial* GetMaterial() = 0;
    };

    using MaterialRequestBus = AZ::EBus<MaterialRequests>;

    /*!
     * MeshComponentRequestBus
     * Messages serviced by the mesh component.
     */
    class MeshComponentRequests
        : public AZ::ComponentBus
    {
    public:

        virtual AZ::Aabb GetWorldBounds() = 0;
        virtual AZ::Aabb GetLocalBounds() = 0;
        virtual void SetMeshAsset(const AZ::Data::AssetId& id) = 0;
    };

    using MeshComponentRequestBus = AZ::EBus<MeshComponentRequests>;

    /*!
     * SkinnedMeshComponentRequestBus
     * Messages serviced by the mesh component.
     */
    class SkinnedMeshComponentRequests
        : public AZ::ComponentBus
    {
    public:
        virtual ICharacterInstance* GetCharacterInstance() = 0;
    };

    using SkinnedMeshComponentRequestBus = AZ::EBus<SkinnedMeshComponentRequests>;

    /*!
    * StaticMeshComponentRequestBus
    * Messages serviced by the mesh component.
    */
    class StaticMeshComponentRequests
        : public AZ::ComponentBus
    {
    public:
        virtual IStatObj* GetStatObj() = 0;
    };

    using StaticMeshComponentRequestBus = AZ::EBus<StaticMeshComponentRequests>;

    /*!
     * MeshComponentNotificationBus
     * Events dispatched by the mesh component.
     */
    class MeshComponentNotifications
        : public AZ::ComponentBus
    {
    public:

        /**
         * Notifies listeners the mesh instance has been created.
         * \param asset - The asset the mesh instance is based on.
         */
        virtual void OnMeshCreated(const AZ::Data::Asset<AZ::Data::AssetData>& asset) { (void)asset; }

        /**
         * Notifies listeners that the mesh instance has been destroyed.
        */
        virtual void OnMeshDestroyed() {}
    };

    using MeshComponentNotificationBus = AZ::EBus<MeshComponentNotifications>;

    /*
    * The Mesh Information Bus is a NON ID'd bus that broadcasts information about new skinned and static meshes being created and 
    * destroyed on Component Entities.
    */
    class SkinnedMeshInformation : public AZ::EBusTraits
    {

    public:

        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        /**
        * \brief Informs listeners when a skinned mesh is created
        * \param characterInstance The character instance associated with this skinned mesh
        * \param entityId The entity Id for the entity on which this mesh component (and the character instance) exists
        */
        virtual void OnSkinnedMeshCreated(ICharacterInstance* characterInstance, const AZ::EntityId& entityId) {};

        /**
        * \brief Informs listeners when a skinned mesh is destroyed
        * \param entityId The entity Id for the entity on which this mesh component (and the character instance) exists
        */
        virtual void OnSkinnedMeshDestroyed(const AZ::EntityId& entityId) {};

    };

    using SkinnedMeshInformationBus = AZ::EBus<SkinnedMeshInformation>;
} // namespace LmbrCentral
