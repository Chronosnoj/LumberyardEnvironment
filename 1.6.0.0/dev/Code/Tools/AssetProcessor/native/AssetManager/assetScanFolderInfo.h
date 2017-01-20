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
#ifndef ASSETSCANFOLDERINFO_H
#define ASSETSCANFOLDERINFO_H

#include <QString>

namespace AssetProcessor
{
    /** This Class contains information about the folders to be scanned
     * */
    class ScanFolderInfo
    {
    public:
        ScanFolderInfo(QString path, QString prefix = QString(), bool isRoot = false, bool recurseSubFolders = true, int order = 0)
            : m_scanPath(path)
            , m_outputPrefix(prefix)
            , m_isRoot(isRoot)
            , m_recurseSubFolders(recurseSubFolders)
            , m_order(order)
        {
        }

        ScanFolderInfo() = default;
        ScanFolderInfo(const ScanFolderInfo& other) = default;

        QString ScanPath() const
        {
            return m_scanPath;
        }

        QString OutputPrefix() const
        {
            return m_outputPrefix;
        }

        bool IsRoot() const
        {
            return m_isRoot;
        }

        bool RecurseSubFolders() const
        {
            return m_recurseSubFolders;
        }

        int GetOrder() const
        {
            return m_order;
        }

    private:
        QString m_scanPath;
        QString m_outputPrefix;
        bool m_isRoot = false;
        bool m_recurseSubFolders = true;
        int m_order = 0;
    };
} // end namespace AssetProcessor

#endif //ASSETSCANFOLDERINFO_H
