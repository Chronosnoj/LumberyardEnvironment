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

// Description : Defines the entry point for the DLL application.


#include "stdafx.h"
#include "CryAssert_impl.h"
#include "ICryXML.h"
#include "XMLSerializer.h"

struct SSystemGlobalEnvironment;
SSystemGlobalEnvironment* gEnv = nullptr;

class CryXML
    : public ICryXML
{
public:
    CryXML();
    virtual void AddRef();
    virtual void Release();
    virtual IXMLSerializer* GetXMLSerializer();

private:
    int nRefCount;
    XMLSerializer serializer;
};

static CryXML* s_pCryXML = nullptr;

#if defined(WIN32)
#ifndef AZ_MONOLITHIC_BUILD
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}
#endif // AZ_MONOLITHIC_BUILD
#endif

ICryXML * __stdcall GetICryXML()
{
    if (!s_pCryXML)
    {
        s_pCryXML = new CryXML;
    }
    return s_pCryXML;
}

CryXML::CryXML()
    :   nRefCount(0)
{
}

void CryXML::AddRef()
{
    ++this->nRefCount;
}

void CryXML::Release()
{
    --this->nRefCount;
    if (this->nRefCount == 0)
    {
        delete this;
    }
}

IXMLSerializer* CryXML::GetXMLSerializer()
{
    return &this->serializer;
}


// STLPort requires folowing functions defined:

// when using STL Port _STLP_DEBUG and _STLP_DEBUG_TERMINATE - avoid actually
// crashing (default terminator seems to kill the thread, which isn't nice).
#ifdef _STLP_DEBUG_TERMINATE
void __stl_debug_terminate(void)
{
    assert(0 && "STL Debug Error");
}
#endif
#ifdef _STLP_DEBUG_MESSAGE
void __stl_debug_message(const char* format_str, ...)
{
    va_list __args;
    va_start(__args, format_str);
    vprintf(format_str, __args);
    va_end(__args);
}
#endif //_STLP_DEBUG_MESSAGE
