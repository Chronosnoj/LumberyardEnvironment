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

// Description : This is simplified/modified version of the code
//               written by Sergiy Shaikin (see
//               Sandbox\Plugins\PerforcePlugin\PerforceSourceControl.cpp)

#include "stdafx.h"

#include "IResCompiler.h"
#include "PerforceSourceControl.h"
#include <time.h>      // ctime_s()

#define WIN32_LEAN_AND_MEAN
#include <windows.h>   // HANDLE, MessageBox()
#include "platform_implRC.h"

#if defined(DEBUG) || defined(_DEBUG)
#   pragma message("Note: pragma-including Perforce SDK lib*.lib")
#   pragma comment(lib, "../../../SDKs/p4api/Debug/libclient.lib")
#   pragma comment(lib, "../../../SDKs/p4api/Debug/librpc.lib")
#   pragma comment(lib, "../../../SDKs/p4api/Debug/libsupp.lib")
#   pragma comment(lib, "../../../SDKs/p4api/Debug/libp4sslstub.lib")
#else
#   pragma message("Note: pragma-including Perforce SDK lib*.lib")
#   pragma comment(lib, "../../../SDKs/p4api/Release/libclient.lib")
#   pragma comment(lib, "../../../SDKs/p4api/Release/librpc.lib")
#   pragma comment(lib, "../../../SDKs/p4api/Release/libsupp.lib")
#   pragma comment(lib, "../../../SDKs/p4api/Release/libp4sslstub.lib")
#endif

//////////////////////////////////////////////////////////////////////////


static HMODULE g_hInst;

ICrySourceControl* _stdcall CreateSourceControl()
{
    return CPerforceSourceControl::Create();
}

void _stdcall DestroySourceControl(ICrySourceControl* pSC)
{
    CPerforceSourceControl::Destroy(pSC);
}

namespace
{
    CryCriticalSection g_critSec;
    volatile bool g_bOperationInProgress;
}

BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hInst = (HMODULE)hModule;
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////

ICrySourceControl* CPerforceSourceControl::Create()
{
    CPerforceSourceControl* p = new CPerforceSourceControl();
    if (p && p->m_bIsFailedToConnect)
    {
        delete p;
        p = 0;
    }
    return p;
}

void CPerforceSourceControl::Destroy(ICrySourceControl* pSC)
{
    if (pSC)
    {
        delete pSC;
        pSC = 0;
    }
}

static void SafeStrCpy(char* dst, size_t maxSizeInBytes, const char* src)
{
    assert(src);
    assert(dst);

    const size_t srcSizeInBytes = (strlen(src) + 1) * sizeof(src[0]);

    if (srcSizeInBytes > maxSizeInBytes)
    {
        MessageBox(NULL, "SafeStrCpy(): unexpected long string", "Debug", MB_OK | MB_ICONERROR);
        assert(0);
    }

    memcpy(dst, src, srcSizeInBytes);
}


static void SafeStrCat(char* dst, size_t maxSizeInBytes, const char* src)
{
    assert(src);
    assert(dst);

    const size_t srcSizeInBytes = (strlen(src) + 1) * sizeof(src[0]);
    const size_t dstLen = strlen(dst);
    const size_t dstWithoutEndingZeroSizeInBytes = dstLen * sizeof(dst[0]);

    if (dstWithoutEndingZeroSizeInBytes + srcSizeInBytes > maxSizeInBytes)
    {
        MessageBox(NULL, "SafeStrCat(): unexpected long string", "Debug", MB_OK | MB_ICONERROR);
        assert(0);
    }

    memcpy(&dst[dstLen], src, srcSizeInBytes);
}



CClientUser::CClientUser()
{
    Reset();
}

CClientUser::~CClientUser()
{
    Reset();
}


void CClientUser::Reset()
{
    Clear();
    m_error.Clear();
}

void CClientUser::Clear()
{
    m_fileInfo.Reset();
    m_userInfo.Reset();
}


void CClientUser::HandleError(Error* e)
{
    if (e == 0)
    {
        return;
    }

    if (e->IsInfo())
    {
        e->Clear();
        return;
    }

    if (e->GetSeverity() < m_error.GetSeverity())
    {
        e->Clear();
        return;
    }

    m_error = *e;

    /* For debug only
    StrBuf  m;
    e->Fmt( &m );

    MessageBox( NULL,"CClientUser::HandleError() called","Debug",MB_OK|MB_ICONERROR );

    StrBuf  m;
    e->Fmt( &m );
    if ( strstr( m.Text(), "file(s) not in client view." ) )
        e->Clear();
    else if ( strstr( m.Text(), "no such file(s)" ) )
        e->Clear();
    else if ( strstr( m.Text(), "access denied" ) )
        e->Clear();
    */

    e->Clear();
}


void CClientUser::Edit(FileSys* f1, Error* e)
{
    MessageBox(NULL, "CClientUser::Edit() called", "Debug", MB_OK | MB_ICONERROR);
}


// All formatted ("tagged") Perforce's output goes through this function.
// We'll store incoming data in appropriate container variables, based
// on data tags.
void CClientUser::OutputStat(StrDict* varList)
{
    StrRef var, val;

    for (int i = 0; varList->GetVar(i, var, val); ++i)
    {
        if (var == "clientFile")
        {
            SafeStrCpy(m_fileInfo.m_clientFileName, sizeof(m_fileInfo.m_clientFileName), val.Text());
        }
        else if (var == "depotFile")
        {
            SafeStrCpy(m_fileInfo.m_depotFileName, sizeof(m_fileInfo.m_depotFileName), val.Text());
        }
        else if (var == "desc")
        {
            SafeStrCpy(m_fileInfo.m_changeDescription, sizeof(m_fileInfo.m_changeDescription), val.Text());
        }
        else if (var == "headTime")
        {
            m_fileInfo.m_time = val.Atoi();
        }
        else if (var == "headChange")
        {
            m_fileInfo.m_change = val.Atoi();
        }
        else if (var == "headAction")
        {
            // one of add, edit, delete, branch, move/add, move/delete, or integrate
            if (val == "add")
            {
                m_fileInfo.m_action = CSourceControlFileInfo::eP4_Action_Add;
            }
            else if (val == "edit")
            {
                m_fileInfo.m_action = CSourceControlFileInfo::eP4_Action_Edit;
            }
            else if (val == "delete")
            {
                m_fileInfo.m_action = CSourceControlFileInfo::eP4_Action_Delete;
            }
            else if (val == "branch")
            {
                m_fileInfo.m_action = CSourceControlFileInfo::eP4_Action_Branch;
            }
            else if (val == "integrate")
            {
                m_fileInfo.m_action = CSourceControlFileInfo::eP4_Action_Integrate;
            }
            else if (val == "move/add")
            {
                m_fileInfo.m_action = CSourceControlFileInfo::eP4_Action_MoveAdd;
            }
            else if (val == "move/delete")
            {
                m_fileInfo.m_action = CSourceControlFileInfo::eP4_Action_MoveDelete;
            }
        }
        else if (var == "headRev")
        {
            m_fileInfo.m_revision = val.Atoi();
        }
        else if ((var == "user") || (var == "User"))
        {
            SafeStrCpy(m_userInfo.m_userName, sizeof(m_userInfo.m_userName), val.Text());
        }
        else if (var == "client")
        {
            SafeStrCpy(m_userInfo.m_workspaceName, sizeof(m_userInfo.m_workspaceName), val.Text());
        }
        else if (var == "FullName")
        {
            SafeStrCpy(m_userInfo.m_userFullName, sizeof(m_userInfo.m_userFullName), val.Text());
        }
        else if (var == "Email")
        {
            SafeStrCpy(m_userInfo.m_email, sizeof(m_userInfo.m_email), val.Text());
        }
        else
        {
            // MessageBox( NULL,"Unknown field found in OutputStat()","Debug",MB_OK|MB_ICONERROR );
        }
    }
}


void CClientUser::OutputBinary(const char* data, int size)
{
    assert(data);
    assert(size >= 0);
    printf("OutputBinary() called. Size is %i\n", size);
}

void CClientUser::OutputText(const char* data, int length)
{
    assert(data);
    assert(length >= 0);
    printf("OutputText(): ");
    while (length--)
    {
        printf("%c", (*data++));
    }
}

void CClientUser::OutputInfo(char level, const char* data)
{
    assert(data);
    printf("OutputInfo(): level %i: %s", level, data);
}

void CClientUser::OutputError(const char* data)
{
    assert(data);
    printf("OutputError(): %s", data);
}


int CClientUser::OutputBinary(char* data, int length)
{
    OutputBinary((const char*)data, length);
    return 0;
}

int CClientUser::OutputText(char* data, int length)
{
    OutputText((const char*)data, length);
    return 0;
}

int CClientUser::OutputInfo(char level, char* data)
{
    OutputInfo(level, (const char*)data);
    return 0;
}

int CClientUser::OutputError(char* data)
{
    OutputError((const char*)data);
    return 0;
}


const CSourceControlFileInfo& CClientUser::GetFileInfo() const
{
    return m_fileInfo;
}

const CSourceControlUserInfo& CClientUser::GetUserInfo() const
{
    return m_userInfo;
}

const Error& CClientUser::GetError() const
{
    return m_error;
}
//////////////////////////////////////////////////////////////////////////
//class used to make sure another operation is not in progress. It works like
//an autolock, setting and unsetting a shared volatile bool. it sets it on instantiation and
//unsets it on exit of scope. I causes a fatal error if violated. This is used to make sure
//no one in the future violates the critical section integrity that would result in deadlock.
class OperationAutoGuard
{
private:
    volatile bool* m_pGuard;
public:
    OperationAutoGuard(volatile bool* pGuard)
        : m_pGuard(pGuard)
    {
        if (!m_pGuard)
        {
            CryFatalError("Guard construct fail");
        }
        if (*m_pGuard)
        {
            CryFatalError("Operation already in progress");
        }
        else
        {
            *m_pGuard = true;
        }
    }
    ~OperationAutoGuard()
    {
        if (!m_pGuard)
        {
            CryFatalError("Guard destruct fail");
        }
        if (!*m_pGuard)
        {
            CryFatalError("Operation should be in progress and isn't.");
        }
        else
        {
            *m_pGuard = false;
        }
    }
};

////////////////////////////////////////////////////////////

CPerforceSourceControl::CPerforceSourceControl()
{
    m_bIsFailedToConnect = false;
    g_bOperationInProgress = false;
    m_client.SetProtocol("tag", "");    // forcing "tagged" output for all commands

    Error e;
    m_client.Init(&e);

    if (e.Test())
    {
        m_bIsFailedToConnect = true;
    }
}

CPerforceSourceControl::~CPerforceSourceControl()
{
    Error e;
    m_client.Final(&e);
}

bool CPerforceSourceControl::GetFileHeadRevisionInfo(
    const char* fileName,
    CSourceControlFileInfo& fileInfo,
    CSourceControlUserInfo& userInfo)
{
    CryAutoLock<CryCriticalSection> locker(g_critSec);
    OperationAutoGuard guard(&g_bOperationInProgress); //guard against re-entry in the same thread without resulting in deadlock

    assert(fileName);
    assert(fileName[0]);

    char str[MAX_PATH];

    m_clientUser.Reset();

    if (m_clientUser.GetError().Test())
    {
        return false;
    }

    if (m_client.Dropped())
    {
        return false;
    }

    // Obtain names of the file, head revision number, head change list number, head time
    {
        SafeStrCpy(str, sizeof(str), fileName);
        char* argv[] =
        {
            &str[0]
        };
        m_client.SetArgv(sizeof(argv) / sizeof(argv[0]), argv);

        m_client.Run("fstat", &m_clientUser);

        m_client.WaitTag();

        if (m_clientUser.GetError().Test())
        {
            return false;
        }

        fileInfo = m_clientUser.GetFileInfo();

        if ((fileInfo.m_clientFileName[0] == 0) || (fileInfo.m_depotFileName[0] == 0))
        {
            return false;
        }
    }

    // Obtain information about last-known user who changed the file (user name, workspace)
    // and change description
    {
        m_clientUser.Clear();

        SafeStrCpy(str, sizeof(str), fileInfo.m_depotFileName);
        char* argv[] =
        {
            "-m",
            "1",
            &str[0]
        };
        m_client.SetArgv(sizeof(argv) / sizeof(argv[0]), argv);

        m_client.Run("changes", &m_clientUser);

        m_client.WaitTag();

        if (m_clientUser.GetError().Test())
        {
            return false;
        }

        SafeStrCpy(fileInfo.m_changeDescription, sizeof(fileInfo.m_changeDescription), m_clientUser.GetFileInfo().m_changeDescription);
        userInfo = m_clientUser.GetUserInfo();

        if (userInfo.m_userName[0] == 0)
        {
            return false;
        }
    }

    // Obtain additional information about the user (full name, eMail address)
    {
        SafeStrCpy(str, sizeof(str), userInfo.m_userName);
        char* argv[] =
        {
            &str[0]
        };
        m_client.SetArgv(sizeof(argv) / sizeof(argv[0]), argv);

        m_client.Run("users", &m_clientUser);

        m_client.WaitTag();

        if (m_clientUser.GetError().Test())
        {
            return false;
        }

        SafeStrCpy(userInfo.m_userFullName, sizeof(userInfo.m_userFullName), m_clientUser.GetUserInfo().m_userFullName);
        SafeStrCpy(userInfo.m_email, sizeof(userInfo.m_email), m_clientUser.GetUserInfo().m_email);
    }

    return true;
}

void CPerforceSourceControl::ConvertTimeToText(
    int time,
    char* buffer,
    size_t bufferSizeInBytes)
{
    if ((buffer == 0) || (bufferSizeInBytes <= 0))
    {
        return;
    }

    static const size_t minSizeRequired = strlen("Fri Apr 25 13:51:23 2003\n") + 1;

    if (bufferSizeInBytes < minSizeRequired)
    {
        buffer[0] = 0;
        return;
    }

    const time_t t = (time_t)time;
    if (ctime_s(buffer, bufferSizeInBytes, &t) != 0)
    {
        buffer[0] = 0;
        return;
    }

    // Removing trailing '\r' and '\n' characters from time string
    size_t len = strlen(buffer);
    while ((len > 0) && ((buffer[len - 1] == '\r') || (buffer[len - 1] == '\n')))
    {
        buffer[--len] = 0;
    }
}

bool CPerforceSourceControl::GetLastErrorText(char* buffer, size_t bufSize) const
{
    CryAutoLock<CryCriticalSection> locker(g_critSec);
    OperationAutoGuard guard(&g_bOperationInProgress);//guard against re-entry in the same thread without resulting in deadlock

    if (!buffer || bufSize == 0)
    {
        assert(0);
        return false;
    }

    if (!m_clientUser.GetError().Test())
    {
        return false;
    }

    StrBuf buf;
    m_clientUser.GetError().Fmt(&buf, EF_NOXLATE | EF_PLAIN);

    _snprintf_s(buffer, bufSize, _TRUNCATE, "%s (%i): %s", m_clientUser.GetError().FmtSeverity(), m_clientUser.GetError().GetGeneric(), buf.Text());

    return true;
}

bool CPerforceSourceControl::MakeWritable(const char* fileName)
{
    CryAutoLock<CryCriticalSection> locker(g_critSec);
    OperationAutoGuard guard(&g_bOperationInProgress);//guard against re-entry in the same thread without resulting in deadlock

    assert(fileName);
    assert(fileName[0]);

    char str[MAX_PATH];

    m_clientUser.Reset();

    if (m_clientUser.GetError().Test())
    {
        return false;
    }

    if (m_client.Dropped())
    {
        return false;
    }

    // Obtain names of the file, head revision number, head change list number, head time
    {
        SafeStrCpy(str, sizeof(str), fileName);
        char* argv[] =
        {
            &str[0]
        };
        m_client.SetArgv(sizeof(argv) / sizeof(argv[0]), argv);

        m_client.Run("edit", &m_clientUser);

        m_client.WaitTag();

        if (m_clientUser.GetError().Test())
        {
            return false;
        }
    }

    return true;
}

bool CPerforceSourceControl::Add(const char* fileName)
{
    CryAutoLock<CryCriticalSection> locker(g_critSec);
    OperationAutoGuard guard(&g_bOperationInProgress);//guard against re-entry in the same thread without resulting in deadlock

    assert(fileName);
    assert(fileName[0]);

    char str[MAX_PATH];

    m_clientUser.Reset();

    if (m_clientUser.GetError().Test())
    {
        return false;
    }

    if (m_client.Dropped())
    {
        return false;
    }

    // Obtain names of the file, head revision number, head change list number, head time
    {
        SafeStrCpy(str, sizeof(str), fileName);
        char* argv[] =
        {
            &str[0]
        };
        m_client.SetArgv(sizeof(argv) / sizeof(argv[0]), argv);

        m_client.Run("add", &m_clientUser);

        m_client.WaitTag();

        if (m_clientUser.GetError().Test())
        {
            return false;
        }
    }

    return true;
}

void CPerforceSourceControl::SetSourceControlWorkspace(const char* strWorkspace)
{
    CryAutoLock<CryCriticalSection> locker(g_critSec);
    OperationAutoGuard guard(&g_bOperationInProgress);//guard against re-entry in the same thread without resulting in deadlock

    m_client.SetClient(strWorkspace);
}

void CPerforceSourceControl::SetSourceControlUser(const char* strUser)
{
    CryAutoLock<CryCriticalSection> locker(g_critSec);
    OperationAutoGuard guard(&g_bOperationInProgress);//guard against re-entry in the same thread without resulting in deadlock

    m_client.SetUser(strUser);
}

bool CPerforceSourceControl::IsUnderSourceControl(const char* fileName)
{
    CSourceControlFileInfo fileinfo;
    CSourceControlUserInfo userinfo;
    if (GetFileHeadRevisionInfo(fileName, fileinfo, userinfo))
    {
        if (strlen(fileinfo.m_depotFileName) &&
            fileinfo.m_action != CSourceControlFileInfo::eP4_Action_Delete &&
            fileinfo.m_action != CSourceControlFileInfo::eP4_Action_MoveDelete &&
            fileinfo.m_action != CSourceControlFileInfo::eP4_Action_Unknown)
        {
            return true;
        }
    }
    return false;
}

