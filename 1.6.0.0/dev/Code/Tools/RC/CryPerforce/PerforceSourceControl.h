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
#ifndef CRYINCLUDE_CRYPERFORCE_PERFORCESOURCECONTROL_H
#define CRYINCLUDE_CRYPERFORCE_PERFORCESOURCECONTROL_H
#pragma once


#include "ICrySourceControl.h"

// Perforce SDK 
#pragma warning(disable: 4244) // Suppress the following warning: conversion from '__int64' to 'p4size_t', possible loss of data
#include "../../../SDKs/p4api/include/p4/clientapi.h"
#pragma warning(default: 4244)


//////////////////////////////////////////////////////////////////////////
class CClientUser
    : public ClientUser          // Perforce SDK class
{
public:
    CClientUser();
    ~CClientUser();

    void Reset();
    void Clear();

    virtual void HandleError(Error* e);

    virtual void Edit(FileSys* f1, Error* e);

    virtual void OutputStat(StrDict* varList);

    virtual void OutputBinary(const char* data, int size);
    virtual void OutputText(const char* data, int length);
    virtual void OutputInfo(char level, const char* data);
    virtual void OutputError(const char* data);

    virtual int OutputBinary(char* data, int length);
    virtual int OutputText(char* data, int length);
    virtual int OutputInfo(char level, char* data);
    virtual int OutputError(char* data);

    const CSourceControlFileInfo& GetFileInfo() const;
    const CSourceControlUserInfo& GetUserInfo() const;

    const Error& GetError() const;

private:
    CSourceControlFileInfo m_fileInfo;
    CSourceControlUserInfo m_userInfo;
    Error m_error;                   // Perforce SDK class
};

//////////////////////////////////////////////////////////////////////////
class CPerforceSourceControl
    : public ICrySourceControl
{
public:
    /////////////////////////////////////////////////////////
    // ICrySourceControl implementation
    virtual void SetSourceControlWorkspace(const char* strWorkspace);
    virtual void SetSourceControlUser(const char* strUser);

    virtual bool GetFileHeadRevisionInfo(
        const char* fileName,
        CSourceControlFileInfo& fileInfo,
        CSourceControlUserInfo& userInfo);

    virtual bool IsUnderSourceControl(const char* fileName);

    virtual void ConvertTimeToText(
        int time,
        char* buffer,
        size_t bufferSizeInBytes);

    virtual bool MakeWritable(const char* fileName);

    virtual bool Add(const char* fileName);

    virtual bool GetLastErrorText(char* buffer, size_t bufSize) const;
    /////////////////////////////////////////////////////////

    static ICrySourceControl* Create();
    static void Destroy(ICrySourceControl* pSC);

    CPerforceSourceControl();
    ~CPerforceSourceControl();

private:
    bool m_bIsFailedToConnect;
    ClientApi m_client;          // Perforce SDK class
    CClientUser m_clientUser;    // Perforce SDK class derived
};


#endif // CRYINCLUDE_CRYPERFORCE_PERFORCESOURCECONTROL_H
