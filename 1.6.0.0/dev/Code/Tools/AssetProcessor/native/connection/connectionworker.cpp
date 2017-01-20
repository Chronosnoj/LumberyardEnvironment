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

#include "connectionWorker.h"
#include "native/utilities/assetUtils.h"
#include <native/utilities/ByteArrayStream.h>
#include "native/utilities/assetUtilEBusHelper.h"
#include <native/assetprocessor.h>

#include <QReadLocker>
#include <QWriteLocker>
#include <QThread>
#include <QTimer>
#include <QtEndian>
#include <QHostAddress>
#include <QCoreApplication>
#include <QHostInfo>
#include <QNetworkInterface>

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzFramework/StringFunc/StringFunc.h>


// enable this to debug negotiation - it enables a huge delay so that when a debugger attaches we don't fail.
//#define DEBUG_NEGOTATION

#undef SendMessage
namespace AssetProcessor {
ConnectionWorker::ConnectionWorker(qintptr socketDescriptor, bool inProxyMode, QObject* parent)
    : QObject(parent)
    , m_terminate(false)
    , m_inProxyMode(inProxyMode)
{
#ifdef DEBUG_NEGOTIATION
    m_waitDelay = 60 * 10 * 1000; // 10 min in debug, in ms
#endif
    connect(&m_engineSocket, &QTcpSocket::stateChanged, this, &ConnectionWorker::EngineSocketStateChanged, Qt::QueuedConnection);
    connect(&m_upstreamSocket, &QTcpSocket::stateChanged, this, &ConnectionWorker::UpstreamSocketStateChanged, Qt::QueuedConnection);
#if defined(DEBUG_NEGOTATION)
    AZ_TracePrintf(AssetProcessor::DebugChannel, "Connection::ConnectionWorker created for socket %p: %p", socketDescriptor, this);
#endif
}


ConnectionWorker::~ConnectionWorker()
{
#if defined(DEBUG_NEGOTATION)
    AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::~:  %p", this);
#endif
}

bool ConnectionWorker::ReadMessage(QTcpSocket& socket, AssetProcessor::Message& message)
{
    const qint64 sizeOfHeader = static_cast<qint64>(sizeof(AssetProcessor::MessageHeader));
    qint64 bytesAvailable = socket.bytesAvailable();
    if (bytesAvailable == 0 || bytesAvailable < sizeOfHeader)
    {
        return false;
    }

    // read header
    if (!ReadData(socket, (char*)&message.header, sizeOfHeader))
    {
        DisconnectSockets();
        return false;
    }

    // Prepare the payload buffer
    message.payload.resize(message.header.size);

    // read payload
    if (!ReadData(socket, message.payload.data(), message.header.size))
    {
        DisconnectSockets();
        return false;
    }

    return true;
}

bool ConnectionWorker::ReadData(QTcpSocket& socket, char* buffer, qint64 size)
{
    qint64 bytesRemaining = size;
    while (bytesRemaining > 0)
    {
            // check first, or Qt will throw a warning if we try to do this on an already-disconnected-socket
            if (socket.state() != QAbstractSocket::ConnectedState)
            {
                return false;
            }

        qint64 bytesRead = socket.read(buffer, bytesRemaining);
        if (bytesRead == -1)
        {
            return false;
        }
        buffer += bytesRead;
        bytesRemaining -= bytesRead;
        if (bytesRemaining > 0)
        {
            socket.waitForReadyRead();
        }
    }
    return true;
}

bool ConnectionWorker::WriteMessage(QTcpSocket& socket, const AssetProcessor::Message& message)
{
    const qint64 sizeOfHeader = static_cast<qint64>(sizeof(AssetProcessor::MessageHeader));
    AZ_Assert(message.header.size == message.payload.size(), "Message header size does not match payload size");

    // Write header
    if (!WriteData(socket, (char*)&message.header, sizeOfHeader))
    {
        DisconnectSockets();
        return false;
    }

    // write payload
    if (!WriteData(socket, message.payload.data(), message.payload.size()))
    {
        DisconnectSockets();
        return false;
    }

    return true;
}

bool ConnectionWorker::WriteData(QTcpSocket& socket, const char* buffer, qint64 size)
{
    qint64 bytesRemaining = size;
    while (bytesRemaining > 0)
    {
            // check first, or Qt will throw a warning if we try to do this on an already-disconnected-socket
            if (socket.state() != QAbstractSocket::ConnectedState)
            {
                return false;
            }

        qint64 bytesWritten = socket.write(buffer, bytesRemaining);
        if (bytesWritten == -1)
        {
            return false;
        }

        buffer += bytesWritten;
        bytesRemaining -= bytesWritten;
    }

    return true;
}

void ConnectionWorker::EngineSocketHasData()
{
    if (m_terminate)
    {
        return;
    }
    while (m_engineSocket.bytesAvailable() > 0)
    {
        AssetProcessor::Message message;
        if (ReadMessage(m_engineSocket, message))
        {
            if (m_inProxyMode)
            {
                WriteMessage(m_upstreamSocket, message);
            }
            else
            {
                Q_EMIT ReceiveMessage(message.header.type, message.header.serial, message.payload);
            }
        }
        else
        {
            break;
        }
    }
}


void ConnectionWorker::UpstreamSocketHasData()
{
    Q_ASSERT(m_inProxyMode);
    if (m_terminate || !m_inProxyMode)
    {
        return;
    }

    // read the message and immediately forward it to the engine
    while (m_upstreamSocket.bytesAvailable() > 0)
    {
        AssetProcessor::Message message;
        ReadMessage(m_upstreamSocket, message);
        WriteMessage(m_engineSocket, message);
    }
}

void ConnectionWorker::SendMessage(unsigned int type, unsigned int serial, QByteArray payload)
{
    AssetProcessor::Message message;
    message.header.type = type;
    message.header.serial = serial;
    message.header.size = payload.size();
    message.payload = payload;
    WriteMessage(m_engineSocket, message);
}

namespace Detail
{
    template <class N>
    bool WriteNegotiation(ConnectionWorker* worker, QTcpSocket& socket, const N& negotiation, unsigned int serial = AzFramework::AssetSystem::NEGOTIATION_SERIAL)
    {
        AssetProcessor::Message message;
        bool packed = AssetProcessor::PackMessage(negotiation, message.payload);
        if (packed)
        {
            message.header.type = negotiation.GetMessageType();
            message.header.serial = serial;
            message.header.size = message.payload.size();
            return worker->WriteMessage(socket, message);
        }
        return false;
    }

    template <class N>
    bool ReadNegotiation(ConnectionWorker* worker, int waitDelay, QTcpSocket& socket, N& negotiation, unsigned int* serial = nullptr)
    {
        if (socket.bytesAvailable() == 0)
        {
            socket.waitForReadyRead(waitDelay);
        }
        AssetProcessor::Message message;
        if (!worker->ReadMessage(socket, message))
        {
            return false;
        }
        if (serial)
        {
            *serial = message.header.serial;
        }
        return AssetProcessor::UnpackMessage(message.payload, negotiation);
    }
}

// Negotiation directly with a game or downstream AssetProcessor:
// if the connection is initiated from this end:
// 1) Send AP Info to downstream engine
// 2) Get downstream engine info
// if there is an incoming connection
// 1) Get downstream engine info
// 2) Send AP Info
// 3) If downstream is a proxy, then we wait for them to send us engine info from their downstream engine
bool ConnectionWorker::NegotiateDirect(bool initiate)
{
#if defined(DEBUG_NEGOTATION)
    AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateDirect: %p", this);
#endif
    AZ_Assert(!m_inProxyMode, "NegotiateDirect called while in proxy mode");
    using Detail::ReadNegotiation;
    using Detail::WriteNegotiation;
    using namespace AzFramework::AssetSystem;

    QString branchToken = AssetUtilities::GetBranchToken();
    NegotiationMessage myInfo;

    char processId[20];
    azsnprintf(processId, 20, "%lld", QCoreApplication::applicationPid());
    myInfo.m_identifier = "ASSETPROCESSOR";
    myInfo.m_negotiationInfoMap.insert(AZStd::make_pair(NegotiationInfo_ProcessId, AZ::OSString(processId)));
    myInfo.m_negotiationInfoMap.insert(AZStd::make_pair(NegotiationInfo_BranchIndentifier, AZ::OSString(branchToken.toUtf8().data())));
    NegotiationMessage engineInfo;

    if (initiate)
    {
        if (!WriteNegotiation(this, m_engineSocket, myInfo))
        {
            Q_EMIT ErrorMessage("Unable to send negotiation message");
            QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
            return false;
        }

        if (!ReadNegotiation(this, m_waitDelay, m_engineSocket, engineInfo))
        {
            Q_EMIT ErrorMessage("Unable to read negotiation message");
            QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
            return false;
        }
    }
    else
    {
        unsigned int serial = 0;
#if defined(DEBUG_NEGOTATION)
        AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateDirect: Reading negotiation from engine socket %p", this);
#endif
        if (!ReadNegotiation(this, m_waitDelay, m_engineSocket, engineInfo, &serial))
        {
#if defined(DEBUG_NEGOTATION)
            AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateDirect: no negotation arrived %p", this);
#endif
            // note that this is the most common case to happen during a proxy exchange when the downstream is not actually connected to a real engine
            // this can happen thousands of times while idle, so we are supressing this unless debugging
            Q_EMIT ErrorMessage("Unable to read engine negotiation message");
            return false;
        }

#if defined(DEBUG_NEGOTATION)
        AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateDirect: writing negotiation to engine socket %p", this);
#endif
        if (!WriteNegotiation(this, m_engineSocket, myInfo, serial))
        {
#if defined(DEBUG_NEGOTATION)
            AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateDirect: no negotation sent %p", this);
#endif
            Q_EMIT ErrorMessage("Unable to send negotiation message");
            QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
            return false;
        }

        // If the connected client is a proxy, then we will need to get the downstream client's info
        if (engineInfo.m_identifier == "ASSETPROCESSOR-PROXY")
        {
            // reset the struct, so that we don't combine the engine info from the upstream with it:
            engineInfo.m_identifier.clear();
            engineInfo.m_negotiationInfoMap.clear();
            engineInfo.m_apiVersion = 0;

            // if a proxy connected to us, then the proxy really controls the delay, its up to it, so we make a very large delay
            // with the game.
            m_waitDelay = 30 * 1000; // 30 seconds, its up to the other end

#if defined(DEBUG_NEGOTATION)
            AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateDirect: connected socket is a proxy, reading real engine negotiation from it: %p", this);
#endif
            if (!ReadNegotiation(this, m_waitDelay, m_engineSocket, engineInfo, &serial))
            {
                Q_EMIT ErrorMessage("Unable to read downstream negotiation message");
                QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
                return false;
            }

            m_waitDelay = 500; // once connected, we expect a pretty fast throughput
        }
    }

    if (strncmp(engineInfo.m_negotiationInfoMap[NegotiationInfo_ProcessId].c_str(), processId, strlen(processId)) == 0)
    {
        Q_EMIT ErrorMessage("Attempted to negotiate with self");
        QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
        return false;
    }

    if (engineInfo.m_apiVersion != myInfo.m_apiVersion)
    {
        Q_EMIT ErrorMessage("Negotiation Failed.Version Mismatch.");
        QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
        return false;
    }

    QString incomingBranchToken(engineInfo.m_negotiationInfoMap[NegotiationInfo_BranchIndentifier].c_str());
    if (QString::compare(incomingBranchToken, branchToken, Qt::CaseInsensitive) != 0)
    {
        //if we are here it means that the editor/game which is negotiating is running on a different branch
        // note that it could have just read nothing from the engine or a repeat packet, in that case, discard it silently and try again.
#if defined(DEBUG_NEGOTATION)
        AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateDirect: branch token mismatch %p - %s vs %s", this, incomingBranchToken.toUtf8().data(), branchToken.toUtf8().data());
#endif
        if (engineInfo.m_identifier != "ASSETPROCESSOR-PROXY")
        {
            // if we were re-sent the proxy because of a connection queued message, then ignore it.
            EBUS_EVENT(AssetProcessor::MessageInfoBus, NegotiationFailed);
        }
        QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
        return false;
    }

    Q_EMIT Identifier(engineInfo.m_identifier.c_str());
    Q_EMIT AssetPlatform(engineInfo.m_negotiationInfoMap[NegotiationInfo_Platform].c_str());

#if defined(DEBUG_NEGOTATION)
    AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateDirect: negotation complete %p", this);
#endif

    Q_EMIT ConnectionEstablished(m_engineSocket.peerAddress().toString(), m_engineSocket.peerPort());
    connect(&m_engineSocket, &QTcpSocket::readyRead, this, &ConnectionWorker::EngineSocketHasData);
    // force the socket to evaluate any data recv'd between negotiation and now
    QTimer::singleShot(0, this, SLOT(EngineSocketHasData()));

    return true;
}

// Negotiation where this AP is the proxy
// If the connection is initiated from this AP:
// 1) Send our proxy info to the upstream AP
// 2) Get the upstream's AP info
// 3) Send the upstream's AP info to the downstream engine
// 4) Get the downstream engine's info
// 5) Forward the downstream engine's info to the upstream AP
// If the connection came in from the engine
// 1) Read the engine info from downstream
// 2) Forward the engine info to the upstream proxy
// 3) Get the upstream proxy's AP info
// 4) Forward the upstream proxy's AP info to the downstream engine
bool ConnectionWorker::NegotiateProxy(bool initiate)
{
    if (!m_inProxyMode)
    {
        return false;
    }

    // If both sockets aren't connected yet, then bail and wait for the other socket to connect
    if (m_engineSocket.state() != QAbstractSocket::ConnectedState ||
        m_upstreamSocket.state() != QAbstractSocket::ConnectedState)
    {
#if defined(DEBUG_NEGOTATION)
        AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateProxy:  One of the sockets is not connected, waiting.");
#endif
        return true; // dont disconnect, we're waiting for the other socket to come online.
    }

    using Detail::ReadNegotiation;
    using Detail::WriteNegotiation;
    using namespace AzFramework::AssetSystem;
    
    NegotiationMessage myInfo;
    myInfo.m_identifier = "ASSETPROCESSOR-PROXY";

    char processId[20];
    azsnprintf(processId, 20, "%lld", QCoreApplication::applicationPid());
    myInfo.m_negotiationInfoMap.insert(AZStd::make_pair(NegotiationInfo_ProcessId, AZ::OSString(processId)));
    NegotiationMessage upstreamInfo;
    NegotiationMessage engineInfo;
    unsigned int serial = 0;

    if (initiate)
    {
#if defined(DEBUG_NEGOTATION)
        AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateProxy: Writing my negotation to upstream %p", this);
#endif
        // connect to upstream first and send the intro to the engine after
        if (!WriteNegotiation(this, m_upstreamSocket, myInfo))
        {
            Q_EMIT ErrorMessage("Unable to send negotiation to upstream");
            QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
        }

#if defined(DEBUG_NEGOTATION)
        AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateProxy: Reading upstream negotiation %p", this);
#endif
        if (!ReadNegotiation(this, m_waitDelay, m_upstreamSocket, upstreamInfo, &serial))
        {
            Q_EMIT ErrorMessage("Unable to read upstream negotiation");
            QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
            return false;
        }

        if (strncmp(upstreamInfo.m_negotiationInfoMap[NegotiationInfo_ProcessId].c_str(), processId, strlen(processId)) == 0)
        {
            Q_EMIT ErrorMessage("Attempted to negotiate with self");
            QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
            return false;
        }
#if defined(DEBUG_NEGOTATION)
        AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateProxy: Writing my negotiation to ENGINE %p", this);
#endif
        if (!WriteNegotiation(this, m_engineSocket, upstreamInfo, serial))
        {
            Q_EMIT ErrorMessage("Unable to write upstream negotiation to engine");
            QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
            return false;
        }
#if defined(DEBUG_NEGOTATION)
        AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateProxy: Reading ENGINE negotiation from ENGINE %p", this);
#endif
        if (!ReadNegotiation(this, m_waitDelay, m_engineSocket, engineInfo))
        {
            // note this is a special case.  Because of the way ADB FORWARD and tools like itnl work, you will get this the vast majority of the time.
            // often, the tunnel itself is able to connect, so we get this far - but the actual game is not running.
            // this can happen thousands of times while idle, so we are supressing this unless debugging
            Q_EMIT ErrorMessage("Unable to read engine negotation");
            QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
            return false;
        }
#if defined(DEBUG_NEGOTATION)
        AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateProxy: Writing ENGINE negotiation to UPSTREAM %p", this);
#endif
        if (!WriteNegotiation(this, m_upstreamSocket, engineInfo))
        {
            Q_EMIT ErrorMessage("Unable to write engine negotiation to upstream");
            QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
            return false;
        }
    }
    else
    {
        // let engine connect and deliver its info, then pass that upstream
        if (!ReadNegotiation(this, m_waitDelay, m_engineSocket, engineInfo, &serial))
        {
#if defined(DEBUG_NEGOTATION)
            AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateProxy: Unable to read engine negotiation %p", this);
#endif
            Q_EMIT ErrorMessage("Unable to read engine negotiation");
            QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
            return false;
        }

        if (!WriteNegotiation(this, m_upstreamSocket, engineInfo))
        {
#if defined(DEBUG_NEGOTATION)
            AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateProxy: Unable to write upstream negotiation %p", this);
#endif
            Q_EMIT ErrorMessage("Unable to write engine negotiation to upstream");
            QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
            return false;
        }

        if (!ReadNegotiation(this, m_waitDelay, m_upstreamSocket, upstreamInfo))
        {
#if defined(DEBUG_NEGOTATION)
            AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateProxy: Unable to write read upstream negotiation %p", this);
#endif
            Q_EMIT ErrorMessage("Unable to read upstream negotiation");
            QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
            return false;
        }

        if (strncmp(upstreamInfo.m_negotiationInfoMap[NegotiationInfo_ProcessId].c_str(), processId, strlen(processId)) == 0)
        {
#if defined(DEBUG_NEGOTATION)
            AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateProxy: negotiated with self.  %p", this);
#endif
            AZ_Warning("ConnectionWorker", false, "Attempted to negotiate with self");
            QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
            return false;
        }

        if (!WriteNegotiation(this, m_engineSocket, upstreamInfo, serial))
        {
#if defined(DEBUG_NEGOTATION)
            AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::NegotiateProxy: failed to writenegotiation to engine socket %p", this);
#endif

            Q_EMIT ErrorMessage("Unable to write upstream negotiation to engine");
            QTimer::singleShot(0, this, SLOT(DisconnectSockets()));
            return false;
        }
    }

    Q_EMIT Identifier((engineInfo.m_identifier + " PROXY").c_str());

    OnProxyNegotiated();

    return true;
}

// RequestTerminate can be called from anywhere, so we queue the actual
// termination to ensure it happens in the worker's thread
void ConnectionWorker::RequestTerminate()
{
    if (!m_alreadySentTermination)
    {
        m_terminate = true;
        m_alreadySentTermination = true;
        QMetaObject::invokeMethod(this, "TerminateConnection", Qt::BlockingQueuedConnection);
    }
}

void ConnectionWorker::TerminateConnection()
{
    disconnect(&m_engineSocket, &QTcpSocket::stateChanged, this, &ConnectionWorker::EngineSocketStateChanged);
    disconnect(&m_upstreamSocket, &QTcpSocket::stateChanged, this, &ConnectionWorker::UpstreamSocketStateChanged);
    DisconnectSockets();
    deleteLater();
}

void ConnectionWorker::ConnectSocket(qintptr socketDescriptor)
{
    AZ_Assert(socketDescriptor != -1, "ConectionWorker::ConnectSocket: Supplied socket is invalid");
    if (socketDescriptor != -1)
    {
        // calling setsocketdescriptor will cause it to invoke EngineSocketStateChanged instantly, which we don't want, so disconnect it temporarily.
        disconnect(&m_engineSocket, &QTcpSocket::stateChanged, this, &ConnectionWorker::EngineSocketStateChanged);
        m_engineSocket.setSocketDescriptor(socketDescriptor, QAbstractSocket::ConnectedState, QIODevice::ReadWrite);

        // check whitelist!
        if (!ConnectionWhiteListed())
        {
            AZ_TracePrintf(AssetProcessor::ConsoleChannel, " A connection attempt was ignored because it is not whitelisted.  Please consider adding white_list=(IP ADDRESS),localhost to the bootstrap.cfg");

            disconnect(&m_engineSocket, &QTcpSocket::readyRead, this, &ConnectionWorker::EngineSocketHasData);
            disconnect(&m_upstreamSocket, &QTcpSocket::stateChanged, this, &ConnectionWorker::UpstreamSocketStateChanged);

            DisconnectSockets();
            Q_EMIT ConnectionDisconnected();
            return;
        }

        connect(&m_engineSocket, &QTcpSocket::stateChanged, this, &ConnectionWorker::EngineSocketStateChanged);
        EngineSocketStateChanged(QAbstractSocket::ConnectedState);
    }
}

void ConnectionWorker::ConnectToEngine(QString ipAddress, quint16 port)
{
#if defined(DEBUG_NEGOTATION)
    AZ_TracePrintf(AssetProcessor::DebugChannel, " ConnectionWorker::ConnectToEngine");
#endif
    m_terminate = false;
    if (m_engineSocket.state() == QAbstractSocket::UnconnectedState)
    {
        m_initiatedConnection = true;
        m_engineSocket.connectToHost(ipAddress, port, QIODevice::ReadWrite);
        m_engineSocket.setSocketOption(QAbstractSocket::KeepAliveOption, 1);
        m_engineSocket.setSocketOption(QAbstractSocket::LowDelayOption, 1); //disable nagles algorithm
    }
}

void ConnectionWorker::ConnectProxySocket(QString proxyAddr, quint16 proxyPort)
{
#if defined(DEBUG_NEGOTATION)
    AZ_TracePrintf(AssetProcessor::DebugChannel, " ConnectionWorker::ConnectProxySocket");
#endif
    AZ_Assert(m_inProxyMode, "Should not be connecting a proxy socket when not in proxy mode");
    AZ_Assert(proxyPort > 0, "Proxy port should be greater than 0");
    if (m_upstreamSocket.state() == QAbstractSocket::UnconnectedState)
    {
        m_upstreamSocket.connectToHost(proxyAddr, proxyPort, QIODevice::ReadWrite);
        m_upstreamSocket.setSocketOption(QAbstractSocket::KeepAliveOption, 1);
        m_upstreamSocket.setSocketOption(QAbstractSocket::LowDelayOption, 1); //disable nagles algorithm
    }
}

void ConnectionWorker::OnProxyNegotiated()
{
    Q_EMIT ConnectionEstablished(m_engineSocket.peerAddress().toString(), m_engineSocket.peerPort());
    connect(&m_engineSocket, &QTcpSocket::readyRead, this, &ConnectionWorker::EngineSocketHasData);
    connect(&m_upstreamSocket, &QTcpSocket::readyRead, this, &ConnectionWorker::UpstreamSocketHasData);
    // force the sockets to evaluate any data recv'd between negotiation and now
    QTimer::singleShot(0, this, SLOT(EngineSocketHasData()));
    QTimer::singleShot(0, this, SLOT(UpstreamSocketHasData()));
}

bool ConnectionWorker::ConnectionWhiteListed()
{
    //white list connections
    QString incomingAddress;
    QHostAddress incominghostaddr = m_engineSocket.peerAddress();

    if (incominghostaddr != QHostAddress::Null)
    {
        // any ipv4 address will be like ::ffff:A.B.C.D, we have to retrieve A.B.C.D for comparision with the whitelisted addresses.
        // for example Qt will tell us the ipv6 (::ffff:127.0.0.1) for 127.0.0.1, 
        // but the whitelist below will report ipv4 (127.0.0.1) and ipv6 (::1), and they both won't match to ipv6 (::ffff:127.0.0.1).
        bool wasConverted = false;
        quint32 incomingIPv4 = incominghostaddr.toIPv4Address(&wasConverted);
        if (wasConverted)
        {
            incomingAddress = QHostAddress(incomingIPv4).toString();
        }
        else
        {
            incomingAddress = incominghostaddr.toString();
        }
    }

    QHostInfo incomingInfo = QHostInfo::fromName(incomingAddress);

    bool bIsWhitelisted = false;

    QString whitelist = AssetUtilities::ReadWhitelistFromBootstrap();

    AZStd::vector<AZStd::string> whitelistaddressList;
    AzFramework::StringFunc::Tokenize(whitelist.toUtf8().constData(), whitelistaddressList, ", \t\n\r");

    // allow localhost loopback always, regardless, there's no good reason to accidentally lock yourself out your own computer.
    for (const auto& address : QNetworkInterface::allAddresses()) // allAdresses returns the ip address of all local interfaces.
    {
        whitelistaddressList.push_back(address.toString().toUtf8().constData());
    }

    // does the incoming connection match any entries?
    for (const AZStd::string& whitelistaddress : whitelistaddressList)
    {
        if (bIsWhitelisted)
        {
            break;
        }

        QHostInfo whiteInfo;
        QHostAddress whiteHostAddress(whitelistaddress.c_str());
        if ((whiteHostAddress.isNull()))
        {
            whiteInfo = QHostInfo::fromName(QString::fromUtf8(whitelistaddress.c_str()));
        }
        else
        {
            QList<QHostAddress> addresses;
            addresses << whiteHostAddress;
            whiteInfo.setAddresses(addresses);
        }

        for (const auto& whiteAddress : whiteInfo.addresses())
        {
            if (bIsWhitelisted)
            {
                break;
            }

            for (const auto& incomingAddress : incomingInfo.addresses())
            {
                if (incomingAddress == whiteAddress)
                {
                    // its an exact match.
                    EBUS_EVENT(AssetProcessor::IncomingConnectionInfoBus, OnNewIncomingConnection, incomingAddress);
                    bIsWhitelisted = true;
                    break;
                }
            }
        }
    }

    return bIsWhitelisted;
}

void ConnectionWorker::UpstreamSocketStateChanged(QAbstractSocket::SocketState socketState)
{
#if defined(DEBUG_NEGOTATION)
    AZ_TracePrintf(AssetProcessor::DebugChannel, " ConnectionWorker::UpstreamSocketStateChanged to %i", (int)socketState);
#endif
    if (m_terminate)
    {
        return;
    }
    if (socketState == QAbstractSocket::ConnectedState)
    {
        m_upstreamSocketIsConnected = true;
#if defined(DEBUG_NEGOTATION)
        AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::UpstreamSocketStateChanged:  %p connected, now (%s, %s)", this, m_upstreamSocketIsConnected ? "True" : "False", m_engineSocketIsConnected ? "True" : "False");
#endif
        if (m_upstreamSocketIsConnected && m_engineSocketIsConnected) // if both sockets are now connected
        {
            QMetaObject::invokeMethod(this, "NegotiateProxy", Qt::QueuedConnection, Q_ARG(bool, m_initiatedConnection));
        }
    }
    else if (socketState == QAbstractSocket::UnconnectedState)
    {
        m_upstreamSocketIsConnected = false;
#if defined(DEBUG_NEGOTATION)
        AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::UpstreamSocketStateChanged:  %p unconnected, now (%s, %s)", this, m_upstreamSocketIsConnected ? "True" : "False", m_engineSocketIsConnected ? "True" : "False");

#endif
        disconnect(&m_upstreamSocket, &QTcpSocket::readyRead, 0, 0);

        DisconnectSockets();

        Q_EMIT ConnectionDisconnected();
    }
}

void ConnectionWorker::EngineSocketStateChanged(QAbstractSocket::SocketState socketState)
{
#if defined(DEBUG_NEGOTATION)
    AZ_TracePrintf(AssetProcessor::DebugChannel, " ConnectionWorker::EngineSocketStateChanged to %i", (int)socketState);
#endif
    if (m_terminate)
    {
        return;
    }

    if (socketState == QAbstractSocket::ConnectedState)
    {
        m_engineSocketIsConnected = true;
#if defined(DEBUG_NEGOTATION)
        AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::EngineSocketStateChanged:  %p connected now (%s, %s)", this, m_upstreamSocketIsConnected ? "True" : "False", m_engineSocketIsConnected ? "True" : "False");

#endif
        if (IsProxyMode() && m_upstreamSocketIsConnected && m_engineSocketIsConnected)  // if both sockets are now connected and in proxy mode
        {
            QMetaObject::invokeMethod(this, "NegotiateProxy", Qt::QueuedConnection, Q_ARG(bool, m_initiatedConnection));
        }
        else if (!IsProxyMode()) // if not in proxy mode, just one connected is enough
        {
            QMetaObject::invokeMethod(this, "NegotiateDirect", Qt::QueuedConnection, Q_ARG(bool, m_initiatedConnection));
        }
    }
    else if (socketState == QAbstractSocket::UnconnectedState)
    {
        m_engineSocketIsConnected = false;
#if defined(DEBUG_NEGOTATION)
        AZ_TracePrintf(AssetProcessor::DebugChannel, "ConnectionWorker::EngineSocketStateChanged:  %p unconnected, now (%s, %s)", this, m_upstreamSocketIsConnected ? "True" : "False", m_engineSocketIsConnected ? "True" : "False");

#endif
        disconnect(&m_engineSocket, &QTcpSocket::readyRead, 0, 0);
        DisconnectSockets();

        Q_EMIT ConnectionDisconnected();
    }
}

void ConnectionWorker::DisconnectSockets()
{
#if defined(DEBUG_NEGOTATION)
    AZ_TracePrintf(AssetProcessor::DebugChannel, " ConnectionWorker::DisconnectSockets");
#endif
    m_engineSocket.abort();
    m_upstreamSocket.abort();
    m_engineSocket.close();
    m_upstreamSocket.close();
}

bool ConnectionWorker::IsProxyMode() const
{
    return m_inProxyMode;
}

void ConnectionWorker::SetProxyMode(bool inProxyMode)
{
    m_inProxyMode = inProxyMode;
}

QTcpSocket& ConnectionWorker::GetProxySocket()
{
    return m_upstreamSocket;
}

void ConnectionWorker::Reset()
{
    m_terminate = false;
}

bool ConnectionWorker::Terminate()
{
    return m_terminate;
}

QTcpSocket& ConnectionWorker::GetSocket()
{
    return m_engineSocket;
}

bool ConnectionWorker::InitiatedConnection() const
{
    return m_initiatedConnection;
}
} // namespace AssetProcessor
#include <native/connection/connectionWorker.moc>
