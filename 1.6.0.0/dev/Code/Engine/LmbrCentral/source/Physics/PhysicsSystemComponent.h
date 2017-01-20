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

#include <LmbrCentral/Physics/PhysicsSystemComponentBus.h>
#include <AzCore/Component/Component.h>
#include <CrySystemBus.h>

struct IPhysicalWorld;

namespace LmbrCentral
{
    /*!
     * System component which listens for IPhysicalWorld events,
     * filters for events that involve AZ::Entities,
     * and broadcasts these events on the EntityPhysicsEventBus.
     */
    class PhysicsSystemComponent
        : public AZ::Component
        , public PhysicsSystemRequestBus::Handler
        , public CrySystemEventBus::Handler
    {
    public:
        AZ_COMPONENT(PhysicsSystemComponent, "{1586DBA1-F5F0-49AB-9F59-AE62C0E60AE0}");

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("PhysicsSystemService"));
        }

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("PhysicsSystemService"));
        }

        ~PhysicsSystemComponent() override {}

    private:
        ////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // PhysicsSystemRequests
        void PrePhysicsUpdate() override;
        void PostPhysicsUpdate() override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // CrySystemEvents
        void OnCrySystemInitialized(ISystem&, const SSystemInitParams&) override;
        void OnCrySystemShutdown(ISystem&) override;
        ////////////////////////////////////////////////////////////////////////

        void SetEnabled(bool enable);

        //! During system components' Activate()/Deactivate()
        //! gEnv->pPhysicalWorld is not available (in fact, gEnv is null).
        //! Therefore we "enable" ourselves after CrySystem has initialized.
        bool m_enabled = false;

        //! Since gEnv is unreliable for system components,
        //! maintain our own reference to gEnv->pPhysicalWorld.
        IPhysicalWorld* m_physicalWorld = nullptr;
    };
} // namespace LmbrCentral
