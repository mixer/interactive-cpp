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
#include <cpprest/ws_client.h>
#include <thread>
#include "mixer_web_socket_connection.h"

using namespace web::websockets::client;

NAMESPACE_MICROSOFT_MIXER_BEGIN

web_socket_connection::web_socket_connection(
    _In_ string_t bearerToken,
    _In_ string_t interactiveVersion,
    _In_ string_t sharecode,
    _In_ string_t protocolVersion
    ):
    m_bearerToken(std::move(bearerToken)),
    m_interactiveVersion(std::move(interactiveVersion)), 
    m_sharecode(std::move(sharecode)),
    m_protocolVersion(std::move(protocolVersion)),
    m_state(mixer_web_socket_connection_state::disconnected),
    m_client(std::make_shared<mixer_web_socket_client>()),
    m_closeCallbackSet(false),
    m_closeRequested(false),
    m_connectionAttempts(0)
{

}

void
web_socket_connection::ensure_connected()
{
    // As soon as this API gets called, move away from disconnected state
    if (m_connectingTask == pplx::task<void>())
    {
        set_state_helper(mixer_web_socket_connection_state::activated);
        m_connectingTask = pplx::task_from_result();
    }

    std::lock_guard<std::recursive_mutex> lock(m_stateLocker);

    // If it's still connecting or connected return.
    if (!m_connectingTask.is_done() || m_state == mixer_web_socket_connection_state::connected)
    {
        return;
    }

    if (m_connectionAttempts > 6)
    {
        //retry didn't help, notify caller, we're in stable disconnected state
        set_state_helper(mixer_web_socket_connection_state::disconnected);
        return;
    }

    set_state_helper(mixer_web_socket_connection_state::connecting);

    m_closeRequested = false;

    // kick off connection 
    std::weak_ptr<web_socket_connection> thisWeakPtr = shared_from_this();
    m_connectingTask = pplx::create_task([thisWeakPtr]
    {
        // Limit number of re-attempts to something reasonable
        if (auto pThis = thisWeakPtr.lock())
        {
            if (pThis->m_connectionAttempts)
            {
                // Linear growth
                std::chrono::milliseconds retryInterval(150 * pThis->m_connectionAttempts);
                std::this_thread::sleep_for(retryInterval);
            }

            pThis->m_connectionAttempts++;

            if (auto pThisWeak = thisWeakPtr.lock())
            {
                // Trigger connection logic
                pThisWeak->connect_async();
            }
        }
        else
        {
            pplx::task_from_result();
        }
    });
}

pplx::task<void>
web_socket_connection::connect_async()
{
    std::weak_ptr<web_socket_connection> thisWeakPtr = shared_from_this();

    return m_client->connect(
        m_uri,
        m_bearerToken,
        m_interactiveVersion,
        m_sharecode,
        m_protocolVersion
    ).then([thisWeakPtr] (pplx::task<void> connectionResult)
    {
        try
        {
            connectionResult.get();
            LOG_INFO("Websocket connnection established.");

            // This needs to execute after connected 
            // Can't get 'this' shared pointer in constructor, so place socket client calling setting to here.
            if (auto pThis = thisWeakPtr.lock())
            {
                //We've connected, reset retries
                pThis->m_connectionAttempts = 0;

                pThis->set_state_helper(mixer_web_socket_connection_state::connected);

                if (!pThis->m_closeCallbackSet)
                {
                    pThis->m_client->set_closed_handler([thisWeakPtr](uint16_t code, string_t reason)
                    {
                        auto pThis2 = thisWeakPtr.lock();
                        if (pThis2 != nullptr)
                        {
                            pThis2->on_close(code, reason);
                        }
                    });
                    pThis->m_closeCallbackSet = true;
                }
            }
        }
        catch (std::exception& e)
        {
            LOG_INFO("Websocket connection failed");
            return pplx::task_from_result();
        }

        return connectionResult;
    });
}

mixer_web_socket_connection_state
web_socket_connection::state()
{
    std::lock_guard<std::recursive_mutex> lock(m_stateLocker);
    return m_state;
}

pplx::task<void>
web_socket_connection::send(
    _In_ const string_t& message
    )
{
    if (m_client == nullptr)
        return pplx::task_from_exception<void>(std::runtime_error("web socket is not created yet."));

    return m_client->send(message);
}

void 
web_socket_connection::set_uri(_In_ web::uri uri)
{
    m_uri = uri;
}

pplx::task<void>
web_socket_connection::close()
{
    if (m_client == nullptr)
        return pplx::task_from_exception<void>(std::runtime_error("web socket is not created yet."));

    m_closeRequested = true;
    return m_client->close();
}

void 
web_socket_connection::set_received_handler(
    _In_ std::function<void(string_t)> handler
    )
{
    if (m_client != nullptr)
    {
        m_client->set_received_handler(handler);
    }
}

void
web_socket_connection::on_close(uint16_t code, string_t reason)
{
    // websocket_close_status::normal means the socket completed its purpose. Normally it means close
    // was triggered by client. Don't reconnect.
    LOGS_INFO << "web_socket_connection on_close code:" << code << " ,reason:" << reason;
    if (static_cast<websocket_close_status>(code) != websocket_close_status::normal && !m_closeRequested)
    {
        LOG_INFO("web_socket_connection on close, not requested");
        // try to reconnect
        set_state_helper(mixer_web_socket_connection_state::connecting);
        ensure_connected();
    }
    else
    {
        LOG_INFO("web_socket_connection on close, requested");
        set_state_helper(mixer_web_socket_connection_state::disconnected);
    }
}

void
web_socket_connection::set_connection_state_change_handler(
    _In_ std::function<void(mixer_web_socket_connection_state oldState, mixer_web_socket_connection_state newState)> handler
    )
{
    std::lock_guard<std::recursive_mutex> lock(m_stateLocker);
    m_externalStateChangeHandler = handler;
}

void web_socket_connection::set_state_helper(_In_ mixer_web_socket_connection_state newState)
{
    mixer_web_socket_connection_state oldState;
    std::function<void(mixer_web_socket_connection_state oldState, mixer_web_socket_connection_state newState)> externalStateChangeHandlerCopy;
    {
        std::lock_guard<std::recursive_mutex> lock(m_stateLocker);
        // Can only set state to activated if current state is disconnected
        if (newState == mixer_web_socket_connection_state::activated && m_state != mixer_web_socket_connection_state::disconnected)
        {
            return;
        }

        oldState = m_state;
        m_state = newState;
        externalStateChangeHandlerCopy = m_externalStateChangeHandler;
    }

    LOGS_DEBUG << "websocket state change: " << convert_web_socket_connection_state_to_string(oldState) << " -> " << convert_web_socket_connection_state_to_string(newState);

    if (oldState != newState && externalStateChangeHandlerCopy)
    {
        try
        {
            if (externalStateChangeHandlerCopy != nullptr)
            {
                externalStateChangeHandlerCopy(oldState, newState);
            }
        }
        catch (std::exception e)
        {
            LOG_ERROR("web_socket_connection::external state handler had an exception");
        }
    }
}

const string_t
web_socket_connection::convert_web_socket_connection_state_to_string(_In_ mixer_web_socket_connection_state state)
{
    switch (state)
    {
    case mixer_web_socket_connection_state::disconnected: return _T("disconnected");
    case mixer_web_socket_connection_state::activated: return _T("activated");
    case mixer_web_socket_connection_state::connecting: return _T("connecting");
    case mixer_web_socket_connection_state::connected: return _T("connected");
    default: return _T("unknownState");
    }
}

NAMESPACE_MICROSOFT_MIXER_END
