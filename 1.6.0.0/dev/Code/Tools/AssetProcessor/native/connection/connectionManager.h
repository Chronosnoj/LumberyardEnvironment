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
#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H
#include <AzCore/std/function/function_fwd.h> // <functional> complains about exception handling and is not okay to mix with azcore/etc stuff.
#include <QReadWriteLock>
#include <QMap>
#include <QMultiMap>
#include <QObject>
#include <QString>
#include <qabstractitemmodel.h>
#include "native/utilities/assetUtilEBusHelper.h"

class Connection;
typedef AZStd::function<void(unsigned int, unsigned int, unsigned int, QByteArray, QString)> regFunc;
typedef QMap<unsigned int, Connection*> ConnectionMap;
typedef QMultiMap<unsigned int, regFunc> RouteMultiMap;

namespace AssetProcessor
{
    class PlatformConfiguration;
}

/** This is a container class for connection
 */
class ConnectionManager
    : public QAbstractItemModel
    , public AssetProcessor::IncomingConnectionInfoBus::Handler
{
    Q_OBJECT
    Q_PROPERTY(bool proxyConnect  READ ProxyConnect WRITE SetProxyConnect NOTIFY ProxyConnectChanged)
    Q_PROPERTY(QString proxyInformation  READ proxyInformation WRITE SetProxyInformation NOTIFY ProxyInfoChanged)

public:

    enum Column
    {
        StatusColumn,
        IdColumn,
        IpColumn,
        PortColumn,
        PlatformColumn,
        AutoConnectColumn,
        Max
    };

    explicit ConnectionManager(AssetProcessor::PlatformConfiguration* platformConfig, int defaultProxyPort = -1, QObject* parent = 0);
    virtual ~ConnectionManager();
    // Singleton pattern:
    static ConnectionManager* Get();
    Q_INVOKABLE int getCount() const;//lowerCamelCase for qml
    Q_INVOKABLE Connection* getConnection(unsigned int connectionId);//lowerCamelCase for qml
    Q_INVOKABLE ConnectionMap& getConnectionMap();//lowerCamelCase for qml
    Q_INVOKABLE unsigned int addConnection(qintptr socketDescriptor = -1);//lowerCamelCase for qml
    Q_INVOKABLE void removeConnection(unsigned int connectionId);//lowerCamelCase for qml
    unsigned int GetConnectionId(QString ipaddress, int port);
    void SaveConnections();
    void LoadConnections();
    void RegisterService(unsigned int type, regFunc func);
    bool ProxyConnect();
    QString proxyInformation();//lowerCamelCase for qml
    void ReadProxyServerInformation();


    //QAbstractItemListModel
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex&) const override;
    QModelIndex parent(const QModelIndex&) const override;
    int columnCount(const QModelIndex& parent) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    void removeConnection(const QModelIndex& index);

    //////////////////////////////////////////////////////////////////////////
    // AssetProcessor::IncomingConnectionInfoBus::Handler overrides
    void OnNewIncomingConnection(QHostAddress hostAddress) override;
    //////////////////////////////////////////////////////////////////////////

Q_SIGNALS:
    void connectionAdded(unsigned int connectionId, Connection* connection);//lowerCamelCase for qml
    void beforeConnectionRemoved(unsigned int connectionId);//lowerCamelCase for qml
    void ProxyInfoChanged(QString proxyInfo);//lowerCamelCase for qml

    void ConnectionDisconnected(unsigned int connectionId);
    void ConnectionRemoved(unsigned int connectionId);

    void ProxyConnectChanged(bool inProxyMode);
    void ConnectionError(unsigned int connId, QString error);

    void ReadyToQuit(QObject* source);

public Q_SLOTS:
    void SendMessageToService(unsigned int connId, unsigned int type, unsigned int serial, QByteArray payload);
    void SendMessageToConnection(unsigned int connId, unsigned int type, unsigned int serial, QByteArray payload);
    void SendMessageToAllConnections(unsigned int type, unsigned int serial, QByteArray payload);
    void SendMessageToPlatform(QString platform, unsigned int type, unsigned int serial, QByteArray payload);
    void QuitRequested();
    void RemoveConnectionFromMap(unsigned int connectionId);
    void MakeSureConnectionMapEmpty();
    void NewConnection(qintptr socketDescriptor);
    void SetProxyConnect(bool value);
    void SetProxyInformation(QString proxyInformation);
    //metrics
    void AddBytesReceived(unsigned int connId, qint64 add, bool update);
    void AddBytesSent(unsigned int connId, qint64 add, bool update);
    void AddBytesRead(unsigned int connId, qint64 add, bool update);
    void AddBytesWritten(unsigned int connId, qint64 add, bool update);
    void AddOpenRequest(unsigned int connId, bool update);
    void AddCloseRequest(unsigned int connId, bool update);
    void AddOpened(unsigned int connId, bool update);
    void AddClosed(unsigned int connId, bool update);
    void AddReadRequest(unsigned int connId, bool update);
    void AddWriteRequest(unsigned int connId, bool update);
    void AddTellRequest(unsigned int connId, bool update);
    void AddSeekRequest(unsigned int connId, bool update);
    void AddIsReadOnlyRequest(unsigned int connId, bool update);
    void AddIsDirectoryRequest(unsigned int connId, bool update);
    void AddSizeRequest(unsigned int connId, bool update);
    void AddModificationTimeRequest(unsigned int connId, bool update);
    void AddExistsRequest(unsigned int connId, bool update);
    void AddFlushRequest(unsigned int connId, bool update);
    void AddCreatePathRequest(unsigned int connId, bool update);
    void AddDestroyPathRequest(unsigned int connId, bool update);
    void AddRemoveRequest(unsigned int connId, bool update);
    void AddCopyRequest(unsigned int connId, bool update);
    void AddRenameRequest(unsigned int connId, bool update);
    void AddFindFileNamesRequest(unsigned int connId, bool update);

    void UpdateBytesReceived(unsigned int connId);
    void UpdateBytesSent(unsigned int connId);
    void UpdateBytesRead(unsigned int connId);
    void UpdateBytesWritten(unsigned int connId);
    void UpdateOpenRequest(unsigned int connId);
    void UpdateCloseRequest(unsigned int connId);
    void UpdateOpened(unsigned int connId);
    void UpdateClosed(unsigned int connId);
    void UpdateReadRequest(unsigned int connId);
    void UpdateWriteRequest(unsigned int connId);
    void UpdateTellRequest(unsigned int connId);
    void UpdateSeekRequest(unsigned int connId);
    void UpdateIsReadOnlyRequest(unsigned int connId);
    void UpdateIsDirectoryRequest(unsigned int connId);
    void UpdateSizeRequest(unsigned int connId);
    void UpdateModificationTimeRequest(unsigned int connId);
    void UpdateExistsRequest(unsigned int connId);
    void UpdateFlushRequest(unsigned int connId);
    void UpdateCreatePathRequest(unsigned int connId);
    void UpdateDestroyPathRequest(unsigned int connId);
    void UpdateRemoveRequest(unsigned int connId);
    void UpdateCopyRequest(unsigned int connId);
    void UpdateRenameRequest(unsigned int connId);
    void UpdateFindFileNamesRequest(unsigned int connId);

    void UpdateConnectionMetrics();

    void OnStatusChanged(unsigned int connId);

private:
    unsigned int m_nextConnectionId;
    ConnectionMap m_connectionMap;
    RouteMultiMap m_messageRoute;
    int m_defaultProxyPort;
    QHostAddress m_lastHostAddress = QHostAddress::Null;
    AZ::u64 m_lastConnectionTimeInUTCMilliSecs = 0;
    bool m_proxyMode;
    QString m_proxyIPAdressAndPort;
    AssetProcessor::PlatformConfiguration* m_platformConfig = nullptr;

    // keeps track of how many platforms are connected of a given type
    // the key is the name of the platform, and the value is the number of those kind of platforms.
    QHash<QString, int> m_platformsConnected;
};


#endif // CONNECTIONMANAGER_H
