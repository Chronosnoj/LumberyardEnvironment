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
#include "common/LabApplication.h"

#include <AzCore/Module/Module.h>
#include <AzCore/Memory/AllocationRecords.h>
#include <AzFramework/StringFunc/StringFunc.h>

#include <GridMate/Memory.h>

#if !defined(AZ_PLATFORM_WINDOWS)
#include <cstring>
#endif // !WINDOWS

void LabApplication::StartCommon(AZ::Entity* systemEntity)
{
    ToolsApplication::StartCommon(systemEntity);

    m_commandLine.Parse(m_argc, m_argv);

    AZ::IO::FileIOEventBus::Handler::BusConnect();
}

void LabApplication::Stop()
{
    AZ::IO::FileIOEventBus::Handler::BusDisconnect();
    ToolsApplication::Stop();
}

void LabApplication::ReflectSerialize()
{
    ToolsApplication::ReflectSerialize();
}

void LabApplication::RegisterCoreComponents()
{
    ToolsApplication::RegisterCoreComponents();
}

void LabApplication::AddSystemComponents(AZ::Entity* systemEntity)
{
    ToolsApplication::AddSystemComponents(systemEntity);
}

void LabApplication::OnError(const AZ::IO::SystemFile* file, const char* fileName, int errorCode)
{
    static const unsigned int STR_LEN = 1024;
    char errorBuffer[STR_LEN];
#if defined(AZ_PLATFORM_WINDOWS)
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        errorBuffer,
        STR_LEN,
        nullptr);
#else
    char* errorStr = strerror(errorCode);
    azstrcpy(errorBuffer, STR_LEN, errorStr);
#endif // AZ_PLATFORM_WINDOWS

    const char* path = "";
    if (file)
    {
        path = file->Name();
    }
    else if (fileName)
    {
        path = fileName;
    }

    AZ_Assert(false, "File Error:\n%s: %s\n", path, errorBuffer);
}
