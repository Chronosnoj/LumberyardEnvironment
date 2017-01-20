/*
* All or portions of this file Copyright(c) Amazon.com, Inc.or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution(the "License").All use of this software is governed by the License,
*or, if provided, by the license below or the license accompanying this file.Do not
* remove or modify any license notices.This file is distributed on an "AS IS" BASIS,
*WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#ifndef RESOURCECOMPILER_RCBUILDER_H
#define RESOURCECOMPILER_RCBUILDER_H

#include "../utilities/ApplicationManagerAPI.h"
#include "native/AssetManager/assetProcessorManager.h"
#include "native/utilities/PlatformConfiguration.h"

namespace AssetProcessor
{
    //! Worker class to handle shell execution of the legacy rc.exe compiler
    class NativeLegacyRCCompiler
    {
    public:
        //! RC.exe execution result
        struct Result
        {
            Result(int exitCode, bool crashed, const QString& outputDir);
            Result() = default;
            int     m_exitCode = 1;
            bool    m_crashed = false;
            QString m_outputDir;
        };

        NativeLegacyRCCompiler();

        bool Initialize(const QString& systemRoot, const QString& rcExecutableFullPath);
        bool Execute(const QString& inputFile, const QString& platform, const QString& params, const QString& dest, Result& result) const;
        static QString BuildCommand(const QString& inputFile, const QString& platform, const QString& params, const QString& dest);
        void RequestQuit();
    private:
        static const int            s_maxSleepTime;
        static const unsigned int   s_jobMaximumWaitTime;
        bool                        m_resourceCompilerInitialized;
        QDir                        m_systemRoot;
        QString                     m_rcExecutableFullPath;
        volatile bool               m_requestedQuit;
    };

    //! Internal Builder version of the asset recognizer structure that is read in from the platform configuration class 
    struct InternalAssetRecognizer
        : public AssetRecognizer
    {
        //! Filter for conversion of platforms
        typedef std::function<bool(const AssetPlatformSpec&)> PlatformFilter;

        InternalAssetRecognizer() = default;
        InternalAssetRecognizer(const AssetRecognizer& src, PlatformFilter filter);
        InternalAssetRecognizer(const InternalAssetRecognizer& src) = default;

        AZ::u32 CalculateCRC() const;

        //! Map of platform specs based on the enumeration value of a target platform
        QHash<int, AssetPlatformSpec>           m_platformSpecsByPlatform;

        //! unique id that is generated for each unique internal asset recognizer
        //! which can be used as the key for the job parameter map (see AssetBuilderSDK::JobParameterMap)
        AZ::u32                                 m_paramID;
    };
    typedef QHash<AZ::u32, InternalAssetRecognizer*> InternalRecognizerContainer;
    typedef QList<const InternalAssetRecognizer*> InternalRecognizerPointerContainer;
    typedef AZStd::function<void(const AssetBuilderSDK::AssetBuilderDesc& builderDesc)> RegisterBuilderDescCallback;

    class InternalRecognizerBasedBuilder
        : public AssetBuilderSDK::AssetBuilderCommandBus::Handler
    {
    public:
        InternalRecognizerBasedBuilder(const AZStd::string& name,const AZ::Uuid& builderUuid,InternalAssetRecognizer::PlatformFilter platformFilter);
        virtual ~InternalRecognizerBasedBuilder();

        virtual AssetBuilderSDK::AssetBuilderDesc CreateBuilderDesc(const AZStd::vector<AssetBuilderSDK::AssetBuilderPattern>& builderPatterns) = 0;

        void ShutDown() override;

        virtual bool Initialize(const RecognizerConfiguration& recognizerConfig);
        virtual void InitializeAssetRecognizers(const RecognizerContainer& assetRecognizers);
        virtual void InitializeExcludeRecognizers(const ExcludeRecognizerContainer& excludeRecognizers);
        virtual void UnInitialize();

        //! Returns false if there were no matches, otherwise returns true
        virtual bool GetMatchingRecognizers(int platformFlags, const QString fileName, InternalRecognizerPointerContainer& output) const;

        //! Determines if the file is an exclude file (even though it matches a recognized pattern)
        bool IsFileExcluded(const QString& fileName) const;

        AZ::Uuid GetUuid() const;
        const AZStd::string& GetBuilderName() const;
    protected:

        volatile bool                           m_isShuttingDown;
        InternalAssetRecognizer::PlatformFilter m_platformFilter;
        InternalRecognizerContainer             m_assetRecognizerDictionary;
        ExcludeRecognizerContainer              m_excludeAssetRecognizers;
        AZ::Uuid                                m_builderUuid;
        AZStd::string                           m_name;
    };

    class InternalRCBuilder
        : public InternalRecognizerBasedBuilder
    {
    public:
        static AZ::Uuid GetUUID();
        static const char* GetName();
        static InternalAssetRecognizer::PlatformFilter s_platformFilter;

        //////////////////////////////////////////////////////////////////////////
        InternalRCBuilder();
        virtual ~InternalRCBuilder();

        void CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response);
        void ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response);

        //////////////////////////////////////////////////////////////////////////
        //! InternalRecognizerBasedBuilder
        bool Initialize(const RecognizerConfiguration& recognizerConfig) override;
        AssetBuilderSDK::AssetBuilderDesc CreateBuilderDesc(const AZStd::vector<AssetBuilderSDK::AssetBuilderPattern>& builderPatterns) override;

        //////////////////////////////////////////////////////////////////////////
        //!AssetBuilderSDK::AssetBuilderCommandBus interface
        void ShutDown() override;

    protected:
        bool MatchTempFileToSkip(const QString& outputFilename);

        NativeLegacyRCCompiler          m_legacyRCCompiler;
    };

    class InternalCopyBuilder
        : public InternalRecognizerBasedBuilder
    {
    public:
        static AZ::Uuid GetUUID();
        static const char* GetName();
        static InternalAssetRecognizer::PlatformFilter s_platformFilter;

        //////////////////////////////////////////////////////////////////////////
        InternalCopyBuilder();

        virtual ~InternalCopyBuilder();

        void CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response);
        void ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response);

        //////////////////////////////////////////////////////////////////////////
        //! InternalRecognizerBasedBuilder
        AssetBuilderSDK::AssetBuilderDesc CreateBuilderDesc(const AZStd::vector<AssetBuilderSDK::AssetBuilderPattern>& builderPatterns);
    };
}

#endif //RESOURCECOMPILER_RCBUILDER_H