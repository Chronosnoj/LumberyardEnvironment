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

#include <AzCore/Component/Component.h>
#include <AzCore/Module/Module.h>
#include <AzCore/Asset/AssetDatabase.h>

#include <AzCore/std/smart_ptr/unique_ptr.h>

#include <CrySystemBus.h>

/*!
 * \namespace LmbrCentral
 * LmbrCentral ties together systems from CryEngine and systems from the AZ framework.
 */
namespace LmbrCentral
{
    /*!
     * The LmbrCentral module class coordinates with the application
     * to reflect classes and create system components.
     *
     * Note that the \ref LmbrCentralEditorModule is used when working in the Editor.
     */
    class LmbrCentralModule
        : public AZ::Module
    {
    public:
        LmbrCentralModule();
        ~LmbrCentralModule() override = default;
        AZ::ComponentTypeList GetRequiredSystemComponents() const override;
    };

    /*!
     * The LmbrCentral system component performs initialization/shutdown tasks
     * in coordination with other system components.
     */
    class LmbrCentralSystemComponent
        : public AZ::Component
        , private CrySystemEventBus::Handler
        , private AZ::Data::AssetDatabaseNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(LmbrCentralSystemComponent, "{CE249D37-C1D6-4A64-932D-C937B0EC2B8C}")
        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        ~LmbrCentralSystemComponent() override = default;

    private:
        ////////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////////
        // CrySystemEvents
        void OnCrySystemPreInitialize(ISystem& system, const SSystemInitParams& systemInitParams) override;
        void OnCrySystemInitialized(ISystem& system, const SSystemInitParams& systemInitParams) override;
        void OnCrySystemShutdown(ISystem& system) override;
        void OnCrySystemPrePhysicsUpdate() override;
        void OnCrySystemPostPhysicsUpdate() override;
        ////////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AZ::Data::AssetDatabaseNotificationBus::Handler
        //////////////////////////////////////////////////////////////////////////
        void OnAssetEventsDispatched() override;

        AZStd::vector<AZStd::unique_ptr<AZ::Data::AssetHandler> > m_assetHandlers;
    };
} // namespace LmbrCentral