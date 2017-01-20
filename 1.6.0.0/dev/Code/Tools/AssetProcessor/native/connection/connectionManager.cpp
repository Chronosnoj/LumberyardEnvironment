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
#include "connectionManager.h"
#include "connection.h"
#include <QSettings>
#include <QTimer>
#include <QCoreApplication>
#include <AzCore/std/time.h>

#include "native/utilities/IniConfiguration.h"

namespace
{
    // singleton Pattern
    ConnectionManager* s_singleton = nullptr;


    QString TranslateStatus(int status)
    {
        switch (status)
        {
        case 0:
            return QObject::tr("Disconnected");
        case 1:
            return QObject::tr("Connected");
        case 2:
            return QObject::tr("Connecting");
        default:
            return "";
        }
    }
}

ConnectionManager::ConnectionManager(AssetProcessor::PlatformConfiguration* platformConfig, int defaultProxyPort, QObject* parent)
    : QAbstractItemModel(parent)
    , m_nextConnectionId(1)
    , m_defaultProxyPort(defaultProxyPort)
#if defined(FORCE_PROXY_MODE)
    , m_proxyMode(true)
#else
    , m_proxyMode(false)
#endif
    , m_proxyIPAdressAndPort(QString())
    , m_platformConfig(platformConfig)
{
    Q_ASSERT(!s_singleton);
    s_singleton = this;
    qRegisterMetaType<qintptr>("qintptr");
    qRegisterMetaType<quint16>("quint16");

    AssetProcessor::IncomingConnectionInfoBus::Handler::BusConnect();
}

ConnectionManager::~ConnectionManager()
{
    s_singleton = nullptr;
}

ConnectionManager* ConnectionManager::Get()
{
    return s_singleton;
}

void ConnectionManager::ReadProxyServerInformation()
{
    const IniConfiguration* iniConfiguration = IniConfiguration::Get();
    QString proxyInfo = iniConfiguration->proxyInformation();
    SetProxyInformation(proxyInfo);
}

int ConnectionManager::getCount() const
{
    return m_connectionMap.size();
}

Connection* ConnectionManager::getConnection(unsigned int connectionId)
{
    auto iter = m_connectionMap.find(connectionId);
    if (iter != m_connectionMap.end())
    {
        return iter.value();
    }
    return nullptr;
}

ConnectionMap& ConnectionManager::getConnectionMap()
{
    return m_connectionMap;
}

unsigned int ConnectionManager::addConnection(qintptr socketDescriptor)
{
    beginResetModel();

    Connection* connection = new Connection(m_platformConfig, m_proxyMode, socketDescriptor, m_proxyIPAdressAndPort, m_defaultProxyPort, this);
    connection->SetConnectionId(m_nextConnectionId);
    connect(connection, &Connection::StatusChanged, this, &ConnectionManager::OnStatusChanged);
    QObject::connect(connection, &Connection::DeliverMessage, this, &ConnectionManager::SendMessageToService);
    QObject::connect(connection, SIGNAL(DisconnectConnection(unsigned int)), this, SIGNAL(ConnectionDisconnected(unsigned int)));
    QObject::connect(connection, SIGNAL(ConnectionDestroyed(unsigned int)), this, SLOT(RemoveConnectionFromMap(unsigned int)));
    QObject::connect(connection, SIGNAL(Error(unsigned int, QString)), this, SIGNAL(ConnectionError(unsigned int, QString)));
    QObject::connect(this, &ConnectionManager::ProxyInfoChanged, connection, &Connection::SetProxyInformation);
    QObject::connect(this, &ConnectionManager::ProxyConnectChanged, connection, &Connection::SetProxyMode, Qt::UniqueConnection);
    m_connectionMap.insert(m_nextConnectionId, connection);
    Q_EMIT connectionAdded(m_nextConnectionId, connection);
    m_nextConnectionId++;

    endResetModel();

    return m_nextConnectionId - 1;
}

void ConnectionManager::OnStatusChanged(unsigned int connId)
{
    QList<unsigned int> keys = m_connectionMap.keys();
    int row = keys.indexOf(connId);

    QModelIndex theIndex = index(row, 0, QModelIndex());
    Q_EMIT dataChanged(theIndex, theIndex);
    // Here, we only want to emit the bus event when the very last of a particular platform leaves, or the first one joins, so we keep a count:
    auto foundElement = m_connectionMap.find(connId);
    if (foundElement == m_connectionMap.end())
    {
        return;
    }

    Connection* connection = foundElement.value();
    QString assetPlatform = connection->AssetPlatform();

    if (connection->Status() == Connection::Connected)
    {
        int priorCount = 0;
        auto existingEntry = m_platformsConnected.find(connection->AssetPlatform());
        if (existingEntry == m_platformsConnected.end())
        {
            m_platformsConnected.insert(assetPlatform, 1);
        }
        else
        {
            priorCount = existingEntry.value();
            m_platformsConnected[assetPlatform] = priorCount + 1;
        }

        if (priorCount == 0)
        {
            EBUS_EVENT(AssetProcessorPlatformBus, AssetProcessorPlatformConnected, assetPlatform.toUtf8().data());
        }
    }
    else
    {
        // connection dropped!
        int priorCount = m_platformsConnected[assetPlatform];
        m_platformsConnected[assetPlatform] = priorCount - 1;
        if (priorCount == 1)
        {
            EBUS_EVENT(AssetProcessorPlatformBus, AssetProcessorPlatformDisconnected, assetPlatform.toUtf8().data());
        }
    }
}


QModelIndex ConnectionManager::parent(const QModelIndex& index) const
{
    return QModelIndex();
}


QModelIndex ConnectionManager::index(int row, int column, const QModelIndex& parent) const
{
    if (row >= rowCount(parent) || column >= columnCount(parent))
    {
        return QModelIndex();
    }
    return createIndex(row, column);
}


int ConnectionManager::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : Column::Max;
}


int ConnectionManager::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return m_connectionMap.count();
}


QVariant ConnectionManager::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    if (index.row() >= m_connectionMap.count())
    {
        return QVariant();
    }


    QList<unsigned int> keys = m_connectionMap.keys();
    int key = keys[index.row()];

    auto connectionIter = m_connectionMap.find(key);

    if (connectionIter == m_connectionMap.end())
    {
        return QVariant();
    }

    Connection* connection = connectionIter.value();

    switch (role)
    {
    case Qt::CheckStateRole:
        if (index.column() == AutoConnectColumn)
        {
            return connection->AutoConnect() ? Qt::Checked : Qt::Unchecked;
        }
        break;
    case Qt::EditRole:
    case Qt::DisplayRole:
        switch (index.column())
        {
        case StatusColumn:
            return TranslateStatus(connection->Status());
        case IdColumn:
            return connection->Identifier();
        case IpColumn:
            return connection->IpAddress();
        case PortColumn:
            return connection->Port();
        case PlatformColumn:
            return connection->AssetPlatform();
        case AutoConnectColumn:
            return QVariant();
        }
        break;
    }

    return QVariant();
}

Qt::ItemFlags ConnectionManager::flags(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return QAbstractItemModel::flags(index);
    }

    if (index.column() == IdColumn || index.column() == IpColumn || index.column() == PortColumn)
    {
        return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
    }

    if (index.column() == AutoConnectColumn)
    {
        return QAbstractItemModel::flags(index) | Qt::ItemIsUserCheckable;
    }

    return QAbstractItemModel::flags(index);
}


QVariant ConnectionManager::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case StatusColumn:
            return tr("Status");
        case IdColumn:
            return tr("ID");
        case IpColumn:
            return tr("IP");
        case PortColumn:
            return tr("Port");
        case PlatformColumn:
            return tr("Platform");
        case AutoConnectColumn:
            return tr("Auto Connect");
        default:
            break;
        }
    }

    return QAbstractItemModel::headerData(section, orientation, role);
}


bool ConnectionManager::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
    {
        return false;
    }

    QList<unsigned int> keys = m_connectionMap.keys();
    int key = keys[index.row()];
    Connection* connection = m_connectionMap[key];

    switch (index.column())
    {
    case PortColumn:
        connection->SetPort(value.toInt());
        break;
    case IpColumn:
        connection->SetIpAddress(value.toString());
        break;
    case IdColumn:
        connection->SetIdentifier(value.toString());
        break;
    case AutoConnectColumn:
        connection->SetAutoConnect(value.toBool());
        break;
    default:
        break;
    }

    Q_EMIT dataChanged(index, index);

    return true;
}


unsigned int ConnectionManager::GetConnectionId(QString ipaddress, int port)
{
    for (auto iter = m_connectionMap.begin(); iter != m_connectionMap.end(); iter++)
    {
        if ((iter.value()->IpAddress().compare(ipaddress, Qt::CaseInsensitive) == 0) && iter.value()->Port() == port)
        {
            return iter.value()->ConnectionId();
        }
    }
    return 0;
}

void ConnectionManager::removeConnection(const QModelIndex& index)
{
    if (!index.isValid())
    {
        return;
    }

    if (index.row() >= m_connectionMap.count())
    {
        return;
    }


    QList<unsigned int> keys = m_connectionMap.keys();
    int key = keys[index.row()];

    removeConnection(key);
}


void ConnectionManager::SaveConnections()
{
    QSettings settings;
    settings.beginWriteArray("Connections");
    int idx = 0;
    for (auto iter = m_connectionMap.begin(); iter != m_connectionMap.end(); iter++)
    {
        settings.setArrayIndex(idx);
        iter.value()->SaveConnection(settings);
        idx++;
    }
    settings.endArray();
}

void ConnectionManager::LoadConnections()
{
    QSettings settings;
    int numElement = settings.beginReadArray("Connections");
    for (int idx = 0; idx < numElement; ++idx)
    {
        settings.setArrayIndex(idx);
        getConnection(addConnection())->LoadConnection(settings);
    }
    settings.endArray();
}

void ConnectionManager::RegisterService(unsigned int type, regFunc func)
{
    m_messageRoute.insert(type, func);
}

bool ConnectionManager::ProxyConnect()
{
#if defined (FORCE_PROXY_MODE)
    return true;
#else
    return m_proxyMode;
#endif
}

QString ConnectionManager::proxyInformation()
{
    return m_proxyIPAdressAndPort;
}


void ConnectionManager::NewConnection(qintptr socketDescriptor)
{
    addConnection(socketDescriptor);
}

void ConnectionManager::SetProxyConnect(bool value)
{
    if (m_proxyMode != value)
    {
        m_proxyMode = value;
        Q_EMIT ProxyConnectChanged(m_proxyMode);
    }
}

void ConnectionManager::SetProxyInformation(QString proxyInformation)
{
    if (m_proxyIPAdressAndPort.compare(proxyInformation) != 0)
    {
        if (!proxyInformation.contains(':'))
        {
            // if the user has only entered the IPAddress in the proxy field, we will assume that the proxy port is 45643, which is the AP default listening port.
            m_proxyIPAdressAndPort = QString("%1:%2").arg(proxyInformation).arg(45643);
        }
        else
        {
            m_proxyIPAdressAndPort = proxyInformation;
        }
        Q_EMIT ProxyInfoChanged(m_proxyIPAdressAndPort);
    }
}

void ConnectionManager::AddBytesReceived(unsigned int connId, qint64 add, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddBytesReceived(add, update);
    }
}

void ConnectionManager::AddBytesSent(unsigned int connId, qint64 add, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddBytesSent(add, update);
    }
}

void ConnectionManager::AddBytesRead(unsigned int connId, qint64 add, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddBytesRead(add, update);
    }
}

void ConnectionManager::AddBytesWritten(unsigned int connId, qint64 add, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddBytesWritten(add, update);
    }
}

void ConnectionManager::AddOpenRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddOpenRequest(update);
    }
}

void ConnectionManager::AddCloseRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddCloseRequest(update);
    }
}

void ConnectionManager::AddOpened(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddOpened(update);
    }
}

void ConnectionManager::AddClosed(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddClosed(update);
    }
}

void ConnectionManager::AddReadRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddReadRequest(update);
    }
}

void ConnectionManager::AddWriteRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddWriteRequest(update);
    }
}

void ConnectionManager::AddTellRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddTellRequest(update);
    }
}

void ConnectionManager::AddSeekRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddSeekRequest(update);
    }
}

void ConnectionManager::AddIsReadOnlyRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddIsReadOnlyRequest(update);
    }
}

void ConnectionManager::AddIsDirectoryRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddIsDirectoryRequest(update);
    }
}

void ConnectionManager::AddSizeRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddSizeRequest(update);
    }
}

void ConnectionManager::AddModificationTimeRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddModificationTimeRequest(update);
    }
}

void ConnectionManager::AddExistsRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddExistsRequest(update);
    }
}

void ConnectionManager::AddFlushRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddFlushRequest(update);
    }
}

void ConnectionManager::AddCreatePathRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddCreatePathRequest(update);
    }
}

void ConnectionManager::AddDestroyPathRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddDestroyPathRequest(update);
    }
}

void ConnectionManager::AddRemoveRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddRemoveRequest(update);
    }
}

void ConnectionManager::AddCopyRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddCopyRequest(update);
    }
}

void ConnectionManager::AddRenameRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddRenameRequest(update);
    }
}

void ConnectionManager::AddFindFileNamesRequest(unsigned int connId, bool update)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->AddFindFileNamesRequest(update);
    }
}

void ConnectionManager::UpdateBytesReceived(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateBytesReceived();
    }
}

void ConnectionManager::UpdateBytesSent(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateBytesSent();
    }
}

void ConnectionManager::UpdateBytesRead(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateBytesRead();
    }
}

void ConnectionManager::UpdateBytesWritten(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateBytesWritten();
    }
}

void ConnectionManager::UpdateOpenRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateOpenRequest();
    }
}

void ConnectionManager::UpdateCloseRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateCloseRequest();
    }
}

void ConnectionManager::UpdateOpened(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateOpened();
    }
}

void ConnectionManager::UpdateClosed(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateClosed();
    }
}

void ConnectionManager::UpdateReadRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateReadRequest();
    }
}

void ConnectionManager::UpdateWriteRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateWriteRequest();
    }
}

void ConnectionManager::UpdateTellRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateTellRequest();
    }
}

void ConnectionManager::UpdateSeekRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateSeekRequest();
    }
}

void ConnectionManager::UpdateIsReadOnlyRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateIsReadOnlyRequest();
    }
}

void ConnectionManager::UpdateIsDirectoryRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateIsDirectoryRequest();
    }
}

void ConnectionManager::UpdateSizeRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateSizeRequest();
    }
}

void ConnectionManager::UpdateModificationTimeRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateModificationTimeRequest();
    }
}

void ConnectionManager::UpdateExistsRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateExistsRequest();
    }
}

void ConnectionManager::UpdateFlushRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateFlushRequest();
    }
}

void ConnectionManager::UpdateCreatePathRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateCreatePathRequest();
    }
}

void ConnectionManager::UpdateDestroyPathRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateDestroyPathRequest();
    }
}

void ConnectionManager::UpdateRemoveRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateRemoveRequest();
    }
}

void ConnectionManager::UpdateCopyRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateCopyRequest();
    }
}

void ConnectionManager::UpdateRenameRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateRenameRequest();
    }
}

void ConnectionManager::UpdateFindFileNamesRequest(unsigned int connId)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->UpdateFindFileNamesRequest();
    }
}

void ConnectionManager::UpdateConnectionMetrics()
{
    for (auto iter = m_connectionMap.begin(); iter != m_connectionMap.end(); iter++)
    {
        iter.value()->UpdateMetrics();
    }
}

void ConnectionManager::SendMessageToService(unsigned int connId, unsigned int type, unsigned int serial, QByteArray payload)
{
    QString platform = getConnection(connId)->AssetPlatform();
    auto iter = m_messageRoute.find(type);
    while (iter != m_messageRoute.end() && iter.key() == type)
    {
        iter.value()(connId, type, serial, payload, platform);
        iter++;
    }
}

void ConnectionManager::SendMessageToConnection(unsigned int connId, unsigned int type, unsigned int serial, QByteArray payload)
{
    auto iter = m_connectionMap.find(connId);
    if (iter != m_connectionMap.end())
    {
        iter.value()->SendMessageToWorker(type, serial, payload);
    }
}

void ConnectionManager::SendMessageToAllConnections(unsigned int type, unsigned int serial, QByteArray payload)
{
    for (auto iter = m_connectionMap.begin(); iter != m_connectionMap.end(); iter++)
    {
        iter.value()->SendMessageToWorker(type, serial, payload);
    }
}

void ConnectionManager::SendMessageToPlatform(QString platform, unsigned int type, unsigned int serial, QByteArray payload)
{
    for (auto connection : m_connectionMap)
    {
        if (connection->AssetPlatform() == platform)
        {
            connection->SendMessageToWorker(type, serial, payload);
        }
    }
}

//Entry point to removing a connection
//Callable from the GUI or the app about to close
void ConnectionManager::removeConnection(unsigned int connectionId)
{
    Connection* conn = getConnection(connectionId);
    if (conn)
    {
        Q_EMIT beforeConnectionRemoved(connectionId);
        conn->SetAutoConnect(false);
        conn->Terminate();
    }
}


void ConnectionManager::RemoveConnectionFromMap(unsigned int connectionId)
{
    beginResetModel();

    auto iter = m_connectionMap.find(connectionId);
    if (iter != m_connectionMap.end())
    {
        m_connectionMap.erase(iter);
        Q_EMIT ConnectionRemoved(connectionId);
    }

    endResetModel();
}

void ConnectionManager::QuitRequested()
{
    QList<Connection*> temporaryList;
    for (auto iter = m_connectionMap.begin(); iter != m_connectionMap.end(); iter++)
    {
        temporaryList.append(iter.value());
    }

    for (auto iter = temporaryList.begin(); iter != temporaryList.end(); iter++)
    {
        (*iter)->Terminate();
    }
    QTimer::singleShot(0, this, SLOT(MakeSureConnectionMapEmpty()));
}

void ConnectionManager::MakeSureConnectionMapEmpty()
{
    if (!m_connectionMap.isEmpty())
    {
        // keep trying to shut connections down, in case some is in an interesting state, where one was being negotiated while we died.
        // note that the end of QuitRequested will ultimately result in this being tried again next time.
        QuitRequested();
    }
    else
    {
        Q_EMIT ReadyToQuit(this);
    }
}

void ConnectionManager::OnNewIncomingConnection(QHostAddress hostAddress)
{
    AZ::u64 currTime = AZStd::GetTimeUTCMilliSecond();
    if ((m_lastHostAddress == hostAddress) && (currTime - m_lastConnectionTimeInUTCMilliSecs <= 100))
    {
        // if we are here it means that we have detected a proxy connect to itself. Disabling proxy connect and showing a message to the user
        QMetaObject::invokeMethod(this, "SetProxyConnect", Qt::QueuedConnection, Q_ARG(bool, false));// we are queuing it because this function call will happen from the connectionworker thread
        EBUS_EVENT(AssetProcessor::MessageInfoBus, ProxyConnectFailed);
        return;
    }

    m_lastHostAddress = hostAddress;
    m_lastConnectionTimeInUTCMilliSecs = currTime;
}

#include <native/connection/connectionManager.moc>