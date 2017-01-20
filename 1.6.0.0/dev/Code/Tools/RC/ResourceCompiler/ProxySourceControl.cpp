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

#include "ProxySourceControl.h"
#include "IRCLog.h"
#include "CryLibrary.h"

namespace
{
    const char* sourceControlPluginName = "CryPerforce.dll";
}
void CProxySourceControl::SetSourceControlWorkspace(const char* strWorkspace)
{
    azstrncpy(m_workspace, AZ_MAX_PATH_LEN, strWorkspace, AZ_MAX_PATH_LEN);
}

void CProxySourceControl::SetSourceControlUser(const char* strUser)
{
    azstrncpy(m_user, AZ_MAX_PATH_LEN, strUser, AZ_MAX_PATH_LEN);
}

bool CProxySourceControl::GetFileHeadRevisionInfo(const char* fileName, CSourceControlFileInfo& fileInfo, CSourceControlUserInfo& userInfo)
{
    LoadSourceControlDLL();
    if (m_actualSourceControl)
    {
        return m_actualSourceControl->GetFileHeadRevisionInfo(fileName, fileInfo, userInfo);
    }

    return false;
}

bool CProxySourceControl::IsUnderSourceControl(const char* fileName)
{
    LoadSourceControlDLL();
    if (m_actualSourceControl)
    {
        return m_actualSourceControl->IsUnderSourceControl(fileName);
    }

    return false;
}

void CProxySourceControl::ConvertTimeToText(int time, char* buffer, size_t bufferSizeInBytes)
{
    if (m_actualSourceControl)
    {
        m_actualSourceControl->ConvertTimeToText(time, buffer, bufferSizeInBytes);
    }
}

bool CProxySourceControl::MakeWritable(const char* fileName)
{
    LoadSourceControlDLL();
    if (m_actualSourceControl)
    {
        return m_actualSourceControl->MakeWritable(fileName);
    }

    return false;
}

bool CProxySourceControl::Add(const char* fileName)
{
    LoadSourceControlDLL();
    if (m_actualSourceControl)
    {
        return m_actualSourceControl->Add(fileName);
    }

    return false;
}

bool CProxySourceControl::GetLastErrorText(char* buffer, size_t bufSize) const
{
    if (m_actualSourceControl)
    {
        return m_actualSourceControl->GetLastErrorText(buffer, bufSize);
    }

    return false;
}

void CProxySourceControl::LoadSourceControlDLL()
{
    if (!m_actualSourceControl)
    {
        char fullPath[AZ_MAX_PATH_LEN] = { 0 };
        azsnprintf(fullPath, AZ_MAX_PATH_LEN, "%s%s", m_RCExecutableDirectory, sourceControlPluginName);
        if (!AZ::IO::SystemFile::Exists(fullPath))
        {
            RCLogError("Source control plug-in module %s doesn't exist", fullPath);
            return;
        }

        HMODULE hPlugin = LoadLibraryA(fullPath);
        if (!hPlugin)
        {
            RCLogError("Couldn't load source control plug-in module %s", fullPath);
            return;
        }

        m_createSourceControl = (hPlugin) ? (FnCreateSourceControl)CryGetProcAddress(hPlugin, "CreateSourceControl") : nullptr;

        m_destroySourceControl = (hPlugin) ? (FnDestroySourceControl)CryGetProcAddress(hPlugin, "DestroySourceControl") : nullptr;

        if (!m_createSourceControl)
        {
            RCLogError("Source control module %s doesn't have CreateSourceControl function", sourceControlPluginName);
            return;
        }

        if (!m_destroySourceControl)
        {
            RCLogError("Source control module %s doesn't have DestroySourceControl function", sourceControlPluginName);
            return;
        }

        RCLog("Loaded \"%s\"", sourceControlPluginName);

        m_actualSourceControl = m_createSourceControl();

        if (m_actualSourceControl)
        {
            m_actualSourceControl->SetSourceControlUser(m_user);
            m_actualSourceControl->SetSourceControlWorkspace(m_workspace);
        }
        else
        {
            RCLogWarning("Source control module %s failed to start (cannot connect to the server?)", sourceControlPluginName);
        }
    }
}

CProxySourceControl::CProxySourceControl(const char*  rcExeDirectory)
{
    memset(m_RCExecutableDirectory, 0, AZ_MAX_PATH_LEN);
    memset(m_workspace, 0, AZ_MAX_PATH_LEN);
    memset(m_user, 0, AZ_MAX_PATH_LEN);
    azstrncpy(m_RCExecutableDirectory, AZ_MAX_PATH_LEN, rcExeDirectory, AZ_MAX_PATH_LEN);
}

CProxySourceControl::~CProxySourceControl()
{
    if (m_actualSourceControl)
    {
        m_destroySourceControl(m_actualSourceControl);
        m_actualSourceControl = 0;
    }
}

