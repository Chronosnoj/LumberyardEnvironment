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

#include <CryModuleDefs.h>
#define eCryModule eCryM_Launcher

#define _LAUNCHER // Must be defined before including platform_impl.h
#include <platform_impl.h>

#include <IGameStartup.h>
#include <ParseEngineConfig.h>
#include <SystemUtilsApple.h>

#include <AzGameFramework/Application/GameApplication.h>
#include <Foundation/Foundation.h>

#if defined(AZ_MONOLITHIC_BUILD)
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

    extern "C" IGameStartup* CreateGameStartup();
    extern "C" void CreateStaticModules(AZStd::vector<AZ::Module*>&);
#endif // defined(AZ_MONOLITHIC_BUILD)

namespace
{
    const int EXIT_CODE_SUCCESS = 0;
    const int EXIT_CODE_FAILURE = 1;

    // Increase the limit of a system resource.
    // See sys/resource.h for possible values of the first parameter,
    // to indicate for which resource the operation is being performed.
    // Returns false on error, true otherwise (regardless of whether the limit was increased).
    bool IncreaseResourceLimit(int resource, rlim_t limit)
    {
        struct rlimit resourceLimit;
        if (getrlimit(resource, &resourceLimit) != 0)
        {
            AZ_TracePrintf("SystemUtilsApple", "error: getrlimit failed for resource: %d\n", resource);
            return false;
        }

        if (resourceLimit.rlim_max == limit)
        {
            return true;
        }

        if (limit == RLIM_INFINITY || limit > resourceLimit.rlim_max)
        {
            resourceLimit.rlim_max = limit;
            if (setrlimit(resource, &resourceLimit) != 0)
            {
                AZ_TracePrintf("SystemUtilsApple", "error: setrlimit failed for resource: %d\n", resource);
                return false;
            }
        }

        return true;
    }

    // Get the default thread stack size.
    // - Returns false on error, true otherwise.
    bool GetDefaultThreadStackSize(size_t* stackSize)
    {
        pthread_attr_t pthreadAttr;
        pthread_attr_init(&pthreadAttr); // Required on Mac OS or pthread_attr_getstacksize will fail
        const int result = pthread_attr_getstacksize(&pthreadAttr, stackSize);
        if (result != 0)
        {
            AZ_TracePrintf("SystemUtilsApple", "error: pthread_attr_getstacksize returned %d\n", result);
            return false;
        }
        return true;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace AppleLauncher
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    int Launch(const char* commandLine, const char* pathToApplicationPersistentStorage)
    {
        // Increase Resource Limits
        size_t defaultThreadStackSize;
        if (!IncreaseResourceLimit(RLIMIT_CORE, RLIM_INFINITY) ||
            !GetDefaultThreadStackSize(&defaultThreadStackSize) ||
            !IncreaseResourceLimit(RLIMIT_STACK, defaultThreadStackSize))
        {
            AZ_TracePrintf("AppleLauncher", "Could not increase resource limits");
            return EXIT_CODE_FAILURE;
        }

        // Engine Config (bootstrap.cfg)
        char pathToAssets[AZ_MAX_PATH_LEN] = { 0 };
        const char* pathToResources = [[[NSBundle mainBundle] resourcePath] UTF8String];
        azsnprintf(pathToAssets, AZ_MAX_PATH_LEN, "%s/%s", pathToResources, "assets");
        const char* sourcePaths[] = { pathToAssets };
        CEngineConfig engineConfig(sourcePaths, 1);

        // Game Application (AzGameFramework)
        AzGameFramework::GameApplication gameApplication;
        {
            char pathToGameDescriptorFile[AZ_MAX_PATH_LEN] = { 0 };
            AzGameFramework::GameApplication::GetGameDescriptorPath(pathToGameDescriptorFile, engineConfig.m_gameFolder);

            char fullPathToGameDescriptorFile[AZ_MAX_PATH_LEN] = { 0 };
            azsnprintf(fullPathToGameDescriptorFile, AZ_MAX_PATH_LEN, "%s/%s", pathToAssets, pathToGameDescriptorFile);
            if (!AZ::IO::SystemFile::Exists(fullPathToGameDescriptorFile))
            {
                AZ_TracePrintf("AppleLauncher", "Application descriptor file not found: %s\n", pathToGameDescriptorFile);
                return EXIT_CODE_FAILURE;
            }

            AzGameFramework::GameApplication::StartupParameters gameApplicationStartupParams;
        #if defined(AZ_MONOLITHIC_BUILD)
            gameApplicationStartupParams.m_createStaticModulesCallback = CreateStaticModules;
            gameApplicationStartupParams.m_loadDynamicModules = false;
        #endif // defined(AZ_MONOLITHIC_BUILD)
            gameApplication.Start(fullPathToGameDescriptorFile, gameApplicationStartupParams);
        }

        // System Init Params (CryEngine)
        SSystemInitParams systemInitParams;
        memset(&systemInitParams, 0, sizeof(SSystemInitParams));
        engineConfig.CopyToStartupParams(systemInitParams);
        strcpy(systemInitParams.szSystemCmdLine, commandLine);
        systemInitParams.sLogFileName = "@log@/Game.log";
        systemInitParams.bDedicatedServer = false;
        systemInitParams.pSharedEnvironment = AZ::Environment::GetInstance();
        azsnprintf(systemInitParams.userPath, AZ_MAX_PATH_LEN, "%s", pathToApplicationPersistentStorage);

        HMODULE gameLib = 0;
    #if !defined(AZ_MONOLITHIC_BUILD)
        // Load the CrySystem library
        HMODULE systemLib = CryLoadLibraryDefName("CrySystem");
        if (!systemLib)
        {
            AZ_TracePrintf("Failed to load the CrySystem library: %s", dlerror());
            return EXIT_CODE_FAILURE;
        }

        // Load the game library (compute the .so name from the .dll name)
        const string gameDllName = systemInitParams.gameFolderName;
        const string::size_type extensionPos = gameDllName.rfind(".dll");
        const string sharedLibName = string(CrySharedLibraryPrefix) + gameDllName.substr(0, extensionPos) + string(CrySharedLibraryExtension);
        gameLib = CryLoadLibrary(sharedLibName.c_str());
        if (!gameLib)
        {
            AZ_TracePrintf("AppleLauncher", "Failed to load the game library (%s)", dlerror());
            return EXIT_CODE_FAILURE;
        }

        // Get address of the startup function
        IGameStartup::TEntryFunction CreateGameStartup = (IGameStartup::TEntryFunction)CryGetProcAddress(gameLib, "CreateGameStartup");
        if (!CreateGameStartup)
        {
            AZ_TracePrintf("AppleLauncher", "Loaded game library does not contain a CreateGameStartup function");
            CryFreeLibrary(gameLib);
            return EXIT_CODE_FAILURE;
        }
    #endif //!AZ_MONOLITHIC_BUILD

        // Game Startup Interface (CryEngine)
        IGameStartup* gameStartup = CreateGameStartup();
        if (!gameStartup)
        {
            AZ_TracePrintf("AppleLauncher", "Failed to create the GameStartup Interface");
            CryFreeLibrary(gameLib);
            return EXIT_CODE_FAILURE;
        }

        if (!gameStartup->Init(systemInitParams))
        {
            AZ_TracePrintf("AppleLauncher", "Failed to initialize the GameStartup Interface");

            // If initialization failed, we still need to call shutdown.
            gameStartup->Shutdown();
            gameStartup = nullptr;
            CryFreeLibrary(gameLib);
            return EXIT_CODE_FAILURE;
        }

        // Run
        gameStartup->Run(nullptr);

        // Shutdown
        gameStartup->Shutdown();
        gameStartup = nullptr;
        gameApplication.Stop();

        CryFreeLibrary(gameLib);
        return EXIT_CODE_SUCCESS;
    }
}
