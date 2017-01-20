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
#include "shadercompilerjob.h"
#include "native/assetprocessor.h"
#include <AzCore/Debug/Trace.h>

#include <QTcpSocket>
#include <QThread>
#include <QByteArray>
#include <QObject>
#include <QDateTime>

ShaderCompilerJob::ShaderCompilerJob()
    : m_isUnitTesting(false)
    , m_manager(nullptr)
{
}

ShaderCompilerJob::~ShaderCompilerJob()
{
    m_manager = nullptr;
}

ShaderCompilerRequestMessage ShaderCompilerJob::ShaderCompilerMessage() const
{
    return m_ShaderCompilerMessage;
}

void ShaderCompilerJob::initialize(QObject* pManager, const ShaderCompilerRequestMessage& ShaderCompilerMessage)
{
    m_manager = pManager;
    m_ShaderCompilerMessage = ShaderCompilerMessage;
}

QString ShaderCompilerJob::choseRandomServerAddress()
{
    if (isServerListEmpty())
    {
        return QString();
    }
    QString serverAddress;
    if (!m_ShaderCompilerMessage.serverList.contains(","))
    {
        serverAddress = m_ShaderCompilerMessage.serverList;
        m_ShaderCompilerMessage.serverList.clear();
        return serverAddress;
    }

    QStringList serverList = m_ShaderCompilerMessage.serverList.split(",");

    if (m_isUnitTesting)
    {
        serverAddress = serverList.takeAt(0);
    }
    else
    {
        int randomNumber = qrand() % (serverList.count() - 1);
        serverAddress = serverList.takeAt(randomNumber);
    }
    m_ShaderCompilerMessage.serverList = serverList.join(",");
    return serverAddress;
}

bool ShaderCompilerJob::isServerListEmpty()
{
    return m_ShaderCompilerMessage.serverList.isEmpty();
}

bool ShaderCompilerJob::attemptDelivery(QString serverAddress, QByteArray& payload)
{
    QTcpSocket socket;
    QString error;
    int waitingTime = 8000; // 8 sec timeout for sending.
    int jobCompileMaxTime = 1000 * 60; // 60 sec timeout for compilation
    if (m_isUnitTesting)
    {
        waitingTime = 500;
        jobCompileMaxTime = 500;
    }

    socket.connectToHost(serverAddress, m_ShaderCompilerMessage.serverPort, QIODevice::ReadWrite);

    if (socket.waitForConnected(waitingTime))
    {
        qint64 bytesWritten = 0;
        qint64 payloadSize = static_cast<qint64>(m_ShaderCompilerMessage.originalPayload.size());
        // send payload size to server
        while (bytesWritten != sizeof(qint64))
        {
            qint64 currentWrite = socket.write(reinterpret_cast<char*>(&payloadSize) + bytesWritten,
                    sizeof(qint64) - bytesWritten);
            if (currentWrite == -1)
            {
                //It is important to note that we are only outputting the error to debugchannel only here because
                //we are forwarding these error messages upstream to the manager,who will take the appropriate action
                error = "Connection Lost:Unable to send data";
                AZ_TracePrintf(AssetProcessor::DebugChannel, error.toUtf8().data());
                QMetaObject::invokeMethod(m_manager, "shaderCompilerError", Qt::QueuedConnection, Q_ARG(QString, error), Q_ARG(QString, QDateTime::currentDateTime().toString()), Q_ARG(QString, QString(m_ShaderCompilerMessage.originalPayload)), Q_ARG(QString, serverAddress));
                return false;
            }
            socket.flush();
            bytesWritten += currentWrite;
        }
        bytesWritten = 0;
        //send actual payload to server
        while (bytesWritten != m_ShaderCompilerMessage.originalPayload.size())
        {
            qint64 currentWrite = socket.write(m_ShaderCompilerMessage.originalPayload.data() + bytesWritten,
                    m_ShaderCompilerMessage.originalPayload.size() - bytesWritten);
            if (currentWrite == -1)
            {
                error = "Connection Lost:Unable to send data";
                AZ_TracePrintf(AssetProcessor::DebugChannel, error.toUtf8().data());
                QMetaObject::invokeMethod(m_manager, "shaderCompilerError", Qt::QueuedConnection, Q_ARG(QString, error), Q_ARG(QString, QDateTime::currentDateTime().toString()), Q_ARG(QString, QString(m_ShaderCompilerMessage.originalPayload)), Q_ARG(QString, serverAddress));
            }
            socket.flush();
            bytesWritten += currentWrite;
        }
    }
    else
    {
        error = "Unable to connect to IP Address " + serverAddress;
        AZ_TracePrintf(AssetProcessor::DebugChannel, error.toUtf8().data());
        QMetaObject::invokeMethod(m_manager, "shaderCompilerError", Qt::QueuedConnection, Q_ARG(QString, error), Q_ARG(QString, QDateTime::currentDateTime().toString()), Q_ARG(QString, QString(m_ShaderCompilerMessage.originalPayload)), Q_ARG(QString, serverAddress));
        return false;
    }

    unsigned int expectedBytes =  sizeof(unsigned int) + sizeof(qint8);
    unsigned int bytesReadTotal = 0;
    unsigned int messageSize = 0;
    bool isMessageSizeKnown = false;
    //read the entire payload
    while ((bytesReadTotal < expectedBytes + messageSize))
    {
        if (socket.bytesAvailable() == 0)
        {
            if (!socket.waitForReadyRead(jobCompileMaxTime))
            {
                error = "Remote IP is taking too long to respond: " + serverAddress;
                AZ_TracePrintf(AssetProcessor::DebugChannel, error.toUtf8().data());
                QMetaObject::invokeMethod(m_manager, "shaderCompilerError", Qt::QueuedConnection, Q_ARG(QString, error), Q_ARG(QString, QDateTime::currentDateTime().toString()), Q_ARG(QString, QString(m_ShaderCompilerMessage.originalPayload)), Q_ARG(QString, serverAddress));
                payload.clear();
                return false;
            }
        }

        qint64 bytesAvailable = socket.bytesAvailable();

        if (bytesAvailable >= expectedBytes && !isMessageSizeKnown)
        {
            socket.peek(reinterpret_cast<char*>(&messageSize), sizeof(unsigned int));
            payload.resize(expectedBytes + messageSize);
            isMessageSizeKnown = true;
        }

        if (bytesAvailable > 0)
        {
            qint64 bytesRead = socket.read(payload.data() + bytesReadTotal, bytesAvailable);

            if (bytesRead <= 0)
            {
                error = "Connection closed by remote IP Address " + serverAddress;
                AZ_TracePrintf(AssetProcessor::DebugChannel, error.toUtf8().data());
                QMetaObject::invokeMethod(m_manager, "shaderCompilerError", Qt::QueuedConnection, Q_ARG(QString, error), Q_ARG(QString, QDateTime::currentDateTime().toString()), Q_ARG(QString, QString(m_ShaderCompilerMessage.originalPayload)), Q_ARG(QString, serverAddress));
                payload.clear();
                return false;
            }

            bytesReadTotal += bytesRead;
        }
    }

    return true; // payload successfully send
}

void ShaderCompilerJob::run()
{
    QMetaObject::invokeMethod(m_manager, "jobStarted", Qt::QueuedConnection);
    QByteArray payload;
    //until server list is empty, keep trying
    while (!isServerListEmpty())
    {
        QString serverAddress = choseRandomServerAddress();
        //attempt to send payload
        if (attemptDelivery(serverAddress, payload))
        {
            break;
        }
    }
    //we are appending request id at the end of every payload,
    //therefore in the case of any errors also
    //we will be sending atleast four bytes to the game
    payload.append(reinterpret_cast<char*>(&m_ShaderCompilerMessage.requestId), sizeof(unsigned int));
    QMetaObject::invokeMethod(m_manager, "OnShaderCompilerJobComplete", Qt::QueuedConnection, Q_ARG(QByteArray, payload), Q_ARG(unsigned int, m_ShaderCompilerMessage.requestId));
    QMetaObject::invokeMethod(m_manager, "jobEnded", Qt::QueuedConnection);
}


void ShaderCompilerJob::setIsUnitTesting(bool isUnitTesting)
{
    m_isUnitTesting = isUnitTesting;
}
