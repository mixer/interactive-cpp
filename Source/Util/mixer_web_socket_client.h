//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#pragma once
#include <cpprest/ws_client.h>

NAMESPACE_MICROSOFT_MIXER_BEGIN

class mixer_web_socket_client : public std::enable_shared_from_this<mixer_web_socket_client>
{
public:
    mixer_web_socket_client();

    virtual pplx::task<void> connect(
        _In_ const web::uri& uri,
        _In_ const string_t& bearerToken,
        _In_ const string_t& interactiveVersion,
        _In_ const string_t& protocolVersion
        );

    virtual pplx::task<void> send(
        _In_ const string_t& message
        );

    virtual pplx::task<void> close();

    virtual void set_received_handler(
        _In_ std::function<void(string_t)> handler
        );

    virtual void set_closed_handler(
        _In_ std::function<void(uint16_t closeStatus, string_t closeReason)> handler
        );

private:
    std::shared_ptr<web::websockets::client::websocket_callback_client> m_client;
    std::function<void(string_t)> m_receiveHandler;
    std::function<void(uint16_t closeStatus, string_t closeReason)> m_closeHandler;
};

NAMESPACE_MICROSOFT_MIXER_END
