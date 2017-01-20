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
#pragma once

#include <IEditor.h>
#include <Include/IPlugin.h>

namespace CloudCanvas
{
    namespace StaticData
    {
        class StaticDataMonitor;
    }
}

//------------------------------------------------------------------
class StaticDataMonitorEditorPlugin : public IPlugin
{
public:
    StaticDataMonitorEditorPlugin(IEditor* editor);

    void Release() override;
    void ShowAbout() override {}
    const char* GetPluginGUID() override { return "{7E7BCAC7-B72B-45EA-9C98-B48F95D84AA7}"; }
    DWORD GetPluginVersion() override { return 1; }
    const char* GetPluginName() override { return "StaticDataMonitor"; }
    bool CanExitNow() override { return true; }
    void OnEditorNotify(EEditorNotifyEvent aEventId) override;

 private:
    bool m_registered;
    CloudCanvas::StaticData::StaticDataMonitor* m_monitor{ nullptr };
};
