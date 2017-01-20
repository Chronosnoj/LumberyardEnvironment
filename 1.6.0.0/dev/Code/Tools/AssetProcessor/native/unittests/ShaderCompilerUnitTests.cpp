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
#include "ShaderCompilerUnitTests.h"
#include "native/connection/connectionManager.h"
#include "native/connection/connection.h"
#include <QDebug>
#include <AzCore/std/bind/bind.h>
#include <AzCore/std/functional.h>
#include "native/utilities/AssetUtils.h"

#define UNIT_TEST_CONNECT_PORT 12125

ShaderCompilerUnitTest::ShaderCompilerUnitTest()
{
    m_connectionManager = ConnectionManager::Get();
    connect(this, SIGNAL(StartUnitTestForGoodShaderCompiler()), this, SLOT(UnitTestForGoodShaderCompiler()));
    connect(this, SIGNAL(StartUnitTestForFirstBadShaderCompiler()), this, SLOT(UnitTestForFirstBadShaderCompiler()));
    connect(this, SIGNAL(StartUnitTestForSecondBadShaderCompiler()), this, SLOT(UnitTestForSecondBadShaderCompiler()));
    connect(this, SIGNAL(StartUnitTestForThirdBadShaderCompiler()), this, SLOT(UnitTestForThirdBadShaderCompiler()));
    connect(&m_shaderCompilerManager, SIGNAL(sendErrorMessageFromShaderJob(QString, QString, QString, QString)), this, SLOT(ReceiveShaderCompilerErrorMessage(QString, QString, QString, QString)));

    m_shaderCompilerManager.setIsUnitTesting(true);
    m_connectionManager->RegisterService(AssetUtilities::ComputeCRC32Lowercase("ShaderCompilerProxyRequest"), AZStd::bind(&ShaderCompilerManager::process, &m_shaderCompilerManager, AZStd::placeholders::_1, AZStd::placeholders::_2, AZStd::placeholders::_3, AZStd::placeholders::_4));
    ContructPayloadForShaderCompilerServer(m_testPayload);
}

ShaderCompilerUnitTest::~ShaderCompilerUnitTest()
{
    m_connectionManager->removeConnection(m_connectionId);
}

void ShaderCompilerUnitTest::ContructPayloadForShaderCompilerServer(QByteArray& payload)
{
    QString testString = "This is a test string";
    QString testServerList = "127.0.0.3,198.51.100.0,127.0.0.1"; // note - 198.51.100.0 is in the 'test' range that will never be assigned to anyone.
    unsigned int testServerListLength = static_cast<unsigned int>(testServerList.size());
    unsigned short testServerPort = 12348;
    unsigned int testRequestId = 1;
    qint64 testStringLength = static_cast<qint64>(testString.size());
    payload.resize(static_cast<unsigned int>(testStringLength));
    memcpy(payload.data(), (testString.toStdString().c_str()), testStringLength);
    unsigned int payloadSize = payload.size();
    payload.resize(payloadSize + 1 + static_cast<unsigned int>(testServerListLength) + 1 + sizeof(unsigned short) + sizeof(unsigned int) + sizeof(unsigned int));
    char* dataStart = payload.data() + payloadSize;
    *dataStart = 0;// null
    memcpy(payload.data() + payloadSize + 1, (testServerList.toStdString().c_str()), testServerListLength);
    dataStart += 1 + testServerListLength;
    *dataStart = 0; //null
    memcpy(payload.data() + payloadSize + 1 + testServerListLength + 1, reinterpret_cast<char*>(&testServerPort), sizeof(unsigned short));
    memcpy(payload.data() + payloadSize + 1 + testServerListLength + 1 + sizeof(unsigned short), reinterpret_cast<char*>(&testServerListLength), sizeof(unsigned int));
    memcpy(payload.data() + payloadSize + 1 + testServerListLength + 1 + sizeof(unsigned short) + sizeof(unsigned int), reinterpret_cast<char*>(&testRequestId), sizeof(unsigned int));
}

void ShaderCompilerUnitTest::StartTest()
{
    m_connectionId = m_connectionManager->addConnection();
    Connection* connection = m_connectionManager->getConnection(m_connectionId);
    connection->SetPort(UNIT_TEST_CONNECT_PORT);
    connection->SetIpAddress("127.0.0.1");
    connection->SetAutoConnect(true);
    UnitTestForGoodShaderCompiler();
}

int ShaderCompilerUnitTest::UnitTestPriority() const
{
    return -4;
}

void ShaderCompilerUnitTest::UnitTestForGoodShaderCompiler()
{
    fprintf(stdout, "  ... Starting test of 'good' shader compiler...\n");
    connect(&m_shaderCompilerManager, &ShaderCompilerManager::sendResponse, this, &ShaderCompilerUnitTest::VerifyPayloadForGoodShaderCompiler);
    m_server.Init("127.0.0.1", 12348);
    m_server.setServerStatus(UnitTestShaderCompilerServer::GoodServer);
    m_connectionManager->SendMessageToService(m_connectionId, AssetUtilities::ComputeCRC32Lowercase("ShaderCompilerProxyRequest"), 0, m_testPayload);
}

void ShaderCompilerUnitTest::UnitTestForFirstBadShaderCompiler()
{
    fprintf(stdout, "  ... Starting test of 'bad' shader compiler... (Incomplete Payload)\n");
    connect(&m_shaderCompilerManager, &ShaderCompilerManager::sendResponse, this, &ShaderCompilerUnitTest::VerifyPayloadForFirstBadShaderCompiler);
    m_server.setServerStatus(UnitTestShaderCompilerServer::BadServer_SendsIncompletePayload);
    m_connectionManager->SendMessageToService(m_connectionId, AssetUtilities::ComputeCRC32Lowercase("ShaderCompilerProxyRequest"), 0, m_testPayload);
}

void ShaderCompilerUnitTest::UnitTestForSecondBadShaderCompiler()
{
    fprintf(stdout, "  ... Starting test of 'bad' shader compiler... (Payload followed by disconnection)\n");
    connect(&m_shaderCompilerManager, &ShaderCompilerManager::sendResponse, this, &ShaderCompilerUnitTest::VerifyPayloadForSecondBadShaderCompiler);
    m_server.setServerStatus(UnitTestShaderCompilerServer::BadServer_ReadsPayloadAndDisconnect);
    m_connectionManager->SendMessageToService(m_connectionId, AssetUtilities::ComputeCRC32Lowercase("ShaderCompilerProxyRequest"), 0, m_testPayload);
}

void ShaderCompilerUnitTest::UnitTestForThirdBadShaderCompiler()
{
    fprintf(stdout, "  ... Starting test of 'bad' shader compiler... (Connect but disconnect without data)\n");
    connect(&m_shaderCompilerManager, &ShaderCompilerManager::sendResponse, this, &ShaderCompilerUnitTest::VerifyPayloadForThirdBadShaderCompiler);
    m_server.setServerStatus(UnitTestShaderCompilerServer::BadServer_DisconnectAfterConnect);
    m_server.startServer();
    m_connectionManager->SendMessageToService(m_connectionId, AssetUtilities::ComputeCRC32Lowercase("ShaderCompilerProxyRequest"), 0, m_testPayload);
}

void ShaderCompilerUnitTest::VerifyPayloadForGoodShaderCompiler(unsigned int connId, unsigned int type, unsigned int serial, QByteArray payload)
{
    (void) connId;
    (void) type;
    (void) serial;
    disconnect(&m_shaderCompilerManager, &ShaderCompilerManager::sendResponse, this, &ShaderCompilerUnitTest::VerifyPayloadForGoodShaderCompiler);

    unsigned int messageSize;
    quint8 status;
    QByteArray payloadToCheck;
    unsigned int requestId;
    memcpy((&messageSize), payload.data(), sizeof(unsigned int));
    memcpy((&status), payload.data() + sizeof(unsigned int), sizeof(unsigned char));
    payloadToCheck.resize(messageSize);
    memcpy((payloadToCheck.data()), payload.data() + sizeof(unsigned int) + sizeof(unsigned char), messageSize);
    memcpy((&requestId), payload.data() + sizeof(unsigned int) + sizeof(unsigned char) + messageSize, sizeof(unsigned int));
    QString outgoingTestString = "Test string validated";
    if (QString::compare(QString(payloadToCheck), outgoingTestString, Qt::CaseSensitive) != 0)
    {
        Q_EMIT UnitTestFailed("Unit Test for Good Shader Compiler Failed");
        return;
    }
    Q_EMIT StartUnitTestForFirstBadShaderCompiler();
}

void ShaderCompilerUnitTest::VerifyPayloadForFirstBadShaderCompiler(unsigned int connId, unsigned int type, unsigned int serial, QByteArray payload)
{
    (void) connId;
    (void) type;
    (void) serial;
    disconnect(&m_shaderCompilerManager, &ShaderCompilerManager::sendResponse, this, &ShaderCompilerUnitTest::VerifyPayloadForFirstBadShaderCompiler);
    QString error = "Remote IP is taking too long to respond: 127.0.0.1";
    if ((payload.size() != 4) || (QString::compare(m_lastShaderCompilerErrorMessage, error, Qt::CaseSensitive) != 0))
    {
        Q_EMIT UnitTestFailed("Unit Test for First Bad Shader Compiler Failed");
        return;
    }
    m_lastShaderCompilerErrorMessage.clear();
    Q_EMIT StartUnitTestForSecondBadShaderCompiler();
}

void ShaderCompilerUnitTest::VerifyPayloadForSecondBadShaderCompiler(unsigned int connId, unsigned int type, unsigned int serial, QByteArray payload)
{
    (void) connId;
    (void) type;
    (void) serial;
    disconnect(&m_shaderCompilerManager, &ShaderCompilerManager::sendResponse, this, &ShaderCompilerUnitTest::VerifyPayloadForSecondBadShaderCompiler);
    QString error = "Remote IP is taking too long to respond: 127.0.0.1";
    if ((payload.size() != 4) || (QString::compare(m_lastShaderCompilerErrorMessage, error, Qt::CaseSensitive) != 0))
    {
        Q_EMIT UnitTestFailed("Unit Test for Second Bad Shader Compiler Failed");
        return;
    }
    m_lastShaderCompilerErrorMessage.clear();
    Q_EMIT StartUnitTestForThirdBadShaderCompiler();
}

void ShaderCompilerUnitTest::VerifyPayloadForThirdBadShaderCompiler(unsigned int connId, unsigned int type, unsigned int serial, QByteArray payload)
{
    (void) connId;
    (void) type;
    (void) serial;
    disconnect(&m_shaderCompilerManager, &ShaderCompilerManager::sendResponse, this, &ShaderCompilerUnitTest::VerifyPayloadForThirdBadShaderCompiler);
    QString error = "Remote IP is taking too long to respond: 127.0.0.1";
    if ((payload.size() != 4) || (QString::compare(m_lastShaderCompilerErrorMessage, error, Qt::CaseSensitive) != 0))
    {
        Q_EMIT UnitTestFailed("Unit Test for Third Bad Shader Compiler Failed");
        return;
    }
    m_lastShaderCompilerErrorMessage.clear();
    Q_EMIT UnitTestPassed();
}

void ShaderCompilerUnitTest::ReceiveShaderCompilerErrorMessage(QString error, QString server, QString timestamp, QString payload)
{
    (void) server;
    (void) timestamp;
    (void) payload;
    m_lastShaderCompilerErrorMessage = error;
}

#include <native/unittests/ShaderCompilerUnitTests.moc>

REGISTER_UNIT_TEST(ShaderCompilerUnitTest)

