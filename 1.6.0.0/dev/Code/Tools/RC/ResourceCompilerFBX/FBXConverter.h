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

#ifndef CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERFBX_FBXCONVERTER_H
#define CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERFBX_FBXCONVERTER_H
#pragma once


#include "IConvertor.h"


class CFbxConverter
    : public IConvertor
    , public ICompiler
{
public:

public:
    CFbxConverter();
    ~CFbxConverter();

    virtual void Release();

    virtual void BeginProcessing(const IConfig* config);
    virtual void EndProcessing() { }
    virtual IConvertContext* GetConvertContext() { return &m_CC; }
    virtual bool Process();

    virtual void Init(const ConvertorInitContext& context);
    virtual ICompiler* CreateCompiler();
    virtual bool SupportsMultithreading() const;
    virtual const char* GetExt(int index) const;

private:
    int m_refCount;
    ConvertContext m_CC;
};

#endif // CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERFBX_FBXCONVERTER_H
