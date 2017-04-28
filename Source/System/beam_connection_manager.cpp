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
#include "Utils.h"
#include "user_context.h"
#include "xbox_system_factory.h"
#include "xsapi/beam.h"
#include "web_socket_connection.h"
#include "web_socket_connection_state.h"
#include "web_socket_client.h"

using namespace pplx;

#define MSA_AUTH_HEADER_NAME    L"x-matt"
#define MSA_AUTH_HEADER_VAL     L"likesserverz"

NAMESPACE_MICROSOFT_XBOX_SERVICES_BEAM_CPP_BEGIN

beam_connection_manager::beam_connection_manager(
    _In_ std::shared_ptr<xbox::services::user_context> userContext,
    _In_ std::shared_ptr<xbox::services::xbox_live_context_settings> xboxLiveContextSettings,
    _In_ std::shared_ptr<xbox::services::xbox_live_app_config> appConfig
) :
    m_userContext(std::move(userContext)),
    m_xboxLiveContextSettings(std::move(xboxLiveContextSettings)),
    m_appConfig(std::move(appConfig)),
    m_sequenceNumber(0),
    m_connectionStateChangeHandlerCounter(0),
    m_connectionState(beam_interactivity_connection_state::disconnected)
{
}

beam_connection_manager::~beam_connection_manager()
{
    if (m_userContext->caller_context_type() == caller_context_type::title)
    {
        deactivate();
    }
}

void
beam_connection_manager::activate()
{
    std::lock_guard<std::recursive_mutex> lock(m_lock);
    int activationCount = 0;
    {
        // need to take in a JWT and the X-CSRF tokens, use those to set up the websocket connection
    }

    return;
}

void beam_connection_manager::completeActivation(std::shared_ptr<http_call_response> response)
{
    // handle the result?
    //if (response->err_code)
    //xbox_live_result<void>(response->err_code(), response->err_message());

    // get auth token out of response, if not error
    // construct request to send to beam auth
        // open websocket before or after that? do I send another http call to get beam auth in order to get back jwt?
    // take jwt from response, if not error
    // (open websocket now?)
    // construct 

    // looking at beam.client.node and the ATG source to see how they handle this little section...

    std::lock_guard<std::mutex> guard(get_xsapi_singleton()->s_beamActivationCounterLock);
    int activationCount = 0;
    {
        if (m_webSocketConnection == nullptr)
        {
            activationCount = ++get_xsapi_singleton()->s_beamActiveSocketCountPerUser[m_userContext->xbox_user_id()];

            LOGS_DEBUG << "websocket count is at " << get_xsapi_singleton()->s_beamActiveSocketCountPerUser[m_userContext->xbox_user_id()] << " for user " << m_userContext->xbox_user_id();
        }

        /*
        if (m_userContext->caller_context_type() == caller_context_type::multiplayer_manager ||
            m_userContext->caller_context_type() == caller_context_type::social_manager)
        {
            ++get_xsapi_singleton()->s_rtaActiveManagersByUser[m_userContext->xbox_user_id()];
            LOGS_DEBUG << "websocket manager count is at " << get_xsapi_singleton()->s_rtaActiveManagersByUser[m_userContext->xbox_user_id()] << " for user " << m_userContext->xbox_user_id();
        }
        */
    }

    if (activationCount > MAXIMUM_WEBSOCKETS_ACTIVATIONS_ALLOWED_PER_USER)
    {
        std::shared_ptr<xbox_live_app_config> appConfig = xbox::services::xbox_live_app_config::get_app_config_singleton();
        if (utils::str_icmp(appConfig->sandbox(), _T("RETAIL")) != 0)
        {
            bool disableAsserts = m_xboxLiveContextSettings->_Is_disable_asserts_for_max_number_of_websockets_activated();
            if (!disableAsserts)
            {
#if UNIT_TEST_SERVICES
                std::lock_guard<std::mutex> guard(get_xsapi_singleton()->s_beamActivationCounterLock);
                --get_xsapi_singleton()->s_beamActiveSocketCountPerUser[m_userContext->xbox_user_id()];
#endif
                std::stringstream msg;
                LOGS_ERROR << "You've currently activated  " << activationCount << " websockets.";
                LOGS_ERROR << "We recommend you don't activate more than " << MAXIMUM_WEBSOCKETS_ACTIVATIONS_ALLOWED_PER_USER << " websockets";
                LOGS_ERROR << "You can temporarily disable the assert by calling";
                LOGS_ERROR << "xboxLiveContext->settings()->disable_asserts_for_maximum_number_of_websockets_activated()";
                LOGS_ERROR << "however the issue must be addressed before certification.";

                XSAPI_ASSERT(false);
            }
        }
    }

    if (m_webSocketConnection == nullptr)
    {
        std::weak_ptr<beam_connection_manager> thisWeakPtr = shared_from_this();

#if TV_API
        m_rtaShutdownToken =
            Windows::ApplicationModel::Core::CoreApplication::Suspending += ref new Windows::Foundation::EventHandler<Windows::ApplicationModel::SuspendingEventArgs^>
            ([thisWeakPtr](_In_opt_ Platform::Object^ sender, _In_ Windows::ApplicationModel::SuspendingEventArgs^ eventArgs)
        {
            std::shared_ptr<beam_connection_manager> pThis(thisWeakPtr.lock());
            if (pThis != nullptr)
            {
                pThis->deactivate();
            }
        });
#elif UWP_API
        m_rtaShutdownToken =
            Windows::ApplicationModel::Core::CoreApplication::Exiting += ref new Windows::Foundation::EventHandler<Platform::Object^>
            ([thisWeakPtr](_In_opt_ Platform::Object^ sender, _In_ Platform::Object^ eventArgs)
        {
            std::shared_ptr<beam_connection_manager> pThis(thisWeakPtr.lock());
            if (pThis != nullptr)
            {
                pThis->deactivate();
            }
        });
#endif
        // TODO: this needs to be the beam services
        stringstream_t endpoint;
        endpoint << utils::create_xboxlive_endpoint(_T("rta"), m_appConfig, _T("wss"));
        endpoint << _T("/connect");

        m_webSocketConnection = std::make_shared<web_socket_connection>(
            m_userContext,
            endpoint.str(),
            _T("rta.xboxlive.com.V2"),
            m_xboxLiveContextSettings
            );

        // We will reset these event handlers on destructor, so it's safe to pass in 'this' here.
        m_webSocketConnection->set_connection_state_change_handler([thisWeakPtr](web_socket_connection_state oldState, web_socket_connection_state newState)
        {
            std::shared_ptr<beam_connection_manager> pThis(thisWeakPtr.lock());
            if (pThis != nullptr)
            {
                pThis->on_socket_connection_state_change(oldState, newState);
            }
        });

        m_webSocketConnection->set_received_handler([thisWeakPtr](string_t message)
        {
            std::shared_ptr<beam_connection_manager> pThis(thisWeakPtr.lock());
            if (pThis != nullptr)
            {
                pThis->on_socket_message_received(message);
            }
        });

        m_webSocketConnection->ensure_connected();
    }
}

void
beam_connection_manager::deactivate()
{
    if (get_xsapi_singleton(false) != nullptr) // skip this if process is shutting down
    {
        std::lock_guard<std::mutex> guard(get_xsapi_singleton()->s_beamActivationCounterLock);
        auto& xuid = m_userContext->xbox_user_id();
        if (m_webSocketConnection != nullptr && get_xsapi_singleton()->s_beamActiveSocketCountPerUser[xuid] > 0)
        {
            --get_xsapi_singleton()->s_beamActiveSocketCountPerUser[xuid];
        }
    }

    try
    {
#if TV_API
        Windows::ApplicationModel::Core::CoreApplication::Suspending -= m_rtaShutdownToken;
#elif UWP_API
        Windows::ApplicationModel::Core::CoreApplication::Exiting -= m_rtaShutdownToken;
#endif
    }
    catch (...)
    {
        LOG_ERROR("Exception on unregistering CoreApplication events!");
    }

    _Close_websocket();

    // _Close_websocket has it's own locking inside, don't include in the next lock
    {
        std::lock_guard<std::recursive_mutex> lock(m_lock);
        m_connectionStateChangeHandler.clear();
        m_connectionStateChangeHandlerCounter = 0;
    }

}

function_context
beam_connection_manager::add_connection_state_change_handler(
    _In_ std::function<void(beam_interactivity_connection_state)> handler
)
{
    std::lock_guard<std::recursive_mutex> lock(m_lock);
    function_context context = -1;
    if (handler != nullptr)
    {
        context = ++m_connectionStateChangeHandlerCounter;
        m_connectionStateChangeHandler[m_connectionStateChangeHandlerCounter] = std::move(handler);

        // Since you could have activated the service already for this context, trigger a state changed event.
        if (m_connectionState != disconnected)
        {
            trigger_connection_state_changed_event(m_connectionState);
        }
    }

    return context;
}

void
beam_connection_manager::remove_connection_state_change_handler(
    _In_ function_context remove
)
{
    std::lock_guard<std::recursive_mutex> lock(m_lock);
    m_connectionStateChangeHandler.erase(remove);
}

function_context
beam_connection_manager::add_connection_error_handler(
    _In_ std::function<void(const beam_interactivity_connection_error_event_args&)> handler
)
{
    std::lock_guard<std::recursive_mutex> lock(m_lock);
    function_context context = -1;
    if (handler != nullptr)
    {
        context = ++m_connectionErrorHandlerCounter;
        m_connectionErrorHandler[m_connectionErrorHandlerCounter] = std::move(handler);
    }

    return context;
}

void
beam_connection_manager::remove_connection_error_handler(
    _In_ function_context remove
)
{
    std::lock_guard<std::recursive_mutex> lock(m_lock);
    m_connectionErrorHandler.erase(remove);
}

/*
void
beam_connection_manager::_Trigger_connection_error(
    beam_interactivity_connection_error_event_args args
)
{
    std::unordered_map<function_context, std::function<void(beam_interactivity_connection_error_event_args)>> connectionErrorHandlerCopy;
    LOGS_DEBUG << "Beam interactivity connection error occurred";
    {
        std::lock_guard<std::recursive_mutex> lock(m_lock);
        connectionErrorHandlerCopy = m_connectionErrorHandler;
    }

    for (auto& subHandler : connectionErrorHandlerCopy)
    {
        XSAPI_ASSERT(subHandler.second != nullptr);
        if (subHandler.second != nullptr)
        {
            try
            {
                subHandler.second(args);
            }
            catch (...)
            {
            }
        }
    }
}
*/

void
beam_connection_manager::on_socket_connection_state_change(
    _In_ web_socket_connection_state oldState,
    _In_ web_socket_connection_state newState
)
{
    if (oldState == newState) return;

    if (web_socket_connection_state::activated == newState) return;

    std::lock_guard<std::recursive_mutex> lock(m_lock);
    {
        if (newState == web_socket_connection_state::disconnected)
        {
            m_connectionState = beam_interactivity_connection_state::disconnected;
            trigger_connection_state_changed_event(beam_interactivity_connection_state::disconnected);
        }

        // On connecting, set subscriptions state accordingly.
        if (newState == web_socket_connection_state::connecting)
        {
            m_connectionState = beam_interactivity_connection_state::connecting;
            trigger_connection_state_changed_event(beam_interactivity_connection_state::connecting);
        }

        // socket reconnected, re-subscribe everything
        if (newState == web_socket_connection_state::connected)
        {
            m_connectionState = beam_interactivity_connection_state::connected;
            trigger_connection_state_changed_event(beam_interactivity_connection_state::connected);
        }
    }
}

void
beam_connection_manager::trigger_connection_state_changed_event(
    _In_ beam_interactivity_connection_state connectionState
)
{
    std::unordered_map<function_context, std::function<void(beam_interactivity_connection_state)>> connectionStateChangeHandlers;
    {
        std::lock_guard<std::recursive_mutex> lock(m_lock);
        connectionStateChangeHandlers = m_connectionStateChangeHandler;
    }

    for (auto& connectionHandler : connectionStateChangeHandlers)
    {
        XSAPI_ASSERT(connectionHandler.second != nullptr);
        if (connectionHandler.second != nullptr)
        {
            try
            {
                connectionHandler.second(connectionState);
            }
            catch (...)
            {
            }
        }
    }
}

void
beam_connection_manager::on_socket_message_received(
    _In_ const string_t& message
)
{
    /*
    auto msgJson = web::json::value::parse(message);
    real_time_activity_message_type messageType = static_cast<real_time_activity_message_type>(msgJson[0].as_integer());

    switch (messageType)
    {
    case real_time_activity_message_type::subscribe:
        complete_subscribe(msgJson);
        break;
    case real_time_activity_message_type::unsubscribe:
        complete_unsubscribe(msgJson);
        break;
    case real_time_activity_message_type::change_event:
        handle_change_event(msgJson);
        break;
    case real_time_activity_message_type::resync:
        trigger_resync_event();
        break;
    default:
        throw std::runtime_error("Unexpected websocket message");
        break;
    }
    */
}

// Let the connection manager handle the socket messages coming in, but allow the beam interactivity service handle processing and eventing out the messages to the title
void
beam_connection_manager::handle_interactivity_message(
    //_In_ web::json::value& message
    // protobuf base class, in short term
)
{
    // response format:
    //[<API_ID>, <SUB_ID>, <DATA>]
    //int subscriptionId = message[1].as_integer();
    //const auto& data = message[2];

    // the callback that's hooked up from the interactivity service
    //if (subscription != nullptr)
    //{
    //  subscription->on_event_received(data);
    //}
}

void
beam_connection_manager::_Close_websocket()
{
    std::shared_ptr<xbox::services::web_socket_connection> socketToClean;
    {
        std::lock_guard<std::recursive_mutex> lock(m_lock);

        socketToClean = m_webSocketConnection;
        m_webSocketConnection = nullptr;
        m_connectionState = beam_interactivity_connection_state::disconnected;
    }

    if (socketToClean != nullptr)
    {
        socketToClean->set_received_handler(nullptr);

        socketToClean->close().then([socketToClean](task<void> t)
        {
            try
            {
                // Hold the reference to the shared point, so it won't deconstruct  
                auto socketConnectionSharedCopy = socketToClean;
                t.get();
                socketConnectionSharedCopy = nullptr;
            }
            catch (...)
            {
            }
        });

        socketToClean->set_connection_state_change_handler(nullptr);
    }
}

std::error_code
beam_connection_manager::convert_beam_error_code_to_xbox_live_error_code(
    _In_ int32_t beamErrorCode
)
{
    switch (beamErrorCode)
    {
    case 0: return xbox_live_error_code::no_error;
    case 1: return xbox_live_error_code::beam_connection_limit_reached;
    case 2: return xbox_live_error_code::beam_access_denied;
    default: return xbox_live_error_code::beam_generic_error;
    }
}


NAMESPACE_MICROSOFT_XBOX_SERVICES_BEAM_CPP_END