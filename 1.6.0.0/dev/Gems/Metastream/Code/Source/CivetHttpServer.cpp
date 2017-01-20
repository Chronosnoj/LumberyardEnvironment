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
#include "StdAfx.h"
#include "CivetHttpServer.h"
#include <AzCore/base.h>

#include <sstream>

using namespace Metastream;

class CivetHttpHandler : public CivetHandler
{
public:
    CivetHttpHandler(const CivetHttpServer* parent)
        : m_parent(parent)
    {
    }

    bool handleGet(CivetServer* server, struct mg_connection* conn)
    {
        const mg_request_info* request = mg_get_request_info(conn);

        std::map<std::string, std::string> filters;
        if (request->query_string != nullptr)
        {
            filters = BaseHttpServer::TokenizeQuery(request->query_string);
        }
                
        HttpResponse response;

        auto table = filters.find("table");
        if (table != filters.end())
        {
            auto key = filters.find("key");
            if (key != filters.end())
            {
                std::vector<std::string> keyList = BaseHttpServer::SplitValueList(key->second, ',');
                response = m_parent->GetDataValues(table->second, keyList);
            }
            else
            {
                response = m_parent->GetDataKeys(table->second);
            }
        }
        else
        {
            response = m_parent->GetDataTables();
        }

        mg_printf(conn, BaseHttpServer::HttpStatus(response.code).c_str());
        mg_printf(conn, BaseHttpServer::SerializeHeaders(response.headers).c_str());
        mg_printf(conn, response.body.c_str());
        return true;
    }

private:
    const CivetHttpServer* m_parent;
};

class CivetWSHandler : public CivetWebSocketHandler
{
public:
    CivetWSHandler(const CivetHttpServer* parent)
        : m_parent(parent)
    {
    }

    bool handleConnection(CivetServer *server, const struct mg_connection *conn) override
    {
        return true;
    }

    void handleReadyState(CivetServer *server, struct mg_connection *conn) override
    {
    }

    bool handleData(CivetServer *server, struct mg_connection *conn, int bits, char *data, size_t data_len) override
    {
        // RFC for websockets: https://tools.ietf.org/html/rfc6455
        // bits represents the websocket frame flags

        // we check if this is the final fragment (FIN)
        if (bits & 0x80) {
            bits &= 0x7f; // extract only the opcode
            switch (bits) {
            case WEBSOCKET_OPCODE_CONTINUATION:
                break;
            case WEBSOCKET_OPCODE_TEXT:
            {
                std::map<std::string, std::string> filters;
                if (data != nullptr)
                {
                    filters = BaseHttpServer::TokenizeQuery(std::string(data, data_len).c_str());
                }

                HttpResponse response;

                auto table = filters.find("table");
                if (table != filters.end())
                {
                    auto key = filters.find("key");
                    if (key != filters.end())
                    {
                        std::vector<std::string> keyList = BaseHttpServer::SplitValueList(key->second, ',');
                        response = m_parent->GetDataValues(table->second, keyList);
                    }
                    else
                    {
                        response = m_parent->GetDataKeys(table->second);
                    }
                }
                else
                {
                    response = m_parent->GetDataTables();
                }
                std::string payload(response.body);
                mg_websocket_write(conn, WEBSOCKET_OPCODE_TEXT, payload.c_str(), payload.size() + 1);
                break;
            }
            case WEBSOCKET_OPCODE_BINARY:
                break;
            case WEBSOCKET_OPCODE_CONNECTION_CLOSE:
                /* If client initiated close, respond with close message in acknowledgment */
                mg_websocket_write(conn, WEBSOCKET_OPCODE_CONNECTION_CLOSE, "", 0);
                return 0; /* time to close the connection */
                break;
            case WEBSOCKET_OPCODE_PING:
                /* client sent PING, respond with PONG */
                mg_websocket_write(conn, WEBSOCKET_OPCODE_PONG, "", 0);
                break;
            case WEBSOCKET_OPCODE_PONG:
                /* received PONG to our PING, no action */
                break;
            default:
                AZ_Error("Metastream", false, "Unknown flags: %02x\n", bits);
                break;
            }
        }

        return true;
    }

    virtual void handleClose(CivetServer *server, const struct mg_connection *conn) override
    {
    }

private:
    const CivetHttpServer* m_parent;
};

CivetHttpServer::CivetHttpServer(const DataCache* cache) :
    BaseHttpServer(cache),
    m_server(nullptr)
{
    m_handler = new CivetHttpHandler(this);
    m_webSocketHandler = new CivetWSHandler(this);
}

CivetHttpServer::~CivetHttpServer()
{
    Stop();
    delete m_handler;
    delete m_webSocketHandler;
}

bool CivetHttpServer::Start(int port, const std::string& path)
{
    std::vector<std::string> options = {
        "listening_ports", std::to_string(port),
        "document_root", path,
        "enable_directory_listing", "no"
    };

    // Note: the 3rd party software, Civetweb, uses exceptions.
    // Using try/catch to handle failure gracefully without having to modify Civetweb.
    try
    {
        // Create and start the server
        m_server = new CivetServer(options);
    }
    catch (CivetException)
    {
        // Failed to create/start server
        return false;
    }
    

    // Add a handler for all requests
    m_server->addHandler("/data", m_handler);
    m_server->addWebSocketHandler("/ws", m_webSocketHandler);

    return true;
}

void CivetHttpServer::Stop()
{
    if (m_server)
    {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
}

