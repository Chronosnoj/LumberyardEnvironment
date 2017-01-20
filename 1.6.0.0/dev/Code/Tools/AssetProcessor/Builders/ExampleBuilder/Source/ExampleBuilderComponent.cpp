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

#include "ExampleBuilderComponent.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzCore/Debug/Trace.h>

namespace ExampleBuilder
{
    BuilderPluginComponent::BuilderPluginComponent()
    {
        // AZ Components should only initialize their members to null and empty in constructor
        // after construction, they may be deserialized from file.
    }

    BuilderPluginComponent::~BuilderPluginComponent()
    {
    }

    void BuilderPluginComponent::Init()
    {
        // init is where you'd actually allocate memory or create objects since it happens after deserialization.
    }

    void BuilderPluginComponent::Activate()
    {
        // activate is where you'd perform registration with other objects and systems.

        // since we want to register our builder, we do that here:
        AssetBuilderSDK::AssetBuilderDesc builderDescriptor;
        builderDescriptor.m_name = "Example Worker Builder";
        builderDescriptor.m_patterns.push_back(AssetBuilderSDK::AssetBuilderPattern("*.example", AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard));
        builderDescriptor.m_busId = ExampleBuilderWorker::GetUUID();
        builderDescriptor.m_createJobFunction = AZStd::bind(&ExampleBuilderWorker::CreateJobs, &m_exampleBuilder, AZStd::placeholders::_1, AZStd::placeholders::_2);
        builderDescriptor.m_processJobFunction = AZStd::bind(&ExampleBuilderWorker::ProcessJob, &m_exampleBuilder, AZStd::placeholders::_1, AZStd::placeholders::_2);
        m_exampleBuilder.BusConnect(builderDescriptor.m_busId);

        EBUS_EVENT(AssetBuilderSDK::AssetBuilderBus, RegisterBuilderInformation, builderDescriptor);
    }

    void BuilderPluginComponent::Deactivate()
    {
        m_exampleBuilder.BusDisconnect();
    }

    void BuilderPluginComponent::Reflect(AZ::ReflectContext* context)
    {
        // components also get Reflect called automatically
        // this is your opportunity to perform static reflection or type registration of any types you want the serializer to know about
    }

    
    ExampleBuilderWorker::ExampleBuilderWorker()
    {
    }
    ExampleBuilderWorker::~ExampleBuilderWorker()
    {
    }

    void ExampleBuilderWorker::ShutDown()
    {
        // it is important to note that this will be called on a different thread than your process job thread
        m_isShuttingDown = true;
    }

    // this happens early on in the file scanning pass
    // this function should consistently always create the same jobs, and should do no checking whether the job is up to date or not - just be consistent.
    void ExampleBuilderWorker::CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response)
    {
        // Creating one job descriptor for pc platform only.  Normally you'd make a job per platform you can make assets for.
        if (request.m_platformFlags & AssetBuilderSDK::Platform_PC)
        {
            AssetBuilderSDK::JobDescriptor descriptor;
            descriptor.m_jobKey = "Compile Example";
            descriptor.m_platform = AssetBuilderSDK::Platform_PC;
            
            // you can also place whatever parameters you want to save for later into this map:
            descriptor.m_jobParameters[AZ_CRC("hello")] = "World";
            response.m_createJobOutputs.push_back(descriptor);

            response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
        }
    }

    // later on, this function will be called for jobs that actually need doing.
    // the request will contain the CreateJobResponse you constructed earlier, including any keys and values you placed into the hash table
    void ExampleBuilderWorker::ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response)
    {
        AZ_TracePrintf(AssetBuilderSDK::InfoWindow, "Starting Job.");
        AZStd::string fileName;
        AzFramework::StringFunc::Path::GetFullFileName(request.m_fullPath.c_str(), fileName);
        AzFramework::StringFunc::Path::ReplaceExtension(fileName, "example1");
        AZStd::string destPath;

        // you should do all your work inside the tempDirPath.
        // don't write outside this path
        AzFramework::StringFunc::Path::ConstructFull(request.m_tempDirPath.c_str(), fileName.c_str(), destPath, true);

        // use AZ_TracePrintF to communicate job details.  The logging system will automatically file the text under the appropriate log file and category.

        AZ::IO::LocalFileIO fileIO;
        if (!m_isShuttingDown && fileIO.Copy(request.m_fullPath.c_str(), destPath.c_str()) == AZ::IO::ResultCode::Success)
        {
            // if you succeed in building assets into your temp dir, you should push them back into the response's product list
            // Assets you created in your temp path can be specified using paths relative to the temp path, since that is assumed where you're writing stuff.
            AZStd::string relPath = destPath;
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;

            AssetBuilderSDK::JobProduct jobProduct(fileName);
            response.m_outputProducts.push_back(jobProduct);
        }
        else
        {
            if (m_isShuttingDown)
            {
                AZ_TracePrintf(AssetBuilderSDK::ErrorWindow, "Cancelled job %s because shutdown was requested", request.m_fullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            }
            else
            {
                AZ_TracePrintf(AssetBuilderSDK::ErrorWindow, "Error during processing job %s.", request.m_fullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            }
        }
    }

    AZ::Uuid ExampleBuilderWorker::GetUUID()
    {
        return AZ::Uuid::CreateString("C163F950-BF25-4D60-90D7-8E181E25A9EA");
    }
}
