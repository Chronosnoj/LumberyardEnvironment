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

#ifndef CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_SLIDERVIEW_H
#define CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_SLIDERVIEW_H
#pragma once


#pragma warning(push)
#pragma warning(disable : 4005)
#include <atlbase.h>
#include <atlwin.h>
#pragma warning(pop)

#include "TinyDocument.h"

class SliderView
    : public CWindowImpl<SliderView>
    , public TinyDocument<float>::Listener
{
public:
    DECLARE_WND_SUPERCLASS(NULL, TRACKBAR_CLASS);

    SliderView();
    ~SliderView();
    void SetDocument(TinyDocument<float>* pDocument);
    void ClearDocument();
    BOOL SubclassWindow(HWND hWnd);
    void UpdatePosition();

    BEGIN_MSG_MAP(SliderView)
    MESSAGE_HANDLER(OCM_HSCROLL, OnScroll)
    DEFAULT_REFLECTION_HANDLER()
    END_MSG_MAP()

private:
    virtual void OnTinyDocumentChanged(TinyDocument<float>* pDocument);

    LRESULT OnScroll(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    TinyDocument<float>* pDocument;
};

#endif // CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_SLIDERVIEW_H
