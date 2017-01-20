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

#ifndef CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_FORMATS_HDR_H
#define CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_FORMATS_HDR_H
#pragma once

#include <vector>

#include <assert.h>
#include <CryString.h>

class CImageProperties;
class ImageObject;

namespace ImageHDR
{
    ImageObject* LoadByUsingHDRLoader(const char* filenameRead, CImageProperties* pProps, string& res_specialInstructions);

    bool SaveByUsingHDRSaver(const char* filenameWrite, const CImageProperties* pProps, const ImageObject* pImageObject);
};

#endif // CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_FORMATS_HDR_H
