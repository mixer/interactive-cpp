//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "pch.h"
#include "mixer_web_socket_client.h"
#include <cpprest/uri.h>

using namespace web::websockets::client;

NAMESPACE_MICROSOFT_MIXER_BEGIN
 
mixer_web_socket_client::mixer_web_socket_client()
{
}

pplx::task<void>
mixer_web_socket_client::connect(
    _In_ const web::uri& uri,
    _In_ const string_t& bearerToken,
    _In_ const string_t& interactiveVersion,
    _In_ const string_t& protocolVersion
    )
{
    std::weak_ptr<mixer_web_socket_client> thisWeakPtr = shared_from_this();

    auto task = pplx::create_task([uri, bearerToken, interactiveVersion, protocolVersion, thisWeakPtr]
    {
        std::shared_ptr<mixer_web_socket_client> pThis(thisWeakPtr.lock());
        if (pThis == nullptr)
        {
            throw std::runtime_error("mixer_web_socket_client shutting down");
        }

        websocket_client_config config;
        
        config.headers().add(_T("Authorization"), bearerToken);
        config.headers().add(_T("X-Interactive-Version"), interactiveVersion);
        config.headers().add(_T("X-Protocol-Version"), protocolVersion);

        pThis->m_client = std::make_shared<websocket_callback_client>(config);

        pThis->m_client->set_message_handler([thisWeakPtr](websocket_incoming_message msg)
        {
            try
            {
                std::shared_ptr<mixer_web_socket_client> pThis2(thisWeakPtr.lock());
                if (pThis2 == nullptr)
                {
                    throw std::runtime_error("xbox_web_socket_client shutting down");
                }

                if (msg.message_type() == websocket_message_type::text_message)
                {
                    auto msg_body = msg.extract_string().get();
                    if (pThis2->m_receiveHandler)
                    {
                        pThis2->m_receiveHandler(utility::conversions::to_string_t(msg_body));
                    }
                }
            }
            catch(std::exception e)
            {
                LOG_ERROR("Exception happened in web socket receiving handler.");
            }
            
        });

        pThis->m_client->set_close_handler([thisWeakPtr](websocket_close_status closeStatus, utility::string_t closeReason, const std::error_code errc)
        {
            UNREFERENCED_PARAMETER(errc);

            try
            {
                std::shared_ptr<mixer_web_socket_client> pThis2(thisWeakPtr.lock());
                if (pThis2 == nullptr)
                {
                    LOG_DEBUG("mixer_web_socket_client shutting down");
                    throw std::runtime_error("mixer_web_socket_client shutting down");
                }

                if (pThis2->m_closeHandler)
                {
                    pThis2->m_closeHandler(static_cast<uint16_t>(closeStatus), closeReason);
                };
            }
            catch (std::exception e)
            {
                LOG_ERROR("Exception happened in web socket receiving handler.");
            }
        });

        return pThis->m_client->connect(uri);
    });

    return task;
}

pplx::task<void>
mixer_web_socket_client::send(
    _In_ const string_t& message
    )
{
    if (m_client == nullptr)
        return pplx::task_from_exception<void>(std::runtime_error("web socket is not created yet."));

    websocket_outgoing_message msg;
    msg.set_utf8_message(utility::conversions::to_utf8string(message));
    return m_client->send(msg);
}

pplx::task<void>
mixer_web_socket_client::close()
{
    if (m_client == nullptr)
        return pplx::task_from_exception<void>(std::runtime_error("web socket is not created yet."));

    return m_client->close();
}

void 
mixer_web_socket_client::set_received_handler(
    _In_ std::function<void(string_t)> handler
    )
{
    m_receiveHandler = handler;
}

void 
mixer_web_socket_client::set_closed_handler(
    _In_ std::function<void(uint16_t closeStatus, string_t closeReason)> handler
    )
{
    m_closeHandler = handler;
}

NAMESPACE_MICROSOFT_MIXER_END
