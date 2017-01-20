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
#include "IEventActionHandler.h"

namespace Movement
{
    class EventActionBindingComponent
        : public AZ::Component
    {
    public:
        ~EventActionBindingComponent() override = default;
        AZ_COMPONENT(EventActionBindingComponent, "{2BB79CFC-7EBC-4EF4-A62E-5D64CB45CDBD}");

        static void Reflect(AZ::ReflectContext* reflection);

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;
    private:
        AZStd::vector<IEventActionSubComponent*> m_eventActionBindings;
        AZ::EntityId m_sourceEntity;
    };
} // namespace Movement