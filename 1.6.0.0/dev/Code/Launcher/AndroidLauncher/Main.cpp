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

// Description : Launcher implementation for Android.
#include "StdAfx.h"

#include <platform_impl.h>
#include <AndroidSpecific.h>
#include <android/log.h>
#include <jni.h>

#include <AzGameFramework/Application/GameApplication.h>
#include <AzFramework/API/ApplicationAPI_android.h>

#include <SDL.h>

#include <IGameStartup.h>
#include <IEntity.h>
#include <IGameFramework.h>
#include <IConsole.h>

#include <netdb.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <ParseEngineConfig.h>
#include "android_descriptor.h"

#if defined(AZ_MONOLITHIC_BUILD)
#include "Common_TypeInfo.h"

STRUCT_INFO_T_INSTANTIATE(Vec2_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Vec2_tpl, <int>)
STRUCT_INFO_T_INSTANTIATE(Vec4_tpl, <short>)
STRUCT_INFO_T_INSTANTIATE(Vec3_tpl, <int>)
STRUCT_INFO_T_INSTANTIATE(Vec3_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Ang3_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Quat_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(QuatT_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Plane_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Matrix33_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Color_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Color_tpl, <uint8>)

// from the game library
extern "C" DLL_IMPORT IGameStartup * CreateGameStartup(); // from the game library
extern "C" void CreateStaticModules(AZStd::vector<AZ::Module*>&);

// from CrySystem
extern "C" DLL_IMPORT const char* GetAndroidPakPath();
extern "C" DLL_IMPORT const char* GetGameProjectName();
extern "C" DLL_IMPORT void OnEngineRendererTakeover(bool engineSplashActive);
#else
// function sig for the GetAndroidPakPath and GetGameProjectName functions
// defined in CrySystem
typedef const char* (* GetStringFunc)();

typedef const void (* OnEngineRendererTakeoverFunc)(bool);

#endif //  defined(AZ_MONOLITHIC_BUILD)


#if !defined(_RELEASE)

#define LOG_TAG "LMBR"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))

struct COutputPrintSink
    : public IOutputPrintSink
{
    virtual void Print(const char* inszText)
    {
        LOGI("%s", inszText);
    }
};

COutputPrintSink g_androidPrintSink;

#else

#define LOGI(...)
#define LOGE(...)

#endif // !defined(_RELEASE)


#define RunGame_EXIT(exitCode) (exit(exitCode))

int RunGame(const char*) __attribute__ ((noreturn));


namespace
{
    bool g_unitTestMode = false;
}


//////////////////////////////////////////////////////////////////////////
bool GetDefaultThreadStackSize(size_t* pStackSize)
{
    pthread_attr_t kDefAttr;
    pthread_attr_init(&kDefAttr);   // Required on Mac OS or pthread_attr_getstacksize will fail
    int iRes(pthread_attr_getstacksize(&kDefAttr, pStackSize));
    if (iRes != 0)
    {
        fprintf(stderr, "error: pthread_attr_getstacksize returned %d\n", iRes);
        return false;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////
bool IncreaseResourceMaxLimit(int iResource, rlim_t uMax)
{
    struct rlimit kLimit;
    if (getrlimit(iResource, &kLimit) != 0)
    {
        fprintf(stderr, "error: getrlimit (%d) failed\n", iResource);
        return false;
    }

    if (uMax != kLimit.rlim_max)
    {
        //if (uMax == RLIM_INFINITY || uMax > kLimit.rlim_max)
        {
            kLimit.rlim_max = uMax;
            if (setrlimit(iResource, &kLimit) != 0)
            {
                fprintf(stderr, "error: setrlimit (%d, %lu) failed\n", iResource, uMax);
                return false;
            }
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////

int RunGame(const char* commandLine)
{
    //Adding a start up banner so you can see when the game is starting up in amongst the logcat spam
    LOGI("******************************************************");
    LOGI("*         Amazon Lumberyard - Launching Game....     *");
    LOGI("******************************************************");

    char absPath[MAX_PATH];
    memset(absPath, 0, sizeof(char) * MAX_PATH);

    if (!getcwd(absPath, sizeof(char) * MAX_PATH))
    {
        LOGE("[ERROR] Unable to get current working path!");
        RunGame_EXIT(1);
    }
    CryLog("CWD = %s", absPath);

    size_t uDefStackSize;
    if (!IncreaseResourceMaxLimit(RLIMIT_CORE, RLIM_INFINITY)
        || !GetDefaultThreadStackSize(&uDefStackSize)
        || !IncreaseResourceMaxLimit(RLIMIT_STACK, RLIM_INFINITY * uDefStackSize))
    {
        RunGame_EXIT(1);
    }


#if !defined(AZ_MONOLITHIC_BUILD)

    HMODULE systemlib = CryLoadLibrary("libCrySystem.so");
    if (!systemlib)
    {
        LOGE("[ERROR] Failed to load CrySystem: %s", dlerror());
        RunGame_EXIT(1);
    }

    GetStringFunc GetAndroidPakPath = (GetStringFunc)CryGetProcAddress(systemlib, "GetAndroidPakPath");
    if (!GetAndroidPakPath)
    {
        LOGE("[ERROR] GetAndroidPakPath could not be found in CrySystem!\n");
        CryFreeLibrary(systemlib);
        RunGame_EXIT(1);
    }

    GetStringFunc GetGameProjectName = (GetStringFunc)CryGetProcAddress(systemlib, "GetGameProjectName");
    if (!GetGameProjectName)
    {
        LOGE("[ERROR] GetGameProjectName could not be found in CrySystem!\n");
        CryFreeLibrary(systemlib);
        RunGame_EXIT(1);
    }

    OnEngineRendererTakeoverFunc OnEngineRendererTakeover = (OnEngineRendererTakeoverFunc)CryGetProcAddress(systemlib, "OnEngineRendererTakeover");
    if (!OnEngineRendererTakeover)
    {
        LOGE("[ERROR] OnEngineRendererTakeover could not be found in CrySystem!\n");
        CryFreeLibrary(systemlib);
        RunGame_EXIT(1);
    }
    const char* gameName = GetGameProjectName();

    HMODULE gameDll = 0;
    {
        char gameSoFilename[1024];
        snprintf(gameSoFilename, sizeof(gameSoFilename), "lib%s.so", gameName);
        gameDll = CryLoadLibrary(gameSoFilename);
    }
    if (!gameDll)
    {
        LOGE("[ERROR] Failed to load GAME DLL (%s)\n", dlerror());
        CryFreeLibrary(systemlib);
        RunGame_EXIT(1);
    }

    // get address of startup function
    IGameStartup::TEntryFunction CreateGameStartup = (IGameStartup::TEntryFunction)CryGetProcAddress(gameDll, "CreateGameStartup");
    if (!CreateGameStartup)
    {
        LOGE("[ERROR] CreateGameStartup could not be found in %s!\n", gameName);
        CryFreeLibrary(gameDll);
        CryFreeLibrary(systemlib);
        RunGame_EXIT(1);
    }
#endif // !defined(AZ_MONOLITHIC_BUILD)

    const char* pakPath = GetAndroidPakPath();
    if (!pakPath)
    {
        LOGE("#################################################");
        LOGE("#################################################");
        LOGE("[ERROR] Unable to locate bootstrap.cfg - Exiting!");
        LOGE("#################################################");
        LOGE("#################################################");
        RunGame_EXIT(1);
    }

    char searchPathName[1024];

    if (stricmp(pakPath, g_ApkPrefix) == 0)
    {
        sprintf_s(searchPathName, "%s", pakPath);
    }
    else
    {
        sprintf_s(searchPathName, "%s/%s", pakPath, gameName);
    }

    const char* searchPaths[] = {searchPathName};


    CEngineConfig engineCfg(searchPaths, 1);

    SSystemInitParams startupParams;
    memset(&startupParams, 0, sizeof(SSystemInitParams));
    engineCfg.CopyToStartupParams(startupParams);

    startupParams.hInstance = 0;
    strcpy(startupParams.szSystemCmdLine, commandLine);
    startupParams.sLogFileName = "Game.log";
    startupParams.pUserCallback = NULL;
    startupParams.pSharedEnvironment = AZ::Environment::GetInstance();
    sprintf_s(startupParams.logPath, "%s/log", searchPathName);

    const char* externalStorage = SDL_AndroidGetExternalStoragePath();

    if (stricmp(startupParams.rootPath, "/APK") == 0)
    {
        LOGI("[INFO] Using APK for files");
        sprintf(startupParams.logPath, "%s/%s/log", externalStorage, gameName);
        sprintf(startupParams.userPath, "%s/%s/user", externalStorage, gameName);
    }

    LOGI("***** ROOT path set to:%s ***** ", startupParams.rootPath);
    LOGI("***** ASSET path set to:%s ***** ", startupParams.assetsPath);
    LOGI("***** USER path set to:%s ***** ", startupParams.userPath);
    LOGI("***** LOG path set to:%s ***** ", startupParams.logPath);

#if !defined(_RELEASE)
    startupParams.pPrintSync = &g_androidPrintSink;
#endif

    chdir(searchPathName);

    // Init AzGameFramework
    AzGameFramework::GameApplication gameApp;
    AzGameFramework::GameApplication::StartupParameters gameAppParams;
    AZ::AllocatorInstance<AZ::OSAllocator>::Create();
    gameAppParams.m_allocator = &AZ::AllocatorInstance<AZ::OSAllocator>::Get();
#ifdef AZ_MONOLITHIC_BUILD
    gameAppParams.m_createStaticModulesCallback = CreateStaticModules;
    gameAppParams.m_loadDynamicModules = false;
#endif

    AZ::ComponentApplication::Descriptor androidDescriptor;
    SetupAndroidDescriptor(gameName, androidDescriptor);

    gameApp.Start(androidDescriptor, gameAppParams);

    // create the startup interface
    IGameStartup* pGameStartup = CreateGameStartup();
    if (!pGameStartup)
    {
        LOGE("[ERROR] Failed to create the GameStartup Interface!\n");
        CryFreeLibrary(gameDll);
        CryFreeLibrary(systemlib);
        RunGame_EXIT(1);
    }

    // run the game
    int exitCode = 0;
    IGame* game = pGameStartup->Init(startupParams);
    if (game)
    {
        bool bailOutImmediately = false;

        if (!bailOutImmediately)
        {
            // passing false because this would be the point where the engine renderer takes over if no engine splash screen was supplied
            OnEngineRendererTakeover(false);
            exitCode = pGameStartup->Run(nullptr);
        }
    }
    else
    {
        LOGE("[ERROR] Failed to initialize the GameStartup Interface!\n");
    }

    pGameStartup->Shutdown();
    pGameStartup = 0;

    gameApp.Stop();

    RunGame_EXIT(exitCode);
}

// An unreferenced function.  This function is needed to make sure that
// unneeded functions don't make it into the final executable.  The function
// section for this function should be removed in the final linking.
void this_function_is_not_used(void)
{
}

//-------------------------------------------------------------------------------------
int HandleApplicationLifecycleEvents(void* userdata, SDL_Event* event)
{
    switch (event->type)
    {
    // SDL2 generates two events when the native onPause is called:
    // SDL_APP_WILLENTERBACKGROUND and SDL_APP_DIDENTERBACKGROUND.
    case SDL_APP_DIDENTERBACKGROUND:
    {
        EBUS_EVENT(AzFramework::AndroidLifecycleEvents::Bus, OnPause);
        return 0;
    }
    break;

    // SDL2 generates two events when the native onResume is called:
    // SDL_APP_WILLENTERFOREGROUND and SDL_APP_DIDENTERFOREGROUND.
    case SDL_APP_DIDENTERFOREGROUND:
    {
        EBUS_EVENT(AzFramework::AndroidLifecycleEvents::Bus, OnResume);
        return 0;
    }
    break;
    case SDL_APP_TERMINATING:
    {
        EBUS_EVENT(AzFramework::AndroidLifecycleEvents::Bus, OnDestroy);
        return 0;
    }
    break;
    case SDL_APP_LOWMEMORY:
    {
        EBUS_EVENT(AzFramework::AndroidLifecycleEvents::Bus, OnLowMemory);
        return 0;
    }
    break;
    default:
    {
        // No special handling, add event to the queue
        return 1;
    }
    break;
    }
}

//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
int _main(int argc, char** argv)
{
    // Initialize SDL.
    SDL_SetEventFilter(HandleApplicationLifecycleEvents, NULL);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER) < 0)
    {
        fprintf(stderr, "ERROR: SDL initialization failed: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    atexit(SDL_Quit);

    // Build the command line.
    // We'll attempt to re-create the argument quoting that was used in the
    // command invocation.
    size_t cmdLength = 0;
    char needQuote[argc];
    for (int i = 0; i < argc; ++i)
    {
        bool haveSingleQuote = false, haveDoubleQuote = false, haveBrackets = false;
        bool haveSpace = false;
        for (const char* p = argv[i]; *p; ++p)
        {
            switch (*p)
            {
            case '"':
                haveDoubleQuote = true;
                break;
            case '\'':
                haveSingleQuote = true;
                break;
            case '[':
            case ']':
                haveBrackets = true;
                break;
            case ' ':
                haveSpace = true;
                break;
            default:
                break;
            }
        }
        needQuote[i] = 0;
        if (haveSpace || haveSingleQuote || haveDoubleQuote || haveBrackets)
        {
            if (!haveSingleQuote)
            {
                needQuote[i] = '\'';
            }
            else if (!haveDoubleQuote)
            {
                needQuote[i] = '"';
            }
            else if (!haveBrackets)
            {
                needQuote[i] = '[';
            }
            else
            {
                fprintf(stderr, "Lumberyard Android Launcher Error: Garbled command line\n");
                exit(EXIT_FAILURE);
            }
        }
        cmdLength += strlen(argv[i]) + (needQuote[i] ? 2 : 0);
        if (i > 0)
        {
            ++cmdLength;
        }
    }
    char cmdLine[cmdLength + 1], * q = cmdLine;
    for (int i = 0; i < argc; ++i)
    {
        if (i > 0)
        {
            *q++ = ' ';
        }
        if (needQuote[i])
        {
            *q++ = needQuote[i];
        }
        strcpy(q, argv[i]);
        q += strlen(q);
        if (needQuote[i])
        {
            if (needQuote[i] == '[')
            {
                *q++ = ']';
            }
            else
            {
                *q++ = needQuote[i];
            }
        }
    }
    *q = 0;
    assert(q - cmdLine == cmdLength);

#if CAPTURE_REPLAY_LOG
    // Since Android doesn't support native command line argument, please
    // uncomment the following line if -memreplay is needed.
    // CryGetIMemReplay()->StartOnCommandLine("-memreplay");
    CryGetIMemReplay()->StartOnCommandLine(cmdLine);
#endif // CAPTURE_REPLAY_LOG

    return RunGame(cmdLine);
}

/**
 *Enable unit test mode from arguments
 */
extern "C" JNIEXPORT void JNICALL Java_org_libsdl_app_SDLActivity_nativeEnableUnitTestMode(JNIEnv* env, jclass cls, jobject obj)
{
    (void)env;
    (void)cls;
    (void)obj;
    LOGI("Java NATIVE code call:  Enabling Unit Test Mode");
    g_unitTestMode = true;
}

/**
 * Entry point when running in SDL framework.
 */
extern "C" int SDL_main(int argc, char* argv[])
{
    return _main(0, NULL);
}

// vim:sw=4:ts=4:si:noet
