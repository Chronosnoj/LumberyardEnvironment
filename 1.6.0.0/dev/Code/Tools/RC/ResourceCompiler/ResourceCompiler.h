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

#pragma once

#include "IRCLog.h"
#include "IResCompiler.h"
#include "CfgFile.h"
#include "Config.h"
#include "DependencyList.h"
#include "ExtensionManager.h"
#include "MultiplatformConfig.h"
#include "PakSystem.h"
#include "PakManager.h"
#include "RcFile.h"
#include "ThreadUtils.h"
#include "CryVersion.h"
#include "IProgress.h"
#include "ICrySourceControl.h"
#include <ISystem.h>

#include <map>                 // stl multimap<>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>
#include "ProxySourceControl.h"

class CPropertyVars;
class XmlNodeRef;

/** Implementation of IResCompiler interface.
*/
class ResourceCompiler
    : public IResourceCompiler
    , public IProgress
    , public IRCLog
    , public IConfigKeyRegistry
    , protected AzToolsFramework::AssetSystemRequestBus::Handler
{
public:
    ResourceCompiler();
    virtual ~ResourceCompiler();

    //! e.g. print statistics
    void PostBuild();

    int GetNumWarnings() const {return m_numWarnings; }
    int GetNumErrors() const {return m_numErrors; }

    // interface IProgress --------------------------------------------------

    virtual void StartProgress();
    virtual void ShowProgress(const char* pMessage, size_t progressValue, size_t maxProgressValue);
    virtual void FinishProgress();

    // interface IConfigKeyRegistry ------------------------------------------

    virtual void VerifyKeyRegistration(const char* szKey) const;
    virtual bool HasKeyRegistered(const char* szKey) const;
    // -----------------------------------------------------------------------

    // interface IResourceCompiler -------------------------------------------

    virtual void RegisterKey(const char* key, const char* helptxt);

    virtual const char* GetExePath() const;
    virtual const char* GetTmpPath() const;
    virtual const char* GetInitialCurrentDir() const;

    virtual void RegisterConvertor(const char* name, IConvertor* conv);

    void AddPluginDLL(HMODULE pluginDLL);

    void AddUnitTest(FnRunUnitTests unitTestFunction) override;

    virtual IPakSystem* GetPakSystem();

    virtual const ICfgFile* GetIniFile() const;

    virtual int GetPlatformCount() const;
    virtual const PlatformInfo* GetPlatformInfo(int index) const;
    virtual int FindPlatform(const char* name) const;

    virtual XmlNodeRef LoadXml(const char* filename);
    virtual XmlNodeRef CreateXml(const char* tag);

    virtual void AddInputOutputFilePair(const char* inputFilename, const char* outputFilename);
    virtual void MarkOutputFileForRemoval(const char* sOutputFilename);

    virtual void AddExitObserver(IExitObserver* p);
    virtual void RemoveExitObserver(IExitObserver* p);

    virtual IRCLog* GetIRCLog()
    {
        return this;
    }

    virtual int GetVerbosityLevel() const
    {
        return m_verbosityLevel;
    }

    virtual const SFileVersion& GetFileVersion() const
    {
        return m_fileVersion;
    }

    virtual const void GetGenericInfo(char* buffer, size_t bufferSize, const char* rowSeparator) const;

    virtual bool CompileSingleFileBySingleProcess(const char* filename);

    virtual void SetAssetWriter(IAssetWriter* pAssetWriter)
    {
        m_pAssetWriter = pAssetWriter;
    }

    virtual IAssetWriter* GetAssetWriter() const
    {
        return m_pAssetWriter;
    }

    virtual ICrySourceControl* GetCrySourceControl()
    {
        if (!m_pSourceControl)
        {
            bool isSourceControlDisabled = m_multiConfig.getConfig().GetAsBool("nosourcecontrol", false, false);
            // Checking whether source control is disabled
            if (!isSourceControlDisabled)
            {
                string executablePath = StringHelpers::Replace(m_exePath, '\\', '/');
                m_pSourceControl = new CProxySourceControl(executablePath.c_str());
            }
        }
        return m_pSourceControl;
    }
    // -----------------------------------------------------------------------

    // interface IRCLog ------------------------------------------------------

    virtual void LogV(const IRCLog::EType eType, const char* szFormat, va_list args);
    // -----------------------------------------------------------------------

    //////////////////////////////////////////////////////////////////////////
    // Resource compiler implementation.
    //////////////////////////////////////////////////////////////////////////

    void Init(Config& config);
    bool LoadIniFile();
    void UnregisterConvertors();

    bool AddPlatform(const string& names, bool bBigEndian, int pointerSize);

    MultiplatformConfig& GetMultiplatformConfig();

    void SetupMaxThreads();
    int  GetMaxThreads() const;

    bool CollectFilesToCompile(const string& filespec, std::vector<RcFile>& files);
    bool CompileFilesBySingleProcess(const std::vector<RcFile>& files);
    int  ProcessJobFile();
    void RemoveOutputFiles();     // to remove old files for less confusion
    void CleanTargetFolder(bool bUseOnlyInputFiles);

    bool CompileFile(const char* sourceFullFileName, const char* targetLeftPath, const char* sourceInnerPath, ICompiler* compiler);

    const char* GetLogPrefix() const;

    //! call this if user asks for help
    void ShowHelp(bool bDetailed);
    //////////////////////////////////////////////////////////////////////////

    void QueryVersionInfo();

    void InitPaths();

    void NotifyExitObservers();

    void InitLogs(Config& config);
    string FormLogFileName(const char* suffix) const;
    const string& GetMainLogFileName() const;
    const string& GetErrorLogFileName() const;

    clock_t GetStartTime() const;
    bool GetTimeLogging() const;
    void SetTimeLogging(bool enable);

    void LogMemoryUsage(bool bReportProblemsOnly);

    void InitPakManager();

    static string FindSuitableSourceRoot(const std::vector<string>& sourceRootsReversed, const string& fileName);
    static void GetSourceRootsReversed(const IConfig* config, std::vector<string>& sourceRootsReversed);

    int RunUnitTests();

    void InitSystem(SSystemInitParams& startupParams);

private:
    void FilterExcludedFiles(std::vector<RcFile>& files);
    void ProcessInputFilesWithSourceControl(std::vector<RcFile>& files);

    void CopyFiles(const std::vector<RcFile>& files, bool bNoOverwrite = false);

    void ExtractJobDefaultProperties(std::vector<string>& properties, XmlNodeRef& jobNode);
    int  EvaluateJobXmlNode(CPropertyVars& properties, XmlNodeRef& jobNode, bool runJobs);
    int  RunJobByName(CPropertyVars& properties, XmlNodeRef& anyNode, const char* name);

    void ScanForAssetReferences(std::vector<string>& outReferences, const string& refsRoot);
    void SaveAssetReferences(const std::vector<string>& references, const string& filename, const string& includeMasks, const string& excludeMasks);

    void InitializeThreadIds();

    void LogLine(const IRCLog::EType eType, const char* szText);

    // to log multiple lines (\n separated) with padding before
    void LogMultiLine(const char* szText);

    virtual SSystemGlobalEnvironment* GetSystemEnvironment() override;

    // -----------------------------------------------------------------------
public:
    static const char* const m_filenameRcExe;
    static const char* const m_filenameRcIni;
    static const char* const m_filenameOptions;
    static const char* const m_filenameLog;
    static const char* const m_filenameLogWarnings;
    static const char* const m_filenameLogErrors;
    static const char* const m_filenameCrashDump;
    static const char* const m_filenameOutputFileList;  //!< list of source=target filenames that rc processed, used for cleaning target folder
    static const char* const m_filenameDeletedFileList;
    static const char* const m_filenameCreatedFileList;

    bool m_bQuiet;                                      //!< true= don't log anything to console

private:
    enum
    {
        kMaxPlatformCount = 20
    };
    int m_platformCount;
    PlatformInfo m_platforms[kMaxPlatformCount];

    ExtensionManager        m_extensionManager;

    // pointer to the source control tool. note: it's allowed to be null.
    ICrySourceControl*      m_pSourceControl;
    IAssetWriter*             m_pAssetWriter;

    ThreadUtils::CriticalSection m_inputOutputFilesLock;
    CDependencyList         m_inputOutputFileList;

    std::vector<IExitObserver*> m_exitObservers;
    ThreadUtils::CriticalSection m_exitObserversLock;

    float                   m_memorySizePeakMb;
    ThreadUtils::CriticalSection m_memorySizeLock;

    // log files
    string                  m_logPrefix;
    string                  m_mainLogFileName;      //!< for all messages, might be empty (no file logging)
    string                  m_warningLogFileName;   //!< for warnings only, might be empty (no file logging)
    string                  m_errorLogFileName;     //!< for errors only, might be empty (no file logging)
    string                  m_logHeaderLine;
    ThreadUtils::CriticalSection m_logLock;

    clock_t                 m_startTime;
    bool                    m_bTimeLogging;

    float                   m_progressLastPercent;
    int                     m_verbosityLevel;

    CfgFile                 m_iniFile;
    MultiplatformConfig     m_multiConfig;

    bool                    m_bWarningHeaderLine;   //!< true= header was already printed, false= header needs to be printed
    bool                    m_bErrorHeaderLine;     //!< true= header was already printed, false= header needs to be printed
    bool                    m_bWarningsAsErrors;    //!< true= treat any warning as error.

    int                     m_systemCpuCoreCount;
    int                     m_processCpuCoreCount;
    int                     m_systemLogicalProcessorCount;
    int                     m_processLogicalProcessorCount;
    int                     m_maxThreads;           //!< max number of threads (set by /threads command-line option)

    SFileVersion            m_fileVersion;
    string                  m_exePath;
    string                  m_tempPath;
    string                  m_initialCurrentDir;

    std::map<string, string> m_KeyHelp;              // [lower key] = help, created from RegisterKey

    // Files to delete
    std::vector<RcFile>     m_inputFilesDeleted;

    PakManager*             m_pPakManager;

    int                     m_numWarnings;
    int                     m_numErrors;

    unsigned long           m_tlsIndex_pThreadData;

    std::vector<FnRunUnitTests> m_unitTestFunctions;
    std::unique_ptr<ISystem> m_pSystem;
    void* m_systemDll;

    std::vector<HMODULE> m_loadedPlugins;

protected:
    // -------------- Implement AzToolsFramework::AssetSystemRequestBus::Handler -----------
    bool ConvertFullPathToRelativeAssetPath(const AZStd::string& fullPath, AZStd::string& outputPath) override;
    bool ConvertRelativePathToFullAssetPath(const AZStd::string& relPath, AZStd::string& fullPath) override;

    const char* GetAbsoluteDevGameFolderPath() override;
    const char* GetAbsoluteDevRootFolderPath() override;

    void UpdateQueuedEvents() override {}
    // --------------------------------------------------------------------------------
};

