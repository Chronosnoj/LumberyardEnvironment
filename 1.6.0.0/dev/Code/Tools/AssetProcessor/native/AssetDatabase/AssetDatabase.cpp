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


#include "AssetDatabase.h"

#include <AzCore/IO/SystemFile.h>
#include <AzToolsFramework/SQLite/SQLiteConnection.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/std/string/conversions.h>
#include "native/assetprocessor.h"
#include <AzFramework/StringFunc/StringFunc.h>

#include <QStringList>

namespace AssetProcessor
{
    using namespace AzToolsFramework;
    using namespace AzToolsFramework::AssetSystem;
    using namespace AzToolsFramework::AssetDatabase;
    using namespace AzToolsFramework::SQLite;

    namespace
    {
        static const char* INSERT_FINGERPRINT_NAME = "InsertFingerprint";
        static const char* UPDATE_FINGERPRINT_NAME = "UpdateFingerprint";
        static const char* GET_FINGERPRINT_NAME = "GetFingerprintIDs";
        static const char* DELETE_FINGERPRINT_NAME = "DeleteFingerprint";
        static const char* FIND_PRODUCTS_BY_FINGERPRINT = "FindProductsByFP";
        static const char* FIND_SOURCES_BY_PRODUCTNAME = "FindProductsPN";
        static const char* GET_JOB_DESCRIPTIONS = "GetJobDescriptions";
        static const char* GET_FINGERPRINTS_BY_PK = "GetFingerprintsByPK";
        static const char* DELETE_PRODUCTS_BY_FINGERPRINTPK = "DeleteProductsByFingerpintPK";
        static const char* INSERT_PRODUCT_BY_FINGEPRINTPK = "InsertProductByFingerprintPK";
        static const char* INSERT_JOBLOG_BY_FINGEPRINTPK = "InsertJobLogByFingerprintPK";
        static const char* SET_JOBS_NOT_LATEST = "SetMatchingJobsNotLatest";
        static const char* GET_MAX_JOBLOG_ID = "GetMaxJobsID";
        static const char* FIND_JOB_INFOS_BY_SOURCENAME = "FindJobInfosBySourceName";
        static const char* FIND_JOB_INFOS_BY_JOBLOG_PK = "FindJobInfosByJobLogPK";
        static const char* FIND_PRODUCTS_BY_PRODUCTNAME = "FindProductsByProductName";
        static const char* FIND_SOURCES_BY_SOURCENAME = "FindSourcesBySourceName";
        static const char* FIND_PRODUCTS_BY_PLATFORM = "FindProductsByPlatform";
        static const char* FIND_ALL_SOURCES = "FindAllSources";
        static const char* DELETE_FINGERPRINTS_BY_SOURCE = "DeleteFingerprintsBySource";
        
    }

    DatabaseConnection::DatabaseConnection()
    {
    }

    DatabaseConnection::~DatabaseConnection()
    {
        CloseDatabase();
    }

    bool DatabaseConnection::DataExists() const
    {
        AZStd::string dbFilePath = GetAssetDatabaseFilePath();
        return AZ::IO::SystemFile::Exists(dbFilePath.c_str());
    }

    void DatabaseConnection::LoadData()
    {
        if ((!m_databaseConnection) || (!m_databaseConnection->IsOpen()))
        {
            OpenDatabase();
        }
    }

    void DatabaseConnection::ClearData()
    {
        if ((m_databaseConnection) && (m_databaseConnection->IsOpen()))
        {
            CloseDatabase();
        }
        AZStd::string dbFilePath = GetAssetDatabaseFilePath();
        AZ::IO::SystemFile::Delete(dbFilePath.c_str());
        OpenDatabase();
    }


    bool DatabaseConnection::PostOpenDatabase()
    {
        DatabaseVersion foundVersion = GetDatabaseVersion();
        bool dropAllTables = true;

        // over here, check the version number, and perform upgrading if you need to
        if (foundVersion == CurrentDatabaseVersion())
        {
            dropAllTables = false;
        }

        // example, if you know how to get from version 1 to version 2, and we're on version 1 and should be on version 2,
        // we can either drop all tables and recreate them, or we can write statements which upgrade the database.
        // if you know how to upgrade, write your modify statements here, then set dropAllTables to false.
        // otherwise it will re-create from scratch.

        if (foundVersion == DatabaseVersion::StartingVersion)
        {
            // we can update, by adding the indices and removing the old ones.
            dropAllTables = false;
            m_databaseConnection->ExecuteOneOffStatement("CREATEINDEX_LF_Triplet");
            m_databaseConnection->ExecuteOneOffStatement("CREATEINDEX_LF_Pair");
            m_databaseConnection->ExecuteOneOffStatement("CREATEINDEX_LF_Name");
            m_databaseConnection->ExecuteOneOffStatement("CREATEINDEX_LF_Platform");

            m_databaseConnection->ExecuteOneOffStatement("CREATEINDEX_LP_FingerprintPK");
            m_databaseConnection->ExecuteOneOffStatement("CREATEINDEX_LP_ProductName");

            // drop the old indices (even the platform, we'll create a new one for it to match the new naming)
            m_databaseConnection->AddStatement("dropper", "DROP INDEX IF EXISTS LegacyFingerprints_JobDescription_idx;");
            m_databaseConnection->ExecuteOneOffStatement("dropper");
            m_databaseConnection->RemoveStatement("dropper");

            m_databaseConnection->AddStatement("dropper", "DROP INDEX IF EXISTS LegacyFingerprints_Platform_idx;");
            m_databaseConnection->ExecuteOneOffStatement("dropper");
            m_databaseConnection->RemoveStatement("dropper");

            m_databaseConnection->AddStatement("dropper", "DROP INDEX IF EXISTS LegacyFingerprints_SourceName_idx;");
            m_databaseConnection->ExecuteOneOffStatement("dropper");
            m_databaseConnection->RemoveStatement("dropper");

            m_databaseConnection->AddStatement("dropper", "DROP INDEX IF EXISTS LegacyProducts_FingerprintPK_idx;");
            m_databaseConnection->ExecuteOneOffStatement("dropper");
            m_databaseConnection->RemoveStatement("dropper");

            m_databaseConnection->AddStatement("dropper", "DROP INDEX IF EXISTS LegacyProducts_ProductName_idx;");
            m_databaseConnection->ExecuteOneOffStatement("dropper");
            m_databaseConnection->RemoveStatement("dropper");
        }
        else if (foundVersion >= DatabaseVersion::StartingVersion && foundVersion < DatabaseVersion::AddedJobLogTable)
        {
            m_databaseConnection->ExecuteOneOffStatement("CreateJobLogTable");
            m_databaseConnection->ExecuteOneOffStatement("CREATEINDEX_JL_Quadrilateral");
        }


        if (dropAllTables)
        {
            // drop all tables by destroying the entire database.
            m_databaseConnection->Close();
            AZStd::string dbFilePath = GetAssetDatabaseFilePath();
            AZ::IO::SystemFile::Delete(dbFilePath.c_str());
            if (!m_databaseConnection->Open(dbFilePath, IsReadOnly()))
            {
                delete m_databaseConnection;
                m_databaseConnection = nullptr;
                AZ_Error(ConsoleChannel, false, "Unable to open the asset database at %s\n", dbFilePath.c_str());
                return false;
            }

            CreateStatements();
            ExecuteCreateStatements();
        }

        // now that the database matches the schema, update it:
        SetDatabaseVersion(CurrentDatabaseVersion());

        return Connection::PostOpenDatabase();
    }


    void DatabaseConnection::ExecuteCreateStatements()
    {
        AZ_Assert(m_databaseConnection, "No connection!");
        for (const auto& element : m_createStatements)
        {
            m_databaseConnection->ExecuteOneOffStatement(element.c_str());
        }
    }

    void DatabaseConnection::SetDatabaseVersion(DatabaseVersion ver)
    {
        AZ_Assert(m_databaseConnection, "No connection!");
        auto pStatement = m_databaseConnection->GetStatement("SetDatabaseVersion");
        pStatement->BindValueInt(pStatement->GetNamedParamIdx(":ver"), static_cast<int>(ver));
        int retVal = pStatement->Step();
        pStatement->Finalize();
        AZ_Assert(retVal != SQLite::Statement::SqlOK, "Failed to execute SetDatabaseVersion.");
    }


    void DatabaseConnection::CreateStatements()
    {
        AZ_Assert(m_databaseConnection, "No connection!");
        AZ_Assert(m_databaseConnection->IsOpen(), "Connection is not open");

        Connection::CreateStatements();


        // -----------------------------------------------------------------------------------------------------------------------------
        //                  HOUSEKEEPING
        // -----------------------------------------------------------------------------------------------------------------------------
        m_databaseConnection->AddStatement("VACUUM", "VACUUM");
        m_databaseConnection->AddStatement("ANALYZE", "ANALYZE");

        // -----------------------------------------------------------------------------------------------------------------------------
        //                  DATABASE INFO TABLE
        // -----------------------------------------------------------------------------------------------------------------------------

        m_databaseConnection->AddStatement("CreateDatabaseInfoTable",
            "CREATE TABLE IF NOT EXISTS dbinfo(rowID            INTEGER PRIMARY KEY ,"
            "                                                    version INTEGER"
            "                                                    );");

        m_databaseConnection->AddStatement("SetDatabaseVersion", "INSERT OR REPLACE INTO dbinfo(rowID, version) VALUES (1, :ver);");
        m_createStatements.push_back("CreateDatabaseInfoTable");

        // -----------------------------------------------------------------------------------------------------------------------------
        //                  Legacy Fingerprints table
        // -----------------------------------------------------------------------------------------------------------------------------

        m_databaseConnection->AddStatement("CreateLFTable",
            "CREATE TABLE IF NOT EXISTS LegacyFingerprints(               "
            "    FingerprintID    INTEGER PRIMARY KEY AUTOINCREMENT,      "
            "    SourceName       TEXT                    collate nocase, "
            "    Platform         TEXT                    collate nocase, "
            "    JobDescription   TEXT                    collate nocase, "
            "    Fingerprint      BLOB "
            "    );");

        m_createStatements.push_back("CreateLFTable");

        m_databaseConnection->AddStatement("CREATEINDEX_LF_Triplet", "CREATE INDEX IF NOT EXISTS LF_TRIPLE ON LegacyFingerprints (SourceName, Platform, JobDescription);");
        m_databaseConnection->AddStatement("CREATEINDEX_LF_Pair", "CREATE INDEX IF NOT EXISTS LF_PAIR ON LegacyFingerprints (SourceName, Platform);");
        m_databaseConnection->AddStatement("CREATEINDEX_LF_Name", "CREATE INDEX IF NOT EXISTS LF_SINGLE ON LegacyFingerprints (SourceName);");
        m_databaseConnection->AddStatement("CREATEINDEX_LF_Platform", "CREATE INDEX IF NOT EXISTS LF_PLATFORM ON LegacyFingerprints (Platform);");

        m_createStatements.push_back("CREATEINDEX_LF_Triplet");
        m_createStatements.push_back("CREATEINDEX_LF_Pair");
        m_createStatements.push_back("CREATEINDEX_LF_Name");
        m_createStatements.push_back("CREATEINDEX_LF_Platform");

        // lookup statements:
        // get key given (Source, Platform, JobDescription)
        m_databaseConnection->AddStatement(GET_FINGERPRINT_NAME,
            "SELECT FingerprintID, Fingerprint FROM LegacyFingerprints WHERE "
            "(SourceName = :sourceName) AND "
            "(Platform = :platform) AND "
            "(JobDescription = :jobDescription);");

        // lookup all matching jobs for a given platform and source file
        m_databaseConnection->AddStatement(GET_JOB_DESCRIPTIONS,
            "SELECT FingerprintID, JobDescription FROM LegacyFingerprints WHERE "
            "(SourceName = :sourceName) AND "
            "(Platform = :platform); ");

        // lookup by primary key
        m_databaseConnection->AddStatement(GET_FINGERPRINTS_BY_PK,
            "SELECT * FROM LegacyFingerprints WHERE "
            "FingerprintID = :fingerprintID; ");

        // find by match query
        m_databaseConnection->AddStatement(FIND_SOURCES_BY_SOURCENAME,
            "SELECT DISTINCT * FROM LegacyFingerprints WHERE "
            "SourceName LIKE :sourceName ESCAPE '|';"); // use the pipe to escape since its NOT a valid file path or operator

        // lookup all source file names.
        // find by match query
        m_databaseConnection->AddStatement(FIND_ALL_SOURCES,
            "SELECT DISTINCT SourceName FROM LegacyFingerprints;");

        // insert fingerprint by (Source, Platform, JobDescription)
        m_databaseConnection->AddStatement(INSERT_FINGERPRINT_NAME,
            "INSERT INTO LegacyFingerprints ( SourceName, Platform, JobDescription, Fingerprint) "
            "VALUES ( :sourceName, :platform, :jobDescription, :fingerprint);");

        // update fingerprint by (fingerprintID)  RESIST the urge to use insert or update, it does not behave how you might expect
        m_databaseConnection->AddStatement(UPDATE_FINGERPRINT_NAME,
            "UPDATE LegacyFingerprints SET Fingerprint = :fingerprint WHERE "
            "FingerprintID = :fingerprintID;");

        // remove fingerprint by (Source, Platform, JobDescription)
        m_databaseConnection->AddStatement(DELETE_FINGERPRINT_NAME,
            "DELETE FROM LegacyFingerprints WHERE "
            "(SourceName = :sourceName) AND "
            "(Platform = :platform) AND "
            "(JobDescription = :jobDescription);");

        // Lookup all job descriptions which match (Source, Platform)
        m_databaseConnection->AddStatement("SelectJobDescriptions",
            "SELECT * FROM LegacyFingerprints WHERE "
            "(SourceName = :sourceName) AND "
            "(Platform = :platform);");

        // -----------------------------------------------------------------------------------------------------------------------------
        //                   Products table
        // -----------------------------------------------------------------------------------------------------------------------------

        m_databaseConnection->AddStatement("CreateLegacyProductsTable",
            "CREATE TABLE IF NOT EXISTS LegacyProducts(                     "
            "    ProductID INTEGER PRIMARY KEY AUTOINCREMENT,               "
            "    FingerprintPK            INTEGER,                          "
            "    ProductName              TEXT              collate nocase, "
            "    FOREIGN KEY (FingerprintPK) REFERENCES LegacyFingerprints(FingerprintID) ON DELETE CASCADE"
            ");");

        m_createStatements.push_back("CreateLegacyProductsTable");

        m_databaseConnection->AddStatement("CREATEINDEX_LP_FingerprintPK", "CREATE INDEX IF NOT EXISTS LP_FingerprintPK ON LegacyProducts (FingerprintPK);");
        m_databaseConnection->AddStatement("CREATEINDEX_LP_ProductName", "CREATE INDEX IF NOT EXISTS LP_ProductName ON LegacyProducts (ProductName);");

        m_createStatements.push_back("CREATEINDEX_LP_FingerprintPK");
        m_createStatements.push_back("CREATEINDEX_LP_ProductName");

        // get products by FingerprintPK
        m_databaseConnection->AddStatement(FIND_PRODUCTS_BY_FINGERPRINT,
            "SELECT * FROM LegacyProducts WHERE "
            "FingerprintPK = :fingerprintPK;");

        // get products by Match
        m_databaseConnection->AddStatement(FIND_PRODUCTS_BY_PRODUCTNAME,
            "SELECT DISTINCT * FROM LegacyProducts WHERE "
            "ProductName LIKE :productName ESCAPE '|';");     // use the pipe to escape since its NOT a valid file path or operator

        // find products by platform
        m_databaseConnection->AddStatement(FIND_PRODUCTS_BY_PLATFORM,
            "SELECT LegacyProducts.ProductName FROM LegacyProducts INNER JOIN LegacyFingerprints "
            "ON LegacyFingerprints.FingerprintID = LegacyProducts.FingerPrintPK "
            "WHERE LegacyFingerprints.Platform = :platform; ");

        m_databaseConnection->AddStatement(DELETE_PRODUCTS_BY_FINGERPRINTPK,
            "DELETE FROM LegacyProducts WHERE "
            "FingerprintPK = :fingerprintPK;");

        m_databaseConnection->AddStatement(INSERT_PRODUCT_BY_FINGEPRINTPK,
            "INSERT INTO LegacyProducts (FingerprintPK, ProductName) VALUES "
            "(:fingerprintPK, :productName);");


        // -----------------------------------------------------------------------------------------------------------------------------
        //                   Job table
        // -----------------------------------------------------------------------------------------------------------------------------

        m_databaseConnection->AddStatement("CreateJobLogTable",
            "CREATE TABLE IF NOT EXISTS JobLog(                             "
            "    JobID INTEGER PRIMARY KEY,                                 " // NOTE that we are actually manually responsible for this PK it is NOT autoincrement.
            "    FingerprintPK            INTEGER,                          "
            "    Platform                 TEXT              collate nocase, "
            "    BuilderGuid              BLOB,                             "
            "    JobKey                   TEXT              collate nocase, "
            "    Status                   INTEGER,                          "
            "    Latest                   INTEGER,                          "
            "    FOREIGN KEY (FingerprintPK) REFERENCES LegacyFingerprints(FingerprintID) ON DELETE CASCADE"
            ");");

        m_createStatements.push_back("CreateJobLogTable");

        m_databaseConnection->AddStatement("CREATEINDEX_JL_Quadrilateral", "CREATE INDEX IF NOT EXISTS JL_Quadrilateral ON JobLog (FingerprintPK, Platform, BuilderGuid, JobKey);");
        m_createStatements.push_back("CREATEINDEX_JL_Quadrilateral");

        m_databaseConnection->AddStatement(GET_MAX_JOBLOG_ID, "SELECT JobID FROM JobLog ORDER BY JobID DESC LIMIT 1");
        
        m_databaseConnection->AddStatement(INSERT_JOBLOG_BY_FINGEPRINTPK,
            "INSERT INTO JobLog (JobID, FingerprintPK, Platform, BuilderGuid, JobKey, Status, Latest) VALUES "
            "(:jobId, :fingerprintPK, :platform, :builderGuid, :jobKey, :status, :latest);");

        m_databaseConnection->AddStatement(SET_JOBS_NOT_LATEST,
            "UPDATE JobLog SET Latest = 0 WHERE "
            "(FingerprintPK = :fingerprintPK) AND " 
            "(Platform = :platform) AND "
            "(BuilderGuid = :builderGuid) AND "
            "(JobKey = :jobKey);");

        // note that searching by "latest" is actually very slow since most of the entries are latest anyway
        m_databaseConnection->AddStatement(FIND_JOB_INFOS_BY_SOURCENAME,
            "SELECT JobLog.JobID, JobLog.Platform, JobLog.BuilderGuid, JobLog.JobKey, JobLog.Status, LegacyFingerprints.SourceName, JobLog.Latest FROM LegacyFingerprints INNER JOIN JobLog "
            "ON JobLog.FingerprintPK = LegacyFingerprints.FingerprintID "
            "WHERE LegacyFingerPrints.SourceName = :sourceName;");

        m_databaseConnection->AddStatement(FIND_JOB_INFOS_BY_JOBLOG_PK,
            "SELECT JobLog.JobID, JobLog.Platform, JobLog.BuilderGuid, JobLog.JobKey, JobLog.Status, LegacyFingerprints.SourceName FROM JobLog INNER JOIN LegacyFingerprints "
            "ON JobLog.FingerprintPK = LegacyFingerprints.FingerprintID "
            "WHERE JobLog.JobID = :jobLogPK; ");
        

        // --------------------------- COMPLEX QUERIES --------------------------------
        // get FingerprintPK by product name
        m_databaseConnection->AddStatement(FIND_SOURCES_BY_PRODUCTNAME,
            "SELECT LegacyFingerprints.* FROM LegacyFingerprints INNER JOIN LegacyProducts "
            "ON LegacyFingerprints.FingerprintID = LegacyProducts.FingerprintPK "
            "WHERE LegacyProducts.ProductName = :productName; ");

        m_databaseConnection->AddStatement(DELETE_FINGERPRINTS_BY_SOURCE,
            "DELETE FROM LegacyFingerprints WHERE "
            "(SourceName = :sourceName) AND "
            "(Platform = :platform) AND "
            "(JobDescription = :jobDescription);");
    }

    bool  DatabaseConnection::GetFingerprintsMatching(const QString& sourceName, const QString& platform, const QString& jobDescription, AZStd::vector< FingerprintKeyValueType >& result) const
    {
        AZ_Error(ConsoleChannel, m_databaseConnection, "Fatal: attempt to work on a database connection that doesn't exist");
        AZ_Error(ConsoleChannel, m_databaseConnection->IsOpen(), "Fatal: attempt to work on a database connection that isn't open");

        if ((!m_databaseConnection) || (!m_databaseConnection->IsOpen()))
        {
            return false;
        }

        StatementAutoFinalizer autoFinal(*m_databaseConnection, GET_FINGERPRINT_NAME);
        Statement* statement = autoFinal.Get();

        AZ_Assert(statement, "Statement not found: %s", GET_FINGERPRINT_NAME); // should never happen...

        // QStrings are stored ephemerally unfortunately, due to the fact that they are actually UNICODE WIDE in memory
        AZStd::string encodedSource = sourceName.toUtf8().constData();
        AZStd::string encodedPlatform = platform.toUtf8().constData();
        AZStd::string encodedJobDescription = jobDescription.toUtf8().constData();

        statement->BindValueText(statement->GetNamedParamIdx(":sourceName"), encodedSource.c_str());
        statement->BindValueText(statement->GetNamedParamIdx(":platform"), encodedPlatform.c_str());
        statement->BindValueText(statement->GetNamedParamIdx(":jobDescription"), encodedJobDescription.c_str());

        int fingerprintIDColumn = -1;
        int fingerprintColumn = -1;
        while (statement->Step() == Statement::SqlOK) // this could also be SqlDone, meaning, no more result values available
        {
            if (fingerprintColumn == -1)
            {
                fingerprintColumn = statement->FindColumn("Fingerprint"); // thus must be done inside step
                AZ_Assert(fingerprintColumn != -1, "The %s SQLite statement is incorrectly formed.", GET_FINGERPRINT_NAME);
                if (fingerprintColumn == -1)
                {
                    return false; // for safety
                }
            }

            if (fingerprintIDColumn == -1)
            {
                fingerprintIDColumn = statement->FindColumn("FingerprintID"); // thus must be done inside step
                AZ_Assert(fingerprintIDColumn != -1, "The %s SQLite statement is incorrectly formed (fingerprintID column)", GET_FINGERPRINT_NAME);
                if (fingerprintIDColumn == -1)
                {
                    return false; // for safety
                }
            }

            // the following assert will only happen during development if this actual cpp file has bugs in it

            int numBytes = statement->GetColumnBlobBytes(fingerprintColumn);
            AZ_Assert(numBytes == sizeof(AZ::u32), "The %s SQLite statement is incorrectly formed (blob size)", GET_FINGERPRINT_NAME);
            if (numBytes != sizeof(AZ::u32))
            {
                return false;
            }
            AZ::u32 value;
            memcpy(&value, statement->GetColumnBlob(fingerprintColumn), sizeof(AZ::u32));
            result.push_back(FingerprintKeyValueType(statement->GetColumnInt(fingerprintIDColumn), value));
        }

        return !result.empty();
    }

    unsigned int DatabaseConnection::GetFingerprint(const QString& sourceName, const QString& platform, const QString& jobDescription) const
    {
        AZ_Error(ConsoleChannel, m_databaseConnection, "Fatal: attempt to work on a database connection that doesn't exist");
        AZ_Error(ConsoleChannel, m_databaseConnection->IsOpen(), "Fatal: attempt to work on a database connection that isn't open");

        if ((!m_databaseConnection) || (!m_databaseConnection->IsOpen()))
        {
            return 0;
        }

        AZStd::vector<FingerprintKeyValueType> results;

        if (!GetFingerprintsMatching(sourceName, platform, jobDescription, results))
        {
            return 0;
        }
        AZ_WarningOnce("Asset database", results.size() == 1, "Unexpected number of matching fingerprints for %s %s %s", sourceName.toUtf8().constData(), platform.toUtf8().constData(), jobDescription.toUtf8().constData());

        return results[0].second;
    }

    void DatabaseConnection::SetFingerprint(const QString& sourceName, const QString& platform, const QString& jobDescription, unsigned int fingerprint)
    {
        AZ_Error(ConsoleChannel, m_databaseConnection, "Fatal: attempt to work on a database connection that doesn't exist");
        AZ_Error(ConsoleChannel, m_databaseConnection->IsOpen(), "Fatal: attempt to work on a database connection that isn't open");

        if ((!m_databaseConnection) || (!m_databaseConnection->IsOpen()))
        {
            return;
        }
        // otherwise, we need to find it and update it.  If this is a bottleneck (super unlikely) we can always find more complex SQL to do it
        AZStd::vector<FingerprintKeyValueType> results;

        if (!GetFingerprintsMatching(sourceName, platform, jobDescription, results))
        {
            if (fingerprint == 0)
            {
                return; // don't set fingerprints to zero since this is the default return value for GetFingerprint (do not waste space)
            }
            // it it is a single statement, do not wrap it in a transaction, this wastes a lot of time.
            StatementAutoFinalizer autoFinal(*m_databaseConnection, INSERT_FINGERPRINT_NAME);
            Statement* statement = autoFinal.Get();
            AZ_Error(ConsoleChannel, statement, "Could not get statement: %s", INSERT_FINGERPRINT_NAME);

            int sourceNameVar =     statement->GetNamedParamIdx(":sourceName");
            int platformVar =       statement->GetNamedParamIdx(":platform");
            int jobDescriptionVar = statement->GetNamedParamIdx(":jobDescription");
            int fingerprintVar =    statement->GetNamedParamIdx(":fingerprint");

            if ((!sourceNameVar) || (!platformVar) || (!jobDescriptionVar) || (!fingerprintVar))
            {
                AZ_WarningOnce(ConsoleChannel, false, "Could not find the column for sourceName %i or platform %i or jobDescription %i or fingerprint %i", sourceNameVar, platformVar, jobDescriptionVar, fingerprintVar);
                return;
            }

            // convert to char * (must persist for lifetime of statement)
            AZStd::string sourceNameStr = sourceName.toUtf8().constData();
            AZStd::string platformStr = platform.toUtf8().constData();
            AZStd::string jobDescriptionStr = jobDescription.toUtf8().constData();

            statement->BindValueText(sourceNameVar, sourceNameStr.c_str());
            statement->BindValueText(platformVar, platformStr.c_str());
            statement->BindValueText(jobDescriptionVar, jobDescriptionStr.c_str());
            statement->BindValueBlob(fingerprintVar, &fingerprint, sizeof(unsigned int));
            if (statement->Step() == Statement::SqlError)
            {
                AZ_WarningOnce(ConsoleChannel, false, "Failed to write the new fingerprint into the database.");
                return;
            }
        }
        else
        {
            // existing fingerprint!  do an update!
            // it it is a single statement, do not wrap it in a transaction, this wastes a lot of time.
            StatementAutoFinalizer autoFinal(*m_databaseConnection, UPDATE_FINGERPRINT_NAME);
            Statement* statement = autoFinal.Get();
            AZ_Error(ConsoleChannel, statement, "Could not get statement: %s", UPDATE_FINGERPRINT_NAME);
            int fingerprintIDColumn = statement->GetNamedParamIdx(":fingerprintID");
            int fingerprintColumn = statement->GetNamedParamIdx(":fingerprint");
            if ((!fingerprintIDColumn) || (!fingerprintColumn))
            {
                AZ_WarningOnce(ConsoleChannel, false, "Could not find the column for fingerprint %i or FingerprintID %i", fingerprintColumn, fingerprintIDColumn);
                return;
            }

            statement->BindValueInt(fingerprintIDColumn, results[0].first);
            statement->BindValueBlob(fingerprintColumn, &fingerprint, sizeof(unsigned int));
            if (statement->Step() == Statement::SqlError)
            {
                AZ_Warning(ConsoleChannel, false, "Failed to execute %s to update fingerprints (key %i)", UPDATE_FINGERPRINT_NAME, results[0].first);
                return;
            }
        }
    }

    // this must actually delete the fingerprints and the products.
    void DatabaseConnection::ClearFingerprint(const QString& sourceName, const QString& platform, const QString& jobDescription)
    {
        ScopedTransaction transact(m_databaseConnection);

        // no existing, do an insert!
        StatementAutoFinalizer autoFinal(*m_databaseConnection, DELETE_FINGERPRINTS_BY_SOURCE);
        Statement* statement = autoFinal.Get();
        AZ_Error(ConsoleChannel, statement, "Could not get statement: %s", DELETE_FINGERPRINTS_BY_SOURCE);

        int sourceNameVar = statement->GetNamedParamIdx(":sourceName");
        int platformVar = statement->GetNamedParamIdx(":platform");
        int jobDescriptionVar = statement->GetNamedParamIdx(":jobDescription");

        if ((!sourceNameVar) || (!platformVar) || (!jobDescriptionVar))
        {
            AZ_WarningOnce(ConsoleChannel, false, "Could not find the column for sourceName %i or platform %i or jobDescription %i in %s", sourceNameVar, platformVar, jobDescriptionVar, DELETE_FINGERPRINTS_BY_SOURCE);
            return;
        }

        // convert to char * (must persist for lifetime of statement)
        AZStd::string sourceNameStr = sourceName.toUtf8().constData();
        AZStd::string platformStr = platform.toUtf8().constData();
        AZStd::string jobDescriptionStr = jobDescription.toUtf8().constData();

        statement->BindValueText(sourceNameVar, sourceNameStr.c_str());
        statement->BindValueText(platformVar, platformStr.c_str());
        statement->BindValueText(jobDescriptionVar, jobDescriptionStr.c_str());
        if (statement->Step() == Statement::SqlError)
        {
            AZ_WarningOnce(ConsoleChannel, false, "Failed to ClearFingerprint");
            return;
        }
        transact.Commit();
    }

    void DatabaseConnection::GetSourceFileNames(QSet<QString>& resultSet) const
    {
        StatementAutoFinalizer autoFinal(*m_databaseConnection, FIND_ALL_SOURCES);
        Statement* statement = autoFinal.Get();

        AZ_Assert(statement, "Statement not found: %s", FIND_ALL_SOURCES); // should never happen...

        Statement::SqlStatus result = statement->Step();

        int resultColumnIdx = -1;

        while (result == Statement::SqlOK)
        {
            if (resultColumnIdx == -1)
            {
                resultColumnIdx = statement->FindColumn("SourceName");
                if (resultColumnIdx == -1)
                {
                    AZ_Error(ConsoleChannel, false, "Results from %s failed to have a %s column", "SourceName", FIND_ALL_SOURCES);
                    return;
                }
            }

            resultSet.insert(QString::fromUtf8(statement->GetColumnText(resultColumnIdx).c_str()));

            result = statement->Step();
        }

        AZ_Warning(ConsoleChannel, result != Statement::SqlError, "Error occurred during statement step of %s", FIND_ALL_SOURCES);
    }


    bool DatabaseConnection::GetProductsForSource(const QString& sourceName, const QString& platform, const QString& jobDescription, QStringList& productList) const
    {
        AZ_Error(ConsoleChannel, m_databaseConnection, "Fatal: attempt to work on a database connection that doesn't exist");
        AZ_Error(ConsoleChannel, m_databaseConnection->IsOpen(), "Fatal: attempt to work on a database connection that isn't open");

        if ((!m_databaseConnection) || (!m_databaseConnection->IsOpen()))
        {
            return false;
        }

        AZStd::vector<FingerprintKeyValueType> results;

        if (!GetFingerprintsMatching(sourceName, platform, jobDescription, results))
        {
            return false; // I don't know about that source file on that platform at all!
        }

        // okay, find the products for that source:
        StatementAutoFinalizer fin(*m_databaseConnection, FIND_PRODUCTS_BY_FINGERPRINT);
        Statement* statement = fin.Get();
        if (!statement)
        {
            AZ_Error(ConsoleChannel, false, "Unable to find SQL statement: %s", FIND_PRODUCTS_BY_FINGERPRINT);
            return false;
        }

        int fingerprintIdx = statement->GetNamedParamIdx(":fingerprintPK");
        if (!fingerprintIdx)
        {
            AZ_Error(ConsoleChannel, false, "could not find fingerprintPK in statement %s", FIND_PRODUCTS_BY_FINGERPRINT);
            return false;
        }

        statement->BindValueInt(fingerprintIdx, results[0].first);
        int productNameColumn = -1;
        Statement::SqlStatus stepResult = statement->Step();

        while (stepResult == Statement::SqlOK)
        {
            if (productNameColumn == -1)
            {
                productNameColumn = statement->FindColumn("ProductName");
                if (productNameColumn == -1)
                {
                    AZ_Error(ConsoleChannel, false, "%s didn't return a column called ProductName", FIND_PRODUCTS_BY_FINGERPRINT);
                    return false;
                }
            }
            AZStd::string value = statement->GetColumnText(productNameColumn);
            if (value.empty())
            {
                AZ_Error(ConsoleChannel, false, "%s returned an empty string for ProductName", FIND_PRODUCTS_BY_FINGERPRINT);
                return false;
            }
            productList.push_back(QString::fromUtf8(value.c_str()));
            stepResult = statement->Step();
        }
        return stepResult != Statement::SqlError;
    }

    AZ::s64 DatabaseConnection::GetHighestJobID()
    {
        AZ_Error(ConsoleChannel, m_databaseConnection, "Fatal: attempt to work on a database connection that doesn't exist");
        AZ_Error(ConsoleChannel, m_databaseConnection->IsOpen(), "Fatal: attempt to work on a database connection that isn't open");

        if ((!m_databaseConnection) || (!m_databaseConnection->IsOpen()))
        {
            return false;
        }

        // okay, find the products for that source:
        StatementAutoFinalizer fin(*m_databaseConnection, GET_MAX_JOBLOG_ID);
        Statement* statement = fin.Get();
        if (!statement)
        {
            AZ_Error(ConsoleChannel, false, "Unable to find SQL statement: %s", GET_JOB_DESCRIPTIONS);
            return false;
        }

        if (statement->Step() == Statement::SqlOK)
        {
            return statement->GetColumnInt64(0);
        }
        
        return -1;
    }

    bool DatabaseConnection::GetJobDescriptionsForSource(const QString& sourceName, const QString& platform, QStringList& jobDescription) const
    {
        AZ_Error(ConsoleChannel, m_databaseConnection, "Fatal: attempt to work on a database connection that doesn't exist");
        AZ_Error(ConsoleChannel, m_databaseConnection->IsOpen(), "Fatal: attempt to work on a database connection that isn't open");

        if ((!m_databaseConnection) || (!m_databaseConnection->IsOpen()))
        {
            return false;
        }

        // okay, find the products for that source:
        StatementAutoFinalizer fin(*m_databaseConnection, GET_JOB_DESCRIPTIONS);
        Statement* statement = fin.Get();
        if (!statement)
        {
            AZ_Error(ConsoleChannel, false, "Unable to find SQL statement: %s", GET_JOB_DESCRIPTIONS);
            return false;
        }

        int sourceNameIdx = statement->GetNamedParamIdx(":sourceName");
        int platformIdx = statement->GetNamedParamIdx(":platform");
        if (!sourceNameIdx)
        {
            AZ_Error(ConsoleChannel, false, "could not find :sourceName in statement %s", GET_JOB_DESCRIPTIONS);
            return false;
        }

        if (!platformIdx)
        {
            AZ_Error(ConsoleChannel, false, "could not find :platform in statement %s", GET_JOB_DESCRIPTIONS);
            return false;
        }

        AZStd::string sourceNameStr = sourceName.toUtf8().constData();
        AZStd::string platformStr = platform.toUtf8().constData();

        statement->BindValueText(sourceNameIdx, sourceNameStr.c_str());
        statement->BindValueText(platformIdx, platformStr.c_str());

        int resultColumn = -1;
        while (statement->Step() == Statement::SqlOK)
        {
            if (resultColumn == -1)
            {
                resultColumn = statement->FindColumn("JobDescription");
                if (resultColumn == -1)
                {
                    AZ_Error(ConsoleChannel, false, "Unable to find the JobDescription column while executing %s", GET_JOB_DESCRIPTIONS);
                    return false;
                }
            }

            AZStd::string resultString = statement->GetColumnText(resultColumn);
            if (resultString.empty())
            {
                AZ_Error(ConsoleChannel, false, "Was unable to decode JobDescription while executing %s", GET_JOB_DESCRIPTIONS);
                return false;
            }

            jobDescription.push_back(QString::fromUtf8(resultString.c_str()));
        }

        return !jobDescription.isEmpty();
    }

    //! Given an product file name, compute source file name
    //! False is returned if its never heard of that product.
    bool DatabaseConnection::GetSourceFromProduct(const QString& productName, QString& sourceName, QString& platform, QString& jobDescription)
    {
        AZ_Error(ConsoleChannel, m_databaseConnection, "Fatal: attempt to work on a database connection that doesn't exist");
        AZ_Error(ConsoleChannel, m_databaseConnection->IsOpen(), "Fatal: attempt to work on a database connection that isn't open");

        if ((!m_databaseConnection) || (!m_databaseConnection->IsOpen()))
        {
            return false;
        }

        StatementAutoFinalizer fin(*m_databaseConnection, FIND_SOURCES_BY_PRODUCTNAME);
        Statement* statement = fin.Get();
        if (!statement)
        {
            AZ_Error(ConsoleChannel, false, "Unable to find SQL statement: %s", FIND_SOURCES_BY_PRODUCTNAME);
            return false;
        }

        int productNameIdx = statement->GetNamedParamIdx(":productName");
        if (!productNameIdx)
        {
            AZ_Error(ConsoleChannel, false, "could not find :productName in statement %s", FIND_SOURCES_BY_PRODUCTNAME);
            return false;
        }

        AZStd::string productNameStr = productName.toUtf8().constData();

        statement->BindValueText(productNameIdx, productNameStr.c_str());

        Statement::SqlStatus resultCode = statement->Step();
        if (resultCode == Statement::SqlError)
        {
            AZ_Error(ConsoleChannel, false, "Failed to execute statement %s", FIND_SOURCES_BY_PRODUCTNAME);
            return false;
        }

        if (resultCode == Statement::SqlDone)
        {
            // no results.
            return false;
        }

        // we have a result!
        int resultColumn_SourceName = statement->FindColumn("SourceName");
        int resultColumn_Platform = statement->FindColumn("Platform");
        int resultColumn_JobDescription = statement->FindColumn("JobDescription");

        if (resultColumn_SourceName == -1)
        {
            AZ_Error(ConsoleChannel, false, "Unable to find the SourceName column while executing %s", FIND_SOURCES_BY_PRODUCTNAME);
            return false;
        }
        if (resultColumn_Platform == -1)
        {
            AZ_Error(ConsoleChannel, false, "Unable to find the Platform column while executing %s", FIND_SOURCES_BY_PRODUCTNAME);
            return false;
        }
        if (resultColumn_JobDescription == -1)
        {
            AZ_Error(ConsoleChannel, false, "Unable to find the JobDescription column while executing %s", FIND_SOURCES_BY_PRODUCTNAME);
            return false;
        }

        sourceName = QString::fromUtf8(statement->GetColumnText(resultColumn_SourceName).c_str());
        platform = QString::fromUtf8(statement->GetColumnText(resultColumn_Platform).c_str());
        jobDescription = QString::fromUtf8(statement->GetColumnText(resultColumn_JobDescription).c_str());

        return (!sourceName.isEmpty()) && (!platform.isEmpty()) && (!jobDescription.isEmpty());
    }

    // this utiltity function is used internally by the other functions and assumes all other checking has already been done.
    bool DatabaseConnection::ClearProductsByFingerprintPK(int fingerprintPK)
    {
        StatementAutoFinalizer autoFinalizer(*m_databaseConnection, DELETE_PRODUCTS_BY_FINGERPRINTPK);
        Statement* statement = autoFinalizer.Get();

        AZ_Assert(statement, "Statement not found: %s", DELETE_PRODUCTS_BY_FINGERPRINTPK); // should never happen...

        int fingerprintPKVar = statement->GetNamedParamIdx(":fingerprintPK");

        if (!fingerprintPKVar)
        {
            AZ_WarningOnce(ConsoleChannel, false, "Could not find the column for :fingerprintPK for %s ", DELETE_PRODUCTS_BY_FINGERPRINTPK);
            return false;
        }

        // convert to char * (must persist for lifetime of statement)
        statement->BindValueInt(fingerprintPKVar, fingerprintPK);
        if (statement->Step() == Statement::SqlError)
        {
            AZ_Warning(ConsoleChannel, false, "Failed to execute the DELETE_PRODUCTS_BY_FINGERPRINTPK statment on pk %i", fingerprintPK);
            return false;
        }
        return true;
    }

    //! For a given source, set the list of products for that source.
    //! Removes any data that's present and overwrites it with the new list
    //! Note that an empty list is in fact acceptable data, it means the source emitted no products
    void DatabaseConnection::SetProductsForSource(const QString& sourceName, const QString& platform, const QString& jobDescription, const QStringList& productList)
    {
        AZ_Error(ConsoleChannel, m_databaseConnection, "Fatal: attempt to work on a database connection that doesn't exist");
        AZ_Error(ConsoleChannel, m_databaseConnection->IsOpen(), "Fatal: attempt to work on a database connection that isn't open");

        if ((!m_databaseConnection) || (!m_databaseConnection->IsOpen()))
        {
            return;
        }

        AZStd::vector<FingerprintKeyValueType> results;

        if (!GetFingerprintsMatching(sourceName, platform, jobDescription, results))
        {
            AZ_TracePrintf(ConsoleChannel, "Warning: asked to set a product list for a source which does not exist (%s %s %s)\n",
                sourceName.toUtf8().constData(), platform.toUtf8().constData(), jobDescription.toUtf8().constData());
            return; // there can't be any such products.
        }

        ScopedTransaction transaction(m_databaseConnection);

        if (!ClearProductsByFingerprintPK(results[0].first))
        {
            return;
        }


        {
            // scope created for the statement.
            StatementAutoFinalizer autoFinalizer(*m_databaseConnection, INSERT_PRODUCT_BY_FINGEPRINTPK);
            Statement* statement = autoFinalizer.Get();
            AZ_Assert(statement, "Statement not found: %s", INSERT_PRODUCT_BY_FINGEPRINTPK); // should never happen...

            int fingerprintPKVar = statement->GetNamedParamIdx(":fingerprintPK");
            if (!fingerprintPKVar)
            {
                AZ_WarningOnce(ConsoleChannel, false, "Could not find the column for :fingerprintPK for %s ", INSERT_PRODUCT_BY_FINGEPRINTPK);
                return;
            }
            int productNameVar = statement->GetNamedParamIdx(":productName");
            if (!productNameVar)
            {
                AZ_WarningOnce(ConsoleChannel, false, "Could not find the column for :productNameVar for %s ", INSERT_PRODUCT_BY_FINGEPRINTPK);
                return;
            }

            AZStd::string utf8Encoded;

            // now add the new set of products as new rows:
            for (const QString& newproduct : productList)
            {
                // convert to char * (must persist for lifetime of statement)
                statement->BindValueInt(fingerprintPKVar, results[0].first);  // rebind because we are resetting between each.
                utf8Encoded = newproduct.toUtf8().constData();
                statement->BindValueText(productNameVar, utf8Encoded.c_str());

                if (statement->Step() == Statement::SqlError)
                {
                    AZ_Warning(ConsoleChannel, false, "Failed to execute the INSERT_PRODUCT_BY_FINGEPRINTPK statment on pk %i for %s", results[0].first, utf8Encoded.c_str());
                    return;
                }
                statement->Reset();
            }

            // allow statement to exit scope here
        }

        transaction.Commit();
    }

    //! Clear the products for a given source.  This removes the entry entirely, not just sets it to empty.
    void DatabaseConnection::ClearProducts(const QString& sourceName, const QString& platform, const QString& jobDescription)
    {
        AZ_Error(ConsoleChannel, m_databaseConnection, "Fatal: attempt to work on a database connection that doesn't exist");
        AZ_Error(ConsoleChannel, m_databaseConnection->IsOpen(), "Fatal: attempt to work on a database connection that isn't open");

        if ((!m_databaseConnection) || (!m_databaseConnection->IsOpen()))
        {
            return;
        }

        AZStd::vector<FingerprintKeyValueType> results;

        if (!GetFingerprintsMatching(sourceName, platform, jobDescription, results))
        {
            return; // there can't be any such products.
        }

        ScopedTransaction transaction(m_databaseConnection);

        if (!ClearProductsByFingerprintPK(results[0].first))
        {
            return;
        }

        transaction.Commit();
    }

    bool DatabaseConnection::SearchMatchingFiles(const char* queryName, const char* searchKeyName, const char* resultColumn, const QString& searchTerm, QSet<QString>& output) const
    {
        AZ_Warning(ConsoleChannel, !searchTerm.isEmpty(), "Empty match string fed to GetMatchingProductFiles");

        StatementAutoFinalizer autoFinal(*m_databaseConnection, queryName);
        Statement* statement = autoFinal.Get();

        AZ_Assert(statement, "Statement not found: %s", queryName); // should never happen...

        int valueIdx = statement->GetNamedParamIdx(searchKeyName);
        if (!valueIdx)
        {
            AZ_Error(ConsoleChannel, false, "Could not find the %s parameter in %s", searchKeyName, queryName);
            return false;
        }
        AZStd::string bindableValue = searchTerm.toUtf8().constData(); // beacause constdata does not survive beyond the call its invoked in.
        statement->BindValueText(valueIdx, bindableValue.c_str());

        Statement::SqlStatus result = statement->Step();

        int resultColumnIdx = -1;

        while (result == Statement::SqlOK)
        {
            if (resultColumnIdx == -1)
            {
                resultColumnIdx = statement->FindColumn(resultColumn);
                if (resultColumnIdx == -1)
                {
                    AZ_Error(ConsoleChannel, false, "Results from %s failed to have a %s column", resultColumn, queryName);
                    return false;
                }
            }

            QString decoded = QString::fromUtf8(statement->GetColumnText(resultColumnIdx).c_str());
            output.insert(decoded);

            result = statement->Step();
        }

        AZ_Warning(ConsoleChannel, result != Statement::SqlError, "Error occurred during statement step of %s", queryName);
        return result != Statement::SqlError;
    }

    void DatabaseConnection::GetMatchingProductFiles(const QString& matchCheck, QSet<QString>& output) const
    {
        QString actualSearchTerm = matchCheck;
        actualSearchTerm.replace("%", "|%");
        actualSearchTerm.replace("_", "|_");
        actualSearchTerm.append("%"); // the actual wildcard

        SearchMatchingFiles(FIND_PRODUCTS_BY_PRODUCTNAME, ":productName", "ProductName", actualSearchTerm, output);
    }

    void DatabaseConnection::GetMatchingSourceFiles(const QString& matchCheck, QSet<QString>& output) const
    {
        QString actualSearchTerm = matchCheck;
        actualSearchTerm.replace("%", "|%");
        actualSearchTerm.replace("_", "|_");
        actualSearchTerm.append("%"); // the actual wildcard

        SearchMatchingFiles(FIND_SOURCES_BY_SOURCENAME, ":sourceName", "SourceName", actualSearchTerm, output);
    }

    void DatabaseConnection::GetSourceWithExtension(const QString& extension, QSet<QString>& output) const
    {
        QString actualSearchTerm = extension;
        actualSearchTerm.replace("%", "|%");
        actualSearchTerm.replace("_", "|_");
        actualSearchTerm = QString("%") + actualSearchTerm;

        SearchMatchingFiles(FIND_SOURCES_BY_SOURCENAME, ":sourceName", "SourceName", actualSearchTerm, output);
    }

    //! SetJobLogForSource updates the Job Log table to record the status of a particular job.
    //! It also sets all prior jobs that match that job exactly to not be the "latest one" but keeps them in the database.
    void DatabaseConnection::SetJobLogForSource(AZ::s64 jobId, const AZStd::string& sourceName, const AZStd::string& platform, const AZ::Uuid& builderUuid, const AZStd::string& jobKey, AzToolsFramework::AssetSystem::JobStatus status)
    {
        int fingerprintId = 0;
        StatementAutoFinalizer fin(*m_databaseConnection, GET_FINGERPRINT_NAME);
        Statement* statement = fin.Get();
        if (!statement)
        {
            AZ_Error(ConsoleChannel, false, "Unable to find SQL statement: %s", GET_FINGERPRINT_NAME);
            return;
        }

        int sourceNameIdx = statement->GetNamedParamIdx(":sourceName");
        int platformIdx = statement->GetNamedParamIdx(":platform");
        int jobDescriptionIdx = statement->GetNamedParamIdx(":jobDescription");
        if (!sourceNameIdx)
        {
            AZ_Error(ConsoleChannel, false, "could not find :sourceName in statement %s", GET_FINGERPRINT_NAME);
            return;
        }

        if (!platformIdx)
        {
            AZ_Error(ConsoleChannel, false, "could not find :platform in statement %s", GET_FINGERPRINT_NAME);
            return;
        }
        if (!jobDescriptionIdx)
        {
            AZ_Error(ConsoleChannel, false, "could not find :jobDescription in statement %s", GET_FINGERPRINT_NAME);
            return;
        }

        statement->BindValueText(sourceNameIdx, sourceName.c_str());
        statement->BindValueText(platformIdx, platform.c_str());
        statement->BindValueText(jobDescriptionIdx, jobKey.c_str());

        int resultColumn = -1;
        while (statement->Step() == Statement::SqlOK)
        {
            if (resultColumn == -1)
            {
                resultColumn = statement->FindColumn("FingerprintID");
                if (resultColumn == -1)
                {
                    AZ_Error(ConsoleChannel, false, "Unable to find the FingerprintID column while executing %s", GET_FINGERPRINT_NAME);
                    return;
                }
            }

            fingerprintId = statement->GetColumnInt(resultColumn);
            if (!fingerprintId)
            {
                AZ_Error(ConsoleChannel, false, "Was unable to decode FingerprintID while executing %s", GET_FINGERPRINT_NAME);
                return;
            }
        }

        ScopedTransaction transaction(m_databaseConnection);
        {
            // scope created for the // scope created for the statement auto-finalizers lifetime
            // update all job entries with the same fingerprintPK,platfrom,builderGuid and jobkey as being NOT THE LATEST ONE
            
            StatementAutoFinalizer updateJobLogFinalizer(*m_databaseConnection, SET_JOBS_NOT_LATEST);
            statement = updateJobLogFinalizer.Get();
            
            if (!statement)
            {
                AZ_Error(ConsoleChannel, false, "Unable to find SQL statement: %s", SET_JOBS_NOT_LATEST);
                return;
            }

            int fingerprintPKVar = statement->GetNamedParamIdx(":fingerprintPK");

            if (!fingerprintPKVar)
            {
                AZ_WarningOnce(ConsoleChannel, false, "Could not find the column for :fingerprintPK for %s ", SET_JOBS_NOT_LATEST);
                return;
            }

            int platformNameVar = statement->GetNamedParamIdx(":platform");
            if (!platformNameVar)
            {
                AZ_WarningOnce(ConsoleChannel, false, "Could not find the column for :platform for %s ", SET_JOBS_NOT_LATEST);
                return;
            }

            int builderGuidVar = statement->GetNamedParamIdx(":builderGuid");
            if (!builderGuidVar)
            {
                AZ_WarningOnce(ConsoleChannel, false, "Could not find the column for :builderGuid for %s ", SET_JOBS_NOT_LATEST);
                return;
            }

            int jobKeyVar = statement->GetNamedParamIdx(":jobKey");
            if (!jobKeyVar)
            {
                AZ_Error(ConsoleChannel, false, "could not find :jobKey in statement %s", SET_JOBS_NOT_LATEST);
                return;
            }

            statement->BindValueInt(fingerprintPKVar, fingerprintId);
            statement->BindValueText(platformNameVar, platform.c_str());
            statement->BindValueUuid(builderGuidVar, builderUuid);
            statement->BindValueText(jobKeyVar, jobKey.c_str());

            if (statement->Step() == Statement::SqlError)
            {
                AZ_Warning(ConsoleChannel, false, "Failed to execute %s to update jobLog", SET_JOBS_NOT_LATEST);
                return;
            }
        }

        {
            // scope created for the statement auto-finalizers lifetime
            StatementAutoFinalizer autoFinalizer(*m_databaseConnection, INSERT_JOBLOG_BY_FINGEPRINTPK);
            statement = autoFinalizer.Get();

            int jobIdVar = statement->GetNamedParamIdx(":jobId");
            if (!jobIdVar)
            {
                AZ_WarningOnce(ConsoleChannel, false, "Could not find the column for :jobId for %s ", INSERT_JOBLOG_BY_FINGEPRINTPK);
                return;
            }

            int fingerprintPKVar = statement->GetNamedParamIdx(":fingerprintPK");
            if (!fingerprintPKVar)
            {
                AZ_WarningOnce(ConsoleChannel, false, "Could not find the column for :fingerprintPK for %s ", INSERT_JOBLOG_BY_FINGEPRINTPK);
                return;
            }
            int platformNameVar = statement->GetNamedParamIdx(":platform");
            if (!platformNameVar)
            {
                AZ_WarningOnce(ConsoleChannel, false, "Could not find the column for :platform for %s ", INSERT_JOBLOG_BY_FINGEPRINTPK);
                return;
            }

            int builderGuidVar = statement->GetNamedParamIdx(":builderGuid");
            if (!builderGuidVar)
            {
                AZ_Error(ConsoleChannel, false, "Unable to find the :builderGuid column while executing %s", INSERT_JOBLOG_BY_FINGEPRINTPK);
                return;
            }

            int jobKeyVar = statement->GetNamedParamIdx(":jobKey");
            if (!jobKeyVar)
            {
                AZ_Error(ConsoleChannel, false, "Unable to find the :jobKeyVar column while executing %s", INSERT_JOBLOG_BY_FINGEPRINTPK);
                return;
            }

            int jobStatusVar = statement->GetNamedParamIdx(":status");
            if (!jobStatusVar)
            {
                AZ_WarningOnce(ConsoleChannel, false, "Could not find the column for :status for %s ", INSERT_JOBLOG_BY_FINGEPRINTPK);
                return;
            }

            int latestVar = statement->GetNamedParamIdx(":latest");
            if (!latestVar)
            {
                AZ_Error(ConsoleChannel, false, "Unable to find the Latest column while executing %s", INSERT_JOBLOG_BY_FINGEPRINTPK);
                return;
            }
            AZStd::string utf8EncodedPlatform;
            AZStd::string utf8EncodeJobKey;

            statement->BindValueInt64(jobIdVar, jobId);
            statement->BindValueInt(fingerprintPKVar, fingerprintId);
            statement->BindValueText(platformNameVar, platform.c_str());

            statement->BindValueUuid(builderGuidVar, builderUuid);

            statement->BindValueText(jobKeyVar, jobKey.c_str());
            statement->BindValueInt(jobStatusVar, static_cast<int>(status));
            statement->BindValueInt(latestVar, 1);
            if (statement->Step() == Statement::SqlError)
            {
                char tempBuffer[128];
                builderUuid.ToString(tempBuffer, 128);
                AZ_Warning(ConsoleChannel, false, "Failed to execute the INSERT_JOBLOG_BY_FINGEPRINTPK statment on pk %i for platform %s and builderguid %s", fingerprintId, platform.c_str(), tempBuffer);
                return;
            }
        }

        transaction.Commit();
    }

    void DatabaseConnection::GetJobInfosForSource(const AZStd::string sourceName, JobLogCallback jobsFunction)
    {
        int fingerprintId = 0;
        StatementAutoFinalizer fin(*m_databaseConnection, FIND_JOB_INFOS_BY_SOURCENAME);
        Statement* statement = fin.Get();
        if (!statement)
        {
            AZ_Error(ConsoleChannel, false, "Unable to find SQL statement: %s", FIND_JOB_INFOS_BY_SOURCENAME);
            return;
        }

        int sourceNameIdx = statement->GetNamedParamIdx(":sourceName");
        if (!sourceNameIdx)
        {
            AZ_Error(ConsoleChannel, false, "could not find :sourceName in statement %s", FIND_JOB_INFOS_BY_SOURCENAME);
            return;
        }

        statement->BindValueText(sourceNameIdx, sourceName.c_str());

        // create a JobInfo
        AzToolsFramework::AssetSystem::JobInfo info;
        // we're going to need all the fields:
        // the Job ID is the unique ID that you would use to ask for logs for that job.
        unsigned int m_jobId = 0;

        AZStd::string m_platform;
        AZStd::string m_builderGuid;

        // Job Key is arbitrarily defined by the builder.  Used to differentiate between different jobs emitted for the same input file, for the same platform, for the same builder.
        // for example, you might want to split a particularly complicated and time consuming job into multiple sub-jobs.  In which case they'd all have the same input file,
        // the same platform, the same builder UUID (since its the UUID of the builder itself)
        // but would have different job keys.
        AZStd::string m_jobKey;
        JobStatus m_status = JobStatus::Queued;
        bool m_isLatest = true;

        AzToolsFramework::AssetSystem::JobInfo currentInfo;

        Statement::SqlStatus result = statement->Step();
        while (result == Statement::SqlOK)
        {
            //  we select by JobLog.JobID, JobLog.Platform, JobLog.BuilderGuid, JobLog.JobKey, JobLog.Status, LegacyFingerprints.SourceName, Latest
            currentInfo.m_jobId = statement->GetColumnInt(0);
            currentInfo.m_platform = statement->GetColumnText(1);
            currentInfo.m_builderGuid = statement->GetColumnUuid(2);
            currentInfo.m_jobKey = statement->GetColumnText(3);
            currentInfo.m_status = static_cast<AzToolsFramework::AssetSystem::JobStatus>(statement->GetColumnInt(4));
            currentInfo.m_sourceFile = statement->GetColumnText(5);
            int latest = statement->GetColumnInt(6);

            // we're manually filtering on latest here because of the type of data
            // Latest=1 is the usual situation.
            if (latest != 0)
            {
                if (!jobsFunction(currentInfo))
                {
                    break;
                }
            }
            result = statement->Step();
        }

        AZ_Warning(ConsoleChannel, result != Statement::SqlError, "Error occurred while stepping %s", FIND_PRODUCTS_BY_PLATFORM);
    }


    void DatabaseConnection::GetJobInfosForJobLogPK(int jobLogPK, JobLogCallback jobsFunction)
    {
        int fingerprintId = 0;
        StatementAutoFinalizer fin(*m_databaseConnection, FIND_JOB_INFOS_BY_JOBLOG_PK);
        Statement* statement = fin.Get();
        if (!statement)
        {
            AZ_Error(ConsoleChannel, false, "Unable to find SQL statement: %s", FIND_JOB_INFOS_BY_JOBLOG_PK);
            return;
        }

        int jobLogPKIndex = statement->GetNamedParamIdx(":jobLogPK");
        if (!jobLogPKIndex)
        {
            AZ_Error(ConsoleChannel, false, "could not find :jobLogPK in statement %s", FIND_JOB_INFOS_BY_JOBLOG_PK);
            return;
        }

        statement->BindValueInt(jobLogPKIndex, jobLogPK);

        // create a JobInfo
        AzToolsFramework::AssetSystem::JobInfo info;
        // we're going to need all the fields:
        // the Job ID is the unique ID that you would use to ask for logs for that job.
        unsigned int m_jobId = 0;

        AZStd::string m_platform;
        AZStd::string m_builderGuid;

        // Job Key is arbitrarily defined by the builder.  Used to differentiate between different jobs emitted for the same input file, for the same platform, for the same builder.
        // for example, you might want to split a particularly complicated and time consuming job into multiple sub-jobs.  In which case they'd all have the same input file,
        // the same platform, the same builder UUID (since its the UUID of the builder itself)
        // but would have different job keys.
        AZStd::string m_jobKey;
        JobStatus m_status = JobStatus::Queued;
        bool m_isLatest = true;

        AzToolsFramework::AssetSystem::JobInfo currentInfo;

        Statement::SqlStatus result = statement->Step();
        while (result == Statement::SqlOK)
        {
            //  we select by JobLog.JobID, JobLog.Platform, JobLog.BuilderGuid, JobLog.JobKey, JobLog.Status
            currentInfo.m_jobId = statement->GetColumnInt(0);
            currentInfo.m_platform = statement->GetColumnText(1);
            currentInfo.m_builderGuid = statement->GetColumnUuid(2);
            currentInfo.m_jobKey = statement->GetColumnText(3);
            currentInfo.m_status = static_cast<AzToolsFramework::AssetSystem::JobStatus>(statement->GetColumnInt(4));
            currentInfo.m_sourceFile = statement->GetColumnText(5);
            if (!jobsFunction(currentInfo))
            {
                break;
            }
            result = statement->Step();
        }

        AZ_Warning(ConsoleChannel, result != Statement::SqlError, "Error occurred while stepping %s", FIND_PRODUCTS_BY_PLATFORM);
    }

    void DatabaseConnection::EnumeratePlatformProducts(const QString& platform, PlatformProductHandler handler) const
    {
        AZ_Warning(ConsoleChannel, !platform.isEmpty(), "Empty platform given to EnumeratePlatformProducts");

        StatementAutoFinalizer autoFinal(*m_databaseConnection, FIND_PRODUCTS_BY_PLATFORM);
        Statement* statement = autoFinal.Get();

        int platformIdx = statement->GetNamedParamIdx(":platform");
        if (!platformIdx)
        {
            AZ_Error(ConsoleChannel, false, "Could not find :platform parameter in %s", FIND_PRODUCTS_BY_PLATFORM);
            return;
        }

        AZStd::string boundPlatform = platform.toUtf8().constData();
        statement->BindValueText(platformIdx, boundPlatform.c_str());

        Statement::SqlStatus result = statement->Step();
        while (result == Statement::SqlOK)
        {
            QString productPath = QString::fromUtf8(statement->GetColumnText(0).c_str());
            handler(productPath);
            result = statement->Step();
        }

        AZ_Warning(ConsoleChannel, result != Statement::SqlError, "Error occurred while stepping %s", FIND_PRODUCTS_BY_PLATFORM);
    }

    void DatabaseConnection::VacuumAndAnalyze()
    {
        if (m_databaseConnection)
        {
            m_databaseConnection->ExecuteOneOffStatement("VACUUM");
            m_databaseConnection->ExecuteOneOffStatement("ANALYZE");
        }
    }
}//namespace AssetProcessor

