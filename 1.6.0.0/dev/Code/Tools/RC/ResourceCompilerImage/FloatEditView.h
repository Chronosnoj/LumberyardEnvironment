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

#ifndef CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_FLOATEDITVIEW_H
#define CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_FLOATEDITVIEW_H
#pragma once


#pragma warning(push)
#pragma warning(disable : 4005)
#include <atlbase.h>
#include <atlwin.h>
#pragma warning(pop)

#include "TinyDocument.h"

class FloatEditView
    : public CWindowImpl<FloatEditView>
    , public TinyDocument<float>::Listener
{
public:
    DECLARE_WND_SUPERCLASS(NULL, "EDIT");

    FloatEditView();
    ~FloatEditView();
    void SetDocument(TinyDocument<float>* pDocument);
    void ClearDocument();
    BOOL SubclassWindow(HWND hWnd);
    void UpdateText();

    BEGIN_MSG_MAP(SliderView)
    REFLECTED_COMMAND_CODE_HANDLER(EN_KILLFOCUS, OnKillFocus);
    MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown);
    DEFAULT_REFLECTION_HANDLER()
    END_MSG_MAP()

private:
    virtual void OnTinyDocumentChanged(TinyDocument<float>* pDocument);
    LRESULT OnKillFocus(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

    TinyDocument<float>* pDocument;
};

#endif // CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_FLOATEDITVIEW_H
