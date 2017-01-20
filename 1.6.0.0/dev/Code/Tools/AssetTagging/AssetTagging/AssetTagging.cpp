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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

// Description : Defines the exported functions for the DLL application.


#include "stdafx.h"
#include "AssetTagging.h"
#include "DBAPI.h"
#include "IDBConnection.h"
#include "DBAPIHelpers.h"
#include <string.h>
#include <vector>

IDBConnection* m_pConnection = NULL;
IDBConnection* m_pReadConnection = NULL;
int m_nDBType = 0;
int m_nDBPort = 0;
CString m_strDBConnection;
CString m_strDBReadConnection;
CString m_strDBName;
CString m_strDBUser;
CString m_strDBPassword;
CString m_strError;

const int m_AssetTagging_MaxStringLen = 1024;

bool CreateDatabase(IDBConnection* pConn, const CString& schemafile, int type, const CString& dbname)
{
    CString fullfilename(schemafile);
    switch (type)
    {
    case eDT_SQLite:
        fullfilename.Append("_sqlite.sql");
        break;
    default:
        fullfilename.Append("_sqlite.sql");
        break;
    }

    FILE* fp = fopen(fullfilename.GetBuffer(), "r");
    if (!fp)
    {
        return false;
    }

    fseek (fp, 0, SEEK_END);
    int nFileSize = ftell(fp) + 1;
    rewind (fp);


    char* sqlSchema = new char[nFileSize];
    memset(sqlSchema, '\0', nFileSize);
    size_t nFileRead = fread(sqlSchema, 1, nFileSize - 1, fp);
    fclose(fp);

    if (sqlSchema[0] == '\0')
    {
        return false;
    }

    std::vector<CString> commands;
    int curPos = 0;

    CString str(sqlSchema);
    CString resToken = str.Tokenize(";", curPos);

    while (resToken != "")
    {
        commands.push_back(resToken);
        curPos += 1;
        resToken = str.Tokenize(";", curPos);
    }

    delete [] sqlSchema;
    sqlSchema = NULL;

    IDBStatement* pStatement = pConn->CreateStatement();
    if (!pStatement)
    {
        return false;
    }

    bool ret = true;
    for (std::vector<CString>::iterator item = commands.begin(), end = commands.end(); item != end; ++item)
    {
        if (!pStatement->Execute((*item)))
        {
            printf("Error retrieving result set for query: %s", pConn->GetRawErrorMessage());
            ret = false;
        }
    }

    pConn->DestroyStatement(pStatement);
    return ret;
}

IDBConnection* OpenConnection(const char* localpath)
{
    m_strError = "OpenConnection called";
    if (m_pConnection)
    {
        return m_pConnection;
    }

    m_strError = "Creating new connection";
    IDBConnection* pConn = DriverManager::CreateConnection((EDBType)m_nDBType, m_strDBConnection, m_nDBPort, m_strDBName, m_strDBUser, m_strDBPassword, localpath);
    if (pConn)
    {
        m_strError = "connection created";
        pConn->SetAutoCommit(true);

        if (!DriverManager::DoesDatabaseExist(pConn, (EDBType)m_nDBType, m_strDBName))
        {
            m_strError = "creating asset database";
            CreateDatabase(pConn, "Editor/cryassetdb", m_nDBType, m_strDBName);
        }

        m_strError = "opened connection";

        m_pConnection = pConn;
        /*
        TODO: MySQL has been removed, so this line should not be possible.  Need to determine why m_pReadConnection is only set for MySQL (and not for SQL Lite)
        if ( m_nDBType == eDT_MySQL && m_strDBReadConnection.IsEmpty() == false)
        {
            m_pReadConnection = DriverManager::CreateConnection((EDBType)m_nDBType, m_strDBReadConnection, m_nDBPort, m_strDBName, m_strDBUser, m_strDBPassword, localpath);
        }*/
    }
    else
    {
        m_strError = "Failed to create connection";
    }

    return m_pConnection;
}

bool GetDBCredentials()
{
    HKEY  hKey;
    bool setup = true;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Amazon\\Lumberyard\\Settings", 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS)
    {
        setup = false;
    }

    if (setup == true)
    {
        unsigned long type = REG_SZ, size = 1024;
        char buffer[1024];
        memset(buffer, '\0', sizeof(buffer));
        size = 1024;
        if (RegQueryValueEx(hKey, "EDT_DatabaseConnection", NULL, &type, (LPBYTE)&buffer[0], &size) != ERROR_SUCCESS)
        {
            setup = false;
        }
        else
        {
            m_strDBConnection.Format("%s", buffer);
        }

        memset(buffer, '\0', sizeof(buffer));
        size = 1024;
        if (RegQueryValueEx(hKey, "EDT_DatabaseReadConnection", NULL, &type, (LPBYTE)&buffer[0], &size) == ERROR_SUCCESS)
        {
            m_strDBReadConnection.Format("%s", buffer);
        }

        memset(buffer, '\0', sizeof(buffer));
        size = 1024;
        int ret = RegQueryValueEx(hKey, "EDT_DatabaseName", NULL, &type, (LPBYTE)&buffer[0], &size);
        if (ret != ERROR_SUCCESS)
        {
            setup = false;
        }
        else
        {
            m_strDBName.Format("%s", buffer);
        }

        memset(buffer, '\0', sizeof(buffer));
        size = 1024;
        if (RegQueryValueEx(hKey, "EDT_DatabaseUser", NULL, &type, (LPBYTE)&buffer[0], &size) != ERROR_SUCCESS)
        {
            setup = false;
        }
        else
        {
            m_strDBUser.Format("%s", buffer);
        }

        memset(buffer, '\0', sizeof(buffer));
        size = 1024;
        if (RegQueryValueEx(hKey, "EDT_DatabasePassword", NULL, &type, (LPBYTE)&buffer[0], &size) != ERROR_SUCCESS)
        {
            setup = false;
        }
        else
        {
            m_strDBPassword.Format("%s", buffer);
        }

        DWORD dwType, dwVal = 0, dwSize = sizeof(dwVal);
        if (RegQueryValueEx((HKEY)hKey, "EDT_DatabaseType", NULL, &dwType, (BYTE*)&dwVal, &dwSize) != ERROR_SUCCESS)
        {
            setup = false;
        }
        else
        {
            m_nDBType = dwVal;
        }

        if (RegQueryValueEx(hKey, "EDT_DatabasePort", NULL, &dwType, (BYTE*)&dwVal, &dwSize) != ERROR_SUCCESS)
        {
            setup = false;
        }
        else
        {
            m_nDBPort = dwVal;
        }

        RegCloseKey(hKey);
    }

    if (setup == false)
    {
        m_strDBName.Format("%s", "CryAssetDB");
        m_nDBType = 5;
    }

    return true;
}


//------------------------------------

ASSETTAGGING_API int AssetTagging_MaxStringLen()
{
    return m_AssetTagging_MaxStringLen;
}

ASSETTAGGING_API bool AssetTagging_Initialize(const char* localpath)
{
    if (GetDBCredentials())
    {
        if (OpenConnection(localpath))
        {
            return true;
        }
    }
    return false;
}

ASSETTAGGING_API bool AssetTagging_IsLocal()
{
    if (!m_pConnection)
    {
        return true;
    }

    return m_pConnection->IsLocal();
}

ASSETTAGGING_API void AssetTagging_CloseConnection()
{
    if (!m_pConnection)
    {
        return;
    }

    DriverManager::DestroyConnection(m_pConnection);
}


ASSETTAGGING_API int AssetTagging_CreateTag(const char* tag, const char* category, const char* project)
{
    if (!m_pConnection)
    {
        return 0;
    }

    unsigned int id = AssetTagging_TagExists(tag, category, project);
    if (id > 0)
    {
        return id;
    }

    int categoryid = AssetTagging_CreateCategory(category, project);
    if (categoryid == 0)
    {
        return 0;
    }

    IDBStatement* pStatement = m_pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("INSERT INTO `tags` (`tag`,`category_id`) VALUES (\"%s\",%d) ", tag, categoryid);
        if (!pStatement->Execute(strQuery))
        {
            printf("Error executing query: %s", m_pConnection->GetRawErrorMessage());
        }
        m_pConnection->DestroyStatement(pStatement);
    }

    return AssetTagging_TagExists(tag, category, project);
}

ASSETTAGGING_API int AssetTagging_CreateAsset(const char* path, const char* project)
{
    int assetid = AssetTagging_AssetExists(path, project);
    if (assetid > 0)
    {
        return assetid;
    }

    int projectId = AssetTagging_CreateProject(project);
    if (projectId == 0)
    {
        return 0;
    }

    if (!m_pConnection)
    {
        return assetid;
    }

    IDBStatement* pStatement = m_pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("INSERT INTO `asset_inventory` (`relpath`,`project_id`) VALUES (\"%s\",%d)", path, projectId);
        if (!pStatement->Execute(strQuery))
        {
            printf("Error executing query: %s", m_pConnection->GetRawErrorMessage());
        }
        m_pConnection->DestroyStatement(pStatement);
    }

    return AssetTagging_AssetExists(path, project);
}

ASSETTAGGING_API int AssetTagging_CreateProject(const char* project)
{
    uint32 projectId = AssetTagging_ProjectExists(project);
    if (projectId > 0)
    {
        return projectId;
    }

    if (!m_pConnection)
    {
        return 0;
    }

    IDBStatement* pStatement = m_pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("INSERT INTO `projects` (`name`) VALUES (\"%s\")", project);
        if (!pStatement->Execute(strQuery))
        {
            printf("Error executing query: %s", m_pConnection->GetRawErrorMessage());
        }
        m_pConnection->DestroyStatement(pStatement);
    }

    return AssetTagging_ProjectExists(project);
}

ASSETTAGGING_API int AssetTagging_CreateCategory(const char* category, const char* project)
{
    uint32 categoryId = AssetTagging_CategoryExists(category, project);
    if (categoryId > 0)
    {
        return categoryId;
    }

    uint32 projectId = AssetTagging_CreateProject(project);
    if (projectId == 0)
    {
        return 0;
    }

    if (!m_pConnection)
    {
        return 0;
    }

    IDBStatement* pStatement = m_pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("INSERT INTO `categories` (`category`,`project_id`) VALUES (\"%s\",%d)", category, projectId);
        if (!pStatement->Execute(strQuery))
        {
            printf("Error executing query: %s", m_pConnection->GetRawErrorMessage());
        }
        m_pConnection->DestroyStatement(pStatement);
    }

    return AssetTagging_ProjectExists(category);
}

ASSETTAGGING_API void AssetTagging_AddAssetsToTag(const char* tag, const char* category, const char* project, char** assets, int nAssets)
{
    int tagid = AssetTagging_CreateTag(tag, category, project);
    if (tagid == 0)
    {
        return;
    }

    std::vector<int> ids;
    for (int idx = 0; idx < nAssets; ++idx)
    {
        ids.push_back(AssetTagging_CreateAsset(assets[idx], project));
    }

    if (!m_pConnection)
    {
        return;
    }

    IDBStatement* pStatement = m_pConnection->CreateStatement();
    if (pStatement)
    {
        for (std::vector<int>::iterator item = ids.begin(), end = ids.end(); item != end; ++item)
        {
            if ((*item) == 0)
            {
                continue;
            }

            CString strQuery;
            strQuery.Format("INSERT INTO `asset_tags` (`asset_id`,`tag_id`) VALUES (%d,%d)", (*item), tagid);
            if (!pStatement->Execute(strQuery))
            {
                printf("Error executing query: %s", m_pConnection->GetRawErrorMessage());
            }
        }
        m_pConnection->DestroyStatement(pStatement);
    }
}

ASSETTAGGING_API void AssetTagging_RemoveAssetsFromTag(const char* tag, const char* category, const char* project, char** assets, int nAssets)
{
    if (!m_pConnection)
    {
        return;
    }

    std::vector<uint32> assetids;
    for (int idx = 0; idx < nAssets; ++idx)
    {
        assetids.push_back(AssetTagging_AssetExists(assets[idx], project));
    }

    uint32 tagid = AssetTagging_TagExists(tag, category, project);
    if (tagid == 0)
    {
        return;
    }

    IDBStatement* pStatement = m_pConnection->CreateStatement();
    if (pStatement)
    {
        for (std::vector<uint32>::const_iterator item = assetids.begin(), end = assetids.end(); item != end; ++item)
        {
            if ((*item) == 0)
            {
                continue;
            }

            CString strQuery;
            strQuery.Format("DELETE FROM `asset_tags` WHERE `asset_tags`.`asset_id` = %d AND `asset_tags`.`tag_id` = %d", (*item), tagid);
            if (!pStatement->Execute(strQuery))
            {
                printf("Error executing query: %s", m_pConnection->GetRawErrorMessage());
            }
        }
        m_pConnection->DestroyStatement(pStatement);
    }
}

ASSETTAGGING_API void AssetTagging_RemoveTagFromAsset(const char* tag, const char* category, const char* project, const char* asset)
{
    if (!m_pConnection)
    {
        return;
    }

    int assetid = AssetTagging_AssetExists(asset, project);
    if (assetid == 0)
    {
        return;
    }

    uint32 tagid = AssetTagging_TagExists(tag, category, project);
    if (tagid == 0)
    {
        return;
    }

    IDBStatement* pStatement = m_pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("DELETE FROM `asset_tags` WHERE `asset_tags`.`asset_id` = %d AND `asset_tags`.`tag_id` = %d", assetid, tagid);
        if (!pStatement->Execute(strQuery))
        {
            printf("Error executing query: %s", m_pConnection->GetRawErrorMessage());
        }
        m_pConnection->DestroyStatement(pStatement);
    }
}

ASSETTAGGING_API void AssetTagging_DestroyTag(const char* tag, const char* project)
{
    if (!m_pConnection)
    {
        return;
    }

    IDBStatement* pStatement = m_pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        //strQuery.Format("DELETE FROM `tags`,`categories`,`projects` WHERE `tag` = \"%s\" AND `tags`.`category_id` = `categories`.`category_id` AND `categories`.`project_id` = `projects`.`id` AND `projects`.`name` = \"%s\"", tag, project);
        strQuery.Format("DELETE FROM `tags` WHERE `tags`.`id` = (SELECT `tags`.`id` FROM `categories`,`projects` WHERE `tag` = \"%s\" AND `tags`.`category_id` = `categories`.`category_id` AND `categories`.`project_id` = `projects`.`id` AND `projects`.`name` = \"%s\")", tag, project);
        if (!pStatement->Execute(strQuery))
        {
            printf("Error executing query: %s", m_pConnection->GetRawErrorMessage());
        }
        m_pConnection->DestroyStatement(pStatement);
    }
}


ASSETTAGGING_API void AssetTagging_DestroyCategory(const char* category, const char* project)
{
    if (!m_pConnection)
    {
        return;
    }

    IDBStatement* pStatement = m_pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("DELETE FROM `categories` WHERE `categories`.`category_id` = (SELECT `categories`.`category_id` FROM `projects` WHERE `projects`.`name` = \"%s\" AND `projects`.`id` = `categories`.`project_id` AND `categories`.`category` = \"%s\")", project, category);
        //strQuery.Format("DELETE FROM `categories`,`projects` WHERE `category` = \"%s\" AND `categories`.`project_id` = `projects`.`id` AND `projects`.`name` = \"%s\"", category, project);
        if (!pStatement->Execute(strQuery))
        {
            printf("Error executing query: %s", m_pConnection->GetRawErrorMessage());
        }
        m_pConnection->DestroyStatement(pStatement);
    }
}

ASSETTAGGING_API int AssetTagging_GetNumTagsForAsset(const char* asset, const char* project)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    int ret = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT count(tag) FROM `tags`,`asset_tags`,`asset_inventory`,`projects`,`categories` WHERE `asset_inventory`.`relpath`=\"%s\" AND `asset_inventory`.`id` = `asset_tags`.`asset_id` AND `asset_tags`.`tag_id` = `tags`.`id` AND `tags`.`category_id` = `categories`.`category_id` AND `categories`.`project_id` = `projects`.`id` AND `projects`.`name` = \"%s\"", asset, project);

        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();
                if (retrieved)
                {
                    pResultSet->GetInt32ByIndex(0, ret);
                }
                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return ret;
}

ASSETTAGGING_API int AssetTagging_GetTagsForAsset(char** tags, int nTags, const char* asset, const char* project)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    int count = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT `tag` FROM `tags`,`asset_tags`,`asset_inventory`,`projects`,`categories` WHERE `asset_inventory`.`relpath`=\"%s\" AND `asset_inventory`.`id` = `asset_tags`.`asset_id` AND `asset_tags`.`tag_id` = `tags`.`id` AND `tags`.`category_id` = `categories`.`category_id` AND `categories`.`project_id` = `projects`.`id` AND `projects`.`name` = \"%s\"", asset, project);

        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();
                const char* strValue = NULL;

                while (retrieved && nTags > 0 && count < nTags)
                {
                    if (pResultSet->GetStringByName("tag", strValue))
                    {
                        _snprintf(tags[count], m_AssetTagging_MaxStringLen, "%s", strValue);
                    }

                    retrieved = pResultSet->Next();
                    count++;
                }

                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return count;
}


ASSETTAGGING_API int AssetTagging_GetTagForAssetInCategory(char* tag, const char* asset, const char* category, const char* project)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    int count = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT `tag` FROM `tags`,`asset_tags`,`asset_inventory`,`categories`,`projects` WHERE `asset_inventory`.`relpath`=\"%s\" AND `asset_inventory`.`id` = `asset_tags`.`asset_id` AND `asset_tags`.`tag_id` = `tags`.`id` AND `tags`.`category_id` = `categories`.`category_id` AND `categories`.`category` = \"%s\" AND `categories`.`project_id` = `projects`.`id` AND `projects`.`name` = \"%s\" LIMIT 1", asset, category, project);

        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                const char* strValue = NULL;

                if (pResultSet->Next())
                {
                    if (pResultSet->GetStringByName("tag", strValue))
                    {
                        _snprintf(tag, m_AssetTagging_MaxStringLen, "%s", strValue);
                        count++;
                    }
                }
                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return count;
}


ASSETTAGGING_API int AssetTagging_GetNumAssetsForTag(const char* tag, const char* project)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    int ret = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT count(relpath) FROM `asset_inventory`,`asset_tags`,`tags`,`categories`,`projects` WHERE `tags`.`tag`=\"%s\" AND `tags`.`id` = `asset_tags`.`tag_id` AND `asset_tags`.`asset_id` = `asset_inventory`.`id` AND `tags`.`category_id` = `categories`.`category_id` AND `categories`.`project_id` = `projects`.`id` AND `projects`.`name` = \"%s\"", tag, project);

        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();
                if (retrieved)
                {
                    pResultSet->GetInt32ByIndex(0, ret);
                }
                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return ret;
}

ASSETTAGGING_API int AssetTagging_GetAssetsForTag(char** assets, int nAssets, const char* tag, const char* project)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    int count = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT `asset_inventory`.`relpath` FROM `asset_inventory`,`asset_tags`,`tags`,`categories`,`projects` WHERE `tags`.`tag`=\"%s\" AND `tags`.`id` = `asset_tags`.`tag_id` AND `asset_tags`.`asset_id` = `asset_inventory`.`id` AND `tags`.`category_id` = `categories`.`category_id` AND `categories`.`project_id` = `projects`.`id` AND `projects`.`name` = \"%s\"", tag, project);

        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();

                while (retrieved && nAssets > 0 && count < nAssets)
                {
                    const char* strValue = NULL;

                    if (pResultSet->GetStringByName("relpath", strValue))
                    {
                        _snprintf(assets[count], m_AssetTagging_MaxStringLen, "%s", strValue);
                    }

                    retrieved = pResultSet->Next();
                    count++;
                }
                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return count;
}

ASSETTAGGING_API int AssetTagging_GetNumAssetsWithDescription(const char* description)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    int count = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT count(relpath) FROM `asset_inventory` WHERE `asset_inventory`.`description` LIKE \"%%%s%%\"", description);

        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();
                if (retrieved)
                {
                    pResultSet->GetInt32ByIndex(0, count);
                }
                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return count;
}

ASSETTAGGING_API int AssetTagging_GetAssetsWithDescription(char** assets, int nAssets, const char* description)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    int count = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT `asset_inventory`.`relpath` FROM `asset_inventory` WHERE `asset_inventory`.`description` LIKE \"%%%s%%\"", description);

        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();

                while (retrieved && nAssets > 0 && count < nAssets)
                {
                    const char* strValue = NULL;

                    if (pResultSet->GetStringByName("relpath", strValue))
                    {
                        _snprintf(assets[count], m_AssetTagging_MaxStringLen, "%s", strValue);
                    }

                    retrieved = pResultSet->Next();
                    count++;
                }
                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return count;
}

ASSETTAGGING_API int AssetTagging_GetNumTags(const char* project)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    int ret = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT count(tag) FROM `tags`,`categories`,`projects` WHERE `tags`.`category_id` = `categories`.`category_id` AND `categories`.`project_id` = `projects`.`id` AND `projects`.`name` = \"%s\"", project);
        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();
                if (retrieved)
                {
                    pResultSet->GetInt32ByIndex(0, ret);
                }

                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return ret;
}

ASSETTAGGING_API int AssetTagging_GetAllTags(char** tags, int nTags, const char* project)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    int count = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT `tag` FROM `tags`,`categories`,`projects` WHERE `tags`.`category_id` = `categories`.`category_id` AND `categories`.`project_id` = `projects`.`id` AND `projects`.`name` = \"%s\"", project);
        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();

                while (retrieved && nTags > 0 && count < nTags)
                {
                    const char* strValue = NULL;

                    if (pResultSet->GetStringByName("tag", strValue))
                    {
                        _snprintf(tags[count], m_AssetTagging_MaxStringLen, "%s", strValue);
                    }

                    retrieved = pResultSet->Next();
                    count++;
                }

                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return count;
}


ASSETTAGGING_API int AssetTagging_GetNumCategories(const char* project)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    int ret = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT count(category) FROM `categories`,`projects` WHERE `categories`.`project_id` = `projects`.`id` AND `projects`.`name` = \"%s\"", project);
        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();
                if (retrieved)
                {
                    pResultSet->GetInt32ByIndex(0, ret);
                }

                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return ret;
}

ASSETTAGGING_API int AssetTagging_GetAllCategories(const char* project, char** categories, int nCategories)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    int count = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT `category` FROM `categories`,`projects` WHERE `categories`.`project_id` = `projects`.`id` AND `projects`.`name` = \"%s\" ORDER BY `order_id` ASC", project);
        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();

                while (retrieved && nCategories > 0 && count < nCategories)
                {
                    const char* strValue = NULL;

                    if (pResultSet->GetStringByName("category", strValue))
                    {
                        _snprintf(categories[count], m_AssetTagging_MaxStringLen, "%s", strValue);
                    }

                    retrieved = pResultSet->Next();
                    count++;
                }

                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return count;
}

ASSETTAGGING_API int AssetTagging_GetNumTagsForCategory(const char* category, const char* project)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    int ret = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT count(tag) FROM `categories`,`tags`,`projects` WHERE `categories`.`category` = \"%s\" AND `categories`.`category_id` = `tags`.`category_id` AND `categories`.`project_id` = `projects`.`id` AND `projects`.`name` = \"%s\"", category, project);
        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();
                if (retrieved)
                {
                    pResultSet->GetInt32ByIndex(0, ret);
                }

                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return ret;
}

ASSETTAGGING_API int AssetTagging_GetTagsForCategory(const char* category, const char* project, char** tags, int nTags)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    int count = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT tag FROM `categories`,`tags`,`projects` WHERE `categories`.`category` = \"%s\" AND `categories`.`category_id` = `tags`.`category_id` AND `categories`.`project_id` = `projects`.`id` AND `projects`.`name` = \"%s\" ORDER BY `tags`.`tag` ASC", category, project);
        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();

                while (retrieved && nTags > 0 && count < nTags)
                {
                    const char* strValue = NULL;

                    if (pResultSet->GetStringByName("tag", strValue))
                    {
                        _snprintf(tags[count], m_AssetTagging_MaxStringLen, "%s", strValue);
                    }

                    retrieved = pResultSet->Next();
                    count++;
                }

                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return count;
}


ASSETTAGGING_API int AssetTagging_GetNumProjects()
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    int ret = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT count(id) FROM `projects`");
        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();
                if (retrieved)
                {
                    pResultSet->GetInt32ByIndex(0, ret);
                }

                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return ret;
}

ASSETTAGGING_API int AssetTagging_GetProjects(char** projects, int nProjects)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    int count = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT `name` FROM `projects`");
        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();

                while (retrieved && nProjects > 0 && count < nProjects)
                {
                    const char* strValue = NULL;

                    if (pResultSet->GetStringByName("name", strValue))
                    {
                        _snprintf(projects[count], m_AssetTagging_MaxStringLen, "%s", strValue);
                    }

                    retrieved = pResultSet->Next();
                    count++;
                }

                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return count;
}


ASSETTAGGING_API int AssetTagging_TagExists(const char* tag, const char* category, const char* project)
{
    int categoryid = AssetTagging_CreateCategory(category, project);
    if (categoryid == 0)
    {
        return 0;
    }

    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    uint32 id = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT id FROM `tags`,`categories` WHERE `tags`.`tag` = \"%s\" AND `tags`.`category_id` = %d LIMIT 1", tag, categoryid);
        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();
                if (retrieved && pResultSet->GetNumRows() > 0)
                {
                    pResultSet->GetUInt32ByName("id", id);
                }
                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return id;
}

ASSETTAGGING_API int AssetTagging_AssetExists(const char* relpath, const char* project)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    int projectid = AssetTagging_CreateProject(project);
    if (projectid == 0)
    {
        return 0;
    }

    uint32 id = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT id FROM asset_inventory WHERE asset_inventory.relpath = \"%s\" AND asset_inventory.project_id = %d LIMIT 1", relpath, projectid);
        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();
                if (retrieved && pResultSet->GetNumRows() > 0)
                {
                    pResultSet->GetUInt32ByName("id", id);
                }
                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return id;
}

ASSETTAGGING_API int AssetTagging_ProjectExists(const char* project)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    uint32 id = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT id FROM projects WHERE projects.name = \"%s\" LIMIT 1", project);

        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();
                if (retrieved && pResultSet->GetNumRows() > 0)
                {
                    pResultSet->GetUInt32ByName("id", id);
                }
                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return id;
}

ASSETTAGGING_API int AssetTagging_CategoryExists(const char* category, const char* project)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    uint32 id = 0;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT category_id FROM `categories`,`projects` WHERE `categories`.`category` = \"%s\" AND `categories`.`project_id` = `projects`.`id` AND `projects`.`name` = \"%s\" LIMIT 1", category, project);

        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();
                if (retrieved && pResultSet->GetNumRows() > 0)
                {
                    pResultSet->GetUInt32ByName("category_id", id);
                }
                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return id;
}

ASSETTAGGING_API int AssetTagging_GetErrorString(LPSTR errorString, int nLen)
{
    _snprintf(errorString, nLen, "%s", m_strError.GetString());
    return m_strError.IsEmpty() ? 0 : 1;
}

ASSETTAGGING_API bool AssetTagging_GetAssetDescription(const char* relpath, const char* project, char* description, int nChars)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    bool done = false;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT description FROM `asset_inventory`,`projects` WHERE `asset_inventory`.`relpath` = \"%s\" AND `asset_inventory`.`project_id` = `projects`.`id` AND `projects`.`name` = \"%s\"", relpath, project);
        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();
                const char* strValue = NULL;
                if (retrieved)
                {
                    if (pResultSet->GetStringByName("description", strValue))
                    {
                        _snprintf(description, nChars, "%s", strValue);
                        done = true;
                    }
                }

                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return done;
}

ASSETTAGGING_API void AssetTagging_SetAssetDescription(const char* relpath, const char* project, const char* description)
{
    if (!m_pConnection)
    {
        return;
    }

    int assetid = AssetTagging_CreateAsset(relpath, project);
    if (assetid == 0)
    {
        return;
    }

    IDBStatement* pStatement = m_pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("UPDATE `asset_inventory` SET `description` = \"%s\" WHERE `asset_inventory`.`id` = %d", description, assetid);
        if (!pStatement->Execute(strQuery))
        {
            printf("Error executing query: %s", m_pConnection->GetRawErrorMessage());
        }
        m_pConnection->DestroyStatement(pStatement);
    }
}

ASSETTAGGING_API void AssetTagging_UpdateCategoryOrderId(const char* category, int idx, const char* project)
{
    if (!m_pConnection)
    {
        return;
    }

    int categoryId = AssetTagging_CreateCategory(category, project);
    if (categoryId == 0)
    {
        return;
    }

    IDBStatement* pStatement = m_pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("UPDATE `categories`,`projects` SET `order_id` = %d WHERE `category_id` = %d AND `categories`.`project_id` = `projects`.`id` AND `projects`.`name` = \"%s\"", idx, categoryId, project);
        if (!pStatement->Execute(strQuery))
        {
            printf("Error executing query: %s", m_pConnection->GetRawErrorMessage());
        }
        m_pConnection->DestroyStatement(pStatement);
    }
}

ASSETTAGGING_API bool AssetTagging_AutoCompleteDescription(const char* partdesc, char* description, int nChars)
{
    IDBConnection* pConnection = m_pReadConnection != NULL ? m_pReadConnection : m_pConnection;
    if (!pConnection)
    {
        return 0;
    }

    bool done = false;

    IDBStatement* pStatement = pConnection->CreateStatement();
    if (pStatement)
    {
        CString strQuery;
        strQuery.Format("SELECT `description` FROM `asset_inventory` WHERE `asset_inventory`.`description` LIKE \"%s%%\" LIMIT 1", partdesc);
        if (pStatement->Execute(strQuery))
        {
            IDBResultSet* pResultSet = pStatement->GetResultSet();
            if (pResultSet)
            {
                bool retrieved = pResultSet->Next();
                const char* strValue = NULL;
                if (retrieved)
                {
                    if (pResultSet->GetStringByName("description", strValue))
                    {
                        _snprintf(description, nChars, "%s", strValue);
                        done = true;
                    }
                }
                pStatement->CloseResultSet();
            }
        }
        else
        {
            printf("Error executing query: %s", pConnection->GetRawErrorMessage());
        }
        pConnection->DestroyStatement(pStatement);
    }
    return done;
}