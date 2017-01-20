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

#include "EventActionBindingComponent.h"
#include "MoveEntityAction.h"
#include "RotateEntityAction.h"
#include "AddPhysicsImpulseAction.h"

#include <AzCore/Module/Module.h>

namespace StartingPointMovement
{
    class StartingPointMovementModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(StartingPointMovementModule, "{AE406AE3-77AE-4CA6-84AD-842224EE2188}", AZ::Module);

        StartingPointMovementModule()
            : AZ::Module()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {
                Movement::EventActionBindingComponent::CreateDescriptor(),
            });
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(StartingPointMovement_73d8779dc28a4123b7c9ed76217464af, StartingPointMovement::StartingPointMovementModule)
