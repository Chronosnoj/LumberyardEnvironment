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
#include "ConnectionManagerUnitTests.h"
#include "native/connection/connectionManager.h"
#include "native/connection/connection.h"
#include <QDebug>
#include <QCoreApplication>

ConnectionManagerUnitTest::ConnectionManagerUnitTest()
{
    m_connectionManager = ConnectionManager::Get();
    QObject::connect(m_connectionManager, &ConnectionManager::ConnectionRemoved, this,  &ConnectionManagerUnitTest::ConnectionDeleted);
    QObject::connect(this, &ConnectionManagerUnitTest::RunFirstPhaseofUnitTestsForConnectionManager, this, &ConnectionManagerUnitTest::RunFirstPartOfUnitTestsForConnectionManager);
    QObject::connect(this, &ConnectionManagerUnitTest::AllConnectionsRemoved, this, &ConnectionManagerUnitTest::RunSecondPartOfUnitTestsForConnectionManager);
    QObject::connect(this, &ConnectionManagerUnitTest::ConnectionRemoved, this, &ConnectionManagerUnitTest::RunThirdPartOfUnitTestsForConnectionManager);
}

void ConnectionManagerUnitTest::StartTest()
{
    UpdateConnectionManager();
}

int ConnectionManagerUnitTest::UnitTestPriority() const
{
    return -10;
}

ConnectionManagerUnitTest::~ConnectionManagerUnitTest()
{
}

void ConnectionManagerUnitTest::RunFirstPartOfUnitTestsForConnectionManager()
{
    int count = m_connectionManager->getCount();
    if (count != 4)
    {
        Q_EMIT UnitTestFailed("Count is Invalid");
        return;
    }
    m_connectionManager->SaveConnections();
    //removing all connections
    for (auto iter = m_connectionManager->getConnectionMap().begin(); iter != m_connectionManager->getConnectionMap().end(); iter++)
    {
        iter.value()->Terminate();
    }
}

void ConnectionManagerUnitTest::RunSecondPartOfUnitTestsForConnectionManager()
{
    int count = m_connectionManager->getCount();
    if (count != 0)
    {
        Q_EMIT UnitTestFailed("Count is Invalid");
        return;
    }
    m_connectionManager->LoadConnections();
    count = m_connectionManager->getCount();
    if (count != 4)
    {
        Q_EMIT UnitTestFailed("Count is Invalid");
        return;
    }
    unsigned int connId = m_connectionManager->GetConnectionId("127.0.0.2", 12345);
    if (connId == 0)
    {
        Q_EMIT UnitTestFailed("Connection is not present ,which is Invalid");
        return;
    }
    Connection* testConnection = m_connectionManager->getConnection(connId);
    if (testConnection->Identifier().compare("PC Game", Qt::CaseSensitive) != 0)
    {
        Q_EMIT UnitTestFailed("Identifier is Invalid");
        return;
    }
    if (testConnection->IpAddress().compare("127.0.0.2", Qt::CaseInsensitive) != 0)
    {
        Q_EMIT UnitTestFailed("IpAddress is Invalid");
        return;
    }
    if (testConnection->Port() != 12345)
    {
        Q_EMIT UnitTestFailed("Port is Invalid");
        return;
    }
    if (testConnection->Status() != Connection::Disconnected)
    {
        Q_EMIT UnitTestFailed("Status is Invalid");
        return;
    }
    if (testConnection->AutoConnect() != false)
    {
        Q_EMIT UnitTestFailed("autoConnect status is Invalid");
        return;
    }

    connId = m_connectionManager->addConnection();
    count = m_connectionManager->getCount();
    if (count != 5)
    {
        Q_EMIT UnitTestFailed("Count is Invalid");
        return;
    }
    if (connId == 0)
    {
        Q_EMIT UnitTestFailed("Index of the connection is Invalid");
        return;
    }
    testConnection = m_connectionManager->getConnection(connId);
    testConnection->SetIdentifier("PC Game");
    testConnection->SetIpAddress("98.45.67.89");
    testConnection->SetPort(22234);
    testConnection->SetStatus(Connection::Connecting);
    testConnection->SetAutoConnect(true);

    if (testConnection->Identifier().compare("PC Game", Qt::CaseSensitive) != 0)
    {
        Q_EMIT UnitTestFailed("Identifier is Invalid");
        return;
    }
    if (testConnection->IpAddress().compare("98.45.67.89", Qt::CaseInsensitive) != 0)
    {
        Q_EMIT UnitTestFailed("IpAddress is Invalid");
        return;
    }
    if (testConnection->Port() != 22234)
    {
        Q_EMIT UnitTestFailed("Port is Invalid");
        return;
    }
    if (testConnection->Status() != Connection::Connecting)
    {
        Q_EMIT UnitTestFailed("Status is Invalid");
        return;
    }
    if (testConnection->AutoConnect() != true)
    {
        Q_EMIT UnitTestFailed("Autoconnect status is Invalid");
        return;
    }
    connId = m_connectionManager->GetConnectionId("98.45.67.89", 22234);
    if (connId == 0)
    {
        Q_EMIT UnitTestFailed("Connection is not present ,which is Invalid");
        return;
    }
    count = m_connectionManager->getCount();
    if (count != 5)
    {
        Q_EMIT UnitTestFailed("Count is Invalid");
        return;
    }
    m_connectionManager->removeConnection(connId);
}

void ConnectionManagerUnitTest::RunThirdPartOfUnitTestsForConnectionManager()
{
    int count = m_connectionManager->getCount();
    if (count != 4)
    {
        Q_EMIT UnitTestFailed("Count is Invalid");
        return;
    }

    unsigned int connId = m_connectionManager->GetConnectionId("98.45.67.89", 22234);
    if (connId != 0)
    {
        Q_EMIT UnitTestFailed("Connection is present ,which is Invalid");
        return;
    }

    QObject::disconnect(this, &ConnectionManagerUnitTest::AllConnectionsRemoved, this, &ConnectionManagerUnitTest::RunSecondPartOfUnitTestsForConnectionManager);
    QObject::connect(this, &ConnectionManagerUnitTest::AllConnectionsRemoved, this, &ConnectionManagerUnitTest::RunFourthPartOfUnitTestsForConnectionManager);
    //removing all connections
    for (auto iter = m_connectionManager->getConnectionMap().begin(); iter != m_connectionManager->getConnectionMap().end(); iter++)
    {
        iter.value()->Terminate();
    }
}

void ConnectionManagerUnitTest::RunFourthPartOfUnitTestsForConnectionManager()
{
    qApp->processEvents();

    Q_EMIT UnitTestPassed();
}

void ConnectionManagerUnitTest::ConnectionDeleted(unsigned int connId)
{
    if (m_connectionManager->getConnectionMap().isEmpty())
    {
        Q_EMIT AllConnectionsRemoved();
    }
    else if (connId == 9)
    {
        Q_EMIT ConnectionRemoved();
    }
}

void ConnectionManagerUnitTest::UpdateConnectionManager()
{
    unsigned int connId1 = m_connectionManager->addConnection();
    unsigned int connId2 = m_connectionManager->addConnection();
    unsigned int connId3 = m_connectionManager->addConnection();
    unsigned int connId4 = m_connectionManager->addConnection();
    Connection* c1 = m_connectionManager->getConnection(connId1);
    Connection* c2 = m_connectionManager->getConnection(connId2);
    Connection* c3 = m_connectionManager->getConnection(connId3);
    Connection* c4 = m_connectionManager->getConnection(connId4);
    c1->SetIdentifier("Android Game");
    c1->SetStatus(Connection::Disconnected);
    c1->SetIpAddress("127.0.0.1");
    c1->SetPort(12345);
    c1->SetAutoConnect(false);

    c2->SetIdentifier("PC Game");
    c2->SetStatus(Connection::Disconnected);
    c2->SetIpAddress("127.0.0.2");
    c2->SetPort(12345);
    c2->SetAutoConnect(false);

    c3->SetIdentifier("Mac Game");
    c3->SetStatus(Connection::Disconnected);
    c3->SetIpAddress("127.0.0.3");
    c3->SetPort(12345);
    c3->SetAutoConnect(false);

    c4->SetIdentifier("Android Game");
    c4->SetStatus(Connection::Disconnected);
    c4->SetIpAddress("127.0.0.4");
    c4->SetPort(12345);
    c4->SetAutoConnect(false);
    Q_EMIT RunFirstPhaseofUnitTestsForConnectionManager();
}

#include <native/unittests/ConnectionManagerUnitTests.moc>

REGISTER_UNIT_TEST(ConnectionManagerUnitTest)

