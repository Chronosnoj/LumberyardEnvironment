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
#include <IGem.h>
#include <FlowSystem/Nodes/FlowBaseNode.h>
#include <platform_impl.h>

#if defined(AZ_PLATFORM_WINDOWS_X64)
#include "OpenVRDevice.h"
#endif // Windows x64

namespace OpenVR
{
    class OpenVRGem
        : public CryHooksModule
    {
    public:
        AZ_RTTI(OpenVRGem, "{80D4C0A7-DAAD-49C6-B222-F4F1936489CE}", AZ::Module);

        OpenVRGem()
            : CryHooksModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {
#if defined(AZ_PLATFORM_WINDOWS_X64)
                OpenVRDevice::CreateDescriptor(),
#endif // Windows x64
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
#if defined(AZ_PLATFORM_WINDOWS_X64)
                azrtti_typeid<OpenVRDevice>(),
#endif // Windows x64
            };
        }

        void OnSystemEvent(ESystemEvent event, UINT_PTR, UINT_PTR) override
        {
            switch (event)
            {
            case ESYSTEM_EVENT_FLOW_SYSTEM_REGISTER_EXTERNAL_NODES:
                RegisterExternalFlowNodes();
                break;
            }
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(OpenVR_3b1c41404cb147fe87e25bf72e035faa, OpenVR::OpenVRGem)
