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

// Description : interface for source control system

#ifndef CRYINCLUDE_TOOLS_RC_RESOURCECOMPILER_ICRYSOURCECONTROL_H
#define CRYINCLUDE_TOOLS_RC_RESOURCECOMPILER_ICRYSOURCECONTROL_H
#pragma once


class CSourceControlFileInfo
{
public:
    enum EAction
    {
        eP4_Action_Unknown,
        eP4_Action_Add,
        eP4_Action_Edit,
        eP4_Action_Delete,
        eP4_Action_Branch,
        eP4_Action_MoveAdd,
        eP4_Action_MoveDelete,
        eP4_Action_Integrate
    };
    CSourceControlFileInfo()
    {
        Reset();
    }

    ~CSourceControlFileInfo()
    {
    }

    void Reset()
    {
        m_depotFileName[0] = 0;
        m_clientFileName[0] = 0;
        m_changeDescription[0] = 0;
        m_revision = -1;
        m_change = -1;
        m_action = eP4_Action_Unknown;
        m_time = 0;
    }

    static const int MAX_DEPOT_FILE_NAME_LENGTH = 1023;
    static const int MAX_CLIENT_FILE_NAME_LENGTH = 1023;
    static const int MAX_CHANGE_DESCRIPTION_LENGTH = 1023;

    char m_depotFileName[MAX_DEPOT_FILE_NAME_LENGTH + 1];
    char m_clientFileName[MAX_CLIENT_FILE_NAME_LENGTH + 1];
    char m_changeDescription[MAX_CHANGE_DESCRIPTION_LENGTH + 1];
    int m_revision;
    int m_change;
    int m_time;
    EAction m_action;
};


class CSourceControlUserInfo
{
public:
    CSourceControlUserInfo()
    {
        Reset();
    }

    ~CSourceControlUserInfo()
    {
    }

    void Reset()
    {
        m_userName[0] = 0;
        m_userFullName[0] = 0;
        m_workspaceName[0] = 0;
        m_email[0] = 0;
    }

    static const int MAX_USER_NAME_LENGTH = 127;
    static const int MAX_USER_FULL_NAME_LENGTH = 127;
    static const int MAX_CLIENT_NAME_LENGTH = 127;
    static const int MAX_EMAIL_LENGTH = 127;

    char m_userName[MAX_USER_NAME_LENGTH + 1];
    char m_userFullName[MAX_USER_FULL_NAME_LENGTH + 1];
    char m_workspaceName[MAX_CLIENT_NAME_LENGTH + 1];
    char m_email[MAX_EMAIL_LENGTH + 1];
};


class ICrySourceControl
{
public:
    virtual ~ICrySourceControl()
    {
    }

    virtual void SetSourceControlWorkspace(const char* strWorkspace) = 0;
    virtual void SetSourceControlUser(const char* strUser) = 0;

    virtual bool GetFileHeadRevisionInfo(
        const char* fileName,
        CSourceControlFileInfo& fileInfo,
        CSourceControlUserInfo& userInfo) = 0;

    virtual bool IsUnderSourceControl(const char* fileName) = 0;

    virtual bool MakeWritable(const char* fileName) = 0;
    virtual bool Add(const char* fileName) = 0;

    // Returned string is in ctime_s() format, but without '\n' at the end (for example: "Fri Apr 25 13:51:23 2003")
    virtual void ConvertTimeToText(
        int time,
        char* buffer,
        size_t bufferSizeInBytes) = 0;

    // Returns false if no error set
    virtual bool GetLastErrorText(char* buffer, size_t bufSize) const = 0;
};

#endif // CRYINCLUDE_TOOLS_RC_RESOURCECOMPILER_ICRYSOURCECONTROL_H
