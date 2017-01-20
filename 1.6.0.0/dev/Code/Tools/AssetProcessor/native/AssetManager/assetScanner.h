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
#ifndef ASSETSCANNER_H
#define ASSETSCANNER_H

#include "native/assetprocessor.h"
#include "assetScannerWorker.h"
#include "assetScanFolderInfo.h"
#include <QString>
#include <QThread>
#include <QList>

namespace AssetProcessor
{
    class PlatformConfiguration;

    /** This Class is responsible for scanning for assets at startup
     */

    class AssetScanner
        : public QObject
    {
        Q_OBJECT
    public:

        explicit AssetScanner(PlatformConfiguration* config, QObject* parent = 0);
        virtual ~AssetScanner();

        void StartScan();//Should be called to start a scan
        void StopScan();//Should be called to stop a scan

        Q_INVOKABLE AssetProcessor::AssetScanningStatus status() const;

Q_SIGNALS:
        void AssetScanningStatusChanged(AssetProcessor::AssetScanningStatus status);
        void FileOfInterestFound(QString file);

    private:

        QThread m_assetWorkerScannerThread;
        AssetScannerWorker m_assetScannerWorker;
        AssetProcessor::AssetScanningStatus m_status = AssetProcessor::AssetScanningStatus::Unknown;
    };
}// end namespace AssetProcessor

#endif // ASSETSCANNER_H
