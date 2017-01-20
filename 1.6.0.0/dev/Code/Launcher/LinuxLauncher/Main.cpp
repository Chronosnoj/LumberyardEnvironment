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

// Description : Launcher implementation for the linux client and the linux dedicated server.


#include "StdAfx.h"
#include "SystemInit.h"

#include <platform_impl.h>

#include <Linux64Specific.h>

#include <IGameStartup.h>
#include <IEntity.h>
#include <IGameFramework.h>
#include <IConsole.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <libgen.h>
#include <execinfo.h>
#include "Serialization/ClassFactory.h"

#include <ParseEngineConfig.h>

#include <AzGameFramework/Application/GameApplication.h>

// FIXME: get the correct version string from somewhere, maybe a -D supplied
// by the build environment?
#undef VERSION_STRING
#define VERSION_STRING "0.0.1"

size_t linux_autoload_level_maxsize = PATH_MAX;
char linux_autoload_level_buf[PATH_MAX];
char* linux_autoload_level = linux_autoload_level_buf;

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

bool IncreaseStackSizeToMax()
{
    struct rlimit kLimit;
    if (getrlimit(RLIMIT_STACK, &kLimit) != 0)
    {
        fprintf(stderr, "error: getrlimit (%d) failed\n", RLIMIT_STACK);
        return false;
    }

    if (kLimit.rlim_cur < kLimit.rlim_max)
    {
        kLimit.rlim_cur = kLimit.rlim_max;
        if (setrlimit(RLIMIT_STACK, &kLimit) != 0)
        {
            fprintf(stderr, "error: setrlimit (%d, %ld) failed\n", RLIMIT_STACK, kLimit.rlim_max);
            return false;
        }
    }

    return true;
}


extern "C" DLL_IMPORT IGameStartup * CreateGameStartup();

#ifdef AZ_MONOLITHIC_BUILD
extern "C" void CreateStaticModules(AZStd::vector<AZ::Module*>&);
#endif // AZ_MONOLITHIC_BUILD

#define RunGame_EXIT(exitCode) (exit(exitCode))

#define LINUX_LAUNCHER_CONF "launcher.cfg"

static void strip(char* s)
{
    char* p = s, * p_end = s + strlen(s);

    while (*p && isspace(*p))
    {
        ++p;
    }
    if (p > s)
    {
        memmove(s, p, p_end - s + 1);
        p_end -= p - s;
    }
    for (p = p_end; p > s && isspace(p[-1]); --p)
    {
        ;
    }
    *p = 0;
}

static void LoadLauncherConfig(void)
{
    char conf_filename[MAX_PATH] = { 0 };
    char line[1024], * eq = 0;
    int n = 0;

    if (!getcwd(conf_filename, MAX_PATH))
    {
        fprintf(stderr, "[ERROR] Unable to get current working path!");
        exit(EXIT_FAILURE);
    }

    strncat(conf_filename, LINUX_LAUNCHER_CONF, strlen(LINUX_LAUNCHER_CONF));

    conf_filename[sizeof conf_filename - 1] = 0;
    FILE* fp = fopen(conf_filename, "r");
    if (!fp)
    {
        return;
    }
    while (true)
    {
        ++n;
        if (!fgets(line, sizeof line - 1, fp))
        {
            break;
        }
        line[sizeof line - 1] = 0;
        strip(line);
        if (!line[0] || line[0] == '#')
        {
            continue;
        }
        eq = strchr(line, '=');
        if (!eq)
        {
            fprintf(stderr, "'%s': syntax error in line %i\n",
                conf_filename, n);
            exit(EXIT_FAILURE);
        }
        *eq = 0;
        strip(line);
        strip(++eq);

        if (!strcasecmp(line, "autoload"))
        {
            if (strlen(eq) >= linux_autoload_level_maxsize)
            {
                fprintf(stderr, "'%s', line %i: autoload value too long\n",
                    conf_filename, n);
                exit(EXIT_FAILURE);
            }
            strcpy(linux_autoload_level, eq);
        }
        else
        {
            fprintf(stderr, "'%s': unrecognized config variable '%s' in line %i\n",
                conf_filename, line, n);
            exit(EXIT_FAILURE);
        }
    }
    fclose(fp);
}

//-------------------------------------------------------------------------------------
// Backtrace + core dump
//-------------------------------------------------------------------------------------

static const char* g_btLibPath = 0;

static void SignalHandler(int sig, siginfo_t* info, void* secret)
{
    void* trace[32];
    int trace_size;

    pid_t pid = getpid();

    FILE* ftrace = fopen("backtrace.log", "w");
    if (!ftrace)
    {
        ftrace = stderr;
    }

    // Log symbol dump first
    fprintf(ftrace, "# Strack Trace with Symbols:\n");
    fflush(ftrace);

    trace_size = backtrace(trace, 32);
    backtrace_symbols_fd(trace, trace_size, fileno(ftrace));

    // Log demangled Stack trace
    fprintf(ftrace, "\n# Strack Trace Demangled (note: Some System libraries cannot be demangled correctly:\n");

    // close file since the following ouput is sent there from the shell
    if (ftrace != stderr)
    {
        fclose(ftrace);
    }

    for (int i = 0; i < trace_size; ++i)
    {
        char objname[MAX_PATH];

        Dl_info info;
        if (dladdr(trace[i], &info))
        {
            uintptr_t addr = (uintptr_t)trace[i] - (uintptr_t)info.dli_fbase;
            string base_name = basename((char*)info.dli_fname);

            if (info.dli_fname[0] != '/')
            {
                snprintf(objname, MAX_PATH, "%s/%s", g_btLibPath, base_name.c_str());
            }
            else
            {
                snprintf(objname, MAX_PATH, "%s", info.dli_fname);
            }

            char cmd[MAX_PATH];
            // check if it is the launcher or not. If it is the laucnher we need ot use the
            // absolute address instead of the relative address inside the module
            size_t pos = base_name.find_first_of("LinuxLauncher", 0);
            if (pos != 0)
            {
                snprintf(cmd, MAX_PATH, "echo [%02d] $(addr2line  -C -f -i -e \"%s\" -p %p) >> 'backtrace.log'", i, objname, (void*) addr);
            }
            else
            {
                snprintf(cmd, MAX_PATH, "echo [%02d] $(addr2line  -C -f -i -e \"%s\" -p %p) >> 'backtrace.log'", i, objname, trace[i]);
            }
            system(cmd);
        }
    }
    abort();
}

static void InitStackTracer(const char* libPath)
{
    g_btLibPath = libPath;
    struct sigaction sa;
    sa.sa_sigaction = SignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction(SIGSEGV, &sa, 0);
    sigaction(SIGBUS, &sa, 0);
    sigaction(SIGILL, &sa, 0);
    prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);
}

//-------------------------------------------------------------------------------------
int RunGame(const char*) __attribute__ ((noreturn));

int RunGame(const char* commandLine)
{
    char exePath[ MAX_PATH];
    memset(exePath, 0, sizeof(exePath));
    if (readlink("/proc/self/exe", exePath, MAX_PATH) == -1)
    {
        RunGame_EXIT(1);
    }
    string exeDir = PathUtil::GetParentDirectory(exePath);

    InitStackTracer(exeDir);

    int exitCode = 0;

    size_t uDefStackSize;

    if (!IncreaseResourceMaxLimit(RLIMIT_CORE, RLIM_INFINITY) || !IncreaseStackSizeToMax())
    {
        RunGame_EXIT(1);
    }

    CEngineConfig engineCfg;
    SSystemInitParams startupParams;

    startupParams.hInstance = 0;
    strcpy(startupParams.szSystemCmdLine, commandLine);
    startupParams.sLogFileName = "Server.log";
    startupParams.bDedicatedServer = true;
    startupParams.pUserCallback = NULL;
    startupParams.bMinimal = true;

    startupParams.pSharedEnvironment = AZ::Environment::GetInstance();

    engineCfg.CopyToStartupParams(startupParams);
    
    char configPath[AZ_MAX_PATH_LEN];
    AzGameFramework::GameApplication::GetGameDescriptorPath(configPath, engineCfg.m_gameFolder);

    AzGameFramework::GameApplication gameApp;
    AzGameFramework::GameApplication::StartupParameters gameAppParams;
#ifdef AZ_MONOLITHIC_BUILD
    gameAppParams.m_createStaticModulesCallback = CreateStaticModules;
    gameAppParams.m_loadDynamicModules = false;
#endif
    gameApp.Start(configPath, gameAppParams);

#if !defined(AZ_MONOLITHIC_BUILD)
    SetModulePath(exeDir);
    HMODULE systemlib = CryLoadLibraryDefName("CrySystem");
    if (!systemlib)
    {
        printf("Failed to load CrySystem: %s", dlerror());
        exit(1);
    }
#endif

    // We need pass the full command line, including the filename
    // lpCmdLine does not contain the filename.
#if CAPTURE_REPLAY_LOG
    CryGetIMemReplay()->StartOnCommandLine(commandLine);
#endif

    HMODULE gameDll = 0;

#ifndef AZ_MONOLITHIC_BUILD
    // workaround: compute .so name from dll name
    string dll_name = engineCfg.m_gameDLL.c_str();

    string::size_type extension_pos = dll_name.rfind(".dll");
    string shared_lib_name = string(CrySharedLibraryPrefix) + dll_name.substr(0, extension_pos) + string(CrySharedLibraryExtension);

    gameDll = CryLoadLibrary(shared_lib_name.c_str());
    if (!gameDll)
    {
        fprintf(stderr, "ERROR: failed to load GAME DLL (%s)\n", dlerror());
        RunGame_EXIT(1);
    }
    // get address of startup function
    IGameStartup::TEntryFunction CreateGameStartup = (IGameStartup::TEntryFunction)CryGetProcAddress(gameDll, "CreateGameStartup");
    if (!CreateGameStartup)
    {
        // dll is not a compatible game dll
        CryFreeLibrary(gameDll);
        fprintf(stderr, "Specified Game DLL is not valid!\n");
        RunGame_EXIT(1);
    }
#endif //AZ_MONOLITHIC_BUILD

    // create the startup interface
    IGameStartup* pGameStartup = CreateGameStartup();

    const char* const szAutostartLevel
        = linux_autoload_level[0] ? linux_autoload_level : NULL;

    if (!pGameStartup)
    {
        fprintf(stderr, "ERROR: Failed to create the GameStartup Interface!\n");
        RunGame_EXIT(1);
    }

    // run the game
    IGame* game = pGameStartup->Init(startupParams);
    if (game)
    {
        exitCode = pGameStartup->Run(szAutostartLevel);
        pGameStartup->Shutdown();
        pGameStartup = 0;
        RunGame_EXIT(exitCode);
    }

    // if initialization failed, we still need to call shutdown
    pGameStartup->Shutdown();
    pGameStartup = 0;

    fprintf(stderr, "ERROR: Failed to initialize the GameStartup Interface!\n");
    RunGame_EXIT(exitCode);

    gameApp.Stop();
}

// An unreferenced function.  This function is needed to make sure that
// unneeded functions don't make it into the final executable.  The function
// section for this function should be removed in the final linking.
void this_function_is_not_used(void)
{
}

#if defined(_DEBUG)
// Debug code for running the executable with GDB attached.  We'll start an
// XTerm with a GDB attaching to the running process.
void AttachGDB(const char* program)
{
    int pid = getpid();
    char command[1024];

    // check if more advanced terminal is available
    // e.g.: gnome-terminal on ubuntu
    if (system("which gnome-terminal") == 0)
    {
        snprintf(
            command, sizeof command,
            "gnome-terminal -x gdb '%s' %i",
            program,
            pid);
    }
    else //fall back to xterm
    {
        snprintf(
            command, sizeof command,
            "xterm -n 'GDB [Linux Launcher]' -T 'GDB [Linux Launcher]' -e gdb '%s' %i",
            program,
            pid);
    }
    int shell_pid = fork();
    if (shell_pid == -1)
    {
        perror("fork()");
        exit(EXIT_FAILURE);
    }
    if (shell_pid > 0)
    {
        // Allow a few seconds for GDB to attach.
        sleep(5);
        return;
    }
    // Detach.
    for (int i = 0; i < 100; ++i)
    {
        close(i);
    }
    setsid();
    system(command);
    exit(EXIT_SUCCESS);
}
#endif

//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
int main(int argc, char** argv)
{

#if defined(_DEBUG)
    // If the first command line option is -debug, then we'll attach GDB.
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-debug"))
        {
            AttachGDB(argv[0]);
            break;
        }
    }
#endif

    LoadLauncherConfig();

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
                fprintf(stderr, "CRYSIS LinuxLauncher Error: Garbled command line\n");
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

    int result = RunGame(cmdLine);

    return result;
}

// vim:sw=2:ts=2:si
