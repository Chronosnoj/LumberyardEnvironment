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

// Description : source file that includes just the standard includes

#pragma once

//#define NOT_USE_CRY_MEMORY_MANAGER
//#define NOT_USE_CRY_STRING

// Include Configuration settings for Standard Template Library
#include <STLPortConfig.h>

#include <assert.h>

#define CRY_ASSERT(condition) assert(condition)
#define CRY_ASSERT_TRACE(condition, message) assert(condition)
#define CRY_ASSERT_MESSAGE(condition, message) assert(condition)

// Define this to prevent including CryAssert (there is no proper hook for turning this off, like the above).
#define CRYINCLUDE_CRYCOMMON_CRYASSERT_H

//#include "ixml.h"
#include <platform.h>

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS  // some string constructors will be explicit

// STL headers.
#include <vector>
#include <list>
#include <algorithm>
#include <functional>
#include <map>
#include <set>

#include <stdio.h>
#include <tchar.h>

//#include <StlDbgAlloc.h>
// to make smoother transition back from cry to std namespace...
#define cry std
#define CRY_AS_STD

#include <smartptr.h>

#include "CryVersion.h"
#include "ConvertContext.h"
