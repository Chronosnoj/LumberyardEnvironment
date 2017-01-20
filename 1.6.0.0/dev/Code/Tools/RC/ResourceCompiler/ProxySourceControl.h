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

#include "IResCompiler.h"
#include "ICrySourceControl.h"
#define VC_EXTRALEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <AzCore/IO/SystemFile.h> // only used for AZ_MAX_PATH_LEN

//! This class is only used by the Resource Compiler to lazy load the source control dll
class CProxySourceControl
    : public ICrySourceControl
{
public:
    /////////////////////////////////////////////////////////
    // ICrySourceControl implementation
    virtual void SetSourceControlWorkspace(const char* strWorkspace);
    virtual void SetSourceControlUser(const char* strUser);

    virtual bool GetFileHeadRevisionInfo(const char* fileName, CSourceControlFileInfo& fileInfo, CSourceControlUserInfo& userInfo);

    virtual bool IsUnderSourceControl(const char* fileName);

    virtual void ConvertTimeToText(int time, char* buffer, size_t bufferSizeInBytes);

    virtual bool MakeWritable(const char* fileName);

    virtual bool Add(const char* fileName);

    virtual bool GetLastErrorText(char* buffer, size_t bufSize) const;
    /////////////////////////////////////////////////////////
    CProxySourceControl(const char* rcExeDirectory);
    ~CProxySourceControl();

private:

    void LoadSourceControlDLL();

    FnCreateSourceControl  m_createSourceControl = nullptr;
    FnDestroySourceControl m_destroySourceControl = nullptr;
    ICrySourceControl* m_actualSourceControl = nullptr;
    char m_workspace[AZ_MAX_PATH_LEN];
    char m_user[AZ_MAX_PATH_LEN];
    char m_RCExecutableDirectory[AZ_MAX_PATH_LEN];

    CProxySourceControl(const CProxySourceControl& other) = delete;
    CProxySourceControl& operator=(const CProxySourceControl& other) = delete;
};