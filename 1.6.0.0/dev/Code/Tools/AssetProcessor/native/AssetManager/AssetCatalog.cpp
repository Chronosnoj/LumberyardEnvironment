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

#include "native/AssetManager/AssetCatalog.h"
#include "native/AssetManager/assetProcessorManager.h"
#include "native/utilities/assetUtils.h"
#include "native/AssetDatabase/AssetDatabase.h"

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Serialization/ObjectStream.h>
#include <AzCore/IO/FileIO.h>
#include <AzFramework/Asset/AssetRegistry.h>
#include <AzCore/IO/SystemFile.h>

#include <QElapsedTimer>
#include <QMutexLocker>

namespace AssetProcessor
{
    AssetCatalog::AssetCatalog(QObject* parent, QStringList platforms, AZStd::shared_ptr<DatabaseConnection> db)
        : QObject(parent)
        , m_registry(aznew AzFramework::AssetRegistry())
        , m_platforms(platforms)
        , m_db(db)
        , m_registryBuiltOnce(false)
    {
        bool computedCacheRoot = AssetUtilities::ComputeProjectCacheRoot(m_cacheRoot);
        AZ_Assert(computedCacheRoot, "Could not compute cache root for AssetCatalog");

        BuildRegistry();
        m_registryBuiltOnce = true;
    }

    AssetCatalog::~AssetCatalog()
    {
        SaveRegistry_Impl();
        m_registry.reset();
    }

    void AssetCatalog::OnProductChanged(QString productPath, QString platform)
    {
        AZ_Assert(!productPath.isEmpty(), "Product path is empty");
        m_catalogIsDirty = true;
        AZStd::string assetPath(productPath.toUtf8().constData());
        AZStd::string assetPathFull(QString("%1/%2/%3").arg(m_cacheRoot.absoluteFilePath(platform)).arg(AssetUtilities::ComputeGameName().toLower()).arg(productPath).toUtf8().constData());

        const AZ::IO::SystemFile::SizeType fileSizeBytes = AZ::IO::SystemFile::Length(assetPathFull.c_str());
        // Don't register if the file could not be opened. We can still return a valid asset Id.
        if (fileSizeBytes > 0)
        {
            // For now, seed Uuid from the asset's path. Moved assets will retain original Uuid as long as the monitor is active.
            const AZ::Data::AssetId assetId = AZ::Data::AssetId(AZ::Uuid::CreateName(assetPath.c_str()));

            AzFramework::AssetRegistry::AssetInfo info;
            info.m_relativePath = AZStd::move(assetPath);
            info.m_sizeBytes = fileSizeBytes;
            m_registry->RegisterAsset(assetId, info);

            if (m_registryBuiltOnce)
            {
                AssetProcessor::ProductInfo productInfo;
                productInfo.m_assetId = assetId;
                productInfo.m_platform = platform;
                productInfo.m_assetInfo = info;
                Q_EMIT ProductChangeRegistered(productInfo);
            }
        }
    }

    void AssetCatalog::OnProductRemoved(QString productPath, QString platform)
    {
        AZ_Assert(!productPath.isEmpty(), "Product path is empty");
        m_catalogIsDirty = true;
        QByteArray productPathUtf8Str = productPath.toUtf8();
        const char* assetPath = productPathUtf8Str.constData();
        AZ::Data::AssetId assetId = m_registry->GetAssetId(assetPath);
        if (assetId.IsValid())
        {
            m_registry->UnregisterAsset(assetId, assetPath);
            if (m_registryBuiltOnce)
            {
                AssetProcessor::ProductInfo productInfo;
                productInfo.m_assetId = assetId;
                productInfo.m_platform = platform;
                productInfo.m_assetInfo.m_relativePath = productPath.toUtf8().data();
                Q_EMIT ProductRemoveRegistered(productInfo);
            }
        }
    }

    void AssetCatalog::SaveRegistry_Impl()
    {
        // note that its safe not to save the catalog if the catalog is not dirty
        // because the engine will be accepting updates as long as the update has a higher or equal
        // number to the saveId, not just equal.
        if (m_catalogIsDirty)
        {
            m_catalogIsDirty = false;
            // Reflect registry for serialization.
            AZ::SerializeContext* serializeContext = nullptr;
            EBUS_EVENT_RESULT(serializeContext, AZ::ComponentApplicationBus, GetSerializeContext);
            AZ_Assert(serializeContext, "Unable to retrieve serialize context.");
            if (nullptr == serializeContext->FindClassData(AZ::AzTypeInfo<AzFramework::AssetRegistry>::Uuid()))
            {
                AzFramework::AssetRegistry::ReflectSerialize(serializeContext, *m_registry.get());
            }

            // save out a catalog for each platform
            for (QString platform : m_platforms)
            {
                QString tempRegistryFile = QString("%1/%2/%3").arg(m_cacheRoot.absoluteFilePath(platform)).arg(AssetUtilities::ComputeGameName().toLower()).arg("assetcatalog.xml.tmp");
                AZStd::string catalogRegistryFile(tempRegistryFile.toUtf8().constData());

                // Serialize out the catalog.
                QElapsedTimer timer;
                timer.start();
                AZ::IO::FileIOStream catalogFileStream(catalogRegistryFile.c_str(), AZ::IO::OpenMode::ModeWrite);
                if (catalogFileStream.IsOpen())
                {
                    AZ::ObjectStream* objStream = AZ::ObjectStream::Create(&catalogFileStream, *serializeContext, AZ::ObjectStream::ST_BINARY);
                    objStream->WriteClass(m_registry.get());
                    objStream->Finalize();
                    catalogFileStream.Close();
                    
                    AZ_TracePrintf("Catalog", "Saved %s catalog containing %u assets in %fs\n", platform.toUtf8().constData(), m_registry->m_assetIdToInfo.size(), timer.elapsed() / 1000.0f);

                    QString registryFile(tempRegistryFile);
                    registryFile.replace(".tmp", "");
                    bool moved = AZ::IO::SystemFile::Rename(tempRegistryFile.toUtf8().constData(), registryFile.toUtf8().constData(), true);
                    AZ_Warning("Catalog", moved, "Failed to move %s to %s", tempRegistryFile.toUtf8().constData(), registryFile.toUtf8().constData());
                }
            }
        }

        {
            QMutexLocker locker(&m_savingRegistryMutex);
            m_currentlySavingCatalog = false;
            Q_EMIT Saved(m_currentRegistrySaveVersion);
        }
    }

    int AssetCatalog::SaveRegistry()
    {
        QMutexLocker locker(&m_savingRegistryMutex);

        if (!m_currentlySavingCatalog)
        {
            m_currentlySavingCatalog = true;
            QMetaObject::invokeMethod(this, "SaveRegistry_Impl", Qt::QueuedConnection);
            return ++m_currentRegistrySaveVersion;
        }

        return m_currentRegistrySaveVersion;
    }

    void AssetCatalog::BuildRegistry()
    {
        m_registry->Clear();
        m_catalogIsDirty = true;

        for (QString platform : m_platforms)
        {
            QElapsedTimer timer;
            timer.start();
            const QString prefix = QString("%1/%2/").arg(platform).arg(AssetUtilities::ComputeGameName()).toLower();
            m_db->EnumeratePlatformProducts(platform,
                [&](const QString& productRootPath)
                {
                    QString productPath(productRootPath);
                    productPath.replace(prefix, "");
                    OnProductChanged(productPath, platform);
                }
            );

            AZ_TracePrintf("Catalog", "Read %u assets from database for %s in %fs\n", m_registry->m_assetIdToInfo.size(), platform.toUtf8().constData(), timer.elapsed() / 1000.0f);
        }
    }
}

#include <native/AssetManager/AssetCatalog.moc>
