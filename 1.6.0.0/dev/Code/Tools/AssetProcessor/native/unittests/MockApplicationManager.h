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

#ifndef MOCK_APPLICATION_MANAGER_H
#define MOCK_APPLICATION_MANAGER_H

#include "../utilities/ApplicationManagerAPI.h"
#include "native/AssetManager/assetProcessorManager.h"
#include "native/utilities/PlatformConfiguration.h"
#include "native/utilities/assetUtils.h"
#include "native/resourcecompiler/RCBuilder.h"
namespace AssetProcessor
{

    class InternalMockBuilder
        : public InternalRecognizerBasedBuilder
    {
        friend class MockApplicationManager;
    public:
        static InternalAssetRecognizer::PlatformFilter s_platformFilter;

        //////////////////////////////////////////////////////////////////////////
        InternalMockBuilder(const char* builderName, const AZ::Uuid& builderID);

        virtual ~InternalMockBuilder();

        using InternalRecognizerBasedBuilder::Initialize;
        bool Initialize(const AssetRecognizer& rec, AssetBuilderSDK::AssetBuilderDesc& desc);

        void CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response);
        void ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response);

        //////////////////////////////////////////////////////////////////////////
        //! InternalRecognizerBasedBuilder
        AssetBuilderSDK::AssetBuilderDesc CreateBuilderDesc(const AZStd::vector<AssetBuilderSDK::AssetBuilderPattern>& builderPatterns);


        void ResetCreateJobCalls();
        int GetCreateJobCalls();
        const AssetBuilderSDK::CreateJobsResponse& GetLastCreateJobResponse();
        const AssetBuilderSDK::CreateJobsRequest& GetLastCreateJobRequest();

    protected:
        AssetBuilderSDK::AssetBuilderPattern    m_builderPattern;
        int m_createJobCalls;
        AssetBuilderSDK::CreateJobsRequest  m_lastCreateJobRequest;
        AssetBuilderSDK::CreateJobsResponse m_lastCreateJobResponse;
    };

    class MockApplicationManager
        : public AssetProcessor::AssetBuilderInfoBus::Handler
    {
    public:
        MockApplicationManager() : m_getMatchingBuildersInfoFunctionCalls(0) {}
        bool RegisterAssetRecognizerAsBuilder(const AssetRecognizer& rec, AZ::Uuid* optionalUuid = nullptr);
        bool UnRegisterAssetRecognizerAsBuilder(const char* name);
        void UnRegisterAllBuilders();

        void GetMatchingBuildersInfo(const AZStd::string& assetPath, AssetProcessor::BuilderInfoList& builderInfoList) override;

        void ResetMatchingBuildersInfoFunctionCalls();
        int GetMatchingBuildersInfoFunctionCalls();
        void ResetMockBuilderCreateJobCalls();
        int GetMockBuilderCreateJobCalls();

        bool GetBuilderByID(const AZStd::string& builderName, AZStd::shared_ptr<InternalMockBuilder> & builder);
    protected:
        AZStd::unordered_map< AZStd::string, AZStd::shared_ptr<InternalMockBuilder> > m_internalBuilders;
        AZStd::unordered_map<AZ::Uuid, AssetBuilderSDK::AssetBuilderDesc> m_builderDescMap;
        AZStd::list<AssetUtilities::BuilderFilePatternMatcher> m_matcherBuilderPatterns;
        int m_getMatchingBuildersInfoFunctionCalls;
    };

}

#endif //MOCK_APPLICATION_MANAGER_H