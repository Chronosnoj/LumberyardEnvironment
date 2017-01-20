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
#ifndef UTILITIES_ASSETBUILDERINFO_H
#define UTILITIES_ASSETBUILDERINFO_H
#pragma once

#include <functional>
#include <QVector>
#include <QLibrary>

#include <AzCore/std/base.h>

#include <AssetBuilderSDK/AssetBuilderSDK.h>

#include "native/assetprocessor.h"
#include "native/utilities/PlatformConfiguration.h"
#include "native/resourcecompiler/RCBuilder.h"
class FolderWatchCallbackEx;
class QCoreApplication;

namespace AssetProcessor
{
    //! Class to manage external module builders for the asset processor 
    class ExternalModuleAssetBuilderInfo
    {
    public:
        ExternalModuleAssetBuilderInfo(const QString& modulePath);
        virtual ~ExternalModuleAssetBuilderInfo();

        const QString& GetName() const;

        //! Perform a load of the external module, this is required before initialize.
        bool Load();

        //! Sanity check for the module's status
        bool IsLoaded() const;

        //! Perform the module initialization for the external builder
        void Initialize();

        //! Perform the necessary process of uninitializing an external builder
        void UnInitialize();

        //! Register a builder descriptor ID to track as part of this builders lifecycle managementg
        void RegisterBuilderDesc(const AZ::Uuid& builderDesc);

        //! Register a component descriptor to track as part of this builders lifecycle managementg
        void RegisterComponentDesc(AZ::ComponentDescriptor* descriptor);
    protected:
        AZStd::set<AZ::Uuid>    m_registeredBuilderDescriptorIDs;

        typedef void(* InitializeModuleFunction)(AZ::EnvironmentInstance sharedEnvironment);
        typedef void(* ModuleRegisterDescriptorsFunction)(void);
        typedef void(* ModuleAddComponentsFunction)(AZ::Entity* entity);
        typedef void(* UninitializeModuleFunction)(void);

        template<typename T>
        T ResolveModuleFunction(const char* functionName, QStringList& missingFunctionsList);

        InitializeModuleFunction m_initializeModuleFunction;
        ModuleRegisterDescriptorsFunction m_moduleRegisterDescriptorsFunction;
        ModuleAddComponentsFunction m_moduleAddComponentsFunction;
        UninitializeModuleFunction m_uninitializeModuleFunction;
        QVector<AZ::ComponentDescriptor*> m_componentDescriptorList;
        AZ::Entity* m_entity = nullptr;

        QString m_builderName;
        QString m_modulePath;
        QLibrary m_library;
    };


    //!This EBUS is used to send information from an internal builder to the AssetProcessor
    class AssetBuilderRegistrationBusTraits
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        typedef AZStd::recursive_mutex MutexType;

        virtual ~AssetBuilderRegistrationBusTraits() {}

        virtual void UnRegisterComponentDescriptor(const AZ::ComponentDescriptor* componentDescriptor) {}
        virtual void UnRegisterBuilderDescriptor(const AZ::Uuid& builderId) {}
    };

    typedef AZ::EBus<AssetBuilderRegistrationBusTraits> AssetBuilderRegistrationBus;
} // AssetProcessor
#endif //UTILITIES_ASSETBUILDERINFO_H
