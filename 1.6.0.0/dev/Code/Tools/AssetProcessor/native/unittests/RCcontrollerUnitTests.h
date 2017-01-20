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
#ifndef RCCONTROLLERUNITTESTS_H
#define RCCONTROLLERUNITTESTS_H

#include "UnitTestRunner.h"
#include "native/resourcecompiler/rccontroller.h"


class RCcontrollerUnitTests
    : public UnitTestRun
{
    Q_OBJECT
public:
    virtual void StartTest() override;

    RCcontrollerUnitTests();

Q_SIGNALS:
    void RcControllerPrepared();
public Q_SLOTS:
    void PrepareRCController();
    void RunRCControllerTests();


private:
    AssetProcessor::RCController m_rcController;

    unsigned int m_expectedProcessingJobID = 0;
};

#endif // RCCONTROLLERUNITTESTS_H
