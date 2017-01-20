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

#include <QElapsedTimer>
#include <QCoreApplication>

#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/smart_ptr/make_shared.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <AssetBuilderSDK/AssetBuilderBusses.h>

#include "native/resourcecompiler/rccontroller.h"
#include "native/resourcecompiler/rcjob.h"
#include "native/utilities/assetUtils.h"
#include "native/utilities/ApplicationManagerAPI.h"
#include "native/utilities/AssetBuilderInfo.h"
#include "MockApplicationManager.h"


namespace AssetProcessor
{
    // The filter callback for RC Recognizer Platforms
    InternalAssetRecognizer::PlatformFilter InternalMockBuilder::s_platformFilter =
        [](const AssetPlatformSpec& srcPlatSpec)
        {
            return true;
        };

    InternalMockBuilder::InternalMockBuilder(const char* builderName, const AZ::Uuid& builderID)
        : InternalRecognizerBasedBuilder(
            builderName,
            builderID,
            InternalMockBuilder::s_platformFilter)
            , m_createJobCalls(0)
    {
    }

    InternalMockBuilder::~InternalMockBuilder()
    {
    }

    bool InternalMockBuilder::Initialize(const AssetRecognizer& assetRecognizer, AssetBuilderSDK::AssetBuilderDesc& desc)
    {
        AZStd::vector<AssetBuilderSDK::AssetBuilderPattern>  builderPatterns;

        // Only accept recognizers that match the current platform filter
        InternalAssetRecognizer* internalAssetRecognizer = new InternalAssetRecognizer(assetRecognizer, m_platformFilter);
        if (internalAssetRecognizer->m_platformSpecsByPlatform.size() == 0)
        {
            delete internalAssetRecognizer;
            return false;
        }

        m_assetRecognizerDictionary[internalAssetRecognizer->m_paramID] = internalAssetRecognizer;

        m_builderPattern = assetRecognizer.m_patternMatcher.GetBuilderPattern();

        // Register the builder desc
        desc = CreateBuilderDesc(builderPatterns);
        return true;
    }


    AssetBuilderSDK::AssetBuilderDesc InternalMockBuilder::CreateBuilderDesc(const AZStd::vector<AssetBuilderSDK::AssetBuilderPattern>& builderPatterns)
    {
        AssetBuilderSDK::AssetBuilderDesc   builderDesc;

        builderDesc.m_name = this->m_name;
        builderDesc.m_patterns = builderPatterns;
        builderDesc.m_busId = this->m_builderUuid;
        builderDesc.m_createJobFunction = AZStd::bind(&InternalMockBuilder::CreateJobs, this, AZStd::placeholders::_1, AZStd::placeholders::_2);
        builderDesc.m_processJobFunction = AZStd::bind(&InternalMockBuilder::ProcessJob, this, AZStd::placeholders::_1, AZStd::placeholders::_2);
        return builderDesc;
    }

    void InternalMockBuilder::CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response)
    {
        m_lastCreateJobRequest = request;
        if (m_isShuttingDown)
        {
            response.m_result = AssetBuilderSDK::CreateJobsResultCode::ShuttingDown;
            return;
        }

        response.m_result = AssetBuilderSDK::CreateJobsResultCode::Failed;

        AZStd::string fullPath;
        if (!AzFramework::StringFunc::Path::ConstructFull(request.m_watchFolder.c_str(), request.m_sourceFile.c_str(), fullPath, true))
        {
            AssetBuilderSDK::BuilderLog(this->m_builderUuid, "Cannot construct a full path from folder '%s' and file '%s'", request.m_watchFolder.c_str(), request.m_sourceFile.c_str());
            return;
        }

        // first, is it an input or is it in the cache folder?
        QString normalizedPath = QString(fullPath.c_str());
        QString scanFolderName = QString(request.m_watchFolder.c_str());
        QString relativePathToFile = QString(request.m_sourceFile.c_str());


        // Locate recognizers that match the file
        InternalRecognizerPointerContainer  recognizers;
        if (!GetMatchingRecognizers(request.m_platformFlags, request.m_sourceFile.c_str(), recognizers))
        {
            AssetBuilderSDK::BuilderLog(this->m_builderUuid, "Cannot find recognizer for %s.", request.m_sourceFile.c_str());
            return;
        }

        m_createJobCalls++;

        // First pass
        for (const InternalAssetRecognizer* recognizer : recognizers)
        {
            // Iterate through the platform specific specs and apply the ones that match the platform flag
            for (auto iterPlatformSpec = recognizer->m_platformSpecsByPlatform.cbegin();
                 iterPlatformSpec != recognizer->m_platformSpecsByPlatform.cend();
                 iterPlatformSpec++)
            {
                if (iterPlatformSpec.key() & request.m_platformFlags)
                {
                    AssetBuilderSDK::JobDescriptor descriptor;
                    descriptor.m_jobKey = recognizer->m_name.toUtf8().data();
                    descriptor.m_platform = iterPlatformSpec.key();
                    descriptor.m_critical = true;   // All copy jobs are critical
                    descriptor.m_checkExclusiveLock = recognizer->m_testLockSource;

                    QString extraInformationForFingerprinting;
                    extraInformationForFingerprinting.append(iterPlatformSpec->m_extraRCParams);
                    extraInformationForFingerprinting.append(recognizer->m_version);
                    descriptor.m_additionalFingerprintInfo = AZStd::string(extraInformationForFingerprinting.toUtf8().data());

                    // Job Parameter Value can be any arbitrary string since we are relying on the key to lookup
                    // the parameter in the process job
                    descriptor.m_jobParameters[recognizer->m_paramID] = descriptor.m_jobKey;
                    response.m_createJobOutputs.push_back(descriptor);
                    response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
                }
            }
        }
        m_lastCreateJobResponse = response;
    }

    void InternalMockBuilder::ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response)
    {
        if (m_isShuttingDown)
        {
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            return;
        }

        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;

        if (request.m_jobDescription.m_jobParameters.empty())
        {
            AZ_TracePrintf(AssetProcessor::ConsoleChannel,
                           "Job request for for %s in builder %s missing job parameters.",
                           request.m_sourceFile.c_str(),
                           this->GetBuilderName().c_str());
            return;
        }

        for (auto jobParam = request.m_jobDescription.m_jobParameters.begin();
             jobParam != request.m_jobDescription.m_jobParameters.end();
             jobParam++)
        {
            if (this->m_assetRecognizerDictionary.find(jobParam->first) == this->m_assetRecognizerDictionary.end())
            {
                AZ_TracePrintf(AssetProcessor::ConsoleChannel,
                               "Job request for for %s in builder %s has invalid job parameter (%ld).",
                               request.m_sourceFile.c_str(),
                               this->GetBuilderName().c_str(),
                               jobParam->first);
                continue;
            }
            InternalAssetRecognizer* assetRecognizer = this->m_assetRecognizerDictionary[jobParam->first];

            if (assetRecognizer->m_platformSpecsByPlatform.contains(request.m_jobDescription.m_platform))
            {
                QString platformParam = (assetRecognizer->m_platformSpecsByPlatform[request.m_jobDescription.m_platform].m_extraRCParams);
                AZ_Assert((QString::compare(platformParam, "copy", Qt::CaseInsensitive) == 0), "Illegal param for copy task recognizer %s", assetRecognizer->m_name.toUtf8().data());
                response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(request.m_fullPath));
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
            }
        }
    }

    void InternalMockBuilder::ResetCreateJobCalls()
    {
        m_createJobCalls = 0;
    }
    int InternalMockBuilder::GetCreateJobCalls()
    {
        return m_createJobCalls;
    }

    const AssetBuilderSDK::CreateJobsResponse& InternalMockBuilder::GetLastCreateJobResponse()
    {
        return m_lastCreateJobResponse;
    }
    const AssetBuilderSDK::CreateJobsRequest& InternalMockBuilder::GetLastCreateJobRequest()
    {
        return m_lastCreateJobRequest;
    }

    bool MockApplicationManager::RegisterAssetRecognizerAsBuilder(const AssetProcessor::AssetRecognizer& rec, AZ::Uuid* optionalUuid)
    {
        AZ::Uuid newUUid = optionalUuid ? (*optionalUuid) : AZ::Uuid::CreateRandom();

        AZStd::shared_ptr<InternalMockBuilder>   builder =
            AZStd::make_shared<AssetProcessor::InternalMockBuilder>(rec.m_name.toUtf8().data(),newUUid);

        AssetBuilderSDK::AssetBuilderDesc   builderDesc;
        if (!builder->Initialize(rec, builderDesc)) 
        {
            return false;
        }

        m_internalBuilders[AZStd::string(builder->GetBuilderName())] = builder;
        m_builderDescMap[builder->GetUuid()] = builderDesc;

        AssetUtilities::BuilderFilePatternMatcher patternMatcher(builder->m_builderPattern, builderDesc.m_busId);
        m_matcherBuilderPatterns.push_back(patternMatcher);
        return true;
    }

    bool MockApplicationManager::UnRegisterAssetRecognizerAsBuilder(const char* name)
    {
        AZStd::string key(name);

        if (m_internalBuilders.find(key) != m_internalBuilders.end())
        {
            // Find the builder and remove the builder desc map and build patterns
            AZStd::shared_ptr<InternalRecognizerBasedBuilder>   builder = m_internalBuilders[key];
            m_builderDescMap.erase(builder->GetUuid());

            // Remove from the matcher builder pattern
            for (auto remover = this->m_matcherBuilderPatterns.begin();
                 remover != this->m_matcherBuilderPatterns.end();
                 remover++)
            {
                if (remover->GetBuilderDescID() == builder->GetUuid()) {
                    auto deleteIter = remover;
                    remover++;
                    this->m_matcherBuilderPatterns.erase(deleteIter);
                }
            }
            m_internalBuilders.erase(key);
            return true;
        }
        else
        {
            return false;
        }
    }

    void MockApplicationManager::UnRegisterAllBuilders()
    {
        AZStd::list<AZStd::string> registeredBuilderNames;
        for (auto builderIter = m_internalBuilders.begin();
             builderIter != m_internalBuilders.end();
             builderIter++)
        {
            registeredBuilderNames.push_back(builderIter->first);
        }
        for (auto builderName : registeredBuilderNames)
        {
            UnRegisterAssetRecognizerAsBuilder(builderName.c_str());
        }
    }

    void MockApplicationManager::GetMatchingBuildersInfo(const AZStd::string& assetPath, AssetProcessor::BuilderInfoList& builderInfoList)
    {
        AZStd::set<AZ::Uuid>  uniqueBuilderDescIDs;
        m_getMatchingBuildersInfoFunctionCalls++;
        for (AssetUtilities::BuilderFilePatternMatcher& matcherPair : m_matcherBuilderPatterns)
        {
            if (uniqueBuilderDescIDs.find(matcherPair.GetBuilderDescID()) != uniqueBuilderDescIDs.end())
            {
                continue;
            }
            if (matcherPair.MatchesPath(assetPath))
            {
                const AssetBuilderSDK::AssetBuilderDesc& builderDesc = m_builderDescMap[matcherPair.GetBuilderDescID()];
                uniqueBuilderDescIDs.insert(matcherPair.GetBuilderDescID());
                builderInfoList.push_back(builderDesc);
            }
        }
    };

    bool MockApplicationManager::GetBuilderByID(const AZStd::string& builderName, AZStd::shared_ptr<InternalMockBuilder> & builder)
    {
        if (m_internalBuilders.find(builderName) == m_internalBuilders.end())
        {
            return false;
        }
        builder = m_internalBuilders[builderName];
        return true;
    }

    void MockApplicationManager::ResetMatchingBuildersInfoFunctionCalls()
    {
        m_getMatchingBuildersInfoFunctionCalls = 0;
    }

    int MockApplicationManager::GetMatchingBuildersInfoFunctionCalls()
    {
        return m_getMatchingBuildersInfoFunctionCalls;
    }

    void MockApplicationManager::ResetMockBuilderCreateJobCalls()
    {
        for (auto builderIter = m_internalBuilders.begin();
             builderIter != m_internalBuilders.end();
             builderIter++)
        {
            builderIter->second->ResetCreateJobCalls();
        }
    }

    int MockApplicationManager::GetMockBuilderCreateJobCalls()
    {
        int jobCalls = 0;
        for (auto builderIter = m_internalBuilders.begin();
             builderIter != m_internalBuilders.end();
             builderIter++)
        {
            jobCalls += builderIter->second->GetCreateJobCalls();
        }
        return jobCalls;
    }

}

