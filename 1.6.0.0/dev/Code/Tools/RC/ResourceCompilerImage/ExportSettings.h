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
///////////////////////////////////////////////////////////////////////////////////
// Export settings i/o using Qt
#ifndef CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_FORMATS_EXPORTSETTINGS_H
#define CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_FORMATS_EXPORTSETTINGS_H
#pragma once

#include <CryString.h>

namespace ImageExportSettings
{
    bool LoadSettings(const char* filename, string& settings);
    bool SaveSettings(const char* filename, const string& settings);
};

#endif // CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_FORMATS_EXPORTSETTINGS_H
