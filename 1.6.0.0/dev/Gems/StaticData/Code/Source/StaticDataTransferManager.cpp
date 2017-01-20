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
#include "StdAfx.h"
#include <StaticDataTransferManager.h>

#pragma warning(push)
#pragma warning(disable: 4819) // Invalid character not in default code page
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/ListObjectsRequest.h>
#include <aws/core/utils/Outcome.h>
#include <aws/core/utils/memory/stl/AWSStreamFwd.h>
#pragma warning(pop)

#include <fstream>
#include <AzCore/IO/SystemFile.h>

#include <AzCore/std/parallel/lock.h>

#include <Util/FlowSystem/AsyncFlowgraphResult.h>

#include <LmbrAWS/ILmbrAWS.h>
#include <LmbrAWS/IAWSClientManager.h>

#include <StaticDataManager.h>

static const int bucketUpdateFrequency = 0;  // Time in ms to poll for bucket updates.  This forces a listObjects request at this frequency and should be used lightly if at all
static const int fileRequestRetryMax = 10000; // Let's not request the same file again too often
static const char* outputDir = "StaticData/CSV/";
static const char* listPrefix = "static-data/";

namespace CloudCanvas
{
    namespace StaticData
    {

        StaticDataTransferManager::StaticDataTransferManager(StaticDataManager* managerPtr) :
            m_manager{ managerPtr }
        {

        }

        void StaticDataTransferManager::DoTimerUpdate()
        {
            UpdateFileRequests();
            RequestAllLists();
        }

        AZStd::string StaticDataTransferManager::GetOutputDir() const
        {
            AZStd::string baseDir = m_manager->GetEditorGameDir();
            if (baseDir.length())
            {
                baseDir += '/';
            }
            baseDir += outputDir;
            return baseDir;
        }
        void StaticDataTransferManager::UpdateFileRequests()
        {
            AZStd::lock_guard<AZStd::mutex> requestLock(m_recentRequestMutex);

            std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
            auto thisPoint = m_recentRequests.begin();
            while (thisPoint != m_recentRequests.end())
            {
                auto lastPoint = thisPoint;
                ++thisPoint;

                if (std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastPoint->second).count() > fileRequestRetryMax)
                {
                    m_recentRequests.erase(lastPoint);
                }
            }
        }
        void StaticDataTransferManager::RequestAllLists()
        {
            for (auto thisList : m_monitoredBuckets)
            {
                RequestList(thisList);
            }
        }

        void StaticDataTransferManager::CheckConfiguration()
        {
            if (!m_appliedConfiguration)
            {
                if (gEnv && gEnv->pLmbrAWS)
                {
                    gEnv->pLmbrAWS->GetClientManager()->ApplyConfiguration();
                    m_appliedConfiguration = true;
                }
            }
        }
        bool StaticDataTransferManager::RequestList(const AZStd::string& bucketName)
        {
            if (!gEnv || !gEnv->pLmbrAWS)
            {
                return false;
            }

            CheckConfiguration();

            auto thisClient = gEnv->pLmbrAWS->GetClientManager()->GetS3Manager().CreateBucketClient(bucketName.c_str());
            if (!thisClient.IsReady())
            {
                return false;
            }
            if (!thisClient.GetUnmanagedClient())
            {
                return false;
            }

            auto thisContext = std::make_shared<const StaticDataTransferContext>(bucketName, AZStd::string{});
            // This is not a bucket client which persists, we just need to grab the mapped name so let's not call applyConfiguration on everything each time
            auto& settings = gEnv->pLmbrAWS->GetClientManager()->GetS3Manager().GetBucketClientSettingsCollection().GetSettings(bucketName.c_str());

            Aws::S3::Model::ListObjectsRequest thisRequest;
            thisRequest.SetBucket(settings.bucketName);
            thisRequest.SetPrefix(listPrefix);

            MARSHALL_AWS_BACKGROUND_REQUEST(S3, thisClient, ListObjects, StaticDataTransferManager::HandleListObjectsResult, thisRequest, thisContext)

            return true;
        }


        void StaticDataTransferManager::HandleListObjectsResult(const Aws::S3::Model::ListObjectsRequest& request,
            const Aws::S3::Model::ListObjectsOutcome& outcome, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& requestContext)
        {
            if (outcome.IsSuccess())
            {
                for (auto thisFile : outcome.GetResult().GetContents())
                {
                    AZStd::string thisTag(thisFile.GetETag().c_str());

                    AZStd::string localStr(csvDir);
                    AZStd::string fileStr(thisFile.GetKey().c_str());

                    if (request.GetPrefix().length())
                    {
                        fileStr.erase(0, request.GetPrefix().length());
                    }
                    localStr += fileStr;

                    auto diskMD5 = m_manager->CalculateMD5(localStr.c_str());

                    if (thisTag != diskMD5)
                    {
                        RequestFile(request.GetBucket().c_str(), request.GetPrefix().c_str(), fileStr.c_str(), GetOutputDir());
                    }
                }
            }
        }

        void StaticDataTransferManager::HandleGetObjectResult(const Aws::S3::Model::GetObjectRequest& request,
            const Aws::S3::Model::GetObjectOutcome& outcome, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& requestContext)
        {
            RemoveOutstandingRequest(request.GetBucket().c_str(), request.GetKey().c_str());
            if (outcome.IsSuccess())
            {
                auto staticContext = std::static_pointer_cast<const StaticDataTransferContext>(requestContext);
                AddPendingCompletion(staticContext);
            }
        }

        void StaticDataTransferManager::AddPendingCompletion(std::shared_ptr<const StaticDataTransferContext> thisCompletion)
        {
            AZStd::lock_guard<AZStd::mutex> thisLock{ m_completedDownloadMutex };
            m_pendingCompletions.push_back(thisCompletion);
            AZ::TickBus::Handler::BusConnect();
        }

        void StaticDataTransferManager::OnTick(float deltaTime, AZ::ScriptTimePoint time)
        {
            UpdatePendingCompletions();
        }

        void StaticDataTransferManager::NewContentReady(const std::shared_ptr<const StaticDataTransferContext> requestContext)
        {
            m_manager->LoadRelativeFile(requestContext->GetOutputFile().c_str());
        }

        void StaticDataTransferManager::UpdatePendingCompletions()
        {
            AZStd::lock_guard<AZStd::mutex> thisLock{ m_completedDownloadMutex };

            size_t thisIndex = 0;
            while (thisIndex < m_pendingCompletions.size())
            {
                std::shared_ptr<const StaticDataTransferContext> thisContext = m_pendingCompletions[thisIndex];
                AZ::IO::HandleType pFile = gEnv->pCryPak->FOpen(thisContext->GetOutputFile().c_str(), "rt");

                if (pFile != AZ::IO::InvalidHandle)
                {
                    m_pendingCompletions[thisIndex] = m_pendingCompletions.back();
                    m_pendingCompletions.pop_back();
                    gEnv->pCryPak->FClose(pFile);
                    NewContentReady(thisContext);
                }
                else
                {
                    ++thisIndex;
                }
            }
            if (!m_pendingCompletions.size())
            {
                AZ::TickBus::Handler::BusDisconnect();
            }
        }

        AZStd::string StaticDataTransferManager::GetRequestString(const AZStd::string& bucketName, const AZStd::string& keyName) 
        {
            return (bucketName + " " + keyName);
        }

        bool StaticDataTransferManager::HasOutstandingRequest(const AZStd::string& bucketName, const AZStd::string& keyName) const
        {
            AZStd::lock_guard<AZStd::mutex> requestLock(m_outstandingRequestMutex);
            return (m_outstandingRequests.find(GetRequestString(bucketName, keyName)) != m_outstandingRequests.end());
        }

        void StaticDataTransferManager::AddOutstandingRequest(const AZStd::string& bucketName, const AZStd::string& keyName) 
        {
            AZStd::lock_guard<AZStd::mutex> requestLock(m_outstandingRequestMutex);
            m_outstandingRequests.insert(GetRequestString(bucketName, keyName));
        }

        bool StaticDataTransferManager::RemoveOutstandingRequest(const AZStd::string& bucketName, const AZStd::string& keyName)
        {
            AZStd::lock_guard<AZStd::mutex> requestLock(m_outstandingRequestMutex);
            return (m_outstandingRequests.erase(GetRequestString(bucketName, keyName)) != 0);
        }

        bool StaticDataTransferManager::CanWriteToFile(const AZStd::string& localFileName)
        {
            Aws::OFStream testFile(localFileName.c_str(), std::ios_base::out | std::ios_base::binary);

            // Can we even write to this? (Possibly not checked out)
            return testFile.good();
        }

        bool StaticDataTransferManager::RequestFile(const AZStd::string& bucketName, const AZStd::string& bucketPrefix, const AZStd::string& keyName, const AZStd::string& destFolder)
        {
            AZStd::string requestKey(bucketPrefix + keyName);

            if (HasOutstandingRequest(bucketName, requestKey) || HasRecentRequest(bucketName, requestKey))
            {
                return false;
            }

            auto thisClient = gEnv->pLmbrAWS->GetClientManager()->GetS3Manager().CreateBucketClient(bucketName.c_str());
            if (!thisClient.IsReady())
            {
                return false;
            }
            if (!thisClient.GetUnmanagedClient())
            {
                return false;
            }

            // This is not a bucket client which persists, we just need to grab the mapped name so let's not call applyConfiguration on everything each time
            auto& settings = gEnv->pLmbrAWS->GetClientManager()->GetS3Manager().GetBucketClientSettingsCollection().GetSettings(bucketName.c_str());

            Aws::S3::Model::GetObjectRequest thisRequest;
            thisRequest.SetKey(requestKey.c_str());
            thisRequest.SetBucket(settings.bucketName);
            AZStd::string localFileName(destFolder);
            localFileName += keyName;

            auto thisContext = std::make_shared<const StaticDataTransferContext>(bucketName, localFileName);

            if (!CanWriteToFile(localFileName))
            {
                // If the directory doesn't exist and we create it and can then write to the file, we're ok
                if (!AZ::IO::SystemFile::Exists(destFolder.c_str()))
                {
                    if (!AZ::IO::SystemFile::CreateDir(destFolder.c_str()))
                    {
                        return false;
                    }
                }
                else
                {
                    return false;
                }

                // Directory didn't exist, we successfully created it, lets see if it looks good now
                if (!CanWriteToFile(localFileName))
                {
                    return false;
                }
            }

            thisRequest.SetResponseStreamFactory([localFileName]() { return Aws::New<Aws::FStream>("TRANSFER", localFileName.c_str(), std::ios_base::out | std::ios_base::in | std::ios_base::binary | std::ios_base::trunc); });

            AddOutstandingRequest(bucketName, requestKey);
            AddRecentRequest(bucketName, requestKey);

            MARSHALL_AWS_BACKGROUND_REQUEST(S3, thisClient, GetObject, StaticDataTransferManager::HandleGetObjectResult, thisRequest, thisContext)
                
            return true;
        }

        bool StaticDataTransferManager::HasRecentRequest(const AZStd::string& bucketName, const AZStd::string& keyName) const
        {
            AZStd::lock_guard<AZStd::mutex> requestLock(m_recentRequestMutex);
            RecentRequestMap::const_iterator thisEntry = m_recentRequests.find(GetRequestString(bucketName, keyName));
            if (thisEntry != m_recentRequests.end())
            {
                std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - thisEntry->second).count() <= fileRequestRetryMax)
                {
                    return true;
                }
            }
            return false;
        }

        void StaticDataTransferManager::AddRecentRequest(const AZStd::string& bucketName, const AZStd::string& keyName)
        {
            AZStd::lock_guard<AZStd::mutex> requestLock(m_recentRequestMutex);
            m_recentRequests[GetRequestString(bucketName, keyName)] = std::chrono::system_clock::now();
        }

        bool StaticDataTransferManager::AddBucket(const AZStd::string& bucketName)
        {
            m_monitoredBuckets.insert(bucketName);
            return true;
        }

        bool StaticDataTransferManager::RemoveBucket(const AZStd::string& bucketName)
        {
            m_monitoredBuckets.erase(bucketName);
            return true;
        }

        bool StaticDataTransferManager::IsUpdateTimerSet() const
        {
            return false;
        }

        bool StaticDataTransferManager::SetUpdateTimer(int timerValue)
        {

            return true;
        }
    }
}