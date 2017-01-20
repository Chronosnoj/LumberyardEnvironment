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
#include "AssetProcessingStateData.h"
#include "native/utilities/assetUtils.h"
#include "native/AssetDatabase/AssetDatabase.h"
#include "native/assetprocessor.h"


#include <QTimer>
#include <QFile>
#include <QSettings>
#include <QDir>

namespace AssetProcessor
{
    //! how long before we write changes, after a change has been triggered
    const unsigned int writeFlushDelayMS = 30000; // every 30 seconds is probably fine

    AssetProcessingStateData::AssetProcessingStateData(QObject* parent)
        : QObject(parent)
    {
    }

    AssetProcessingStateData::~AssetProcessingStateData()
    {
        if (m_writePending)
        {
            SaveData();
            m_writePending = false;
        }
    }

    unsigned int AssetProcessingStateData::GetFingerprint(const QString& sourceName, const QString& platform, const QString& jobDescription) const
    {
        const auto found = m_storedFingerprints.find(DatabaseEntry(sourceName, platform, jobDescription));
        if (found != m_storedFingerprints.end())
        {
            return found.value();
        }
        return 0;
    }

    void AssetProcessingStateData::SetFingerprint(const QString& sourceName, const QString& platform, const QString& jobDescription, unsigned int fingerprint)
    {
        m_storedFingerprints[DatabaseEntry(sourceName, platform, jobDescription)] = fingerprint;
        NamePlatformPair namePlatformPair(sourceName.toLower(), platform);
        auto found = m_storedJobDescriptionsMap.find(namePlatformPair);
        QStringList jobDescriptions;
        if (found != m_storedJobDescriptionsMap.end())
        {
            jobDescriptions = found.value();
            if (!jobDescriptions.contains(jobDescription.toLower()))
            {
                jobDescriptions.append(jobDescription.toLower());
                m_storedJobDescriptionsMap[namePlatformPair] = jobDescriptions;
            }
        }
        else
        {
            jobDescriptions.append(jobDescription.toLower());
            m_storedJobDescriptionsMap[namePlatformPair] = jobDescriptions;
        }
        QueuePendingWrite();
    }

    void AssetProcessingStateData::ClearFingerprint(const QString& sourceName, const QString& platform, const QString& jobDescription)
    {
        m_storedFingerprints.remove(DatabaseEntry(sourceName, platform, jobDescription));
        // products need to die too.
        ClearProducts(sourceName, platform, jobDescription);

        NamePlatformPair namePlatformPair(sourceName.toLower(), platform);
        auto found = m_storedJobDescriptionsMap.find(namePlatformPair);
        if (found != m_storedJobDescriptionsMap.end())
        {
            QStringList jobDescriptionList = found.value();
            if (jobDescriptionList.contains(jobDescription.toLower()))
            {
                jobDescriptionList.removeAll(jobDescription.toLower());
                m_storedJobDescriptionsMap[namePlatformPair] = jobDescriptionList;
            }
        }
        QueuePendingWrite();
    }

    void AssetProcessingStateData::SaveData()
    {
        if (!m_writePending)
        {
            return;
        }
        m_writePending = false;

        QFile::remove(m_storedFileLocation); // dont allow it to reload old data.

        // intentionally write this to a file in cache so that we can just delete it if we want
        // and we don't pollute registry

        QSettings loader(m_storedFileLocation, QSettings::IniFormat);
        loader.remove("StoredFingerprints");
        loader.remove("StoredProducts");

        loader.beginWriteArray("StoredFingerprints", m_storedFingerprints.size());
        int arrayIndex = 0;
        for (auto iterator = m_storedFingerprints.begin(); iterator != m_storedFingerprints.end(); ++iterator)
        {
            DatabaseEntry entry = iterator.key();
            loader.setArrayIndex(arrayIndex++);
            loader.setValue("fingerprintName", entry.m_name);
            loader.setValue("fingerprintPlatform", entry.m_platform);
            loader.setValue("fingerprintJobDescription", entry.m_jobDescription);
            loader.setValue("fingerprintValue", iterator.value());
        }
        loader.endArray();

        arrayIndex = 0;
        loader.beginWriteArray("StoredProducts", m_storedProducts.size());
        for (auto iterator = m_storedProducts.begin(); iterator != m_storedProducts.end(); ++iterator)
        {
            const DatabaseEntry& productKey = iterator.key();
            const QStringList& productValue = iterator.value();
            loader.setArrayIndex(arrayIndex++);
            loader.setValue("productName", productKey.m_name);
            loader.setValue("productPlatform", productKey.m_platform);
            loader.setValue("productJobDescription", productKey.m_jobDescription);

            if (!productValue.isEmpty())
            {
                int innerArrayIndex = 0;
                loader.beginWriteArray("Products", productValue.size());
                for (const QString& element : productValue)
                {
                    loader.setArrayIndex(innerArrayIndex++);
                    loader.setValue("product", element);
                }
                loader.endArray();
            }
        }
        loader.endArray();
    }

    bool AssetProcessingStateData::DataExists() const
    {
        ComputeStoredLocation();
        if (m_storedFileLocation.isEmpty())
        {
            return false; // we were unable to compute a valid location
        }
        return QFile::exists(m_storedFileLocation);
    }

    void AssetProcessingStateData::LoadData()
    {
        m_storedFingerprints.clear();
        m_storedProducts.clear();
        m_storedProductInputMap.clear();
        m_storedJobDescriptionsMap.clear();

        ComputeStoredLocation();
        if (m_storedFileLocation.isEmpty())
        {
            return; // we were unable to compute a valid location
        }

        QSettings loader(m_storedFileLocation, QSettings::IniFormat);
        int numEntries = loader.beginReadArray("StoredFingerprints");
        for (int index = 0; index < numEntries; ++index)
        {
            loader.setArrayIndex(index);
            QString fingerprintName = loader.value("fingerprintName", QString()).toString();
            QString fingerprintPlatform = loader.value("fingerprintPlatform", QString()).toString();
            QString fingerprintJobDescription = loader.value("fingerprintJobDescription", QString()).toString();
            unsigned int fingerprintValue = loader.value("fingerprintValue", 0U).toUInt();
            if ((!fingerprintName.isEmpty()) && (fingerprintValue != 0) && (!fingerprintPlatform.isEmpty()) && (!fingerprintJobDescription.isEmpty()))
            {
                m_storedFingerprints[DatabaseEntry(fingerprintName.toLower(), fingerprintPlatform, fingerprintJobDescription.toLower())] = fingerprintValue;
                NamePlatformPair namePlatformPair(fingerprintName.toLower(), fingerprintPlatform);
                auto found = m_storedJobDescriptionsMap.find(namePlatformPair);
                QStringList jobDescriptionList;
                if (found != m_storedJobDescriptionsMap.end())
                {
                    jobDescriptionList = found.value();
                    if (!jobDescriptionList.contains(fingerprintJobDescription.toLower()))
                    {
                        jobDescriptionList.append(fingerprintJobDescription.toLower());
                        m_storedJobDescriptionsMap[namePlatformPair] = jobDescriptionList;
                    }
                }
                else
                {
                    jobDescriptionList.append(fingerprintJobDescription.toLower());
                    m_storedJobDescriptionsMap[namePlatformPair] = jobDescriptionList;
                }
            }
        }
        loader.endArray();

        numEntries = loader.beginReadArray("StoredProducts");
        for (int index = 0; index < numEntries; ++index)
        {
            loader.setArrayIndex(index);
            QString productName = loader.value("productName", QString()).toString();
            QString productPlatform = loader.value("productPlatform", QString()).toString();
            QString productJobDescription = loader.value("productJobDescription", QString()).toString();
            if ((!productName.isEmpty()) && (!productPlatform.isEmpty()))
            {
                QStringList products;
                int numProducts = loader.beginReadArray("Products");
                for (int productIndex = 0; productIndex < numProducts; ++productIndex)
                {
                    loader.setArrayIndex(productIndex);
                    QString productValue = loader.value("product", QString()).toString();
                    if (!productValue.isEmpty())
                    {
                        products.append(productValue.toLower());
                    }
                }
                loader.endArray();
                SetProductsForSource(productName, productPlatform, productJobDescription, products);
            }
        }
        loader.endArray();
    }

    void AssetProcessingStateData::ClearData()
    {
        m_storedFingerprints.clear();
        m_storedProducts.clear();
        m_storedProductInputMap.clear();
        m_storedJobDescriptionsMap.clear();

        QueuePendingWrite();
        SaveData(); // immediate save, don't let this wait!
    }
    void AssetProcessingStateData::GetSourceFileNames(QSet<QString>& resultSet) const
    {
        resultSet.reserve(m_storedFingerprints.size());

        for (auto iterator = m_storedFingerprints.begin(); iterator != m_storedFingerprints.end(); ++iterator)
        {
            resultSet.insert(iterator.key().m_name);
        }
        return;
    }

    void AssetProcessingStateData::GetSourceWithExtension(const QString& extension, QSet<QString>& output) const
    {
        for (auto iterator = m_storedFingerprints.begin(); iterator != m_storedFingerprints.end(); ++iterator)
        {
            QString fingerprintName = iterator.key().m_name;
            if (fingerprintName.endsWith(extension, Qt::CaseInsensitive))
            {
                output.insert(fingerprintName);
            }
        }
    }

    void AssetProcessingStateData::GetMatchingProductFiles(const QString& matchCheck, QSet<QString>& output) const
    {
        for (const auto& productList : m_storedProducts)
        {
            for (const QString& element : productList)
            {
                if (element.startsWith(matchCheck, Qt::CaseInsensitive))
                {
                    output.insert(element);
                }
            }
        }
    }

    void AssetProcessingStateData::GetMatchingSourceFiles(const QString& matchCheck, QSet<QString>& output) const
    {
        for (auto iterator = m_storedFingerprints.begin(); iterator != m_storedFingerprints.end(); ++iterator)
        {
            const QString& inputName = iterator.key().m_name;
            if ((matchCheck.isEmpty()) || inputName.startsWith(matchCheck, Qt::CaseInsensitive))
            {
                output.insert(inputName);
            }
        }
    }


    void AssetProcessingStateData::QueuePendingWrite()
    {
        if (m_writePending)
        {
            return;
        }
        m_writePending = true;
        QTimer::singleShot(writeFlushDelayMS, this, SLOT(SaveData()));
    }

    void AssetProcessingStateData::ComputeStoredLocation() const
    {
        if (!m_storedFileLocation.isEmpty())
        {
            return;
        }
        QDir cacheRoot;
        if (AssetUtilities::ComputeProjectCacheRoot(cacheRoot))
        {
            QString fullPath = cacheRoot.absolutePath();
            if (!QDir(fullPath).exists())
            {
                QDir().mkdir(fullPath);
            }

            m_storedFileLocation = cacheRoot.filePath("StateCache.ini");
        }
    }

    void AssetProcessingStateData::SetStoredLocation(const QString& value)
    {
        m_storedFileLocation = value;
    }

    // contract requires us to return false only if the fingerprint is invalid
    bool AssetProcessingStateData::GetProductsForSource(const QString& sourceName, const QString& platform, const QString& jobDescription, QStringList& productList) const
    {
        DatabaseEntry entry(sourceName, platform, jobDescription);
        auto found = m_storedProducts.find(entry);
        if (found != m_storedProducts.end())
        {
            productList = found.value();
            return true;
        }

        return (m_storedFingerprints.find(entry) != m_storedFingerprints.end());
    }

    bool AssetProcessingStateData::GetSourceFromProduct(const QString& productName, QString& sourceName, QString& platform, QString& jobDescription)
    {
        auto found = m_storedProductInputMap.find(productName.toLower());
        if (found != m_storedProductInputMap.end())
        {
            DatabaseEntry entry = found.value();
            sourceName = entry.m_name;
            platform = entry.m_platform;
            jobDescription = entry.m_jobDescription;
            return true;
        }
        return false;
    }

    void AssetProcessingStateData::SetProductsForSource(const QString& sourceName, const QString& platform, const QString& jobDescription, const QStringList& productList)
    {
        // you must clear the old products when setting new ones.
        DatabaseEntry key(sourceName, platform, jobDescription);

        if (m_storedFingerprints.find(key) == m_storedFingerprints.end())
        {
            AZ_TracePrintf(ConsoleChannel, "Warning: Attempt to SetProductsForSource when there is no such source file registered\n");
            return;
        }

        auto found = m_storedProducts.find(key);

        if (found != m_storedProducts.end())
        {
            // erase prior products!
            for (const QString productName : found.value())
            {
                m_storedProductInputMap.remove(productName.toLower());
            }
        }

        for (const QString& product : productList)
        {
            m_storedProductInputMap[product.toLower()] = key;
        }

        QStringList tempProductList(productList);

        for (QString& product : tempProductList)
        {
            product = product.toLower();
        }
        m_storedProducts[key] = tempProductList;
        QueuePendingWrite();
    }

    void AssetProcessingStateData::ClearProducts(const QString& sourceName, const QString& platform, const QString& jobDescription)
    {
        auto found = m_storedProducts.find(DatabaseEntry(sourceName, platform, jobDescription));
        if (found != m_storedProducts.end())
        {
            for (const QString& product : found.value())
            {
                m_storedProductInputMap.remove(product.toLower());
            }
            m_storedProducts.erase(found);
        }
        QueuePendingWrite();
    }

    bool AssetProcessingStateData::GetJobDescriptionsForSource(const QString& sourceName, const QString& platform, QStringList& jobDescription) const
    {
        auto found = m_storedJobDescriptionsMap.find(NamePlatformPair(sourceName.toLower(), platform));
        if (found != m_storedJobDescriptionsMap.end())
        {
            jobDescription = found.value();
            return true;
        }
        return false;
    }

    void AssetProcessingStateData::MigrateToDatabase(AssetProcessor::DatabaseConnection& connection)
    {
        AZ_Assert(!connection.IsReadOnly(), "You must open a non-read-only connection.");
        connection.ClearData();

        // assumes an empty database.

        for (auto iterator = m_storedFingerprints.begin(); iterator != m_storedFingerprints.end(); ++iterator)
        {
            DatabaseEntry entry = iterator.key();
            connection.SetFingerprint(entry.m_name, entry.m_platform, entry.m_jobDescription, iterator.value());
        }

        for (auto iterator = m_storedProducts.begin(); iterator != m_storedProducts.end(); ++iterator)
        {
            const DatabaseEntry& productKey = iterator.key();
            const QStringList& productValue = iterator.value();

            connection.SetProductsForSource(productKey.m_name, productKey.m_platform, productKey.m_jobDescription, productValue);
        }
    }
} // end namespace assetprocessor
#include <native/AssetManager/AssetProcessingStateData.moc>


