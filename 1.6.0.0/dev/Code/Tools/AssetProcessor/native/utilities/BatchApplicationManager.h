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
#ifndef BATCHAPPLICATIONMANAGER_H
#define BATCHAPPLICATIONMANAGER_H

#include <memory>
#include <AzCore/std/string/string.h>
#include <AzToolsFramework/API/AssetDatabaseBus.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/containers/vector.h>


#include "native/FileWatcher/FileWatcher.h"
#include "native/resourcecompiler/RCBuilder.h"
#include "native/utilities/ApplicationManager.h"
#include "native/utilities/assetUtils.h"

#include "AssetBuilderInfo.h"

namespace AssetProcessor
{
    class AssetProcessorManager;
    class PlatformConfiguration;
    class AssetScanner;
    class RCController;
    class AssetCatalog;
    class InternalAssetBuilderInfo;
}

class FolderWatchCallbackEx;
//! This class is the Application manager for Batch Mode


class BatchApplicationManager
    : public ApplicationManager
    , public AssetBuilderSDK::AssetBuilderBus::Handler
    , public AssetProcessor::AssetBuilderInfoBus::Handler
    , public AssetProcessor::AssetBuilderRegistrationBus::Handler
    , protected AzToolsFramework::AssetDatabase::AssetDatabaseRequests::Bus::Handler
{
    Q_OBJECT
public:
    explicit BatchApplicationManager(int argc, char** argv, QObject* parent = 0);
    virtual ~BatchApplicationManager();
    ApplicationManager::BeforeRunStatus BeforeRun() override;
    void Destroy() override;
    bool Run() override;
    bool Activate() override;

    AssetProcessor::PlatformConfiguration* GetPlatformConfiguration() const;

    AssetProcessor::AssetProcessorManager* GetAssetProcessorManager() const;

    AssetProcessor::AssetScanner* GetAssetScanner() const;

    AssetProcessor::RCController* GetRCController() const;

    int ProcessedAssetCount() const;
    int FailedAssetsCount() const;
    void ResetProcessedAssetCount();
    void ResetFailedAssetCount();

    //! AssetBuilderSDK::AssetBuilderBus Interface
    void RegisterBuilderInformation(const AssetBuilderSDK::AssetBuilderDesc& builderDesc) override;
    void RegisterComponentDescriptor(AZ::ComponentDescriptor* descriptor) override;
    void BuilderLog(const AZ::Uuid& builderId, const char* message, ...) override;
    void BuilderLogV(const AZ::Uuid& builderId, const char* message, va_list list) override;

    //! AssetBuilderSDK::InternalAssetBuilderBus Interface
    void UnRegisterComponentDescriptor(const AZ::ComponentDescriptor* componentDescriptor) override;
    void UnRegisterBuilderDescriptor(const AZ::Uuid& builderId) override;

    //! AssetProcessor::AssetBuilderInfoBus Interface
    void GetMatchingBuildersInfo(const AZStd::string& assetPath, AssetProcessor::BuilderInfoList& builderInfoList);

protected:
    virtual void InitAssetProcessorManager();//Deletion of assetProcessor Mananger will be handled by the ThreadController
    virtual void InitAssetCatalog();//Deletion of AssetCatalog will be handled when the ThreadController is deleted by the base ApplicationManager
    virtual void InitRCController();
    virtual void DestroyRCController();
    virtual void InitAssetScanner();
    virtual void DestroyAssetScanner();
    virtual void InitPlatformConfiguration();
    virtual void DestroyPlatformConfiguration();
    virtual void InitFileMonitor();
    virtual void DestroyFileMonitor();
    void CreateQtApplication() override;

    void InitializeInternalBuilders();
    bool InitializeExternalBuilders();

    // IMPLEMENTATION OF -------------- AzToolsFramework::AssetDatabase::AssetDatabaseRequests::Bus::Listener
    virtual void GetDatabaseLocation(AZStd::string& result) override;
    // ------------------------------------------------------------

    AssetProcessor::AssetCatalog* GetAssetCatalog() const { return m_assetCatalog; }

private Q_SLOTS:
    void CheckForIdle();

private:
#ifdef UNIT_TEST
    bool RunUnitTests();
#endif

    int m_processedAssetCount = 0;
    int m_failedAssetsCount = 0;

    AZStd::vector<AZStd::unique_ptr<FolderWatchCallbackEx> > m_folderWatches;
    FileWatcher m_fileWatcher;
    AZStd::vector<int> m_watchHandles;
    AssetProcessor::PlatformConfiguration* m_platformConfiguration = nullptr;
    AssetProcessor::AssetProcessorManager* m_assetProcessorManager = nullptr;
    AssetProcessor::AssetCatalog* m_assetCatalog = nullptr;
    AssetProcessor::AssetScanner* m_assetScanner = nullptr;
    AssetProcessor::RCController* m_rcController = nullptr;

    // Collection of all the internal builders
    AZStd::list< AZStd::shared_ptr<AssetProcessor::InternalRecognizerBasedBuilder> > m_internalBuilders;

    // Builder description map based on the builder id
    AZStd::unordered_map<AZ::Uuid, AssetBuilderSDK::AssetBuilderDesc> m_builderDescMap;

    // Lookup for builder ids based on the name.  The builder name must be unique
    AZStd::unordered_map<AZStd::string, AZ::Uuid> m_builderNameToId;

    // Builder pattern matchers to used to locate the builder descriptors that match a pattern
    AZStd::list<AssetUtilities::BuilderFilePatternMatcher> m_matcherBuilderPatterns;

    // Collection of all the external module builders
    AZStd::list<AssetProcessor::ExternalModuleAssetBuilderInfo*>    m_externalAssetBuilders;
    AssetProcessor::ExternalModuleAssetBuilderInfo*                 m_currentExternalAssetBuilder;
};


#endif//BATCHAPPLICATIONMANAGER_H
