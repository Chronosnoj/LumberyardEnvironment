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

#include "Core/StdTypes.hpp"
#include "Core/Server/CrySimpleServer.hpp"
#include "Core/Server/CrySimpleHTTP.hpp"

#include <AzFramework/StringFunc/StringFunc.h>

#include <AzCore/PlatformDef.h>
#include <AzCore/base.h>
#include <AzCore/Jobs/JobContext.h>
#include <AzCore/Jobs/JobManager.h>
#include <AzCore/Memory/PoolAllocator.h>
#include <AzCore/Memory/SystemAllocator.h>

#include <iostream>

#define STD_TCP_PORT 61453

//////////////////////////////////////////////////////////////////////////
class CConfigFile
{
public:
    CConfigFile() {}
    //////////////////////////////////////////////////////////////////////////
    void OnLoadConfigurationEntry(const std::string& strKey, const std::string& strValue, const std::string& strGroup)
    {
        if (azstricmp(strKey.c_str(), "MailError") == 0)
        {
            SEnviropment::Instance().m_FailEMail = strValue;
        }
        if (azstricmp(strKey.c_str(), "port") == 0)
        {
            SEnviropment::Instance().m_port = atoi(strValue.c_str());
        }
        if (azstricmp(strKey.c_str(), "MailInterval") == 0)
        {
            SEnviropment::Instance().m_MailInterval = atoi(strValue.c_str());
        }
        if (azstricmp(strKey.c_str(), "TempDir") == 0)
        {
            SEnviropment::Instance().m_Temp = AddBackSlash(strValue);
        }
        if (azstricmp(strKey.c_str(), "MailServer") == 0)
        {
            SEnviropment::Instance().m_MailServer = strValue;
        }
        if (azstricmp(strKey.c_str(), "Caching") == 0)
        {
            SEnviropment::Instance().m_Caching = atoi(strValue.c_str()) != 0;
        }
        if (azstricmp(strKey.c_str(), "PrintErrors") == 0)
        {
            SEnviropment::Instance().m_PrintErrors = atoi(strValue.c_str()) != 0;
        }
        if (azstricmp(strKey.c_str(), "PrintListUpdates") == 0)
        {
            SEnviropment::Instance().m_PrintListUpdates = atoi(strValue.c_str()) != 0;
        }
        if (azstricmp(strKey.c_str(), "DedupeErrors") == 0)
        {
            SEnviropment::Instance().m_DedupeErrors = atoi(strValue.c_str()) != 0;
        }
        if (azstricmp(strKey.c_str(), "FallbackServer") == 0)
        {
            SEnviropment::Instance().m_FallbackServer = strValue;
        }
        if (azstricmp(strKey.c_str(), "FallbackTreshold") == 0)
        {
            SEnviropment::Instance().m_FallbackTreshold = atoi(strValue.c_str());
        }
        if (azstricmp(strKey.c_str(), "DumpShaders") == 0)
        {
            SEnviropment::Instance().m_DumpShaders = atoi(strValue.c_str()) != 0;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    bool ParseConfig(const char* filename)
    {
        FILE* file = fopen(filename, "rb");
        if (!file)
        {
            std::cout << "Config file not found" << std::endl;
            return false;
        }

        fseek(file, 0, SEEK_END);
        int nLen = ftell(file);
        fseek(file, 0, SEEK_SET);

        char* sAllText = new char [nLen + 16];

        fread(sAllText, 1, nLen, file);

        sAllText[nLen] = '\0';
        sAllText[nLen + 1] = '\0';

        std::string strGroup;           // current group e.g. "[General]"

        char* strLast = sAllText + nLen;
        char* str = sAllText;
        while (str < strLast)
        {
            char* s = str;
            while (str < strLast && *str != '\n' && *str != '\r')
            {
                str++;
            }
            *str = '\0';
            str++;
            while (str < strLast && (*str == '\n' || *str == '\r'))
            {
                str++;
            }


            std::string strLine = s;

            // detect groups e.g. "[General]"   should set strGroup="General"
            {
                std::string strTrimmedLine(RemoveWhiteSpaces(strLine));
                size_t size = strTrimmedLine.size();

                if (size >= 3)
                {
                    if (strTrimmedLine[0] == '[' && strTrimmedLine[size - 1] == ']')       // currently no comments are allowed to be behind groups
                    {
                        strGroup = &strTrimmedLine[1];
                        strGroup.resize(size - 2);                                  // remove [ and ]
                        continue;                                                                                                   // next line
                    }
                }
            }

            // skip comments
            if (0 < strLine.find("--"))
            {
                // extract key
                std::string::size_type posEq(strLine.find("=", 0));
                if (std::string::npos != posEq)
                {
                    std::string stemp(strLine, 0, posEq);
                    std::string strKey(RemoveWhiteSpaces(stemp));

                    //              if (!strKey.empty())
                    {
                        // extract value
                        std::string::size_type posValueStart(strLine.find("\"", posEq + 1) + 1);
                        // std::string::size_type posValueEnd( strLine.find( "\"", posValueStart ) );
                        std::string::size_type posValueEnd(strLine.rfind('\"'));

                        std::string strValue;

                        if (std::string::npos != posValueStart && std::string::npos != posValueEnd)
                        {
                            strValue = std::string(strLine, posValueStart, posValueEnd - posValueStart);
                        }
                        else
                        {
                            std::string strTmp(strLine, posEq + 1, strLine.size() - (posEq + 1));
                            strValue = RemoveWhiteSpaces(strTmp);
                        }
                        OnLoadConfigurationEntry(strKey, strValue, strGroup);
                    }
                }
            } //--
        }
        delete []sAllText;
        fclose(file);

        return true;
    }
    std::string RemoveWhiteSpaces(std::string& str)
    {
        std::string::size_type pos1 = str.find_first_not_of(' ');
        std::string::size_type pos2 = str.find_last_not_of(' ');
        str = str.substr(pos1 == std::string::npos ? 0 : pos1, pos2 == std::string::npos ? str.length() - 1 : pos2 - pos1 + 1);
        return str;
    }
    std::string AddBackSlash(const std::string& str)
    {
        if (!str.empty() && str[str.size() - 1] != '\\')
        {
            return str + "\\";
        }
        return str;
    }
};

namespace
{
    AZ::JobManager* jobManager;
    AZ::JobContext* globalJobContext;

#if defined(AZ_PLATFORM_WINDOWS)
    BOOL ControlHandler(DWORD controlType)
    {
        AZ::AllocatorInstance<AZ::ThreadPoolAllocator>::Destroy();
        AZ::AllocatorInstance<AZ::SystemAllocator>::Destroy();
        return FALSE;
    }
#endif
}

void InitDefaults()
{
    SEnviropment::Instance().m_port                             = STD_TCP_PORT;
    SEnviropment::Instance().m_FailEMail                    = "";
    SEnviropment::Instance().m_MailInterval             = 10;
    SEnviropment::Instance().m_MailServer               = "example.com";
    SEnviropment::Instance().m_Caching                      =   true;
    SEnviropment::Instance().m_PrintErrors              = true;
    SEnviropment::Instance().m_DedupeErrors             = true;
    SEnviropment::Instance().m_PrintListUpdates     = true;
    SEnviropment::Instance().m_FallbackTreshold     =   16;
    SEnviropment::Instance().m_FallbackServer           =   "";
}

int main(int argc, char* argv[])
{
#if defined(AZ_PLATFORM_WINDOWS)
#if defined(_DEBUG)
    int tmpFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    tmpFlag |= _CRTDBG_LEAK_CHECK_DF;
    //  tmpFlag &= ~_CRTDBG_CHECK_CRT_DF;
    _CrtSetDbgFlag(tmpFlag);
#endif
    AZ_Verify(SetConsoleCtrlHandler(ControlHandler, TRUE), "Unable to setup windows console control handler");
#endif

    AZ::AllocatorInstance<AZ::SystemAllocator>::Create();
    AZ::AllocatorInstance<AZ::ThreadPoolAllocator>::Create();

    AZ::JobManagerDesc jobManagerDescription;

    int workers = AZStd::GetMin(AZStd::thread::hardware_concurrency(), static_cast<unsigned int>(8));
    for (int idx = 0; idx < workers; ++idx)
    {
        jobManagerDescription.m_workerThreads.push_back(AZ::JobManagerThreadDesc());
    }

    jobManager = aznew AZ::JobManager(jobManagerDescription);
    globalJobContext = aznew AZ::JobContext(*jobManager);
    AZ::JobContext::SetGlobalContext(globalJobContext);

    InitDefaults();

    if (argc == 1)
    {
        CConfigFile config;

        config.ParseConfig("config.ini");

        CCrySimpleHTTP HTTP;
        CCrySimpleServer();
    }
    else
    {
        const char Info[] = "usage: without param ";
        printf(Info);
    }

    AZ::JobContext::SetGlobalContext(nullptr);
    delete globalJobContext;
    delete jobManager;

    AZ::AllocatorInstance<AZ::ThreadPoolAllocator>::Destroy();
    AZ::AllocatorInstance<AZ::SystemAllocator>::Destroy();

    return 0;
}

