
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
#ifndef ASSETBUILDERSDK_H
#define ASSETBUILDERSDK_H

#include <AzCore/std/string/string.h>
#include <AzCore/std/containers/vector.h>
#include "AssetBuilderBusses.h"

namespace AZ
{
    class ComponentDescriptor;
    class Entity;
}

namespace AssetBuilderSDK
{
    extern const char* const ErrorWindow; //Use this window name to log error messages.
    extern const char* const WarningWindow; //Use this window name to log warning messages.
    extern const char* const InfoWindow; //Use this window name to log info messages.

    //! This method is used for logging builder related messages/error
    //! Do not use this inside ProcessJob, use AZ_TracePrintF instead.  This is only for general messages about your builder, not for job-specific messages
    extern void BuilderLog(AZ::Uuid builderId, const char* message, ...);

    //! Enum used by the builder for sending platform info
    enum Platform
    {
        Platform_NONE = 0x00,
        Platform_PC   = 0x01,
        Platform_ES3  = 0x02,
        Platform_IOS  = 0x04,
        Platform_OSX  = 0x08
    };

    //! Map data structure to holder parameters that are passed into a job for ProcessJob requests.  
    //! These parameters can optionally be set during the create job function of the builder so that they are passed along
    //! to the ProcessJobFunction.  The values (key and value) are arbitrary and is up to the builder on how to use them
    typedef AZStd::unordered_map<AZ::u32, AZStd::string> JobParameterMap;

    //! Callback function type for creating jobs from job requests
    typedef AZStd::function<void(const CreateJobsRequest& request, CreateJobsResponse& response)> CreateJobFunction;

    //! Callback function type for processing jobs from process job requests
    typedef AZStd::function<void(const ProcessJobRequest& request, ProcessJobResponse& response)> ProcessJobFunction;

    //! Structure defining the type of pattern to use to apply
    struct AssetBuilderPattern
    {
        enum PatternType
        {
            //! The pattern is a file wildcard pattern (glob)
            Wildcard,
            //! The pattern is a regular expression pattern
            Regex
        };

        AssetBuilderPattern() = default;
        AssetBuilderPattern(const AssetBuilderPattern& src) = default;
        AssetBuilderPattern(const AZStd::string& pattern, PatternType type)
            : m_pattern(pattern)
            , m_type(type)
        {
        }

        AZStd::string m_pattern;
        PatternType   m_type;
    };

    //!Information that builders will send to the assetprocessor
    struct AssetBuilderDesc
    {
        //! The name of the Builder
        AZStd::string m_name;

        //! The collection of asset builder patterns that the builder will use to 
        //! determine if a file will be processed by that builder
        AZStd::vector<AssetBuilderPattern>  m_patterns;

        //! The builder unique ID
        AZ::Uuid m_busId;

        //! Changing this version number will cause all your assets to be re-submitted to the builder for job creation and rebuilding.
        int m_version = 0;

        //! The required create job function callback that the asset processor will call during the job creation phase
        CreateJobFunction m_createJobFunction;  
        //! The required process job function callback that the asset processor will call during the job processing phase
        ProcessJobFunction m_processJobFunction;
    };

    //! Structure that will be used to store Source File Dependency
    struct SourceFileDependency
    {
        AZStd::string m_absolutePathToFile; //! contains the actual absolute path to the file, including watch folder.

        SourceFileDependency() {};
        SourceFileDependency(const AZStd::string& absolutePathToFile)
            : m_absolutePathToFile(absolutePathToFile)
        {
        }

        SourceFileDependency(AZStd::string&& absolutePathToFile)
            : m_absolutePathToFile(AZStd::move(absolutePathToFile))
        {
        }

        SourceFileDependency(const SourceFileDependency& other) = default;

        SourceFileDependency(SourceFileDependency&& other)
        {
            *this = AZStd::move(other);
        }

        SourceFileDependency& operator=(const SourceFileDependency& other)
        {
            if (this != &other)
            {
                m_absolutePathToFile = other.m_absolutePathToFile;
            }
            return *this;
        }

        SourceFileDependency& operator=(SourceFileDependency&& other)
        {
            if (this != &other)
            {
                m_absolutePathToFile = AZStd::move(other.m_absolutePathToFile);
            }
            return *this;
        }
    };

    //! JobDescriptor is used by the builder to store job related information
    struct JobDescriptor
    {
        //! Any additional info that should be taken into account during fingerprinting for this job
        AZStd::string m_additionalFingerprintInfo;  

        //! The target platform(s) that this job is configured for
        int m_platform = Platform_NONE;             

        //! Job specific key, e.g. TIFF Job, etc
        AZStd::string m_jobKey;

        //! Flag to determine if this is a critical job or not.  Critical jobs are given the higher priority in the processing queue than non-critical jobs
        bool m_critical = false;                    

        //! Priority value for the jobs within the job queue.  If less than zero, than the priority of this job is not considered or or is lowest priority.  
        //! If zero or greater, the value is prioritized by this number (the higher the number, the higher priority).  Note: priorities are set within critical 
        //! and non-critical job separately.
        int m_priority = -1;

        //! Any builder specific parameters to pass to the Process Job Request
        JobParameterMap m_jobParameters;            

        //! Flag to determine whether we need to check the input file for exclusive lock before we process the job 
        bool m_checkExclusiveLock = false;

        AZStd::vector<SourceFileDependency> m_SourceFileDependencyList;

        JobDescriptor(AZStd::string additionalFingerprintInfo, int platform, AZStd::string jobKey)
            : m_additionalFingerprintInfo(additionalFingerprintInfo)
            , m_platform(platform)
            , m_jobKey(jobKey)
        {
        }

        JobDescriptor() {};
    };

    //! CreateJobsRequest contains input job data that will be send by the AssetProcessor to the builder for creating jobs
    struct CreateJobsRequest
    {
        //! The builder id to identify which builder will process this job request
        AZ::Uuid m_builderid; // builder id

        //! m_watchFolder contains the subfolder that the sourceFile came from, out of all the folders being watched by the Asset Processor.  
        //! If you combine the Watch Folder with the Source File (m_sourceFile), you will result in the full absolute path to the file.
        AZStd::string m_watchFolder;

        //! The source file path that is relative to the watch folder (m_watchFolder)
        AZStd::string m_sourceFile; 

        //! Platform flags informs the builder which platforms the AssetProcessor is interested in.
        int m_platformFlags; 

        CreateJobsRequest()
        {
        }

        CreateJobsRequest(AZ::Uuid builderid, AZStd::string sourceFile, AZStd::string watchFolder, int platformFlags)
            : m_builderid(builderid)
            , m_sourceFile(sourceFile)
            , m_watchFolder(watchFolder)
            , m_platformFlags(platformFlags)
        {
        }
    };

    //! Possible result codes from CreateJobs requests
    enum class CreateJobsResultCode
    {
        //! Jobs were created successfully
        Success,
        //! Jobs failed to be created
        Failed,
        //! The builder is in the process of shutting down
        ShuttingDown
    };

    //! CreateJobsResponse contains job data that will be send by the builder to the assetProcessor in response to CreateJobsRequest
    struct CreateJobsResponse
    {
        CreateJobsResultCode         m_result = CreateJobsResultCode::Failed;   // The result code from the create jobs request
        AZStd::vector<JobDescriptor> m_createJobOutputs;
    };

    //! JobProduct is used by the builder to store job product information
    struct JobProduct
    {
        AZStd::string m_productFileName; // relative product file path

        JobProduct() {};
        JobProduct(const AZStd::string& productName)
            : m_productFileName(productName)
        {
        }

        JobProduct(AZStd::string&& productName)
            : m_productFileName(AZStd::move(productName))
        {
        }
        
        JobProduct(const JobProduct& other) = default;

        JobProduct(JobProduct&& other)
        {
            *this = AZStd::move(other);
        }

        JobProduct& operator=(JobProduct&& other)
        {
            if (this != &other)
            {
                m_productFileName = AZStd::move(other.m_productFileName);
            }
            return *this;
        }

        JobProduct& operator=(const JobProduct& other) = default;
    };

    //! ProcessJobRequest contains input job data that will be send by the AssetProcessor to the builder for processing jobs
    struct ProcessJobRequest
    {
        AZStd::string m_sourceFile; // relative source file name
        AZStd::string m_watchFolder; // watch folder for this source file
        AZStd::string m_fullPath; // full source file name
        AZ::Uuid m_builderId; // builder id
        JobDescriptor m_jobDescription; // job descriptor for this job.  Note that this still contains the job parameters from when you emitted it during CreateJobs
        AZStd::string m_tempDirPath; // temp directory that the builder should use to create job outputs for this job request
        ProcessJobRequest() {};
    };


    enum ProcessJobResultCode
    {
        ProcessJobResult_Success = 0,
        ProcessJobResult_Failed = 1,
        ProcessJobResult_Crashed = 2,
        ProcessJobResult_Cancelled = 3
    };

        //! ProcessJobResponse contains job data that will be send by the builder to the assetProcessor in response to ProcessJobRequest
    struct ProcessJobResponse
    {
        ProcessJobResultCode m_resultCode = ProcessJobResult_Success;
        AZStd::vector<JobProduct> m_outputProducts;

        ProcessJobResponse() = default;
        ProcessJobResponse(const ProcessJobResponse& other) = default;
        
        ProcessJobResponse(ProcessJobResponse&& other)
        {
            *this = AZStd::move(other);
        }
        
        ProcessJobResponse& operator=(const ProcessJobResponse& other)
        {
            if (this != &other)
            {
                m_resultCode = other.m_resultCode;
                m_outputProducts = other.m_outputProducts;
            }
            return *this;
        }

        
        ProcessJobResponse& operator=(ProcessJobResponse&& other)
        {
            if (this != &other)
            {
                m_resultCode = other.m_resultCode;
                m_outputProducts.swap(other.m_outputProducts);
            }
            return *this;
        }

    };
}

//! This macro should be used by every AssetBuilder to register itself,
//! AssetProcessor uses these exported function to identify whether a dll is an Assetbuilder or not
//! If you want something highly custom you can do these entry points yourself instead of using the macro.
#define REGISTER_ASSETBUILDER                                                      \
    extern void BuilderOnInit();                                                   \
    extern void BuilderDestroy();                                                  \
    extern void BuilderRegisterDescriptors();                                      \
    extern void BuilderAddComponents(AZ::Entity* entity);                          \
    extern "C"                                                                     \
    {                                                                              \
    AZ_DLL_EXPORT int IsAssetBuilder()                                             \
    {                                                                              \
        return 0;                                                                  \
    }                                                                              \
                                                                                   \
    AZ_DLL_EXPORT void InitializeModule(AZ::EnvironmentInstance sharedEnvironment) \
    {                                                                              \
        AZ::Environment::Attach(sharedEnvironment);                                \
        BuilderOnInit();                                                           \
    }                                                                              \
                                                                                   \
    AZ_DLL_EXPORT void UninitializeModule()                                        \
    {                                                                              \
        BuilderDestroy();                                                          \
        AZ::Environment::Detach();                                                 \
    }                                                                              \
                                                                                   \
    AZ_DLL_EXPORT void ModuleRegisterDescriptors()                                 \
    {                                                                              \
        BuilderRegisterDescriptors();                                              \
    }                                                                              \
                                                                                   \
    AZ_DLL_EXPORT void ModuleAddComponents(AZ::Entity* entity)                     \
    {                                                                              \
        BuilderAddComponents(entity);                                              \
    }                                                                              \
    }

#endif //ASSETBUILDER_H