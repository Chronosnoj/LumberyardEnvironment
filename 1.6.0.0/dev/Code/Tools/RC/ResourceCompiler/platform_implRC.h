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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

// This file should only be included Once in DLL module.

#ifndef CRYINCLUDE_TOOLS_RC_RESOURCECOMPILER_PLATFORM_IMPLRC_H
#define CRYINCLUDE_TOOLS_RC_RESOURCECOMPILER_PLATFORM_IMPLRC_H
#pragma once

#include <platform.h>

#if defined(AZ_MONOLITHIC_BUILD)
#   error It is not allowed to have AZ_MONOLITHIC_BUILD defined
#endif

#include <CryCommon.cpp>
#include <IRCLog.h>

#include <Random.h>

#include "CryThreadImpl_windows.h"

CRndGen CryRandom_Internal::g_random_generator;

struct SSystemGlobalEnvironment;
SSystemGlobalEnvironment* gEnv = 0;
IRCLog* g_pRCLog = 0;

//////////////////////////////////////////////////////////////////////////
// This is an entry to DLL initialization function that must be called for each loaded module
//////////////////////////////////////////////////////////////////////////
extern "C" DLL_EXPORT void ModuleInitISystem(ISystem* pSystem, const char* moduleName)
{
    if (gEnv) // Already registered.
    {
        return;
    }

    if (pSystem)
    {
        gEnv = pSystem->GetGlobalEnvironment();
    }
}

void CrySleep(unsigned int dwMilliseconds)
{
    Sleep(dwMilliseconds);
}


void CryLowLatencySleep(unsigned int dwMilliseconds)
{
    CrySleep(dwMilliseconds);
}


LONG    CryInterlockedCompareExchange(LONG volatile* dst, LONG exchange, LONG comperand)
{
    return InterlockedCompareExchange(dst, exchange, comperand);
}


void SetRCLog(IRCLog* pRCLog)
{
    g_pRCLog = pRCLog;
}

void RCLog(const char* szFormat, ...)
{
    va_list args;
    va_start(args, szFormat);
    if (g_pRCLog)
    {
        g_pRCLog->LogV(IRCLog::eType_Info, szFormat, args);
    }
    else
    {
        vprintf(szFormat, args);
        printf("\n");
        fflush(stdout);
    }
    va_end(args);
}

void RCLogWarning(const char* szFormat, ...)
{
    va_list args;
    va_start(args, szFormat);
    if (g_pRCLog)
    {
        g_pRCLog->LogV(IRCLog::eType_Warning, szFormat, args);
    }
    else
    {
        vprintf(szFormat, args);
        printf("\n");
        fflush(stdout);
    }
    va_end(args);
}

void RCLogError(const char* szFormat, ...)
{
    va_list args;
    va_start (args, szFormat);
    if (g_pRCLog)
    {
        g_pRCLog->LogV(IRCLog::eType_Error, szFormat, args);
    }
    else
    {
        vprintf(szFormat, args);
        printf("\n");
        fflush(stdout);
    }
    va_end(args);
}


//////////////////////////////////////////////////////////////////////////
// Log important data that must be printed regardless verbosity.

void PlatformLogOutput(const char*, ...) PRINTF_PARAMS(1, 2);

inline void PlatformLogOutput(const char* format, ...)
{
    assert(g_pRCLog);
    if (g_pRCLog)
    {
        va_list args;
        va_start(args, format);
        g_pRCLog->LogV(IRCLog::eType_Error,  format, args);
        va_end(args);
    }
}


// when using STL Port _STLP_DEBUG and _STLP_DEBUG_TERMINATE - avoid actually
// crashing (default terminator seems to kill the thread, which isn't nice).
#ifdef _STLP_DEBUG_TERMINATE
void __stl_debug_terminate(void)
{
    assert(0 && "STL Debug Error");
}
#endif

#ifdef _STLP_DEBUG_MESSAGE
void __stl_debug_message(const char* format_str, ...)
{
    va_list __args;
    va_start(__args, format_str);
    char __buffer[4096];
    _vsnprintf_s(__buffer, sizeof(__buffer), sizeof(__buffer) - 1, format_str, __args);
    PlatformLogOutput(__buffer); // Log error.
    va_end(__args);
}
#endif //_STLP_DEBUG_MESSAGE

#endif // CRYINCLUDE_TOOLS_RC_RESOURCECOMPILER_PLATFORM_IMPLRC_H
