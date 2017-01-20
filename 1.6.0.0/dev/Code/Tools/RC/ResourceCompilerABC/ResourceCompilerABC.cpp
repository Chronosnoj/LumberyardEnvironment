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
#include "../../CryXML/ICryXML.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Must be included only once in DLL module.
#include <platform_implRC.h>

#include "AlembicConvertor.h"

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

namespace
{
    ICryXML* LoadICryXML()
    {
        HMODULE hXMLLibrary = LoadLibrary("CryXML.dll");
        if (NULL == hXMLLibrary)
        {
            char szCurrentDirectory[512];
            GetCurrentDirectory(sizeof(szCurrentDirectory), szCurrentDirectory);

            RCLogError("Unable to load xml library (CryXML.dll)");
            RCLogError("  Current Directory: %s", szCurrentDirectory);       // useful to track down errors
            return 0;
        }

        FnGetICryXML pfnGetICryXML = (FnGetICryXML)GetProcAddress(hXMLLibrary, "GetICryXML");
        if (pfnGetICryXML == 0)
        {
            RCLogError("Unable to load xml library (CryXML.dll) - cannot find exported function GetICryXML().");
            return 0;
        }

        return pfnGetICryXML();
    }
}

void __stdcall RegisterConvertors(IResourceCompiler* pRC)
{
    gEnv = pRC->GetSystemEnvironment();

    SetRCLog(pRC->GetIRCLog());

    ICryXML* const pCryXML = LoadICryXML();

    if (pCryXML == 0)
    {
        RCLogError("Loading xml library failed - not registering collada converter.");
    }
    else
    {
        pRC->RegisterConvertor("AlembicCompiler", new AlembicConvertor(pCryXML, pRC->GetPakSystem()));

        pRC->RegisterKey("upAxis", "[ABC] Up axis of alembic file\n"
            "Z = Use Z as up axis: No conversion\n"
            "Y = Use Y as up axis: Convert Y up to Z up (default)");

        pRC->RegisterKey("meshPrediction", "[ABC] Use mesh prediction for index frames\n"
            "0 = No mesh prediction (default)\n"
            "1 = Use mesh prediction");

        pRC->RegisterKey("useBFrames", "[ABC] Use bi-directional predicted frames\n"
            "0 = Don't use b-frames (default)\n"
            "1 = Use b-frames");

        pRC->RegisterKey("indexFrameDistance", "[ABC] Index frame distance when using b-frames (default is 15)");

        pRC->RegisterKey("blockCompressionFormat", "[ABC] Method used to compress data\n"
            "store = No compression\n"
            "deflate = Use deflate (zlib) compression (default)");

        pRC->RegisterKey("playbackFromMemory", "[ABC] Set flag that resulting cache will be played back from memory\n"
            "0 = Do not play back from memory (default)\n"
            "1 = Cache plays from memory after loading");

        pRC->RegisterKey("positionPrecision", "[ABC] Set the position precision in mm. Higher values usually result in better compression (default is 1)");

        pRC->RegisterKey("uvMax", "[ABC] Set the upper value of the UV range. Values above this value will be wrapped.\n"
            "0 = use detected per-mesh uvMax values. (default is 0)");

        pRC->RegisterKey("skipFilesWithoutBuildConfig", "[ABC] Skip files without build configuration (.CBC)");
    }
}
