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
#pragma once

#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/parallel/mutex.h>
#include <Util/Async/AsyncContextWithExtra.h>
#include <AzCore/Component/TickBus.h>

#pragma warning(disable: 4355) // <future> includes ppltasks.h which throws a C4355 warning: 'this' used in base member initializer list
#include <aws/s3/S3Client.h>
#pragma warning(default: 4355)

#include <chrono>
namespace CloudCanvas
{
    namespace StaticData
    {
        class StaticDataManager;

        class StaticDataTransferContext : public Aws::Client::AsyncCallerContext
        {
        public:

            StaticDataTransferContext(const AZStd::string& bucketName, const AZStd::string& outputFile) : m_bucketName(bucketName),
                m_outputFile(outputFile)
            {

            }

            AZStd::string GetBucketName() const { return m_bucketName; }
            AZStd::string GetOutputFile() const { return m_outputFile; }
        private:

            AZStd::string m_bucketName;
            AZStd::string m_outputFile;
        };

        class StaticDataTransferManager : public AZ::TickBus::Handler
        {
        public:

            using RecentRequestMap = AZStd::unordered_map<AZStd::string, std::chrono::time_point<std::chrono::system_clock>>;

            StaticDataTransferManager(StaticDataManager* managerPtr);

            void DoTimerUpdate();
            void UpdateFileRequests();
            void RequestAllLists();

            bool RequestList(const AZStd::string& bucketName);

            bool AddBucket(const AZStd::string& bucketName);
            bool RemoveBucket(const AZStd::string& bucketName);

            void HandleListObjectsResult(const Aws::S3::Model::ListObjectsRequest& request,
                const Aws::S3::Model::ListObjectsOutcome& outcome, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& requestContext);

            void HandleGetObjectResult(const Aws::S3::Model::GetObjectRequest& request,
                const Aws::S3::Model::GetObjectOutcome& outcome, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& requestContext);

            bool RequestFile(const AZStd::string& bucketName, const AZStd::string& bucketPrefix, const AZStd::string& keyName, const AZStd::string& destFolder);

            // Locally we save our requests as a spaced (illegal in bucket/keys) bucket key string
            static AZStd::string GetRequestString(const AZStd::string& bucketName, const AZStd::string& keyName);

            bool HasOutstandingRequest(const AZStd::string& bucketName, const AZStd::string& keyName) const;
            bool RemoveOutstandingRequest(const AZStd::string& bucketName, const AZStd::string& keyName);
            void AddOutstandingRequest(const AZStd::string& bucketName, const AZStd::string& keyName);

            bool HasRecentRequest(const AZStd::string& bucketName, const AZStd::string& keyName) const;
            void AddRecentRequest(const AZStd::string& bucketName, const AZStd::string& keyName);

            bool IsUpdateTimerSet() const;
            bool SetUpdateTimer(int timerValue);

            AZStd::string GetOutputDir() const;
        protected:
            //////////////////////////////////////////////////////////////////////////
            // TickBus
            void	OnTick(float deltaTime, AZ::ScriptTimePoint time) override;
            //////////////////////////////////////////////////////////////////////////
        private:

            static bool CanWriteToFile(const AZStd::string& fileName);
            void CheckConfiguration();

            void AddPendingCompletion(std::shared_ptr<const StaticDataTransferContext> thisCompletion);
            void UpdatePendingCompletions();

            void NewContentReady(std::shared_ptr<const StaticDataTransferContext> requestContext);

            StaticDataManager* m_manager{ nullptr };
            AZStd::unordered_set<AZStd::string> m_monitoredBuckets;

            AZStd::unordered_set<AZStd::string> m_outstandingRequests;
            mutable AZStd::mutex m_outstandingRequestMutex;

            mutable AZStd::mutex m_completedDownloadMutex;

            RecentRequestMap m_recentRequests;
            mutable AZStd::mutex m_recentRequestMutex;
            bool m_appliedConfiguration{ false };

            // These are file requests which have reported success which we need to query for access before acting on the updated content
            AZStd::vector<std::shared_ptr<const StaticDataTransferContext>> m_pendingCompletions;
        };
    }
}