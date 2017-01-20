
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
#include "AssetBuilderSDK.h"
#include "AssetBuilderBusses.h"
namespace AssetBuilderSDK
{
    const char* const ErrorWindow = "Error"; //Use this window name to log error messages.
    const char* const WarningWindow = "Warning"; //Use this window name to log warning messages.
    const char* const InfoWindow = "Info"; //Use this window name to log info messages.

    void BuilderLog(AZ::Uuid builderId, const char* message, ...)
    {
        va_list args;
        va_start(args, message);
        EBUS_EVENT(AssetBuilderSDK::AssetBuilderBus, BuilderLog, builderId, message, args);
        va_end(args);
    }
}