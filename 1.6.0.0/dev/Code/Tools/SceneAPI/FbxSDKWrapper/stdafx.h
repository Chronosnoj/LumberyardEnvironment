#pragma once

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

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

// If defined, we can mix with release reasonably safe, but we can't link Fbx in debug
// Might have to change this so it's defined, and link release FBX instead, if this causes problems
// #define CHANGE_STL_DEBUG_SETTINGS

// Include Configuration settings for Standard Template Library
#include <STLPortConfig.h>

#include <assert.h>

#define CRY_ASSERT_TRACE(condition, message) assert(condition)

// Define this to prevent including CryAssert (there is no proper hook for turning this off, like the above).
#define CRYINCLUDE_CRYCOMMON_CRYASSERT_H

#include <platform.h>

#include <vector>
#include <map>

#include <stdio.h>
#include <tchar.h>