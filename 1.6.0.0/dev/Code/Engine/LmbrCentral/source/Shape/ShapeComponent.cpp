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
#include <AzCore/EBus/ScriptBinder.h>
#include <LmbrCentral/Shape/ShapeComponentBus.h>

namespace LmbrCentral
{
    AZ_SCRIPTABLE_EBUS(
        ShapeComponentRequestsBus,
        ShapeComponentRequestsBus,
        "{0314B791-CA79-4A0E-B240-5EDBCB72F9FC}",
        "{9EE9DD59-AF82-461F-B29E-FBC73EE76FD6}",
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(AZ::Crc32, AZ::Crc32(), GetShapeType)
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(bool, false, IsPointInside, const AZ::Vector3&)
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(float, 0.f, DistanceFromPoint, const AZ::Vector3&)
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(float, 0.f, DistanceSquaredFromPoint, const AZ::Vector3&)
        )

    AZ_SCRIPTABLE_EBUS(
    ShapeComponentNotificationsBus,
    ShapeComponentNotificationsBus,
        "{072E1DC1-717D-446E-9BBD-D7A7EA0EE291}",
        "{A82C9481-693B-4010-9812-1A21B106FCC0}",
        AZ_SCRIPTABLE_EBUS_EVENT(OnShapeChanged)
        )

    void ShapeComponentGeneric::Reflect(AZ::ScriptContext* context)
    {
        AZ::ScriptContext* script = azrtti_cast<AZ::ScriptContext*>(context);
        if (script)
        {
            ScriptableEBus_ShapeComponentRequestsBus::Reflect(script);
            ScriptableEBus_ShapeComponentNotificationsBus::Reflect(script);
        }
    }
} // namespace LmbrCentral
