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
#ifndef ASSETPROCESSORMANAGERUNITTESTS_H
#define ASSETPROCESSORMANAGERUNITTESTS_H

#if defined (UNIT_TEST)
#include "UnitTestRunner.h"

namespace AssetProcessor
{
    class AssetProcessorManagerUnitTests
        : public UnitTestRun
    {
        Q_OBJECT
    public:
        virtual void StartTest() override;

Q_SIGNALS:
        void Test_Emit_AssessableFile(QString filePath);
        void Test_Emit_AssessableDeletedFile(QString filePath);
    };
} // namespace assetprocessor
#endif // defined(UNIT_TEST)
#endif // ASSETPROCESSORMANAGERUNITTESTS_H

