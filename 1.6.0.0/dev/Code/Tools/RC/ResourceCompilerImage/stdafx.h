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

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently

#pragma once

// Include Configuration settings for Standard Template Library
#include <STLPortConfig.h>

#include <cassert>

// Define this to prevent including CryAssert (there is no proper hook for turning this off).
#define CRYINCLUDE_CRYCOMMON_CRYASSERT_H
#define CRY_ASSERT(condition)
#define CRY_ASSERT_MESSAGE(condition, message)
#define CRY_ASSERT_TRACE(condition, parenthese_message)

#include <platform.h>

#define WIN32_LEAN_AND_MEAN     // Exclude rarely-used stuff from Windows headers


#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500     // min Win2000 for GetConsoleWindow()     Baustelle
#endif

// Windows Header Files:
#include <windows.h>

#include <stdio.h>
#include <tchar.h>
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS  // some string constructors will be explicit

#include <atlbase.h>
#include <atlstr.h>

#include <vector>
#include <map>
#include <memory>
#include <algorithm>

class ICrySourceControl;

extern HMODULE g_hInst;
ICrySourceControl* CrySourceControl();


#ifndef ReleasePpo
#define ReleasePpo(ppo)      \
    if (*(ppo) != NULL)      \
    {                        \
        (*(ppo))->Release(); \
        *(ppo) = NULL;       \
    }                        \
    else (VOID)0
#endif

#include "CompileTimeAssert.h"
#include <Cry_Math.h>
#include <IConsole.h>

#include <QDir>
#include <QFile>
// TODO: reference additional headers your program requires here
