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

#define CRY_STATIC_LINK

//#define NEW_OVERRIDEN
#define _LAUNCHER

#include "resource.h"
#include <CryModuleDefs.h>
#define eCryModule eCryM_Launcher

// Insert your headers here
#include <platform.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ShellAPI.h>

// We need shell api for Current Root Extrection.
#include "shlwapi.h"
#pragma comment(lib, "shlwapi.lib")

#include <CryLibrary.h>
#include <IGameStartup.h>
#include <IConsole.h>
#include <ICryPak.h>

#include <platform_impl.h>
#include <StringUtils.h>

#include <ParseEngineConfig.h>

#include <AzCore/IO/SystemFile.h> // used for MAX PATH
#include <AzGameFramework/Application/GameApplication.h>

#ifdef USE_COPYPROTECTION
#define SECUROM_INCLUDE_EXE_FUNCTIONS
#include <CopyProtection.h>
#endif

#ifdef AZ_MONOLITHIC_BUILD
extern "C" IGameStartup * CreateGameStartup();
extern "C" void CreateStaticModules(AZStd::vector<AZ::Module*>&);
#endif //AZ_MONOLITHIC_BUILD
#ifdef AZ_MONOLITHIC_BUILD
// Include common type defines for static linking
// Manually instantiate templates as needed here.
#include "Common_TypeInfo.h"
STRUCT_INFO_T_INSTANTIATE(Vec2_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Vec2_tpl, <int>)
STRUCT_INFO_T_INSTANTIATE(Vec4_tpl, <short>)
STRUCT_INFO_T_INSTANTIATE(Vec3_tpl, <int>)
STRUCT_INFO_T_INSTANTIATE(Ang3_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Quat_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Plane_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Matrix33_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Color_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Color_tpl, <uint8>)
#endif // AZ_MONOLITHIC_BUILD

int RunGame(const char* commandLine, CEngineConfig& engineCfg, const char* szExeFileName)
{
    //restart parameters
    static const size_t MAX_RESTART_LEVEL_NAME = 256;
    char fileName[MAX_RESTART_LEVEL_NAME];
    strcpy(fileName, "");
    static const char logFileName[] = "@log@/Game.log";

    HMODULE gameDll = 0;

#ifndef AZ_MONOLITHIC_BUILD
    // load the game dll
    char fullLibraryName[AZ_MAX_PATH_LEN] = { 0 };
    azsnprintf(fullLibraryName, AZ_MAX_PATH_LEN, "%s%s%s", CrySharedLibraryPrefix, engineCfg.m_gameDLL.c_str(), CrySharedLibraryExtension);
    gameDll = CryLoadLibrary(fullLibraryName);

    if (!gameDll)
    {
        string errorStr;
        errorStr.Format("Failed to load the Game DLL! %s", fullLibraryName);

        MessageBox(0, errorStr.c_str(), "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);
        // failed to load the dll

        return 0;
    }

    // get address of startup function
    IGameStartup::TEntryFunction CreateGameStartup = (IGameStartup::TEntryFunction)CryGetProcAddress(gameDll, "CreateGameStartup");

    if (!CreateGameStartup)
    {
        // dll is not a compatible game dll
        CryFreeLibrary(gameDll);

        MessageBox(0, "Specified Game DLL is not valid! Please make sure you are running the correct executable", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);

        return 0;
    }
#endif // AZ_MONOLITHIC_BUILD

    SSystemInitParams startupParams;
    startupParams.pSharedEnvironment = AZ::Environment::GetInstance();
    startupParams.hInstance = GetModuleHandle(0);
    startupParams.sLogFileName = logFileName;
    strcpy(startupParams.szSystemCmdLine, commandLine);
    //startupParams.pProtectedFunctions[0] = &TestProtectedFunction;

    engineCfg.CopyToStartupParams(startupParams);

#if defined(AZ_PLATFORM_WINDOWS)
    char root[AZ_MAX_PATH_LEN];
    // Override the branch token to be the actual running branch instead of the one in the file:
    if (_fullpath(root, startupParams.rootPath, AZ_MAX_PATH_LEN))
    {
        string devRoot(root);
        devRoot.MakeLower();
        devRoot.replace('/', '_').replace('\\', '_');
        AZ::Crc32 branchTokenCrc = AZ_CRC(devRoot.c_str());
        azsnprintf(startupParams.branchToken, 12, "0x%08X", branchTokenCrc);
    }
#endif

    // on PC, we also might have access to the asset cache directly, in which case, go look there.
    // if we don't have access to the asset cache, default back to the original behavior.

    // create the startup interface
    IGameStartup* pGameStartup = CreateGameStartup();

    if (!pGameStartup)
    {
        // failed to create the startup interface
        CryFreeLibrary(gameDll);

        const char* noPromptArg = strstr(commandLine, "-noprompt");
        if (noPromptArg == NULL)
        {
            MessageBox(0, "Failed to create the GameStartup Interface!", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);
        }

        return 0;
    }

    bool oaRun = false;

    if (strstr(commandLine, "-norandom"))
    {
        startupParams.bNoRandom = 1;
    }

    // run the game
    if (pGameStartup->Init(startupParams))
    {
#if !defined(SYS_ENV_AS_STRUCT)
        gEnv = startupParams.pSystem->GetGlobalEnvironment();
#endif

        char* pRestartLevelName = NULL;
        if (fileName[0])
        {
            pRestartLevelName = fileName;
        }

        CryStackStringT<char, 256> newCommandLine;
        const char* substr = oaRun ? 0 : strstr(commandLine, "restart");
        if (substr != NULL)
        {
            int len = (int)(substr - commandLine);
            newCommandLine.assign(commandLine, len);
            newCommandLine.append(commandLine + len + 7);
            pGameStartup->Run(newCommandLine.c_str());  //restartLevel to be loaded
        }
        else
        {
            const char* loadstr = oaRun ? 0 : strstr(commandLine, "load");
            if (loadstr != NULL)
            {
                int len = (int)(loadstr - commandLine);
                newCommandLine.assign(commandLine, len);
                newCommandLine.append(commandLine + len + 4);
                pGameStartup->Run(newCommandLine.c_str()); //restartLevel to be loaded
            }
            else
            {
                pGameStartup->Run(NULL);
            }
        }

        bool isLevelRequested = pGameStartup->GetRestartLevel(&pRestartLevelName);
        if (pRestartLevelName)
        {
            if (strlen(pRestartLevelName) < MAX_RESTART_LEVEL_NAME)
            {
                strcpy(fileName, pRestartLevelName);
            }
        }

        char pRestartMod[255];
        bool isModRequested = pGameStartup->GetRestartMod(pRestartMod, sizeof(pRestartMod));

        if (isLevelRequested || isModRequested)
        {
            STARTUPINFO si;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi;

            if (isLevelRequested)
            {
                newCommandLine.assign("restart ");
                newCommandLine.append(fileName);
            }

            if (isModRequested)
            {
                newCommandLine.append("-modrestart");
                if (pRestartMod[0] != '\0')
                {
                    newCommandLine.append(" -mod ");
                    newCommandLine.append(pRestartMod);
                }
            }

            if (!CreateProcess(szExeFileName, (char*)newCommandLine.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
            {
                char pMessage[256];
                _snprintf_s(pMessage, sizeof(pMessage), _TRUNCATE, "Failed to restart the game: %s", szExeFileName);
                MessageBox(0, pMessage, "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY | MB_ICONERROR);
            }
        }
        else
        {
            // check if there is a patch to install. If there is, do it now.
            const char* pfilename = pGameStartup->GetPatch();
            if (pfilename)
            {
                STARTUPINFO si;
                ZeroMemory(&si, sizeof(si));
                si.cb = sizeof(si);
                PROCESS_INFORMATION pi;
                CreateProcess(pfilename, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
            }
        }

        pGameStartup->Shutdown();
        pGameStartup = 0;

        CryFreeLibrary(gameDll);
    }
    else
    {
#ifndef _RELEASE
        const char* noPromptArg = strstr(commandLine, "-noprompt");
        if (noPromptArg == NULL)
        {
            MessageBox(0, "Failed to initialize the GameStartup Interface!", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);
        }
#endif
        // if initialization failed, we still need to call shutdown
        pGameStartup->Shutdown();
        pGameStartup = 0;

        CryFreeLibrary(gameDll);

        return 0;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
// Support relaunching for windows media center edition.
//////////////////////////////////////////////////////////////////////////
#if defined(WIN32)
#if (_WIN32_WINNT < 0x0501)
#define SM_MEDIACENTER          87
#endif
bool ReLaunchMediaCenter()
{
    // Skip if not running on a Media Center
    if (GetSystemMetrics(SM_MEDIACENTER) == 0)
    {
        return false;
    }

    // Get the path to Media Center
    char szExpandedPath[MAX_PATH];
    if (!ExpandEnvironmentStrings("%SystemRoot%\\ehome\\ehshell.exe", szExpandedPath, MAX_PATH))
    {
        return false;
    }

    // Skip if ehshell.exe doesn't exist
    if (GetFileAttributes(szExpandedPath) == 0xFFFFFFFF)
    {
        return false;
    }

    // Launch ehshell.exe
    INT_PTR result = (INT_PTR)ShellExecute(NULL, TEXT("open"), szExpandedPath, NULL, NULL, SW_SHOWNORMAL);
    return (result > 32);
}
#endif //defined(WIN32)
///////////////////////////////////////////////
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // we need pass the full command line, including the filename
    // lpCmdLine does not contain the filename.
#if CAPTURE_REPLAY_LOG
#ifndef AZ_MONOLITHIC_BUILD
    CryLoadLibrary("CrySystem.dll");
#endif // AZ_MONOLITHIC_BUILD
    CryGetIMemReplay()->StartOnCommandLine(lpCmdLine);
#endif // CAPTURE_REPLAY_LOG

    char szExeFileName[AZ_MAX_PATH_LEN];
    InitRootDir(szExeFileName, AZ_MAX_PATH_LEN);
    int nRes = 0;
    AzGameFramework::GameApplication gameApp;

    {
        CEngineConfig engineCfg;
        char descriptorPath[AZ_MAX_PATH_LEN] = { 0 };
        AzGameFramework::GameApplication::GetGameDescriptorPath(descriptorPath, engineCfg.m_gameFolder);
        if (!AZ::IO::SystemFile::Exists(descriptorPath))
        {
            char msg[4096] = { 0 };
            azsnprintf(msg, sizeof(msg), "Application descriptor file not found:\n%s", descriptorPath);
            MessageBox(0, msg, "File not found", MB_OK | MB_DEFAULT_DESKTOP_ONLY | MB_ICONERROR);
            return 1;
        }

        AzGameFramework::GameApplication::StartupParameters gameAppParams;
#ifdef AZ_MONOLITHIC_BUILD
        gameAppParams.m_createStaticModulesCallback = CreateStaticModules;
        gameAppParams.m_loadDynamicModules = false;
#endif // AZ_MONOLITHIC_BUILD
        gameApp.Start(descriptorPath, gameAppParams);

        //check for a restart
        const char* pos = strstr(lpCmdLine, "restart");
        if (pos != NULL)
        {
            Sleep(5000); //wait for old instance to be deleted
            nRes = RunGame(lpCmdLine, engineCfg, szExeFileName);  //pass the restart level if restarting
        }
        else
        {
            pos = strstr(lpCmdLine, " -load ");// commandLine.find("load");
            if (pos != NULL)
            {
                nRes = RunGame(lpCmdLine, engineCfg, szExeFileName);
            }
            else
            {
                nRes = RunGame(GetCommandLineA(), engineCfg, szExeFileName);
            }
        }

        //////////////////////////////////////////////////////////////////////////
        // Support relaunching for windows media center edition.
        //////////////////////////////////////////////////////////////////////////
#if defined(WIN32)
        if (strstr(lpCmdLine, "ReLaunchMediaCenter") != 0)
        {
            ReLaunchMediaCenter();
        }
#endif // win32
        //////////////////////////////////////////////////////////////////////////
    } // scoped to get rid of any stack (and any heap they allocated) before we tear down gameapp and thus memory management.

    gameApp.Stop();

#ifndef AZ_MONOLITHIC_BUILD
    // HACK HACK HACK
    // CrySystem module can get loaded multiple times (even from within CrySystem itself)
    // and currently there is no way to track them (\ref _CryMemoryManagerPoolHelper::Init() in CryMemoryManager_impl.h)
    // so we will release it as many times as it takes until it actually unloads.
    void* hModule = CryLoadLibraryDefName("CrySystem");
    if (hModule)
    {
        // loop until we fail (aka unload the DLL)
        while (CryFreeLibrary(hModule))
        {
            ;
        }
    }
#endif // AZ_MONOLITHIC_BUILD

    return nRes;
}
