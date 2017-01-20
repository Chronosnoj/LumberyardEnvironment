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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#ifndef CRYINCLUDE_TOOLS_RC_RESOURCECOMPILER_MAILER_H
#define CRYINCLUDE_TOOLS_RC_RESOURCECOMPILER_MAILER_H
#pragma once

#include <winsock.h>
#include <vector>

class CSMTPMailer
{
public:
    typedef string tstr;
    typedef std::vector<tstr> tstrcol;

    static const int DEFAULT_PORT = 25;

public:
    CSMTPMailer(const tstr& username, const tstr& password, const tstr& server, int port = DEFAULT_PORT);
    ~CSMTPMailer();

    bool Send(const tstr& from, const tstrcol& to, const tstrcol& cc, const tstrcol& bcc, const tstr& subject, const tstr& body, const tstrcol& attachments);
    const char* GetResponse() const;

private:
    void ReceiveLine(SOCKET connection);
    void SendLine(SOCKET connection, const char* format, ...) const;
    void SendRaw(SOCKET connection, const char* data, size_t dataLen) const;
    void SendFile(SOCKET connection, const char* filepath, const char* boundary) const;

    SOCKET Open(const char* host, unsigned short port,  sockaddr_in& serverAddress);

    void AddReceivers(SOCKET connection, const tstrcol& receivers);
    void AssignReceivers(SOCKET connection, const char* receiverTag, const tstrcol& receivers);
    void SendAttachments(SOCKET connection, const tstrcol& attachments, const char* boundary);

    bool IsEmpty(const tstrcol& col) const;

private:
    tstr m_server;
    tstr m_username;
    tstr m_password;
    int m_port;

    bool m_winSockAvail;
    tstr m_response;
};
#endif // CRYINCLUDE_TOOLS_RC_RESOURCECOMPILER_MAILER_H
