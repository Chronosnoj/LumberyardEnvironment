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

// Description : implementation of the CRelativeMouseInput class.


#include "stdafx.h"
#include "RelativeMouseInput.h"


// constuctor
CRelativeMouseInput::CRelativeMouseInput()
{
    m_Captured = false;
}

// destructor
CRelativeMouseInput::~CRelativeMouseInput()
{
}



void CRelativeMouseInput::OnButtonDown(HWND inHwnd)     // call in OnLButtonDown/OnMButtonDown/OnRButtonDown
{
    if (!m_Captured)
    {
        SetCapture(inHwnd);
        m_Captured = true;

        GetCursorPos(&m_savedpos);

        int midx = GetSystemMetrics(SM_CXSCREEN) / 2;
        int midy = GetSystemMetrics(SM_CYSCREEN) / 2;

        SetCursorPos(midx, midy);
        m_oldx = midx;
        m_oldy = midy;
        ShowCursor(false);
    }
}


void CRelativeMouseInput::OnButtonUp(void)      // call in OnLButtonUp/OnMButtonUp/OnRButtonUp
{
    if (m_Captured)
    {
        SetCursorPos(m_savedpos.x, m_savedpos.y);
        ShowCursor(true);
        m_Captured = false;

        ReleaseCapture();
    }
}



bool CRelativeMouseInput::OnMouseMove(HWND inHwnd, bool inbButtonDown, int& outRelx, int& outRely)      // call in OnMouseMove
{
    outRelx = 0;
    outRely = 0;

    if (m_Captured)
    {
        if (!inbButtonDown)
        {
            OnButtonUp();
        }
    }
    else
    {
        if (inbButtonDown)
        {
            OnButtonDown(inHwnd);
        }
    }

    if (m_Captured)
    {
        POINT point;

        GetCursorPos(&point);

        int midx = GetSystemMetrics(SM_CXSCREEN) / 2;
        int midy = GetSystemMetrics(SM_CYSCREEN) / 2;

        int relx = point.x - m_oldx;
        int rely = point.y - m_oldy;

        int absx = point.x - midx;
        int absy = point.y - midy;

        //          GetDC()->SetPixel(relx+300,rely+200,0x000000);
        outRelx = relx;
        outRely = rely;

        bool reset = false;

        if (absx < -200)
        {
            reset = true;
            absx += 200;
        }
        if (absx > 200)
        {
            reset = true;
            absx -= 200;
        }
        if (absy < -200)
        {
            reset = true;
            absy += 200;
        }
        if (absy > 200)
        {
            reset = true;
            absy -= 200;
        }


        if (reset)
        {
            SetCursorPos(midx + absx, midy + absy);
            m_oldx = midx + absx;
            m_oldy = midy + absy;
        }
        else
        {
            m_oldx = point.x;
            m_oldy = point.y;
        }

        if (outRelx == 0 && outRely == 0)
        {
            return(false);
        }

        return(true);
    }
    else
    {
        return(false);
    }
}



bool CRelativeMouseInput::IsCaptured(void)
{
    return(m_Captured);
}

