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
#include <windows.h>
#include <string>
#include <iostream>
#include "main.h"

//-------------------------------------------------------------------------------------------------
class ModuleHandle 
    : public AzTest::IModuleHandle
{
public:
    explicit ModuleHandle(const std::string& lib)
        : m_libHandle(NULL)
    {
        std::string libext = lib;
        if (!AzTest::Utils::endsWith(libext, ".dll"))
        {
            libext += ".dll";
        }
        m_libHandle = LoadLibraryA(libext.c_str());  // LoadLibrary conflicts with CryEngine code, so use LoadLibraryA

        if (m_libHandle == NULL)
        {
            DWORD dw = ::GetLastError();
            std::cerr << "FAILED to load library: " << libext << "; GetLastError() returned " << dw << std::endl;
        }
    }

    ModuleHandle(const ModuleHandle&) = delete;
    ModuleHandle& operator=(const ModuleHandle&) = delete;

    ~ModuleHandle()
    {
        if (m_libHandle != NULL)
        {
            FreeLibrary(m_libHandle);
        }
    }

    bool IsValid() override { return m_libHandle != NULL; }

    std::shared_ptr<AzTest::IFunctionHandle> GetFunction(const std::string& name) override;

private:
    friend class FunctionHandle;

    HINSTANCE m_libHandle;
};

//-------------------------------------------------------------------------------------------------
class FunctionHandle
    : public AzTest::IFunctionHandle
{
public:
    explicit FunctionHandle(ModuleHandle& module, std::string symbol)
        : m_proc(NULL)
    {
        m_proc = ::GetProcAddress(module.m_libHandle, symbol.c_str());
    }

    FunctionHandle(const FunctionHandle&) = delete;
    FunctionHandle& operator=(const FunctionHandle&) = delete;

    ~FunctionHandle()
    {
    }

    int operator()(int argc, char** argv) override
    {
        using Fn = int(int, char**);

        Fn* fn = reinterpret_cast<Fn*>(m_proc);
        return (*fn)(argc, argv);
    }

    int operator()() override
    {
        using Fn = int();

        Fn* fn = reinterpret_cast<Fn*>(m_proc);
        return (*fn)();
    }

    bool IsValid() override { return m_proc != NULL; }

private:
    FARPROC m_proc;
};

//-------------------------------------------------------------------------------------------------

std::shared_ptr<AzTest::IFunctionHandle> ModuleHandle::GetFunction(const std::string& name)
{
    return std::make_shared<FunctionHandle>(*this, name);
}

//-------------------------------------------------------------------------------------------------
namespace AzTest
{
    Platform& GetPlatform()
    {
        static Platform s_platform;
        return s_platform;
    }

    bool Platform::SupportsWaitForDebugger()
    {
        return true;
    }

    std::shared_ptr<IModuleHandle> Platform::GetModule(const std::string& lib)
    {
        return std::make_shared<ModuleHandle>(lib);
    }

    void Platform::WaitForDebugger()
    {
        while (!::IsDebuggerPresent()) {}
    }

    void Platform::SuppressPopupWindows()
    {
        // use SetErrorMode to disable popup windows in case a required library cannot be found
        DWORD errorMode = ::SetErrorMode(SEM_FAILCRITICALERRORS);
        ::SetErrorMode(errorMode | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    }
}