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

#ifndef CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_UIMIPCONTROLPANEL_H
#define CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_UIMIPCONTROLPANEL_H
#pragma once

#include "EditSliderPair.h"
#include "ScrollableDialog.h"
#include "resource.h"
#include "ImageProperties.h"
#include <set>

class CImageProperties;


class CUIMIPControlPanel
{
public:
    enum
    {
        NUM_CONTROLLED_MIP_MAPS = CImageProperties::NUM_CONTROLLED_MIP_MAPS
    };

    CUIMIPControlPanel();

    void InitDialog(HWND hWndParent, const bool bShowBumpmapFileName);

    void GetDataFromDialog(CImageProperties& rProp);

    void SetDataToDialog(const IConfig* pConfig, const CImageProperties& rProp, const bool bInitalUpdate);

    HWND GetHWND() const;

    CEditSliderPair m_MIPAlphaControl[NUM_CONTROLLED_MIP_MAPS];
    CEditSliderPair m_MIPBlurringControl;

    int m_GenIndexToControl[eWindowFunction_Num];
    int m_GenControlToIndex[eWindowFunction_Num];

private: // ---------------------------------------------------------------------------------------

    class CMipMapValuesDialog
        : public CScrollableDialog<CMipMapValuesDialog>
    {
    public:
        CMipMapValuesDialog()
            : m_pPanel(0)
        {
        }

        void SetParent(CUIMIPControlPanel* pPanel)
        {
            m_pPanel = pPanel;
        }

        enum
        {
            IDD = IDD_PANEL_MIPMAPVALUES
        };

        BEGIN_MSG_MAP(CMipMapValuesDialog)
        REFLECT_NOTIFICATIONS()
        CHAIN_MSG_MAP(CScrollableDialog)
        END_MSG_MAP()

    private:
        CUIMIPControlPanel* m_pPanel;
    };

    HWND m_hTab;  // window handle
    CMipMapValuesDialog m_valuesDlg;

    static const int sliderIDs[NUM_CONTROLLED_MIP_MAPS];
    static const int editIDs[NUM_CONTROLLED_MIP_MAPS];

    static INT_PTR CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif // CRYINCLUDE_TOOLS_RC_RESOURCECOMPILERIMAGE_UIMIPCONTROLPANEL_H
