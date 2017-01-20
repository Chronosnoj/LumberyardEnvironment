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

#include "CameraRigComponent.h"

#include <AzCore/Module/Module.h>

namespace CameraFramework
{
    class CameraFrameworkModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(CameraFrameworkModule, "{F9223D55-1D4C-4746-8864-5E2075A4DE50}", AZ::Module);

        CameraFrameworkModule()
            : AZ::Module()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {
                Camera::CameraRigComponent::CreateDescriptor(),
            });
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(CameraFramework_54f2763fe191432fa681ce4a354eedf5, CameraFramework::CameraFrameworkModule)
