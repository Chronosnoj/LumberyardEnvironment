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

#include <AzCore/Component/ComponentBus.h>

namespace LmbrCentral
{
    /*!
     * ParticleComponentRequestBus
     * Messages serviced by the Particle component.
     */
    class ParticleComponentRequests
        : public AZ::ComponentBus
    {
    public:

        virtual ~ParticleComponentRequests() {}
        virtual void Show() = 0;
        virtual void Hide() = 0;
        virtual void SetVisibility(bool visible) = 0;
    };

    using ParticleComponentRequestBus = AZ::EBus <ParticleComponentRequests>;

    /*!
     * ParticleComponentEvents
     * Events dispatched by the Particle component.
     */
    class ParticleComponentEvents
        : public AZ::ComponentBus
    {
    public:

        using Bus = AZ::EBus<ParticleComponentEvents>;
    };
} // namespace LmbrCentral
