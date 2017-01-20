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
#ifndef ASSETPROCESSOR_ASSETDATA_H
#define ASSETPROCESSOR_ASSETDATA_H

#include <QString>
#include <QSet>

namespace AssetProcessor
{
    //! This struct helps in storing and retrieval of asset information from the database
    struct DatabaseEntry
    {
        QString m_name;
        QString m_platform;
        QString m_jobDescription;

        DatabaseEntry();

        DatabaseEntry(QString name, QString platform, QString jobDescription);

        DatabaseEntry& operator=(const DatabaseEntry rhs);

        bool operator==(const DatabaseEntry& rhs) const;
    };

    //This global function is required, if we want to use DatabaseEntry as a key in a QHash
    uint qHash(const DatabaseEntry& key, uint seed = 0);

    //! this is the interface which we use to speak to the legacy database tables.
    // its known as the legacy database interface because the forthcoming tables will completely replace these
    // but this layer exits for compatibility with the previous version and allows us to upgrade in place.
    class LegacyDatabaseInterface
    {
    public:

        virtual ~LegacyDatabaseInterface() {}

        //! Returns true if the database or file exists already
        virtual bool DataExists() const = 0;

        //! Actually connects to the database, loads it, or creates empty database depending on above.
        virtual void LoadData() = 0;

        //! Use with care.  Resets all data!  This causes an immediate commit and save!
        virtual void ClearData() = 0;

        //! Retrieve the fingerprint for a given source name on a given platform for a particular jobDescription
        //! This could return zero if its never seen this file before.
        virtual unsigned int GetFingerprint(const QString& sourceName, const QString& platform, const QString& jobDescription) const = 0;

        //! Set the fingerprint for the given source name, platform and jobDescription to the value provided
        virtual void SetFingerprint(const QString& sourceName, const QString& platform, const QString& jobDescription, unsigned int fingerprint) = 0;

        //! Clearing a fingerprint will destroy its entry in the database
        //! and any entries that refer to it (products, etc).  if you want to merely set it dirty
        //! Then instead call SetFingerprint to zero
        virtual  void ClearFingerprint(const QString& sourceName, const QString& platform, const QString& jobDescription) = 0;

        //! Given a source name, platform and jobDescription, return the list of products by the last compile of that file
        //! returns false if it doesn't know about source.  This is different from returning true, but an empty list
        //! which means that it did process that source, but it emitted no products.
        virtual bool GetProductsForSource(const QString& sourceName, const QString& platform, const QString& jobDescription, QStringList& productList) const = 0;

        //! Given a source name and platform return the list of all jobDescriptions associated with them from the last compile of that file
        //! returns false if it doesn't know about any job description for that source and platform.
        virtual bool GetJobDescriptionsForSource(const QString& sourceName, const QString& platform, QStringList& jobDescription) const = 0;

        //! Given an product file name, compute source file name
        //! False is returned if its never heard of that product.
        virtual bool GetSourceFromProduct(const QString& productName, QString& sourceName, QString& platform, QString& jobDescription) = 0;

        //! For a given source, set the list of products for that source.
        //! Removes any data that's present and overwrites it with the new list
        //! Note that an empty list is acceptable data, it means the source emitted no products
        virtual void SetProductsForSource(const QString& sourceName, const QString& platform, const QString& jobDescription, const QStringList& productList) = 0;

        //! Clear the products for a given source.  This removes the entry entirely, not just sets it to empty.
        virtual void ClearProducts(const QString& sourceName, const QString& platform, const QString& jobDescription) = 0;

        //! GetMatchingProductFiles - checks the database for all products that begin with the given match check
        //! Note that the input string is expected to not include the cache folder
        //! so it probably starts with platformname.
        virtual void GetMatchingProductFiles(const QString& matchCheck, QSet<QString>& output) const = 0;

        //! GetMatchingSourceFiles - checks the database for all source files that begin with the given match  check
        //! note that the input string is expected to be the relative path name
        //! and the output is the relative name (so to convert it to a full path, you will need to call the appropriate function)
        virtual void GetMatchingSourceFiles(const QString& matchCheck, QSet<QString>& output) const = 0;

        //! Get a giant list of ALL known source files in the database.
        virtual void GetSourceFileNames(QSet<QString>& result) const = 0;

        //! finds all elements in the database that ends with the given input.  (Used to look things up by extensions, in general)
        virtual void GetSourceWithExtension(const QString& extension, QSet<QString>& output) const = 0;
    };
} // namespace assetprocessor

#endif // ASSETPROCESSOR_ASSETDATA_H
