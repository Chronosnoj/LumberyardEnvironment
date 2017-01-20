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
#include "AssetData.h"
#include <QHash>

namespace AssetProcessor
{
    DatabaseEntry::DatabaseEntry()
    {
    }

    DatabaseEntry::DatabaseEntry(QString name, QString platform, QString jobDescription)
        : m_name(name)
        , m_platform(platform)
        , m_jobDescription(jobDescription)
    {
    }

    DatabaseEntry& DatabaseEntry::operator=(const DatabaseEntry rhs)
    {
        m_name = rhs.m_name;
        m_platform = rhs.m_platform;
        m_jobDescription = rhs.m_jobDescription;
        return *this;
    }

    bool DatabaseEntry::operator==(const DatabaseEntry& rhs) const
    {
        if (QString::compare(m_name, rhs.m_name, Qt::CaseInsensitive) != 0)
        {
            return false;
        }
        else if (QString::compare(m_platform, rhs.m_platform, Qt::CaseInsensitive) != 0)
        {
            return false;
        }
        else if (QString::compare(m_jobDescription, rhs.m_jobDescription, Qt::CaseInsensitive) != 0)
        {
            return false;
        }

        return true;
    }


    uint qHash(const DatabaseEntry& key, uint seed)
    {
        return qHash(key.m_name.toLower() + key.m_platform.toLower() + key.m_jobDescription.toLower(), seed);
    }
}

