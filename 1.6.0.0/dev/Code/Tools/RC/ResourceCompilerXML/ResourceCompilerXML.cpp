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
#include "XMLConverter.h"
#include "IRCLog.h"
#include "XML/xml.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Must be included only once in DLL module.
#include <platform_implRC.h>

static HMODULE g_hInst;

XmlStrCmpFunc g_pXmlStrCmp = &_stricmp;

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

ICryXML* LoadICryXML()
{
    HMODULE hXMLLibrary = LoadLibrary("CryXML.dll");
    if (NULL == hXMLLibrary)
    {
        RCLogError("Unable to load xml library (CryXML.dll).");
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

void __stdcall RegisterConvertors(IResourceCompiler* pRC)
{
    gEnv = pRC->GetSystemEnvironment();
    SetRCLog(pRC->GetIRCLog());

    ICryXML* pCryXML = LoadICryXML();
    if (pCryXML == 0)
    {
        RCLogError("Loading xml library failed - not registering xml converter.");
        return;
    }

    pCryXML->AddRef();

    pRC->RegisterConvertor("XMLConverter", new XMLConverter(pCryXML));

    pRC->RegisterKey("xmlFilterFile", "specify file with special commands to filter out unneeded XML elements and attributes");

    pCryXML->Release();
}

