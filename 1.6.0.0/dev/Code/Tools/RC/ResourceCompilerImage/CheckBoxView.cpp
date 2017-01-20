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

#include "stdafx.h"
#include "CheckBoxView.h"

void CheckBoxView::OnTinyDocumentChanged(TinyDocument<bool>* pDocument)
{
    this->UpdateView();
}

CheckBoxView::CheckBoxView()
    :   pDocument(0)
{
}

CheckBoxView::~CheckBoxView()
{
    this->ClearDocument();
}

void CheckBoxView::SetDocument(TinyDocument<bool>* pDocument)
{
    this->ClearDocument();

    if (pDocument != 0)
    {
        this->pDocument = pDocument;
        this->pDocument->AddListener(this);
        this->UpdateView();
    }
}

void CheckBoxView::ClearDocument()
{
    if (this->pDocument != 0)
    {
        this->pDocument->RemoveListener(this);
        this->pDocument = 0;
    }
}


LRESULT CheckBoxView::OnChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    if (this->pDocument != 0)
    {
        const bool value = (this->SendMessage(BM_GETCHECK, 0, 0) != 0);

        this->pDocument->SetValue(value);

        bHandled = TRUE;
    }

    return 0;
}

LRESULT CheckBoxView::OnKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    // Check whether we are interested in this key.
    switch (wParam)
    {
    case VK_TAB:
    case VK_RETURN:
    {
        this->GetTopLevelParent().SetFocus();
        bHandled = TRUE;
    }
    default:
        bHandled = FALSE;
        break;
    }
    return 0;
}


BOOL CheckBoxView::SubclassWindow(HWND hWnd)
{
    // Call the base class implementation.
    BOOL bSubclassSuccessful = CWindowImpl<CheckBoxView>::SubclassWindow(hWnd);

    if (bSubclassSuccessful)
    {
        this->UpdateView();
    }

    return bSubclassSuccessful;
}

void CheckBoxView::UpdateView()
{
    if (this->m_hWnd != 0 && this->pDocument != 0)
    {
        const bool value = this->pDocument->GetValue();
        this->SendMessage(BM_SETCHECK, (value ? 1 : 0), 0);
    }
}
