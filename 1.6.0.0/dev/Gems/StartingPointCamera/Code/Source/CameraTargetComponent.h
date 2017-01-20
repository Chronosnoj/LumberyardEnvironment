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
#include <AzCore/std/string/string.h>
#include "StartingPointCamera/CameraTargetBus.h"

namespace Camera
{
    //////////////////////////////////////////////////////////////////////////
    /// When the CameraTargetComponent activates, it registers its entity as a valid camera target.
    /// A list of all camera targets can be acquired by querying the CameraTargetRequestBus.
    //////////////////////////////////////////////////////////////////////////
    class CameraTargetComponent
        : public AZ::Component
        , public CameraTargetRequestBus::Handler
    {
    public:
        ~CameraTargetComponent() override = default;
        AZ_COMPONENT(CameraTargetComponent, "{0D6A6574-4B79-4907-8529-EB61F343D957}");
        static void Reflect(AZ::ReflectContext* reflection);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Init() override {}
        void Activate() override;
        void Deactivate() override;

        //////////////////////////////////////////////////////////////////////////
        // CameraTargetRequestBus::Handler
        void RequestAllCameraTargets(CameraTargetList& targetList) override;
    private:
        AZStd::string m_tag = "";
    };
}