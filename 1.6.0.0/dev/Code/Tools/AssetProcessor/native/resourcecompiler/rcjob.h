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
#ifndef RCJOB_H
#define RCJOB_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QStringList>
#include "RCCommon.h"
#include "native/utilities/PlatformConfiguration.h"
#include <AzCore/Math/Uuid.h>
#include <AssetBuilderSDk/AssetBuilderSDK.h>
#include "native/assetprocessor.h"
#include <QFileInfoList>

namespace AssetProcessor
{
    struct AssetRecognizer;
    class RCJob;

    //! Params Base class
    struct Params
    {
        Params(AssetProcessor::RCJob* job = nullptr)
            : m_rcJob(job)
        {}
        virtual ~Params() = default;

        AssetProcessor::RCJob* m_rcJob;
        QString m_finalOutputDir;

        Params(const Params&) = default;

        virtual bool IsValidParams() const;
    };

    //! RCParams contains info that is required by the rc
    struct RCParams
        : public Params
    {
        QString m_rootDir;
        QString m_rcExe;
        QString m_inputFile;
        QString m_platform;
        QString m_params;

        RCParams(AssetProcessor::RCJob* job = nullptr)
            : Params(job)
        {}

        RCParams(const RCParams&) = default;

        bool IsValidParams() const override;
    };

    //! BuilderParams contains info that is required by the builders
    struct BuilderParams
        : public Params
    {
        AssetBuilderSDK::ProcessJobRequest m_processJobRequest;
        AssetBuilderSDK::AssetBuilderDesc  m_assetBuilderDesc;

        BuilderParams(AssetProcessor::RCJob* job = nullptr)
            : Params(job)
        {}

        BuilderParams(const BuilderParams&) = default;
    };

    //! JobOutputInfo is used to store job related messages.
    //! Messages can be an error or just some information.
    struct JobOutputInfo
    {
        QString m_windowName; // window name is used to specify whether it is an error or not
        QString m_message;// the actual message

        JobOutputInfo() = default;

        JobOutputInfo(QString window, QString message)
            : m_windowName(window)
            , m_message(message)
        {
        }
    };


    /**
    * The RCJob class contains all the necessary information about a single RC job
    */
    class RCJob
        : public QObject
    {
        Q_OBJECT

    public:
        enum JobState
        {
            pending,
            processing,
            completed,
            crashed,
            terminated,
            failed,
        };

        explicit RCJob(QObject* parent = 0);

        virtual ~RCJob();

        void Init(JobDetails& details);

        AZ::s64 jobID() const;
    
        QString params() const;
        QString commandLine() const;

        QDateTime timeCreated() const;
        void setTimeCreated(const QDateTime& timeCreated);

        QDateTime timeLaunched() const;
        void setTimeLaunched(const QDateTime& timeLaunched);

        QDateTime timeCompleted() const;
        void setTimeCompleted(const QDateTime& timeCompleted);

        void SetOriginalFingerprint(unsigned int originalFingerprint);
        unsigned int GetOriginalFingerprint() const;

        JobState GetState() const;
        void SetState(const JobState& state);

        QString GetInputFileAbsolutePath() const;
        QString GetInputFileRelativePath() const;

        QString destination() const;

        //! the final output path is where the actual outputs are copied when processing succeeds
        //! this will be in the asset cache, in the gamename / platform / gamename folder.
        QString GetFinalOutputPath() const;

        QString GetPlatform() const;

        // intentionally non-const to move.
        AssetBuilderSDK::ProcessJobResponse& GetProcessJobResponse();

        const JobEntry& GetJobEntry() const;

        void start();

        const QueueElementID& GetElementID() const { return m_QueueElementID; }

        void SetInputFileAbsolutePath(QString absolutePath);
        void SetCheckExclusiveLock(bool value);

    signals:
        void finished();
        //! This signal will be emitted when we make sure that no other application has a lock on the source file 
        //! and also that the fingerprint of the source file is stable and not changing.
        //! This will basically indicate that we are starting to perform work on the current job
        void BeginWork();
        void jobFinished(AssetBuilderSDK::ProcessJobResponse result);

    public:
        static QString stateDescription(const JobState& state);
        static void executeBuilderCommand(BuilderParams builderParams);
        static bool copyCompiledAssets(BuilderParams& params, AssetBuilderSDK::ProcessJobResponse& response);

        QString GetJobKey() const;
        QString GetWatchFolder() const;
        AZ::Uuid GetBuilderUUID() const;
        bool IsCritical() const;
        int GetPriority() const;

    protected:
        //! DoWork ensure that the job is ready for being processing and than makes the actual builder call   
        virtual void DoWork(AssetBuilderSDK::ProcessJobResponse& result, BuilderParams& builderParams, AssetProcessor::QuitListener& listener);

    private:

        JobDetails m_jobDetails;
        void PopulateProcessJobRequest(AssetBuilderSDK::ProcessJobRequest& processJobRequest);
        JobState m_jobState;

        QueueElementID m_QueueElementID; // cached to prevent lots of construction of this all over the place

        QDateTime m_timeCreated;
        QDateTime m_timeLaunched;
        QDateTime m_timeCompleted;

        unsigned int m_exitCode = 0;

        QStringList m_productFiles;
        
        AssetBuilderSDK::ProcessJobResponse m_processJobResponse;
    };
} // namespace AssetProcessor

Q_DECLARE_METATYPE(AssetProcessor::BuilderParams);
Q_DECLARE_METATYPE(AssetProcessor::JobOutputInfo);
Q_DECLARE_METATYPE(AssetProcessor::RCParams);

#endif // RCJOB_H
