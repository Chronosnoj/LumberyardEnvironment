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
#ifndef ASSETPROCESSINGSTATEDATA_H
#define ASSETPROCESSINGSTATEDATA_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QString>
#include "AssetData.h"
#include <QPair>


namespace AssetProcessor
{
    class DatabaseConnection;
    typedef QPair<QString, QString> NamePlatformPair;

    //! Stores simple state information about what it has already processed
    //! This data can be blown away at any time and it will restart from scratch
    //! Destroying this data cannot lead to corruption or invalid functionality
    //!
    //! This class performs no normalization of input names.  You must normalize them before inputting them
    //! or else you will be seeing cache miss.
    class AssetProcessingStateData
        : public QObject
        , public LegacyDatabaseInterface
    {
        Q_OBJECT
    public:
        //! Constructs and clears the cache
        AssetProcessingStateData(QObject* parent = nullptr);

        // ---------------------------------------------------------------------------------
        // ---------- IMPLEMENTATION OF  public LegacyDatabaseInterface --------------------
        // ---------------------------------------------------------------------------------
        unsigned int GetFingerprint(const QString& sourceName, const QString& platform, const QString& jobDescription) const override;
        void SetFingerprint(const QString& sourceName, const QString& platform, const QString& jobDescription, unsigned int fingerprint)  override;
        void ClearFingerprint(const QString& sourceName, const QString& platform, const QString& jobDescription);
        bool GetProductsForSource(const QString& sourceName, const QString& platform, const QString& jobDescription, QStringList& productList) const override;
        bool GetJobDescriptionsForSource(const QString& sourceName, const QString& platform, QStringList& jobDescription) const override;
        bool GetSourceFromProduct(const QString& productName, QString& sourceName, QString& platform, QString& jobDescription) override;
        void SetProductsForSource(const QString& sourceName, const QString& platform, const QString& jobDescription, const QStringList& productList) override;
        void ClearProducts(const QString& sourceName, const QString& platform, const QString& jobDescription) override;
        void GetMatchingProductFiles(const QString& matchCheck, QSet<QString>& output) const override;
        void GetMatchingSourceFiles(const QString& matchCheck, QSet<QString>& output) const override;
        void GetSourceFileNames(QSet<QString>& resultSet) const override;
        void GetSourceWithExtension(const QString& extension, QSet<QString>& output) const override;

        void LoadData() override;
        bool DataExists() const override;
        void ClearData() override;

        // ---------------------------------------------------------------------------------
        // ---------- END OF  public LegacyDatabaseInterface --------------------
        // ---------------------------------------------------------------------------------

        //! You can override the stored location of the data file used to persist state.
        //! Primarily for unit tests.
        void SetStoredLocation(const QString& location);

        void MigrateToDatabase(AssetProcessor::DatabaseConnection& connection);

        virtual ~AssetProcessingStateData();

    public Q_SLOTS:
        //! Force a save of the data immediately.
        void SaveData();



    private:

        // The map of (DatabaseEntry) --> (fingerprint)
        typedef QHash<DatabaseEntry, unsigned int> FingerprintContainerType;
        FingerprintContainerType m_storedFingerprints;

        // the map of (DatabaseEntry) --> (products)
        typedef QHash<DatabaseEntry, QStringList> ProductsContainerType;
        ProductsContainerType m_storedProducts;

        // the map of (product name) --> (DatabaseEntry)
        typedef QHash<QString, DatabaseEntry> ProductInputContainerType;
        ProductInputContainerType m_storedProductInputMap;

        // the map of (Name, Platform) --> (Job Descriptions)
        typedef QHash<NamePlatformPair, QStringList> JobDescriptionsContainerType;
        JobDescriptionsContainerType m_storedJobDescriptionsMap;

        bool m_writePending = false;

        void QueuePendingWrite();
        void ComputeStoredLocation() const;

        // this could be computed every time, but is used as a cache.
        mutable QString m_storedFileLocation;
    };
} // namespace AssetProcessor

#endif // ASSETPROCESSINGSTATEDATA_H

