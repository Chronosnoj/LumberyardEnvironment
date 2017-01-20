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
#ifndef ASSETBUILDERUTILEBUSHELPER_H
#define ASSETBUILDERUTILEBUSHELPER_H

#include <AzCore/EBus/EBus.h>
#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/string/string.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Math/Uuid.h>

namespace AssetBuilderSDK
{
    struct CreateJobsRequest;
    struct CreateJobsResponse;
    struct ProcessJobRequest;
    struct ProcessJobResponse;
    struct AssetBuilderDesc;

    //! This EBUS is used to send commands from the assetprocessor to the builder
    //! Every new builder should implement a listener for this bus and implement the CreateJobs, Shutdown and ProcessJobs functions.
    class AssetBuilderCommandBusTraits
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        typedef AZ::Uuid BusIdType;

        virtual ~AssetBuilderCommandBusTraits() {};

        //! Shutdown() REQUIRED - Handle the message indicating shutdown.  Cancel all your tasks and get them stopped ASAP
        //! this message will come in from a different thread than your ProcessJob() thread.
        //! failure to terminate promptly can cause a hangup on AP shutdown and restart.
        virtual void ShutDown() = 0;
    };

    typedef AZ::EBus<AssetBuilderCommandBusTraits> AssetBuilderCommandBus;

    //!This EBUS is used to send information from the builder to the AssetProcessor
    class AssetBuilderBusTraits
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        typedef AZStd::recursive_mutex MutexType;

        virtual ~AssetBuilderBusTraits() {}

        // Use this function to send AssetBuilderDesc info to the assetprocessor
        virtual void RegisterBuilderInformation(const AssetBuilderDesc& builderDesc) {}

        // Use this function to register all the component descriptors
        virtual void RegisterComponentDescriptor(AZ::ComponentDescriptor* descriptor) {}

        // Log functions to report general builder related messages/error.
        virtual void BuilderLog(const AZ::Uuid& builderId, const char* message, ...) {}
        virtual void BuilderLogV(const AZ::Uuid& builderId, const char* message, va_list list) {}
    };

    typedef AZ::EBus<AssetBuilderBusTraits> AssetBuilderBus;
}


#endif //ASSETBUILDERUTILEBUSHELPER_H