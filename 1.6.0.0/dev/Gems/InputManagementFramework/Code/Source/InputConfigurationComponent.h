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
#include <AzCore/Asset/AssetCommon.h>

// InputManagementFramework Gem includes
#include "Include/InputManagementFramework/InputEventBindings.h"

namespace AZ
{
    class SerializeContext;
}

namespace Input
{
    class InputConfigurationComponent
        : public AZ::Component
        , private AZ::Data::AssetBus::Handler
    {
    public:

        AZ_COMPONENT(InputConfigurationComponent, "{3106EE2A-4816-433E-B855-D17A6484D5EC}");
        virtual ~InputConfigurationComponent() = default;

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void Reflect(AZ::ReflectContext* reflection);

        void Init() override;
        void Activate() override;
        void Deactivate() override;

    private:

        void OnAssetReady(const AZ::Data::Asset<AZ::Data::AssetData>& asset);

        AZ::Data::Asset<InputEventBindingsAsset> m_inputEventBindingsAsset;
        InputEventBindings m_inputEventBindings;
        AZ::EntityId m_sourceEntity;
    };
} // namespace Input
