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

// Description : include file for standard system include files,

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//


#pragma once


//#define CHANGE_STL_DEBUG_SETTINGS

// Include Configuration settings for Standard Template Library
#include <STLPortConfig.h>

#include <assert.h>

#define CRY_ASSERT(condition) assert(condition)
#define CRY_ASSERT_TRACE(condition, message) assert(condition)
#define CRY_ASSERT_MESSAGE(condition, message) assert(condition)

// Define this to prevent including CryAssert (there is no proper hook for turning this off, like the above).
#define CRYINCLUDE_CRYCOMMON_CRYASSERT_H

#include <platform.h>

#include "ixml.h"

// Standard C headers.
#include <direct.h>

// STL headers.
#include <vector>
#include <list>
#include <algorithm>
#include <functional>
#include <map>
#include <set>

//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////


#include <stdio.h>
#include <tchar.h>

#define VC_EXTRALEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>

//#include <StlDbgAlloc.h>
// to make smoother transition back from cry to std namespace...
#define cry std
#define CRY_AS_STD

#include <smartptr.h>

//////////////////////////////////////////////////////////////////////////
#include "ConvertContext.h"

#include <Cry_Math.h>
#include <primitives.h>
#include <CryHeaders.h>
#include <CryVersion.h>

#include "StlUtils.h"

#include <QApplication>
#include <QDir>
#include <QImage>