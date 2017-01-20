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

#include "stdafx.h"
#include "IResCompiler.h"
#include "IRCLog.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Must be included only once in DLL module.
#include <platform_implRC.h>

#include "FBXConverter.h"

static HMODULE g_hInst;

BOOL APIENTRY DllMain(
    HANDLE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hInst = (HMODULE)hModule;
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}


void __stdcall RegisterConvertors(IResourceCompiler* pRC)
{
    gEnv = pRC->GetSystemEnvironment();

    SetRCLog(pRC->GetIRCLog());

    pRC->RegisterConvertor("FbxConverter", new CFbxConverter());
    pRC->RegisterKey("manifest",
        "[FBX] Specifies path to manifest file that will be created\n"
        "containing high-level information about the FBX file.\n"
        "For internal use only.\n"
        "Subject to change without prior notice.");
}
