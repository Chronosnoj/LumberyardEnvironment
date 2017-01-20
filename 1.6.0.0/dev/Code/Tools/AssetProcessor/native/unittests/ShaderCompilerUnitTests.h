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
#ifndef SHADERCOMPILERUNITTEST_H
#define SHADERCOMPILERUNITTEST_H

#include "UnitTestRunner.h"
#include "native/shadercompiler/shadercompilerManager.h"
//#include "native/shadercompiler/shadercompilerMessages.h"
#include "native/utilities/UnitTestShaderCompilerServer.h"


#include <QString>
#include <QByteArray>

class ConnectionManager;

class ShaderCompilerUnitTest
    : public UnitTestRun
{
    Q_OBJECT
public:
    ShaderCompilerUnitTest();
    ~ShaderCompilerUnitTest();
    virtual void StartTest() override;
    virtual int UnitTestPriority() const override;
    void ContructPayloadForShaderCompilerServer(QByteArray& payload);

Q_SIGNALS:
    void StartUnitTestForGoodShaderCompiler();
    void StartUnitTestForFirstBadShaderCompiler();
    void StartUnitTestForSecondBadShaderCompiler();
    void StartUnitTestForThirdBadShaderCompiler();

public Q_SLOTS:
    void UnitTestForGoodShaderCompiler();
    void UnitTestForFirstBadShaderCompiler();
    void UnitTestForSecondBadShaderCompiler();
    void UnitTestForThirdBadShaderCompiler();
    void VerifyPayloadForGoodShaderCompiler(unsigned int connId, unsigned int type, unsigned int serial, QByteArray payload);
    void VerifyPayloadForFirstBadShaderCompiler(unsigned int connId, unsigned int type, unsigned int serial, QByteArray payload);
    void VerifyPayloadForSecondBadShaderCompiler(unsigned int connId, unsigned int type, unsigned int serial, QByteArray payload);
    void VerifyPayloadForThirdBadShaderCompiler(unsigned int connId, unsigned int type, unsigned int serial, QByteArray payload);
    void ReceiveShaderCompilerErrorMessage(QString error, QString server, QString timestamp, QString payload);


private:
    UnitTestShaderCompilerServer m_server;
    ShaderCompilerManager m_shaderCompilerManager;
    ConnectionManager* m_connectionManager;
    QByteArray m_testPayload;
    QString m_lastShaderCompilerErrorMessage;
    unsigned int m_connectionId = 0;
};

#endif // SHADERCOMPILERUNITTEST_H


