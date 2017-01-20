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
#include "OculusDevice.h"
#endif // Windows x64

namespace Oculus
{
    class OculusGem
        : public CryHooksModule
    {
    public:
        AZ_RTTI(OculusGem, "{4BEB17B4-A97D-40EE-B5C6-A296436B753C}", AZ::Module);

        OculusGem()
            : CryHooksModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {
#if defined(AZ_PLATFORM_WINDOWS_X64)
                OculusDevice::CreateDescriptor(),
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
                azrtti_typeid<OculusDevice>(),
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
AZ_DECLARE_MODULE_CLASS(Oculus_c32178b3c4e94fbead069bd92ff9b04a, Oculus::OculusGem)
