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
#include "AssetRequestHandler.h"

#include "native/AssetManager/AssetData.h"
#include "native/utilities/assetUtilEBusHelper.h"

#include <AzCore/Serialization/Utils.h>
#include <AzFramework/Asset/AssetProcessorMessages.h>
#include <AzCore/IO/GenericStreams.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <QDir>
#include <QFile>
#include <QTimer>

using namespace AssetProcessor;

namespace
{
    static const uint32_t s_assetPath = AssetUtilities::ComputeCRC32Lowercase("assetPath");
}

AssetRequestHandler::AssetRequestLine::AssetRequestLine(QString platform, QString searchTerm, bool isStatusRequest)
    : m_platform(platform)
    , m_searchTerm(searchTerm)
    , m_isStatusRequest(isStatusRequest)
{
}

bool AssetRequestHandler::AssetRequestLine::IsStatusRequest() const
{
    return m_isStatusRequest;
}

QString AssetRequestHandler::AssetRequestLine::GetPlatform() const
{
    return m_platform;
}
QString AssetRequestHandler::AssetRequestLine::GetSearchTerm() const
{
    return m_searchTerm;
}

int AssetRequestHandler::GetNumOutstandingAssetRequests() const
{
    return m_pendingAssetRequests.size();
}

void AssetRequestHandler::ProcessAssetRequest(NetworkRequestID networkRequestId, BaseAssetProcessorMessage* message, QString platform, bool fencingFailed)
{
    using RequestAssetStatus = AzFramework::AssetSystem::RequestAssetStatus;
    RequestAssetStatus* stat = azrtti_cast<RequestAssetStatus*>(message);
    
    if (!stat)
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessAssetRequest: Message is not of type %d.Incoming message type is %d.\n", RequestAssetStatus::MessageType(), message->GetMessageType());
        return;
    }

    QString assetPath = stat->m_searchTerm.c_str();
    bool isStatusRequest = stat->m_isStatusRequest;
    if (assetPath.isEmpty())
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "Failed to decode incoming request for asset status or compilation.\n");
        SendAssetStatus(networkRequestId, RequestAssetStatus::MessageType(), AssetStatus::AssetStatus_Unknown);
        return;
    }

    AZ_TracePrintf(AssetProcessor::DebugChannel, "Compile Group Requested: %s.\n", assetPath.toUtf8().data());
    m_pendingAssetRequests.insert(networkRequestId, AssetRequestHandler::AssetRequestLine(platform, assetPath, isStatusRequest));
    Q_EMIT RequestCompileGroup(networkRequestId, platform, assetPath);
}

void AssetRequestHandler::OnCompileGroupCreated(NetworkRequestID groupID, AssetStatus status)
{
    using namespace AzFramework::AssetSystem;
    auto located = m_pendingAssetRequests.find(groupID);

    if (located == m_pendingAssetRequests.end())
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "OnCompileGroupCreated: No such asset group found, ignoring.\n");
        return;
    }

    // If the RCController doesn't know about the file, then we should look again in the asset database
    // since in that case its either complete already or never existed in the first case
    if (status == AssetStatus_Unknown)
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "No assets in queue exist for compile group, checking on disk %s.\n ", located.value().GetSearchTerm().toUtf8().data());
        // the RC controller has no idea what this search term might equate to
        // ask whether it exists or not...
        Q_EMIT RequestAssetExists(groupID, located.value().GetPlatform(), located.value().GetSearchTerm());
    }
    else
    {
        if (located.value().IsStatusRequest())
        {
            // if its a status request, return it immediately and then remove it.
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Sending back status request for %s.\n", located.value().GetSearchTerm().toUtf8().data());
            SendAssetStatus(groupID, RequestAssetStatus::MessageType(), status);
            m_pendingAssetRequests.erase(located);
        }
        else
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Compile Group created for %s.  Waiting for all assets to be done.\n", located.value().GetSearchTerm().toUtf8().data());
        }

        // if its not a status request then we'll wait for OnCompileGroupFinished before responding.
    }
}


void AssetRequestHandler::OnCompileGroupFinished(NetworkRequestID groupID, AssetStatus status)
{
    auto located = m_pendingAssetRequests.find(groupID);

    if (located == m_pendingAssetRequests.end())
    {
        // this is okay to happen if its a status request.
        return;
    }

    AZ_TracePrintf(AssetProcessor::DebugChannel, "Compile Group finished: %s.\n", located.value().GetSearchTerm().toUtf8().data());
    SendAssetStatus(groupID, AzFramework::AssetSystem::RequestAssetStatus::MessageType(), status);

    m_pendingAssetRequests.erase(located);
}

//! Called from the outside in response to a RequestAssetExists.
void AssetRequestHandler::OnRequestAssetExistsResponse(NetworkRequestID groupID, bool exists)
{
    using namespace AzFramework::AssetSystem;
    auto located = m_pendingAssetRequests.find(groupID);

    if (located == m_pendingAssetRequests.end())
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "OnRequestAssetExistsResponse: No such compile group found, ignoring.\n");
        return;
    }

    AZ_TracePrintf(AssetProcessor::DebugChannel, "Compile Group Asset Exists response for %s.\n", located.value().GetSearchTerm().toUtf8().data());

    AssetStatus stat = exists ? AssetStatus_Compiled : AssetStatus_Missing;
    SendAssetStatus(groupID, RequestAssetStatus::MessageType(), stat);

    m_pendingAssetRequests.erase(located);
}

void AssetRequestHandler::SendAssetStatus(NetworkRequestID groupID, unsigned int type, AssetStatus status)
{
    AzFramework::AssetSystem::ResponseAssetStatus resp;
    resp.m_assetStatus = status;
    EBUS_EVENT_ID(groupID.first, AssetProcessor::ConnectionBus, Send, groupID.second, resp);
}

AssetRequestHandler::AssetRequestHandler()
{
    RegisterRequestHandler(AzFramework::AssetSystem::RequestAssetStatus::MessageType(), this);
}

void AssetRequestHandler::RegisterRequestHandler(unsigned int messageId, QObject* object)
{
    m_RequestHandlerMap.insert(messageId, object);
}

void AssetRequestHandler::DeRegisterRequestHandler(unsigned int messageId, QObject* object)
{
    auto located = m_RequestHandlerMap.find(messageId);

    if (located == m_RequestHandlerMap.end())
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessFilesToExamineQueue: Compile group request not found, ignoring.\n");
        return;
    }

    m_RequestHandlerMap.erase(located);
}


QString AssetRequestHandler::CreateFenceFile(unsigned int fenceId)
{
    QDir fenceDir;
    if (!AssetUtilities::ComputeFenceDirectory(fenceDir))
    {
        return QString();
    }

    QString fileName = QString("fenceFile~%1.%2").arg(fenceId).arg(FENCE_FILE_EXTENSION);
    QString fenceFileName = fenceDir.filePath(fileName);
    QFileInfo fileInfo(fenceFileName);

    if (!fileInfo.absoluteDir().exists())
    {
        // if fence dir does not exists ,than try to create it
        if (!fileInfo.absoluteDir().mkpath("."))
        {
            return QString();
        }
    }

    QFile fenceFile(fenceFileName);

    if (fenceFile.exists())
    {
        return QString();
    }

    bool result = fenceFile.open(QFile::WriteOnly);

    if (!result)
    {
        return QString();
    }

    fenceFile.close();
    return fileInfo.absoluteFilePath();
}

bool AssetRequestHandler::DeleteFenceFile(QString fenceFileName)
{
    return QFile::remove(fenceFileName);
}

void AssetRequestHandler::DeleteFenceFile_Retry(QObject* object, unsigned int fenceId, QString fenceFileName, NetworkRequestID key, BaseAssetProcessorMessage* message, QString platform, int retriesRemaining)
{
    if (DeleteFenceFile(fenceFileName))
    {
        // add an entry in map
        // We have successfully created and deleted the fence file, insert an entry for it in the pendingFenceRequest map
        // and return, we will only process this request once the APM indicates that it has detected the fence file
        m_pendingFenceRequestMap.insert(fenceId, RequestInfo(key, message, platform));
        return;
    }

    retriesRemaining--;

    if (retriesRemaining == 0)
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "AssetProcessor was unable to delete the fence file");

        // send request to the appropriate handler with fencingfailed set to true and return
        QMetaObject::invokeMethod(object, "RequestReady", Qt::QueuedConnection, Q_ARG(NetworkRequestID, key), Q_ARG(BaseAssetProcessorMessage*, message), Q_ARG(QString, platform), Q_ARG(bool, true));
    }
    else
    {
        auto deleteFenceFilefunctor = [this, object, fenceId, fenceFileName, key, message, platform, retriesRemaining]
        {
            DeleteFenceFile_Retry(object, fenceId, fenceFileName, key, message, platform, retriesRemaining);
        };

        QTimer::singleShot(100, this, deleteFenceFilefunctor);
    }
}

void AssetRequestHandler::OnNewIncomingRequest(unsigned int connId, unsigned int serial, QByteArray payload, QString platform)
{
    AZ::SerializeContext* serializeContext = nullptr;
    EBUS_EVENT_RESULT(serializeContext, AZ::ComponentApplicationBus, GetSerializeContext);
    AZ_Assert(serializeContext, "Unable to retrieve serialize context.");
    BaseAssetProcessorMessage* message = AZ::Utils::LoadObjectFromBuffer<BaseAssetProcessorMessage>(payload.constData(), payload.size(), serializeContext);
    if (!message)
    {
        AZ_Warning("Asset Request Handler", false, "OnNewIncomingRequest: Invalid object sent as network message to AssetRequestHandler.");
        return;
    }

    auto located = m_RequestHandlerMap.find(message->GetMessageType());
    if (located == m_RequestHandlerMap.end())
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "OnNewIncomingRequest: Message Handler not found for message type %d, ignoring.\n", message->GetMessageType());
        delete message;
        return;
    }

    NetworkRequestID key(connId, serial);
    QString fenceFileName;
    if (message->RequireFencing())
    {
        bool successfullyCreatedFenceFile = false;
        int fenceID = 0;
        for (int idx = 0; idx < g_RetriesForFenceFile; ++idx)
        {
            fenceID = ++m_fenceId;
            fenceFileName = CreateFenceFile(fenceID);
            if (!fenceFileName.isEmpty())
            {
                successfullyCreatedFenceFile = true;
                break;
            }
        }

        if (!successfullyCreatedFenceFile)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "AssetProcessor was unable to create the fence file");

            // send request to the appropriate handler with fencingfailed set to true and return
            QMetaObject::invokeMethod(located.value(), "RequestReady", Qt::QueuedConnection, Q_ARG(NetworkRequestID, key), Q_ARG(BaseAssetProcessorMessage*, message), Q_ARG(QString, platform), Q_ARG(bool, true));
        }
        else
        {
            // if we are here it means that we were able to create the fence file, we will try to delete it now with a fixed number of retries
            DeleteFenceFile_Retry(located.value(), fenceID, fenceFileName, key, message, platform, g_RetriesForFenceFile);
        }
        return;
    }

    // If we are here it indicates that the request does not require fencing, we will process the request directly
    QMetaObject::invokeMethod(located.value(), "RequestReady", Qt::QueuedConnection, Q_ARG(NetworkRequestID, key), Q_ARG(BaseAssetProcessorMessage*, message), Q_ARG(QString, platform));

}

void AssetRequestHandler::RequestReady(NetworkRequestID networkRequestId, BaseAssetProcessorMessage* message, QString platform,  bool fencingFailed)
{
    using namespace AzFramework::AssetSystem;

    if (message->GetMessageType() == RequestAssetStatus::MessageType())
    {
        ProcessAssetRequest(networkRequestId, message, platform, fencingFailed);
    }

    delete message;
}

void AssetRequestHandler::OnFenceFileDetected(unsigned int fenceId)
{
    auto fenceRequestFound = m_pendingFenceRequestMap.find(fenceId);
    if (fenceRequestFound == m_pendingFenceRequestMap.end())
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "OnFenceFileDetected: Fence File Request not found, ignoring.\n");
        return;
    }

    auto located = m_RequestHandlerMap.find(fenceRequestFound.value().m_message->GetMessageType());
    if (located == m_RequestHandlerMap.end())
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "OnFenceFileDetected: Message Handler not found, ignoring.\n");
        return;
    }

    QMetaObject::invokeMethod(located.value(), "RequestReady", Qt::QueuedConnection, Q_ARG(NetworkRequestID, fenceRequestFound.value().m_requestId), Q_ARG(BaseAssetProcessorMessage*, fenceRequestFound.value().m_message), Q_ARG(QString, fenceRequestFound.value().m_platform));
    m_pendingFenceRequestMap.erase(fenceRequestFound);
}


#include <native/AssetManager/AssetRequestHandler.moc>