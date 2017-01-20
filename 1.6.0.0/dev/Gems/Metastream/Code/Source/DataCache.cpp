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
#include "StdAfx.h"
#include "DataCache.h"

using namespace Metastream;

void DataCache::AddToCache(const std::string& tableName, const std::string& key, const std::string& value)
{
    AZStd::lock_guard<AZStd::mutex> lock(m_mutexDatabase);

    if (m_database.find(tableName) == m_database.end())
    {
        m_database[tableName] = Table();
    }

    m_database[tableName][key] = value;
}


Database DataCache::GetDatabase() const
{
    AZStd::lock_guard<AZStd::mutex> lock(m_mutexDatabase);

    return m_database;
}

Table DataCache::GetTable(const std::string& tableName) const
{
    AZStd::lock_guard<AZStd::mutex> lock(m_mutexDatabase);

    auto it = m_database.find(tableName);

    if (it != m_database.end())
    {
        return it->second;
    }

    return Table();
}

void DataCache::ClearCache()
{
    AZStd::lock_guard<AZStd::mutex> lock(m_mutexDatabase);

    m_database.clear();
}
