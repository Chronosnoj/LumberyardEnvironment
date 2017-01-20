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
#ifndef ASSETPROCESSORUTIL_H
#define ASSETPROCESSORUTIL_H

#include <QPair>
#include <QMetaType>
#include <AzCore/Math/Uuid.h>
#include <QString>

#include <AssetBuilderSDK/AssetBuilderBusses.h>
#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzFramework/Asset/AssetRegistry.h>
#include "native/utilities/ApplicationManagerAPI.h"

namespace AssetProcessor
{
    const char* const DebugChannel = "Debug"; //Use this channel name if you want to write the message to the log file only.
    const char* const ConsoleChannel = "AssetProcessor";// Use this channel name if you want to write the message to both the console and the log file.
    const char* const FENCE_FILE_EXTENSION = "fence"; //fence file extension
    const unsigned int g_RetriesForFenceFile = 5; // number of retries for fencing
    // Even though AP can handle files with path length greater than window's legacy path length limit, we have some 3rdparty sdk's 
    // which do not handle this case ,therefore we will make AP fail any jobs whose either source file or output file name exceeds the windows legacy path length limit 
#define AP_MAX_PATH_LEN 260

    extern AZ::s64 GetThreadLocalJobId();
    extern void SetThreadLocalJobId(AZ::s64 jobId);

    //! a shared convenience typedef for requests that have come over the network
    //! The first element is the connection id it came from and the second element is the serial number
    //! which can be used to send a response.
    typedef QPair<quint32, quint32> NetworkRequestID;

    enum AssetScanningStatus
    {
        Unknown, Started, InProgress, Completed, Stopped
    };
    struct AssetRecognizer;

    //! JobEntry is an internal structure that is used to uniquely identify a specific job and keeps track of it as it flows through the AP system
    //! It prevents us from having to copy the entire of JobDetails, which is a very heavy structure.
    //! In general, communication ABOUT jobs will have the JobEntry as the key
    class JobEntry
    {
        public:
            // note that QStrings are refcounted copy-on-write, so a move operation will not be beneficial unless this struct gains considerable heap allocated fields.

            QString m_relativePathToFile; //! contains relative path (relative to watch folder), used to identify input file and database name
            QString m_absolutePathToFile; //! contains the actual absolute path to the file, including watch folder.
            AZ::Uuid m_builderUUID; //! the builder that will perform the job
            QString m_platform; //! the platform that the job will operate for
            QString m_jobKey; //! a key that uniquely identifies job if all of the above are the same.  
            // JobKey is used when a single input file, for a single platform, for a single builder outputs many seperate jobs

            // another unique identifier that can be used is this, the jobID.  The JobId is 
            // what it will be written into the database as, eventually, and is unique across all jobs.
            AZ::s64 m_jobId; //! assigned by the database system, 

            unsigned int m_computedFingerprint = 0; // what the fingerprint was at the time of job creation.
            bool m_checkExclusiveLock = false;  // indicates whether we need to check the input file for exclusive lock before we process this job 

            JobEntry() = default;
            JobEntry(const JobEntry& other) = default;
            JobEntry(AZ::s64 jobId, const QString& absolutePathToFile, const QString& relativePathToFile, const AZ::Uuid& builderUUID, const QString& platform, const QString& jobKey, int computedFingerprint = 0)
                : m_jobId(jobId)
                , m_absolutePathToFile(absolutePathToFile)
                , m_relativePathToFile(relativePathToFile)
                , m_builderUUID(builderUUID)
                , m_platform(platform)
                , m_jobKey(jobKey)
                , m_computedFingerprint(computedFingerprint)
            {
            }
    };

    //! JobDetails is an internal structure that is used to store job related information by the Asset Processor
    //! Its heavy, since it contains the parameter map and the builder desc so is expensive to copy and in general only used to create jobs
    //! After which, the Job Entry is used to track and identify jobs.
    class JobDetails
    {
    public:
        JobEntry m_jobEntry;
        QString m_extraInformationForFingerprinting;
        QString m_watchFolder; // the watch folder the file was found in
        QString m_destinationPath; // the final folder that will be where your products are placed if you give relative path names
        // destinationPath will be a cache folder.  If you tell it to emit something like "blah.dds"
        // it will put it in (destinationPath)/blah.dds for example

        AZStd::vector<AssetBuilderSDK::SourceFileDependency> m_sourceFileDependencyList;

        bool m_critical = false;
        int m_priority = -1;
        AssetBuilderSDK::AssetBuilderDesc   m_assetBuilderDesc;
        AssetBuilderSDK::JobParameterMap    m_jobParam;

        JobDetails() = default;
    };

    //! ProductInfo is an internal structure used to store product related info
    class ProductInfo
    {
    public:
        AZ::Data::AssetId m_assetId;
        QString m_platform;
        AzFramework::AssetRegistry::AssetInfo m_assetInfo;
    };

    //! QuitListener is an utility class that can be used to listen for application quit notification
    class QuitListener: public ApplicationManagerNotifications::Bus::Handler
    {
    public:

        void ApplicationShutdownRequested() override
        {
            m_requestedQuit = true;
        }

        bool WasQuitRequested() const
        {
            return m_requestedQuit;
        }

    private:
        volatile bool m_requestedQuit = false;
    };
} // namespace AssetProcessor

Q_DECLARE_METATYPE(AssetBuilderSDK::ProcessJobResponse)
Q_DECLARE_METATYPE(AssetProcessor::JobEntry)
Q_DECLARE_METATYPE(AssetProcessor::JobDetails)
Q_DECLARE_METATYPE(AssetProcessor::NetworkRequestID)
Q_DECLARE_METATYPE(AssetProcessor::AssetScanningStatus)
#endif // ASSETPROCESSORUTIL_H
