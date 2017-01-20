#ifndef ASSETPROCESSOR_ASSETDATABASE_H
#define ASSETPROCESSOR_ASSETDATABASE_H

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


#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzToolsFramework/AssetDatabase/AssetDatabaseConnection.h>

#include <QtCore/QSet>
#include <QtCore/QString>

#include "native/AssetManager/AssetData.h"
#include "AzToolsFramework/API/EditorAssetSystemAPI.h"

class QStringList;

namespace AssetProcessor
{
    //! the Asset Processor's database manager's job is to create and modify the actual underlying SQL database.
    //! All queries to make changes to the database go through here.  This includes connecting to existing database
    //! and altering or creating database tables, etc.
    class DatabaseConnection
        : public AzToolsFramework::AssetDatabase::Connection
        , public LegacyDatabaseInterface
    {
    public:
        AZ_CLASS_ALLOCATOR(DatabaseConnection, AZ::SystemAllocator, 0);

        DatabaseConnection();
        ~DatabaseConnection();

        // ---------------------------------------------------------------------------------
        // ---------- IMPLEMENTATION OF  public LegacyDatabaseInterface --------------------
        // ---------------------------------------------------------------------------------
        // its known as the legacy database interface because the forthcoming tables will completely replace these.
        bool DataExists() const override;
        void LoadData() override;
        void ClearData() override;
        unsigned int GetFingerprint(const QString& sourceName, const QString& platform, const QString& jobDescription) const override;
        void SetFingerprint(const QString& sourceName, const QString& platform, const QString& jobDescription, unsigned int fingerprint) override;
        void ClearFingerprint(const QString& sourceName, const QString& platform, const QString& jobDescription) override;
        bool GetProductsForSource(const QString& sourceName, const QString& platform, const QString& jobDescription, QStringList& productList) const override;
        bool GetJobDescriptionsForSource(const QString& sourceName, const QString& platform, QStringList& jobDescription) const override;
        bool GetSourceFromProduct(const QString& productName, QString& sourceName, QString& platform, QString& jobDescription) override;
        void SetProductsForSource(const QString& sourceName, const QString& platform, const QString& jobDescription, const QStringList& productList) override;
        void ClearProducts(const QString& sourceName, const QString& platform, const QString& jobDescription) override;
        void GetMatchingProductFiles(const QString& matchCheck, QSet<QString>& output) const override;
        void GetMatchingSourceFiles(const QString& matchCheck, QSet<QString>& output) const override;
        void GetSourceFileNames(QSet<QString>& resultSet) const override;
        void GetSourceWithExtension(const QString& extension, QSet<QString>& output) const override;

        // ---------------------------------------------------------------------------------
        // ---------- END OF IMPLEMENTATION OF  public LegacyDatabaseInterface -------------
        // ---------------------------------------------------------------------------------

        typedef AZStd::function<bool(AzToolsFramework::AssetSystem::JobInfo& job)> JobLogCallback;

        //! Queries the highest job ID ever written to the database, used for custom pk
        AZ::s64 GetHighestJobID();  // return -1 if not found.

        //! SetJobLogForSource updates the Job Log table to record the status of a particular job.
        //! It also sets all prior jobs that match that job exactly to not be the "latest one" but keeps them in the database.
        void SetJobLogForSource(AZ::s64 jobId, const AZStd::string& sourceName, const AZStd::string& platform, const AZ::Uuid& builderUuid, const AZStd::string& jobKey, AzToolsFramework::AssetSystem::JobStatus status);

        //! Given a source name, (relative to your watched folder), find all relevant jobs we know about and return them.
        //! return FALSE to stop asking for more.
        void GetJobInfosForSource(const AZStd::string sourceName, JobLogCallback jobsFunction);

        //! same callback type, but you feed it the Primary Key of the job log table intead (ie, the ID)
        void GetJobInfosForJobLogPK(int jobLogPK, JobLogCallback jobsFunction);

        virtual bool IsReadOnly() const override { return false; } // we actually curate this database.

        using PlatformProductHandler = AZStd::function<void(const QString&)>;
        void EnumeratePlatformProducts(const QString& platform, PlatformProductHandler handler) const;

        //! Vacuum And Analyze is a housekeeping operation.  It should be used periodically when a lot of data has changed
        //! and you are not currently doing any other work.  It recovers wasted space in the DB and it also analyzes the performance
        //! of queries that have been previously made and collects statistics in the stats table.  SQLITE uses these statistics
        //! In order to optimize queries, so as long as you analyse periodically, it can get faster at performing common queries.
        //! It takes a few moments to perform this operation, which is why it should only be performed at times of rest.
        void VacuumAndAnalyze();

    protected:
        void SetDatabaseVersion(AzToolsFramework::AssetDatabase::DatabaseVersion ver);

        void CreateStatements() override;
        bool PostOpenDatabase() override;

    private:
        void ExecuteCreateStatements();
        AZStd::vector<AZStd::string> m_createStatements; // contains all statments required to create the tables

        typedef AZStd::pair<int, AZ::u32> FingerprintKeyValueType;

        // utility function to do database lookups in the fingerprints table:
        bool GetFingerprintsMatching(const QString& sourceName, const QString& platform, const QString& jobDescription, AZStd::vector< FingerprintKeyValueType >& result) const;

        // utility function for setting or clearing the keys.  used by other queries
        bool ClearProductsByFingerprintPK(int fingerprintPK);

        // utility function used by the various things which search for things by name
        bool SearchMatchingFiles(const char* queryName, const char* searchKeyName, const char* resultColumn, const QString& searchTerm, QSet<QString>& output) const;
    };
}//namespace EditorFramework

#endif // ASSETPROCESSOR_ASSETDATABASE_H