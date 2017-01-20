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

#include "StdAfx.h"
#include "resource.h"
#include <CryLibrary.h>
#include <IGameStartup.h>
#include <IConsole.h>
#include "platform_impl.h"

#define WIN32_LEAN_AND_MEAN
#include "StdAfx.h"
#include <windows.h>
#include <ShellAPI.h>

#include <ParseEngineConfig.h>

#include <AzGameFramework/Application/GameApplication.h>
#include <AzCore/IO/SystemFile.h> // for max path

// We need shell api for Current Root Extruction.
#include "shlwapi.h"
#pragma comment(lib, "shlwapi.lib")

static unsigned key[4] = {1339822019, 3471820962, 4179589276, 4119647811};
static unsigned text[4] = {4114048726, 1217549643, 1454516917, 859556405};
static unsigned hash[4] = {324609294, 3710280652, 1292597317, 513556273};

#ifdef AZ_MONOLITHIC_BUILD
extern "C" IGameStartup * CreateGameStartup();
extern "C" void CreateStaticModules(AZStd::vector<AZ::Module*>&);
#endif // AZ_MONOLITHIC_BUILD

// src and trg can be the same pointer (in place encryption)
// len must be in bytes and must be multiple of 8 byts (64bits).
// key is 128bit:  int key[4] = {n1,n2,n3,n4};
// void encipher(unsigned int *const v,unsigned int *const w,const unsigned int *const k )
#define TEA_ENCODE(src, trg, len, key) {                                                                                              \
        unsigned int* v = (src), * w = (trg), * k = (key), nlen = (len) >> 3;                                                         \
        unsigned int delta = 0x9E3779B9, a = k[0], b = k[1], c = k[2], d = k[3];                                                      \
        while (nlen--) {                                                                                                              \
            unsigned int y = v[0], z = v[1], n = 32, sum = 0;                                                                         \
            while (n-- > 0) { sum += delta; y += (z << 4) + a ^ z + sum ^ (z >> 5) + b; z += (y << 4) + c ^ y + sum ^ (y >> 5) + d; } \
            w[0] = y; w[1] = z; v += 2, w += 2; }                                                                                     \
}

// src and trg can be the same pointer (in place decryption)
// len must be in bytes and must be multiple of 8 byts (64bits).
// key is 128bit: int key[4] = {n1,n2,n3,n4};
// void decipher(unsigned int *const v,unsigned int *const w,const unsigned int *const k)
#define TEA_DECODE(src, trg, len, key) {                                                                                              \
        unsigned int* v = (src), * w = (trg), * k = (key), nlen = (len) >> 3;                                                         \
        unsigned int delta = 0x9E3779B9, a = k[0], b = k[1], c = k[2], d = k[3];                                                      \
        while (nlen--) {                                                                                                              \
            unsigned int y = v[0], z = v[1], sum = 0xC6EF3720, n = 32;                                                                \
            while (n-- > 0) { z -= (y << 4) + c ^ y + sum ^ (y >> 5) + d; y -= (z << 4) + a ^ z + sum ^ (z >> 5) + b; sum -= delta; } \
            w[0] = y; w[1] = z; v += 2, w += 2; }                                                                                     \
}

ILINE unsigned Hash(unsigned a)
{
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

// encode size ignore last 3 bits of size in bytes. (encode by 8bytes min)
#define TEA_GETSIZE(len) ((len) & (~7))

ILINE int RunGame(const char* commandLine, CEngineConfig& engineCfg)
{
    //restart parameters
    static const char logFileName[] = "@log@/Server.log";

    unsigned buf[4];
    TEA_DECODE((unsigned*)text, buf, 16, (unsigned*)key);

    HMODULE gameDll = 0;

#if !defined(AZ_MONOLITHIC_BUILD)
    char fullLibraryName[AZ_MAX_PATH_LEN] = { 0 };
    azsnprintf(fullLibraryName, AZ_MAX_PATH_LEN, "%s%s%s", CrySharedLibraryPrefix, engineCfg.m_gameDLL.c_str(), CrySharedLibraryExtension);
    // load the game dll
    gameDll = CryLoadLibrary(fullLibraryName);

    if (!gameDll)
    {
        // failed to load the dll
        fprintf(stderr, "Failed to load the Game DLL! %s", fullLibraryName);
        return 0;
    }
    // get address of startup function
    IGameStartup::TEntryFunction CreateGameStartup = (IGameStartup::TEntryFunction)CryGetProcAddress(gameDll, "CreateGameStartup");

    if (!CreateGameStartup)
    {
        // dll is not a compatible game dll
        CryFreeLibrary(gameDll);

        fprintf(stderr, "Specified Game DLL %s is not valid (is missing \"CreateGameStartup\")!", gameDll);
        return 0;
    }
#endif //!defined(AZ_MONOLITHIC_BUILD)

    strcat((char*)commandLine, (char*)buf);

    SSystemInitParams startupParams;
    startupParams.pSharedEnvironment = AZ::Environment::GetInstance();
    startupParams.hInstance = GetModuleHandle(0);
    startupParams.sLogFileName = logFileName;

    startupParams.bDedicatedServer = true;

    strcpy(startupParams.szSystemCmdLine, commandLine);


    engineCfg.CopyToStartupParams(startupParams);

    for (int i = 0; i < 4; i++)
    {
        if (Hash(buf[i]) != hash[i])
        {
            return 1;
        }
    }

    // create the startup interface
    IGameStartup* pGameStartup = CreateGameStartup();

    if (!pGameStartup)
    {
        // failed to create the startup interface
        CryFreeLibrary(gameDll);

        fprintf(stderr, "Failed to create the GameStartup Interface!");
        return 0;
    }

    // on PC, we also might have access to the asset cache directly, in which case, go look there.
    // if we dont have access to the asset cache, default back to the original behavior.

    string originalRoot = startupParams.rootPath;
    string originalGameFolder = startupParams.gameFolderName;

    // do we have access to the asset cache?
    char checkPath[AZ_MAX_PATH_LEN] = { 0 };

    azsnprintf(checkPath, AZ_MAX_PATH_LEN, "%s/cache/%s/%s/engineroot.txt", originalRoot.c_str(), originalGameFolder.c_str(), startupParams.assetsPlatform);
    if (GetFileAttributesA(checkPath) != INVALID_FILE_ATTRIBUTES)
    {
        // alter it to be the asset cache instead
        sprintf_s(startupParams.rootPath, "%s/cache/%s/%s", originalRoot.c_str(), originalGameFolder.c_str(), startupParams.assetsPlatform);
        sprintf_s(startupParams.assetsPath, "%s/%s", startupParams.rootPath, originalGameFolder.c_str());
    }


    // run the game
    if (pGameStartup->Init(startupParams))
    {
#if !defined(SYS_ENV_AS_STRUCT)
        gEnv = startupParams.pSystem->GetGlobalEnvironment();
#endif

        pGameStartup->Run(NULL);

        pGameStartup->Shutdown();
        pGameStartup = 0;

        CryFreeLibrary(gameDll);
    }
    else
    {
        fprintf(stderr, "Failed to initialize the GameStartup Interface!");

        // if initialization failed, we still need to call shutdown
        pGameStartup->Shutdown();
        pGameStartup = 0;

        CryFreeLibrary(gameDll);

        return 0;
    }

    return 0;
}


///////////////////////////////////////////////
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // we need pass the full command line, including the filename
    // lpCmdLine does not contain the filename.

    InitRootDir();

    int result = 0;

    AzGameFramework::GameApplication gameApp;
    {
        CEngineConfig engineCfg;

        char configPath[AZ_MAX_PATH_LEN];
        AzGameFramework::GameApplication::GetGameDescriptorPath(configPath, engineCfg.m_gameFolder);
        if (!AZ::IO::SystemFile::Exists(configPath))
        {
            fprintf(stderr, "Application descriptor file not found:\n%s", configPath);
            return 1;
        }


        AzGameFramework::GameApplication::StartupParameters gameAppParams;
#ifdef AZ_MONOLITHIC_BUILD
        gameAppParams.m_createStaticModulesCallback = CreateStaticModules;
        gameAppParams.m_loadDynamicModules = false;
#endif
        gameApp.Start(configPath, gameAppParams);

#if CAPTURE_REPLAY_LOG
#ifndef AZ_MONOLITHIC_BUILD
        CryLoadLibrary("CrySystem.dll");
#endif
        CryGetIMemReplay()->StartOnCommandLine(lpCmdLine);
#endif

        char cmdLine[2048];
        strcpy(cmdLine, GetCommandLineA());

        /*
            unsigned buf[4];
            //  0123456789abcdef
            char secret[16] = "  -dedicated   ";
            TEA_ENCODE((unsigned int*)secret, (unsigned int*)buf, 16, key);
            for (int i=0; i<4; i++)
            hash[i] = Hash(((unsigned*)secret)[i]);
            */

        result = RunGame(cmdLine, engineCfg);

    }// intentionally scoped so that resources can be unloaded in the appropriate order.
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

    return result;
}

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
