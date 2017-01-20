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

#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QStringList>
#include <QDir>
#include "native/AssetDatabase/AssetDatabase.h"
#include "native/assetprocessor.h"
#include <QMutex>

namespace AzFramework
{
    class AssetRegistry;
}

namespace AssetProcessor
{
    class AssetProcessorManager;
    class DatabaseConnection;

    class AssetCatalog : public QObject
    {
        Q_OBJECT;
    
    public:
        AssetCatalog(QObject* parent, QStringList platforms, AZStd::shared_ptr<DatabaseConnection> db);
        ~AssetCatalog();

        //! Calling this function will ensure that we are not putting another save registry event in the event pump if we are already in the process of saving the registry
        //! This function will either return the registry version of the next registry save or of the current one ,if it is in progress 
        int SaveRegistry();

    Q_SIGNALS:
        void Saved(int saveId);
        void ProductChangeRegistered(ProductInfo productInfo);
        void ProductRemoveRegistered(ProductInfo productInfo);

    public Q_SLOTS:
        void OnProductChanged(QString productPath, QString platform);
        void OnProductRemoved(QString productPath, QString platform);

        void SaveRegistry_Impl();
        void BuildRegistry();
        
    private:
        AZStd::unique_ptr<AzFramework::AssetRegistry> m_registry;
        QStringList m_platforms;
        AZStd::shared_ptr<DatabaseConnection> m_db;
        QDir m_cacheRoot;

        bool m_registryBuiltOnce;
        bool m_catalogIsDirty = true;
        bool m_currentlySavingCatalog = false;
        int m_currentRegistrySaveVersion = 0;
        QMutex m_savingRegistryMutex;
    };
}

