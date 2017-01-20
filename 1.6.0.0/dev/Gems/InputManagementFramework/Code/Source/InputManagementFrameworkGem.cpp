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
#include "StdAfx.h"
#include <platform_impl.h>

#include <AzCore/std/containers/vector.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/ObjectStream.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Module/Module.h>

#include <AzFramework/Asset/SimpleAsset.h>
#include <AzFramework/Asset/GenericAssetHandler.h>

#include <InputManagementFramework/InputEventBindings.h>
#include <InputManagementFramework/InputSubComponent.h>
#include <InputManagementFramework/InputEventGroup.h>

#include "InputConfigurationComponent.h"

namespace Input
{
    class InputManagementFrameworkSystemComponent
        : public AZ::Component
    {
    public:
        AZ_COMPONENT(InputManagementFrameworkSystemComponent, "{B52457FC-2DEA-4F0E-B0D0-F17786B40F02}");

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("InputManagementFrameworkService"));
        }

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("InputManagementFrameworkService"));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("AssetDatabaseService"));
        }

        static void Reflect(AZ::ReflectContext* context)
        {
            if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext->Class<InputManagementFrameworkSystemComponent, AZ::Component>()
                    ->Version(1)
                    ->SerializerForEmptyClass()
                    ;

                if (AZ::EditContext* editContext = serializeContext->GetEditContext())
                {
                    editContext->Class<InputManagementFrameworkSystemComponent>(
                        "Input management framework", "Manages input bindings and events")
                        ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                            ->Attribute(AZ::Edit::Attributes::Category, "Editor")
                            ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
                        ;
                }
            }

            Input::InputEventBindings::Reflect(context);
            Input::InputEventGroup::Reflect(context);
            Input::InputEventBindingsAsset::Reflect(context);
        }

        void Activate() override
        {
            // Register asset handlers. Requires "AssetDatabaseService"
            AZ_Assert(AZ::Data::AssetDatabase::IsReady(), "Asset database isn't ready!");

            m_inputEventBindingsAssetHandler = aznew AzFramework::GenericAssetHandler<Input::InputEventBindingsAsset>("inputbindings");
            m_inputEventBindingsAssetHandler->Register();
        }

        void Deactivate() override
        {
            delete m_inputEventBindingsAssetHandler;
            m_inputEventBindingsAssetHandler = nullptr;
        }

    private:
        AzFramework::GenericAssetHandler<Input::InputEventBindingsAsset>* m_inputEventBindingsAssetHandler = nullptr;
    };

    class InputManagementFrameworkModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(InputManagementFrameworkModule, "{F42A5E0C-2EA5-46C1-BA0F-D4A665653B0B}", AZ::Module);

        InputManagementFrameworkModule()
            : AZ::Module()
        {
            m_descriptors.insert(m_descriptors.end(), {
                InputConfigurationComponent::CreateDescriptor(),
                InputManagementFrameworkSystemComponent::CreateDescriptor()
            });
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return{
                azrtti_typeid<InputManagementFrameworkSystemComponent>()
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(InputManagementFramework_59b1b2acc1974aae9f18faddcaddac5b, Input::InputManagementFrameworkModule)
