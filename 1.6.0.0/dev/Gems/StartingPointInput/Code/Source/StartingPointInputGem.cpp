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

#include "Held.h"
#include "Analog.h"
#include "Pressed.h"
#include "Released.h"
#include "OrderedEventCombination.h"
#include "UnorderedEventCombination.h"
#include "VectorizedEventCombination.h"
#include "StartingPointInput/BaseDataTypes.h"

#include <AzCore/Module/Module.h>

namespace StartingPointInput
{
    // Dummy component to get reflect function
    class StartingPointInputDummyComponent
        : public AZ::Component
    {
    public:
        AZ_COMPONENT(StartingPointInputDummyComponent, "{0F784CD5-C5AB-4673-8651-ABFA66060232}");

        static void Reflect(AZ::ReflectContext* context)
        {
            Input::SingleEventToAction::Reflect(context);
            Input::Held::Reflect(context);
            Input::Analog::Reflect(context);
            Input::Pressed::Reflect(context);
            Input::Released::Reflect(context);
            Input::OrderedEventCombination::Reflect(context);
            Input::UnorderedEventCombination::Reflect(context);
            Input::VectorizedEventCombination::Reflect(context);
        }

        void Activate() override { }
        void Deactivate() override { }
    };

    class StartingPointInputModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(StartingPointInputModule, "{B30D421E-127D-4C46-90B1-AC3DDF3EC1D9}", AZ::Module);

        StartingPointInputModule()
            : AZ::Module()
        {
            m_descriptors.insert(m_descriptors.end(), {
                StartingPointInputDummyComponent::CreateDescriptor(),
            });
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(StartingPointInput_09f4bedeee614358bc36788e77f97e51, StartingPointInput::StartingPointInputModule)
