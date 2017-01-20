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
#include "Precompiled.h"
#include "Events/ReflectScriptableEvents.h"
#include <AzCore/Script/ScriptContext.h>
#include <AzCore/EBus/ScriptBinder.h>
#include <GameplayEventBus.h>
#include <AzCore/Math/Vector3.h>
#include <LmbrCentral/Shape/ShapeComponentBus.h>


namespace LmbrCentral
{
    //////////////////////////////////////////////////////////////////////////
    /// Scriptable Ebus macros go here
    AZ_SCRIPTABLE_EBUS(
        FloatGameplayNotificationBus,
        AZ::GameplayNotificationBus<float>,
        "{268A8B9F-75F1-4C9F-9838-F6BA148CFC42}",
        "{227DCFE6-B527-4FED-8A4D-5D723B07EAA5}",
        AZ_SCRIPTABLE_EBUS_EVENT(OnGameplayEventAction, const float&)
        AZ_SCRIPTABLE_EBUS_EVENT(OnGameplayEventFailed)
        )

    AZ_SCRIPTABLE_EBUS(
        Vector3GameplayNotificationBus,
        AZ::GameplayNotificationBus<AZ::Vector3>,
        "{06F3A22D-278C-4F4E-AEC6-028C90CBBEC8}",
        "{EFAFBE6F-13D7-4EEF-BD05-DA7CE64B3651}",
        AZ_SCRIPTABLE_EBUS_EVENT(OnGameplayEventAction, const AZ::Vector3&)
        AZ_SCRIPTABLE_EBUS_EVENT(OnGameplayEventFailed)
        )

    void ReflectScriptableEvents::Reflect(AZ::ScriptContext* scriptContext)
    {
        if (scriptContext)
        {
            scriptContext->Class< AZ::GameplayNotificationIdScriptWrapper, void(AZ::ScriptDataContext&), AZ::ScriptContext::SP_VALUE>("GameplayNotificationId")->
                Wrapping<AZ::GameplayNotificationIdScriptWrapper, AZ::GameplayNotificationId>()->
                Field("actionNameCrc", &AZ::GameplayNotificationIdScriptWrapper::m_actionNameCrc, true, true)->
                Field("deviceIndex", &AZ::GameplayNotificationIdScriptWrapper::m_channel, true, true)->
                Operator(AZ::ScriptContext::OPERATOR_TOSTRING, &AZ::GameplayNotificationIdScriptWrapper::ScriptToString)->
                Operator(AZ::ScriptContext::OPERATOR_EQ, &AZ::GameplayNotificationIdScriptWrapper::operator==)->
                Method("Clone", &AZ::GameplayNotificationIdScriptWrapper::Clone);

            ScriptableEBus_FloatGameplayNotificationBus::Reflect(scriptContext);
            ScriptableEBus_Vector3GameplayNotificationBus::Reflect(scriptContext);

            ShapeComponentGeneric::Reflect(scriptContext);
        }
    }
}