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

#include <AzToolsFramework/Debug/TraceContextLogFormatter.h>
#include <TraceDrillerHook.h>

namespace AZ
{
    namespace RC
    {
        TraceDrillerHook::TraceDrillerHook()
        {
            BusConnect();
        }
        
        TraceDrillerHook::~TraceDrillerHook()
        {
            BusDisconnect();
        }

        bool TraceDrillerHook::OnPrintf(const char* window, const char* message)
        {
            AZStd::string context;
            AZStd::shared_ptr<const AzToolsFramework::Debug::TraceContextStack> stack = m_stacks.GetCurrentStack();
            if (stack)
            {
                AzToolsFramework::Debug::TraceContextLogFormatter::Print(context, *stack);
            }
            
            if (AzFramework::StringFunc::Equal(window, SceneAPI::Utilities::ErrorWindow))
            {
                RCLogError("%s%.*s", context.c_str(), CalculateLineLength(message), message);
            }
            else if (AzFramework::StringFunc::Equal(window, SceneAPI::Utilities::WarningWindow))
            {
                RCLogWarning("%s%.*s", context.c_str(), CalculateLineLength(message), message);
            }
            else
            {
                RCLog("%s%.*s", context.c_str(), CalculateLineLength(message), message);
            }
            return true;
        }

        size_t TraceDrillerHook::CalculateLineLength(const char* message) const
        {
            size_t length = strlen(message);
            while ((message[length - 1] == '\n' || message[length - 1] == '\r' ) && length > 1)
            {
                length--;
            }
            return length;
        }
    } // RC
} // AZ