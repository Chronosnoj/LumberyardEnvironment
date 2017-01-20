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
#include <memory>
#include <string>
#include "utils.h"

// implemented in platform
namespace AzTest
{
    struct IFunctionHandle;

    struct IModuleHandle
    {
        virtual bool IsValid() = 0;
        virtual std::shared_ptr<IFunctionHandle> GetFunction(const std::string& name) = 0;
    };

    struct IFunctionHandle
    {
        virtual bool IsValid() = 0;

        //! call as a "main" function
        virtual int operator()(int argc, char** argv) = 0;

        //! call as simple function
        virtual int operator()() = 0;
    };

    class Platform
    {
    public:
        bool SupportsWaitForDebugger();

        void WaitForDebugger();
        void SuppressPopupWindows();
        std::shared_ptr<IModuleHandle> GetModule(const std::string& lib);
    };

    Platform& GetPlatform();
}
