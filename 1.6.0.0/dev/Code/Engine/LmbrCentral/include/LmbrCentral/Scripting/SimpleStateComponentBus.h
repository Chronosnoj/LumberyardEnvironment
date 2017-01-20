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
    namespace SimpleStateConstants
    {
        static const char* NullStateName = "";
    }

    /*!
     * SimpleStateComponentRequests
     * Messages serviced by the Simple State component.
     */
    class SimpleStateComponentRequests
        : public AZ::ComponentBus
    {
    public:
        virtual ~SimpleStateComponentRequests() {}

        //! Set the active state
        virtual void SetState(const char* stateName) = 0;
        
        //! Get the current state
        virtual const char* GetCurrentState() = 0;
    };

    using SimpleStateComponentRequestBus = AZ::EBus<SimpleStateComponentRequests>;

    /*!
     * SimpleStateComponentNotificationBus
     * Events dispatched by the Simple State component.
     */
    class SimpleStateComponentNotifications
        : public AZ::ComponentBus
    {
    public:
        virtual ~SimpleStateComponentNotifications() {}

        //! Notify that the state has changed from oldState to newState
        virtual void OnStateChanged(const char* oldState, const char* newState) = 0;
    };

    using SimpleStateComponentNotificationBus = AZ::EBus<SimpleStateComponentNotifications>;
} // namespace LmbrCentral
