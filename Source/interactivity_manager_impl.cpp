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
#include "interactivity.h"
#include "interactivity_internal.h"
#include "json_helper.h"

using namespace std::chrono;

const string_t requestServerUri = L"https://beam.pro/api/v1/interactive/hosts";
const string_t localServiceUri = L"ws://127.0.0.1:3000/gameClient";
const string_t protocolVersion = L"2.0";

NAMESPACE_MICROSOFT_MIXER_BEGIN

std::shared_ptr<interactive_event> create_interactive_event(string_t errorMessage, std::error_code errorCode, interactive_event_type type, std::shared_ptr<interactive_event_args> args)
{
    std::shared_ptr<interactive_event> event = std::make_shared<interactive_event>(unix_timestamp_in_ms(), errorCode, errorMessage, type, args);
    return event;
}


string_t interactive_rpc_message::to_string()
{
    return m_json.serialize();
}

uint32_t interactive_rpc_message::id() { return m_id; }

std::chrono::milliseconds interactive_rpc_message::timestamp()
{
    return m_timestamp;
}

interactive_rpc_message::interactive_rpc_message(uint32_t id, web::json::value jsonMessage, std::chrono::milliseconds timestamp) :
    m_retries(0),
    m_id(id),
    m_json(std::move(jsonMessage)),
    m_timestamp(std::move(timestamp))
{
}

interactivity_manager_impl::interactivity_manager_impl() :
    m_connectionState(interactivity_connection_state::disconnected),
    m_interactivityState(interactivity_state::not_initialized),
    m_initRetryAttempt(0),
    m_maxInitRetries(7),
    m_initRetryInterval(std::chrono::milliseconds(100)),
    m_processing(FALSE),
    m_currentScene(RPC_SCENE_DEFAULT),
    m_initScenesComplete(false),
    m_initGroupsComplete(false),
    m_initServerTimeComplete(false),
    m_localUserInitialized(false),
    m_serverTimeOffset(std::chrono::milliseconds(0))
{
    logger::create_logger();
    logger::get_logger()->set_log_level(log_level::error);
    logger::get_logger()->add_log_output(std::make_shared<debug_output>());

    m_initializingTask = pplx::task_from_result();
}

interactivity_manager_impl::~interactivity_manager_impl()
{
    close_websocket();
}

void interactivity_manager_impl::close_websocket()
{
    std::shared_ptr<MICROSOFT_MIXER_NAMESPACE::web_socket_connection> socketToClean;
    {
        std::lock_guard<std::recursive_mutex> lock(m_lock);
        socketToClean = m_webSocketConnection;
        m_webSocketConnection = nullptr;
        m_connectionState = interactivity_connection_state::disconnected;
    }

    if (socketToClean != nullptr)
    {
        socketToClean->set_received_handler(nullptr);

        socketToClean->close().then([socketToClean](pplx::task<void> t)
        {
            try
            {
                // Hold the reference to the shared point, so it won't deconstruct
                auto socketConnectionSharedCopy = socketToClean;
                t.get();
                socketConnectionSharedCopy = nullptr;
            }
            catch (std::exception e)
            {
                std::string errorMessage = "exception closing websocket";
                LOGS_INFO << errorMessage;
            }
        });

        socketToClean->set_connection_state_change_handler(nullptr);
    }
}

bool
interactivity_manager_impl::initialize(
    _In_ string_t interactiveVersion,
    _In_ bool goInteractive,
    _In_ string_t sharecode
)
{
    std::lock_guard<std::recursive_mutex> lock(m_lock);
    if (interactivity_state::not_initialized != m_interactivityState)
    {
        return true;
    }

    m_interactiveVersion = interactiveVersion;
    m_sharecode = sharecode;

    // Create long-running initialization task
    std::weak_ptr<interactivity_manager_impl> thisWeakPtr = shared_from_this();

    if (m_initializingTask.is_done())
    {
        m_initializingTask = pplx::create_task([thisWeakPtr, interactiveVersion, goInteractive]()
        {
            std::shared_ptr<interactivity_manager_impl> pThis;
            pThis = thisWeakPtr.lock();
            if (nullptr != pThis)
            {
                pThis->init_worker(interactiveVersion, goInteractive);
            }
        });
    }
    else
    {
        LOGS_DEBUG << "interactivity_manager initialization already in progress";
        return false;
    }

    // Create long-running message processing task
    if (false == m_processing)
    {
        pplx::create_task([thisWeakPtr]()
        {
            std::shared_ptr<interactivity_manager_impl> pThis;
            pThis = thisWeakPtr.lock();
            if (nullptr != pThis)
            {
                pThis->m_processing = true;
                pThis->process_messages_worker();
            }
        });
    }

    return true;
}

#if TV_API | XBOX_UWP
std::shared_ptr<interactive_event>
interactivity_manager_impl::set_local_user(xbox_live_user_t user)
{
    std::lock_guard<std::recursive_mutex> lock(m_lock);

    auto duration = std::chrono::steady_clock::now().time_since_epoch();
    std::chrono::milliseconds currTime = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    std::shared_ptr<interactive_event> event = nullptr;
    
    if (interactivity_state::interactivity_enabled == m_interactivityState || interactivity_state::interactivity_pending == m_interactivityState)
    {
        event = std::make_shared<MICROSOFT_MIXER_NAMESPACE::interactive_event>(currTime, std::error_code(0, std::generic_category()), L"Cannot change local user while in an interactive state", interactive_event_type::error, nullptr);
    }
    else if (interactivity_state::initializing == m_interactivityState)
    {
        event = std::make_shared<MICROSOFT_MIXER_NAMESPACE::interactive_event>(currTime, std::error_code(0, std::generic_category()), L"Cannot change local user while initialization is in progress", interactive_event_type::error, nullptr);
    }
    else
    {
        // Only one user supported at this time, but allow over-writing of the current user
        if (m_localUsers.size() > 0)
        {
            m_localUsers.clear();
        }
        m_localUsers.push_back(user);

        if (m_localUserInitialized)
        {
            m_localUserInitialized = false;
            set_interactivity_state(interactivity_state::not_initialized);
        }
    }

    return event;
}
#else
std::shared_ptr<interactive_event>
interactivity_manager_impl::set_xtoken(_In_ string_t token)
{
	return set_auth_token(token);
}

std::shared_ptr<interactive_event>
interactivity_manager_impl::set_oauth_token(_In_ string_t token)
{
	string_t fullTokenString = L"Bearer ";
	fullTokenString.append(token);
	return set_auth_token(fullTokenString);
}
#endif

void
interactivity_manager_impl::init_worker(
    _In_ string_t interactiveVersion,
    _In_ bool goInteractive
)
{
    bool success = true;
    std::shared_ptr<interactive_event> errorEvent;

    set_interactivity_state(interactivity_state::initializing);

    success = get_auth_token(errorEvent);

    if (success)
    {
        success = get_interactive_host();
        if (!success)
        {
            errorEvent = create_interactive_event(L"interactivity_manager::initialize failed", std::make_error_code(std::errc::operation_canceled), interactive_event_type::error, nullptr);
        }
    }

    if (success)
    {
        initialize_websockets_helper();
        success = initialize_server_state_helper();
    }

    if (success)
    {
        int retryAttempt = 0;
        std::chrono::milliseconds retryInterval(100);
        while (retryAttempt < m_maxInitRetries)
        {
            std::this_thread::sleep_for(retryInterval);

            if (m_initScenesComplete && m_initGroupsComplete && m_initServerTimeComplete)
            {
                set_interactivity_state(interactivity_state::interactivity_disabled);
                break;
            }

            retryInterval = min((3 * retryInterval), std::chrono::milliseconds(1000 * 60));
            retryAttempt++;
        }

        if (retryAttempt >= m_maxInitRetries)
        {
            success = false;
        }
    }

    if (success && goInteractive)
    {
        // If initialization succeeded, start interactivity
        if (m_interactivityState == interactivity_state::interactivity_disabled)
        {
            start_interactive();
        }
    }
    
    if (!success)
    {
        LOGS_DEBUG << L"Failed to initialize interactivity_manager.";
        set_interactivity_state(interactivity_state::not_initialized);

        if (nullptr == errorEvent)
        {
            errorEvent = create_interactive_event(L"interactivity_manager::initialize failed", std::make_error_code(std::errc::operation_canceled), interactive_event_type::error, nullptr);
        }

        {
            std::lock_guard<std::recursive_mutex> lock(m_lock);
            m_eventsForClient.push_back(*errorEvent);
        }
    }

    return;
}


void MICROSOFT_MIXER_NAMESPACE::interactivity_manager_impl::process_messages_worker()
{
    static const int chunkSize = 10;
    while (m_processing)
    {
        {
            std::lock_guard<std::recursive_mutex> lock(m_messagesLock);
            for (int i = 0; i < chunkSize && !m_unhandledFromService.empty(); i++)
            {
                std::shared_ptr<interactive_rpc_message> currentMessage = m_unhandledFromService.front();
                m_unhandledFromService.pop();
                try
                {
                    if (currentMessage->m_json.has_field(RPC_TYPE))
                    {
                        string_t messageType = currentMessage->m_json[RPC_TYPE].as_string();

                        if (0 == messageType.compare(RPC_REPLY))
                        {
                            process_reply(currentMessage->m_json);
                        }
                        else if (0 == messageType.compare(RPC_METHOD))
                        {
                            process_method(currentMessage->m_json);
                        }
                        else
                        {
                            LOGS_INFO << "Unexpected message from service";
                        }
                    }
                }
                catch (std::exception e)
                {
                    LOGS_ERROR << "Failed to parse incoming socket message";
                }
            }
        }

        if (m_webSocketConnection && m_webSocketConnection->state() == mixer_web_socket_connection_state::connected)
        {
            std::lock_guard<std::recursive_mutex> lock(m_messagesLock);
            for (int i = 0; i < chunkSize && !m_pendingSend.empty(); i++)
            {
                std::shared_ptr<interactive_rpc_message> currentMessage = m_pendingSend.front();
                m_pendingSend.pop();
                send_message(currentMessage);
            }
        }

        {
            static const int numMessageRetries = 10;
            static const std::chrono::milliseconds messageRetryInterval = std::chrono::milliseconds(10 * 1000); // 10-second retry interval

            std::lock_guard<std::recursive_mutex> lock(m_messagesLock);
            for (int i = 0; i < chunkSize && i < m_awaitingReply.size(); i++)
            {
                std::shared_ptr<interactive_rpc_message> currentMessage = m_awaitingReply[i];

                auto currentTime = unix_timestamp_in_ms();
                auto messageElapsedTime = currentTime - currentMessage->m_timestamp;
                if (messageElapsedTime > messageRetryInterval)
                {
                    remove_awaiting_reply(currentMessage->id());

                    if (currentMessage->m_retries < numMessageRetries)
                    {
                        currentMessage->m_retries++;
                        currentMessage->m_timestamp = currentTime;
                        queue_message_for_service(currentMessage);
                    }
                    else
                    {
                        LOGS_DEBUG << "Failed to send message " << currentMessage->to_string();
                    }
                }
            }
        }
    }
}


bool
interactivity_manager_impl::get_auth_token(_Out_ std::shared_ptr<interactive_event> &errorEvent)
{
#if TV_API | XBOX_UWP
    if (0 == m_localUsers.size())
    {
        errorEvent = create_interactive_event(L"No local users registered, cannot complete initialization", std::make_error_code(std::errc::operation_canceled), interactive_event_type::error, nullptr);
        return false;
    }
    else
    {
        string_t mixerUri = L"https://mixer.com";
        string_t authRequestHeaders = L"";
        auto platformHttp = ref new Platform::String(L"POST");
        auto platformUrl = ref new Platform::String(mixerUri.c_str());
        auto platformHeaders = ref new Platform::String(authRequestHeaders.c_str());

        pplx::task<Windows::Xbox::System::GetTokenAndSignatureResult^> asyncTask;

        std::weak_ptr<interactivity_manager_impl> thisWeakPtr = shared_from_this();
        asyncTask = pplx::create_task([thisWeakPtr, platformHttp, platformUrl, platformHeaders]()
        {
            std::shared_ptr<interactivity_manager_impl> pThis;
            pThis = thisWeakPtr.lock();
            return pThis->m_localUsers[0]->GetTokenAndSignatureAsync(
                platformHttp,
                platformUrl,
                platformHeaders
            );
        });
        try
        {
            auto result = asyncTask.get();
            string_t token = result->Token->ToString()->Data();

            if (token.empty())
            {
                LOGS_INFO << "Failed to retrieve token from local user";
                errorEvent = create_interactive_event(L"Failed to retrieve token from local user", std::make_error_code(std::errc::operation_canceled), interactive_event_type::error, nullptr);
                return false;
            }

            m_accessToken = token;
            m_localUserInitialized = true;
        }
        catch (Platform::Exception^ ex)
        {
            LOGS_DEBUG << L"Caught exception, HRESULT: " << ex->HResult;
            return false;
        }
    }
#else
    if (m_accessToken.empty())
    {
        LOGS_INFO << "Token empty";
        errorEvent = create_interactive_event(L"Token empty", std::make_error_code(std::errc::operation_canceled), interactive_event_type::error, nullptr);
        return false;
    }
#endif

    return true;
}

#if !TV_API && !XBOX_UWP
std::shared_ptr<interactive_event>
interactivity_manager_impl::set_auth_token(_In_ string_t token)
{
	std::lock_guard<std::recursive_mutex> lock(m_lock);

	auto duration = std::chrono::steady_clock::now().time_since_epoch();
	std::chrono::milliseconds currTime = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
	std::shared_ptr<interactive_event> event = nullptr;

	if (interactivity_state::interactivity_enabled == m_interactivityState || interactivity_state::interactivity_pending == m_interactivityState)
	{
		event = std::make_shared<MICROSOFT_MIXER_NAMESPACE::interactive_event>(currTime, std::error_code(0, std::generic_category()), L"Cannot set token while in an interactive state", interactive_event_type::error, nullptr);
	}
	else if (interactivity_state::initializing == m_interactivityState)
	{
		event = std::make_shared<MICROSOFT_MIXER_NAMESPACE::interactive_event>(currTime, std::error_code(0, std::generic_category()), L"Cannot set token while initialization is in progress", interactive_event_type::error, nullptr);
	}
	else
	{
		m_accessToken = token;
	}

	return event;
}
#endif

bool
interactivity_manager_impl::get_interactive_host()
{
    bool success = true;

    try
    {
        web::http::client::http_client hostsRequestClient(requestServerUri);
        auto result = hostsRequestClient.request(L"GET");
        auto response = result.get();
        if (response.status_code() == web::http::status_codes::OK)
        {
            std::weak_ptr<interactivity_manager_impl> thisWeakPtr = shared_from_this();
            auto jsonTask = response.extract_json();

            try
            {
                auto jsonResult = jsonTask.get();
                auto hosts = jsonResult.as_array();
                auto hostObject = hosts[0].as_object().at(L"address");

                if (auto pThis = thisWeakPtr.lock())
                {
                    pThis->m_interactiveHostUrl = hostObject.as_string();
                }
            }
            catch (std::exception e)
            {
                LOG_INFO("Failed to extract json from request");
                success = false;
            }
        }
    }
    catch (std::exception e)
    {
        LOG_INFO("Failed to retrieve interactive host");
        success = false;
    }

    return success;
}


void
interactivity_manager_impl::initialize_websockets_helper()
{
    if (m_interactivityState == interactivity_state::initializing)
    {
        if (m_webSocketConnection != nullptr)
        {
            close_websocket();
        }

        m_webSocketConnection = std::make_shared<MICROSOFT_MIXER_NAMESPACE::web_socket_connection>(m_accessToken, m_interactiveVersion, m_sharecode, protocolVersion);

        // We will reset these event handlers on destructor, so it's safe to pass in 'this' here.
        std::weak_ptr<interactivity_manager_impl> thisWeakPtr = shared_from_this();
        m_webSocketConnection->set_connection_state_change_handler([thisWeakPtr](mixer_web_socket_connection_state oldState, mixer_web_socket_connection_state newState)
        {
            std::shared_ptr<interactivity_manager_impl> pThis(thisWeakPtr.lock());
            if (pThis != nullptr)
            {
                pThis->on_socket_connection_state_change(oldState, newState);
            }
        });

        m_webSocketConnection->set_received_handler([thisWeakPtr](string_t message)
        {
            std::shared_ptr<interactivity_manager_impl> pThis(thisWeakPtr.lock());
            if (pThis != nullptr)
            {
                pThis->on_socket_message_received(message);
            }
        });

        m_webSocketConnection->set_uri(m_interactiveHostUrl);
        m_webSocketConnection->ensure_connected();
    }

    return;
}

uint32_t
interactivity_manager_impl::get_next_message_id()
{
    static uint32_t nextMessageId = 0;
    return nextMessageId++;
}


bool
interactivity_manager_impl::initialize_server_state_helper()
{
    int retryAttempt = 0;
    std::chrono::milliseconds retryInterval(100);
    while (retryAttempt < m_maxInitRetries)
    {
        LOGS_INFO << "Init task waiting on websocket connection... attempt " << m_initRetryAttempt;

        std::this_thread::sleep_for(retryInterval);
        if (interactivity_connection_state::connected == m_connectionState)
        {
            break;
        }

        ++retryAttempt;
        retryInterval = min((3 * retryInterval), std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::minutes(1)));
    }

    if (retryAttempt < m_maxInitRetries)
    {
        // request service time, for synchronization purposes
        std::shared_ptr<interactive_rpc_message> getTimeMessage = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_GET_TIME, web::json::value(), false);
        queue_message_for_service(getTimeMessage);

        // request groups
        std::shared_ptr<interactive_rpc_message> getGroupsMessage = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_GET_GROUPS, web::json::value(), false);
        queue_message_for_service(getGroupsMessage);

        // request scenes
        std::shared_ptr<interactive_rpc_message> getScenesMessage = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_GET_SCENES, web::json::value(), false);
        queue_message_for_service(getScenesMessage);

        return true;
    }
    else
    {
        LOG_ERROR("Interactivity manager initialization failed - websocket connection timed out");
        return false;
    }
}

const std::chrono::milliseconds
interactivity_manager_impl::get_server_time()
{
    std::chrono::milliseconds serverTime = unix_timestamp_in_ms() - m_serverTimeOffset;

    return serverTime;
}

const interactivity_state
interactivity_manager_impl::interactivity_state()
{
    return m_interactivityState;
}

bool
interactivity_manager_impl::start_interactive()
{
    std::lock_guard<std::recursive_mutex> lock(m_lock);

    if (!m_webSocketConnection || (m_webSocketConnection && m_webSocketConnection->state() != mixer_web_socket_connection_state::connected))
    {
        queue_interactive_event_for_client(L"Websocket connection has not been established, please wait for the initialized state", std::make_error_code(std::errc::not_connected), interactive_event_type::error, nullptr);
        return false;
    }

    if (m_interactivityState == interactivity_state::not_initialized)
    {
        queue_interactive_event_for_client(L"Interactivity not initialized. Must call interactivity_manager::initialize before requesting start_interactive", std::make_error_code(std::errc::not_connected), interactive_event_type::error, nullptr);
        return false;
    }

    if (m_interactivityState == interactivity_state::initializing)
    {
        queue_interactive_event_for_client(L"Interactivity initialization is pending. Please wait for initialize to complete", std::make_error_code(std::errc::not_connected), interactive_event_type::error, nullptr);
        return false;
    }

    set_interactivity_state(interactivity_state::interactivity_pending);

    web::json::value params;
    params[RPC_PARAM_IS_READY] = web::json::value::boolean(true);
    std::shared_ptr<interactive_rpc_message> sendInteractiveReadyMessage = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_READY, params, true);
    queue_message_for_service(sendInteractiveReadyMessage);

    return true;
}

bool
interactivity_manager_impl::stop_interactive()
{
    std::lock_guard<std::recursive_mutex> lock(m_lock);

    if (m_interactivityState == interactivity_state::not_initialized || m_interactivityState == interactivity_state::interactivity_disabled)
    {
        return true;
    }

    set_interactivity_state(interactivity_state::interactivity_pending);

    web::json::value params;
    params[RPC_PARAM_IS_READY] = web::json::value::boolean(false);
    std::shared_ptr<interactive_rpc_message> sendInteractiveReadyMessage = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_READY, params, false);
    queue_message_for_service(sendInteractiveReadyMessage);

    return true;
}

const std::shared_ptr<interactive_participant>&
interactivity_manager_impl::participant(_In_ uint32_t mixerId)
{
    return m_participants[mixerId];
}


std::vector<std::shared_ptr<interactive_participant>>
interactivity_manager_impl::participants()
{
    std::vector<std::shared_ptr<interactive_participant>> participantsCopy;
    participantsCopy.reserve(m_participants.size());

    for (auto iter = m_participants.begin(); iter != m_participants.end(); iter++)
    {
        participantsCopy.push_back(iter->second);
    }

    return participantsCopy;
}


std::vector<std::shared_ptr<interactive_group>>
interactivity_manager_impl::groups()
{
    std::vector<std::shared_ptr<interactive_group>> groupsCopy;
    groupsCopy.reserve(m_groupsInternal.size());

    for (auto iter = m_groupsInternal.begin(); iter != m_groupsInternal.end(); iter++)
    {
        auto groupImpl = iter->second;
        std::shared_ptr<interactive_group> newGroup = std::make_shared<interactive_group>(groupImpl->group_id());
        newGroup->m_impl = groupImpl;
        groupsCopy.push_back(newGroup);
    }

    return groupsCopy;
}

std::shared_ptr<interactive_group>
interactivity_manager_impl::group(_In_ const string_t& group_id)
{
    auto groupImpl = m_groupsInternal[group_id];

	if (nullptr == groupImpl)
	{
		return nullptr;
	}

    std::shared_ptr<interactive_group> groupPtr = std::make_shared<interactive_group>(groupImpl->group_id());
    groupPtr->m_impl = groupImpl;
    return groupPtr;
}

const std::shared_ptr<interactive_scene>&
interactivity_manager_impl::scene(_In_ const string_t& sceneId)
{
    return m_scenes[sceneId];
}

std::vector<std::shared_ptr<interactive_scene>>
interactivity_manager_impl::scenes()
{
    std::vector<std::shared_ptr<interactive_scene>> scenesCopy;
    scenesCopy.reserve(m_scenes.size());

    for (auto iter = m_scenes.begin(); iter != m_scenes.end(); iter++)
    {
        scenesCopy.push_back(iter->second);
    }

    return scenesCopy;
}


std::shared_ptr<interactive_event>
interactivity_manager_impl::try_set_current_scene(_In_ const string_t& scene_id, _In_ const string_t& group_id)
{
    std::shared_ptr<interactive_event> result;

    if (m_interactivityState == interactivity_state::not_initialized || m_interactivityState == interactivity_state::initializing)
    {
        result = create_interactive_event(L"Interactivity not yet initialized.", std::make_error_code(std::errc::not_connected), interactive_event_type::error, nullptr);
        return result;
    }

    if (m_scenes.end() == m_scenes.find(scene_id))
    {
        result = create_interactive_event(L"Unknown scene ID specified.", std::make_error_code(std::errc::no_such_file_or_directory), interactive_event_type::error, nullptr);
        return result;
    }

    auto group = m_groupsInternal[group_id];
    std::vector<std::shared_ptr<interactive_group_impl>> groupToUpdate;
    groupToUpdate.push_back(group);

    send_update_groups(groupToUpdate);

    return result;
}

std::shared_ptr<interactive_button_control> MICROSOFT_MIXER_NAMESPACE::interactivity_manager_impl::button(const string_t & control_id)
{
    return m_buttons[control_id];
}


void 
interactivity_manager_impl::send_update_participants(std::vector<uint32_t> participantIds)
{
    web::json::value params;
    web::json::value participantsToUpdateJson;

    int participantIndex = 0;
    for (auto iter = participantIds.begin(); iter != participantIds.end(); iter++)
    {
        auto currParticipant = m_participants[(*iter)];
        web::json::value currParticipantJson;

        if (currParticipant->m_impl->m_groupIdUpdated)
        {
            currParticipantJson[RPC_GROUP_ID] = web::json::value::string(currParticipant->m_impl->m_groupId);
        }

        if (currParticipant->m_impl->m_disabledUpdated)
        {
            currParticipantJson[RPC_DISABLED] = web::json::value::boolean(currParticipant->m_impl->m_disabled);
        }
        currParticipantJson[RPC_USERNAME] = web::json::value::string(currParticipant->m_impl->m_username);
        currParticipantJson[RPC_SESSION_ID] = web::json::value::string(currParticipant->m_impl->m_sessionId);
        currParticipantJson[RPC_ETAG] = web::json::value::string(currParticipant->m_impl->m_etag);

        participantsToUpdateJson[participantIndex] = currParticipantJson;
        participantIndex++;
    }

    params[RPC_PARAM_PARTICIPANTS] = participantsToUpdateJson;

    std::shared_ptr<interactive_rpc_message> sendCreateGroupMsg = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_UPDATE_PARTICIPANTS, params, false);

    queue_message_for_service(sendCreateGroupMsg);

    return;
}

void
interactivity_manager_impl::send_create_groups(std::vector<std::shared_ptr<interactive_group_impl>> groupsToCreate)
{
    web::json::value params;
    web::json::value groupsToUpdateJson;

    int groupIndex = 0;
    for (auto iter = groupsToCreate.begin(); iter != groupsToCreate.end(); iter++)
    {
        auto currentGroup = (*iter);
        web::json::value currGroupJson;

        currGroupJson[RPC_GROUP_ID] = web::json::value::string(currentGroup->group_id());
        currGroupJson[RPC_ETAG] = web::json::value::string(currentGroup->m_etag);
        currGroupJson[RPC_SCENE_ID] = web::json::value::string(currentGroup->scene_id());

        groupsToUpdateJson[groupIndex] = currGroupJson;
        groupIndex++;
    }

    params[RPC_PARAM_GROUPS] = groupsToUpdateJson;

    std::shared_ptr<interactive_rpc_message> sendCreateGroupMsg = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_CREATE_GROUPS, params, false);

    queue_message_for_service(sendCreateGroupMsg);

    return;
}

void
interactivity_manager_impl::send_update_groups(std::vector<std::shared_ptr<interactive_group_impl>> groupsToUpdate)
{
    web::json::value params;
    web::json::value groupsToUpdateJson;

    int groupIndex = 0;
    for (auto iter = groupsToUpdate.begin(); iter != groupsToUpdate.end(); iter++)
    {
        web::json::value currGroupJson;

        auto currentGroup = (*iter);

        currGroupJson[RPC_GROUP_ID] = web::json::value::string(currentGroup->group_id());
        currGroupJson[RPC_ETAG] = web::json::value::string(currentGroup->etag());
        currGroupJson[RPC_SCENE_ID] = web::json::value::string(currentGroup->scene_id());

        groupsToUpdateJson[groupIndex] = currGroupJson;
        groupIndex++;
    }

    params[RPC_PARAM_GROUPS] = groupsToUpdateJson;

    std::shared_ptr<interactive_rpc_message> sendUpdateGroupsMsg = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_UPDATE_GROUPS, params, false);

    queue_message_for_service(sendUpdateGroupsMsg);

    return;
}

void
interactivity_manager_impl::move_participant_group(uint32_t participantId, string_t oldGroupId, string_t newGroupId)
{
    std::shared_ptr<interactive_participant> participant = m_participants[participantId];

    if (nullptr == participant)
    {
        LOGS_INFO << L"Failed to find specified participant " << participantId;
        return;
    }

    std::shared_ptr<interactive_group_impl> oldGroup = m_groupsInternal[oldGroupId];
    std::shared_ptr<interactive_group_impl> newGroup = m_groupsInternal[newGroupId];

    auto oldGroupParticipants = m_participantsByGroupId[oldGroup->m_groupId];
    oldGroupParticipants.erase(std::remove(oldGroupParticipants.begin(), oldGroupParticipants.end(), participantId), oldGroupParticipants.end());
    m_participantsByGroupId[oldGroup->m_groupId] = oldGroupParticipants;

    auto newGroupParticipants = m_participantsByGroupId[newGroup->m_groupId];
    newGroupParticipants.push_back(participantId);
    m_participantsByGroupId[newGroup->m_groupId] = newGroupParticipants;

    std::vector<uint32_t> participantsToUpdate;
    participantsToUpdate.push_back(participant->mixer_id());
    send_update_participants(participantsToUpdate);
}

// This should only be used when a participant leaves the channel.
void
interactivity_manager_impl::participant_leave_group(uint32_t participantId, string_t groupId)
{
    std::shared_ptr<interactive_participant> participant = m_participants[participantId];

    if (nullptr == participant)
    {
        LOGS_INFO << L"Failed to find specified participant " << participantId;
        return;
    }

    auto oldGroupParticipants = m_participantsByGroupId[groupId];
    oldGroupParticipants.erase(std::remove(oldGroupParticipants.begin(), oldGroupParticipants.end(), participantId), oldGroupParticipants.end());
    m_participantsByGroupId[groupId] = oldGroupParticipants;

    std::shared_ptr<interactive_group_impl> oldGroup = m_groupsInternal[groupId];
    std::vector<std::shared_ptr<interactive_group_impl>> groupsToUpdate;
    groupsToUpdate.push_back(oldGroup);

    send_update_groups(groupsToUpdate);
}

std::vector<std::shared_ptr<interactive_button_control>>
interactivity_manager_impl::buttons()
{
    std::vector<std::shared_ptr<interactive_button_control>> buttonsCopy;
    buttonsCopy.reserve(m_buttons.size());

    for (auto iter = m_buttons.begin(); iter != m_buttons.end(); iter++)
    {
        buttonsCopy.push_back(iter->second);
    }

    return buttonsCopy;
}

std::shared_ptr<interactive_joystick_control> MICROSOFT_MIXER_NAMESPACE::interactivity_manager_impl::joystick(const string_t & control_id)
{
    return m_joysticks[control_id];
}

std::vector<std::shared_ptr<interactive_joystick_control>>
interactivity_manager_impl::joysticks()
{
    std::vector<std::shared_ptr<interactive_joystick_control>> joysticksCopy;
    joysticksCopy.reserve(m_joysticks.size());

    for (auto iter = m_joysticks.begin(); iter != m_joysticks.end(); iter++)
    {
        joysticksCopy.push_back(iter->second);
    }

    return joysticksCopy;
}

void
interactivity_manager_impl::set_disabled(_In_ const string_t& control_id, _In_ bool disabled)
{
    std::shared_ptr<interactive_control> control = m_controls[control_id];
    if (nullptr != control)
    {
        web::json::value params;
        web::json::value controlJson;
        controlJson[RPC_CONTROL_ID] = web::json::value::string(control_id);
        controlJson[RPC_ETAG] = web::json::value::string(control->m_etag);
        controlJson[RPC_DISABLED] = web::json::value::boolean(disabled);

        params[RPC_SCENE_ID] = web::json::value::string(control->m_parentScene);
        params[RPC_SCENE_CONTROLS][0] = controlJson;

        std::shared_ptr<interactive_rpc_message> updateControlMessage = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_UPDATE_CONTROLS, params, false);
        queue_message_for_service(updateControlMessage);
    }
}

void
interactivity_manager_impl::set_progress(_In_ const string_t& control_id, _In_ float progress)
{
    std::shared_ptr<interactive_control> control = m_controls[control_id];
    if (nullptr != control)
    {
        web::json::value params;
        web::json::value controlJson;
        controlJson[RPC_CONTROL_ID] = web::json::value::string(control_id);
        controlJson[RPC_ETAG] = web::json::value::string(control->m_etag);
        controlJson[RPC_CONTROL_BUTTON_PROGRESS] = web::json::value::number(progress);

        params[RPC_SCENE_ID] = web::json::value::string(control->m_parentScene);
        params[RPC_SCENE_CONTROLS][0] = controlJson;

        std::shared_ptr<interactive_rpc_message> updateControlMessage = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_UPDATE_CONTROLS, params, false);
        queue_message_for_service(updateControlMessage);
    }
}

void
interactivity_manager_impl::trigger_cooldown(_In_ const string_t& control_id, _In_ const std::chrono::milliseconds& cooldown)
{
    std::shared_ptr<interactive_control> control = m_controls[control_id];
    if (nullptr != control)
    {
        std::shared_ptr<interactive_button_control> buttonControl = std::dynamic_pointer_cast<interactive_button_control>(control);

        if (nullptr != buttonControl)
        {
            std::chrono::milliseconds cooldownDeadline = unix_timestamp_in_ms() - m_serverTimeOffset + cooldown;
            buttonControl->m_cooldownDeadline = cooldownDeadline;

            web::json::value params;
            web::json::value controlJson;
            controlJson[RPC_CONTROL_ID] = web::json::value::string(control_id);
            controlJson[RPC_ETAG] = web::json::value::string(buttonControl->m_etag);
            controlJson[RPC_CONTROL_BUTTON_COOLDOWN] = web::json::value::number(cooldownDeadline.count());

            params[RPC_SCENE_ID] = web::json::value::string(buttonControl->m_parentScene);
            params[RPC_SCENE_CONTROLS][0] = controlJson;

            std::shared_ptr<interactive_rpc_message> updateControlMessage = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_UPDATE_CONTROLS, params, false);
            queue_message_for_service(updateControlMessage);
        }
    }
}

void MICROSOFT_MIXER_NAMESPACE::interactivity_manager_impl::send_message(const string_t & message)
{
    // The goal of this API is to be fast, with minimal memory and CPU overhead. This is why
    // it is a pass-through to the websocket rather than packaging the message into a data structure
    // and putting it on a queue to be sent later.
    m_webSocketConnection->send(message)
        .then([](pplx::task<void> t)
    {
        try
        {
            t.get();
        }
        catch (std::exception e)
        {
            // Throws this exception on failure to send, our retry logic once the websocket comes back online will resend
            LOGS_ERROR << "Failed to send on websocket.";
        }
    });
}

void MICROSOFT_MIXER_NAMESPACE::interactivity_manager_impl::capture_transaction(const string_t & transaction_id)
{
    web::json::value params;
    params[RPC_TRANSACTION_ID] = web::json::value::string(transaction_id);

    std::shared_ptr<interactive_rpc_message> captureTransactionMessage = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_CAPTURE, params, false);
    queue_message_for_service(captureTransactionMessage);
}

std::vector<interactive_event>
interactivity_manager_impl::do_work()
{
    std::vector<interactive_event> eventsForClient;
    std::lock_guard<std::recursive_mutex> lock(m_lock);

    if (m_eventsForClient.size() > 0)
    {
        if (m_webSocketConnection != nullptr && m_webSocketConnection->state() != mixer_web_socket_connection_state::disconnected)
        {
            m_webSocketConnection->ensure_connected();
        }

        eventsForClient = std::move(m_eventsForClient);
        m_eventsForClient.clear();

        for (auto iter = m_controls.begin(); iter != m_controls.end(); iter++)
        {
            iter->second->clear_state();
			// Note: We do not clear the joystick state. The reason is because (1) we will get a 0, 0 joystick
			// event if the joystick returns to rest. The other reason is if the user is holding the joytick
			// at a certain value, let's say (1, 1), then we will not continue to recieve input events, however
			// we need to report (1, 1) as the joystick value. The clear_state() of the joystick is a no-op.
        }
    }

    return eventsForClient;
}

void
interactivity_manager_impl::set_interactivity_state(MICROSOFT_MIXER_NAMESPACE::interactivity_state newState)
{
    if (m_interactivityState == newState)
    {
        return;
    }

    bool validStateChange = true;
    MICROSOFT_MIXER_NAMESPACE::interactivity_state oldState = m_interactivityState;
    std::shared_ptr<interactivity_state_change_event_args> eventArgs;

    switch (newState)
    {
    case MICROSOFT_MIXER_NAMESPACE::interactivity_state::not_initialized:
        // can revert to not_initialized from any state
        break;
    case MICROSOFT_MIXER_NAMESPACE::interactivity_state::initializing:
        if (m_interactivityState != MICROSOFT_MIXER_NAMESPACE::interactivity_state::not_initialized)
        {
            validStateChange = false;
        }
        break;
    case MICROSOFT_MIXER_NAMESPACE::interactivity_state::interactivity_disabled:
        if (m_interactivityState == MICROSOFT_MIXER_NAMESPACE::interactivity_state::not_initialized || m_interactivityState == MICROSOFT_MIXER_NAMESPACE::interactivity_state::interactivity_enabled)
        {
            validStateChange = false;
        }
        break;
    case MICROSOFT_MIXER_NAMESPACE::interactivity_state::interactivity_pending:
        if (m_interactivityState != MICROSOFT_MIXER_NAMESPACE::interactivity_state::interactivity_disabled && m_interactivityState != MICROSOFT_MIXER_NAMESPACE::interactivity_state::interactivity_enabled)
        {
            validStateChange = false;
        }
        break;
    case MICROSOFT_MIXER_NAMESPACE::interactivity_state::interactivity_enabled:
        if (m_interactivityState != MICROSOFT_MIXER_NAMESPACE::interactivity_state::interactivity_pending)
        {
            validStateChange = false;
        }
        break;
    default:
        validStateChange = false;
        break;
    }

    if (validStateChange)
    {
        m_interactivityState = newState;
     
        std::shared_ptr<interactivity_state_change_event_args> args = std::make_shared<interactivity_state_change_event_args>(m_interactivityState);
        queue_interactive_event_for_client(L"", std::error_code(0, std::generic_category()), interactive_event_type::interactivity_state_changed, args);

        LOGS_DEBUG << L"Interactivity state change from " << oldState << L" to " << newState;
    }
    else
    {
        LOGS_ERROR << L"Unexpected interactivity state change from " << oldState << L" to " << newState;
    }
}

void
interactivity_manager_impl::on_socket_connection_state_change(
    _In_ mixer_web_socket_connection_state oldState,
    _In_ mixer_web_socket_connection_state newState
)
{
    if (oldState == newState)
    {
        return;
    }

    if (mixer_web_socket_connection_state::activated == newState)
    {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(m_lock);
    {
        if (newState == mixer_web_socket_connection_state::disconnected)
        {
            LOGS_INFO << "Websocket disconnected";
            m_connectionState = interactivity_connection_state::disconnected;
        }

        // On connecting, set subscriptions state accordingly.
        if (newState == mixer_web_socket_connection_state::connecting)
        {
            LOGS_INFO << "Websocket connecting";
            m_connectionState = interactivity_connection_state::connecting;
        }

        // Socket reconnected, re-subscribe everything
        if (newState == mixer_web_socket_connection_state::connected)
        {
            LOGS_INFO << "Websocket connected";
            m_connectionState = interactivity_connection_state::connected;
        }
    }

    return;
}


void
interactivity_manager_impl::on_socket_message_received(
    _In_ const string_t& message
)
{
    // We always return all messages from the service to to the developer, even the ones the SDK will process them.
    std::shared_ptr<interactive_message_event_args> args = std::shared_ptr<interactive_message_event_args>(new interactive_message_event_args(message));
    queue_interactive_event_for_client(L"", std::error_code(0, std::generic_category()), interactive_event_type::unknown, args);

    std::shared_ptr<interactive_rpc_message> rpcMessage = std::shared_ptr<interactive_rpc_message>(new interactive_rpc_message(get_next_message_id(), web::json::value::parse(message), unix_timestamp_in_ms()));
    m_unhandledFromService.push(rpcMessage);
}

void interactivity_manager_impl::process_reply(const web::json::value& jsonReply)
{
    LOGS_INFO << "Received a reply from the service";

    try
    {
        if (jsonReply.has_field(L"id"))
        {
            std::lock_guard<std::recursive_mutex> lock(m_messagesLock);
            std::shared_ptr<interactive_rpc_message> message = remove_awaiting_reply(jsonReply.at(RPC_ID).as_integer());

			if (message == nullptr)
			{
				LOGS_ERROR << L"Message does not exist: " << jsonReply.at(RPC_ID).as_string();
				return;
			}

            if (jsonReply.has_field(RPC_ERROR))
            {
                LOGS_ERROR << L"Full message for error reply: " << message->m_json.serialize();
                process_reply_error(jsonReply);
                return;
            }
            
            if (message->m_json.has_field(RPC_METHOD))
            {
                string_t methodName = message->m_json.at(RPC_METHOD).as_string();
                if (0 == methodName.compare(RPC_METHOD_GET_TIME))
                {
                    process_get_time_reply(message, jsonReply);
                }
                else if (0 == methodName.compare(RPC_METHOD_GET_SCENES))
                {
                    process_get_scenes_reply(jsonReply);
                }
                else if (0 == methodName.compare(RPC_METHOD_GET_GROUPS))
                {
                    process_get_groups_reply(jsonReply);
                }
                else if (0 == methodName.compare(RPC_METHOD_CREATE_GROUPS))
                {
                    process_create_groups_reply(jsonReply);
                }
                else if (0 == methodName.compare(RPC_METHOD_UPDATE_GROUPS))
                {
                    process_update_groups_reply(jsonReply);
                }
                else if (0 == methodName.compare(RPC_METHOD_UPDATE_CONTROLS))
                {
                    process_update_controls_reply(jsonReply);
                }
                else if (0 == methodName.compare(RPC_METHOD_UPDATE_PARTICIPANTS))
                {
                    process_update_participants_reply(jsonReply);
                }
                else
                {
                    LOGS_INFO << L"Unhandled reply: " << jsonReply.serialize().c_str() << "to message " << message->to_string();
                }
            }
        }
        else
        {
            LOGS_INFO << "Unexpected json reply, no id";
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to parse service reply";
    }
}

void interactivity_manager_impl::process_reply_error(const web::json::value& jsonReply)
{
    try
    {
        if (jsonReply.has_field(RPC_ID) &&
            jsonReply.has_field(RPC_ERROR))
        {
            web::json::value jsonError = jsonReply.at(RPC_ERROR);

            if (jsonError.has_field(RPC_ERROR_CODE) &&
                jsonError.has_field(RPC_ERROR_MESSAGE) &&
                jsonError.has_field(RPC_ERROR_PATH))
            {
                uint32_t id = jsonReply.at(RPC_ID).as_number().to_uint32();
                string_t errorCode = jsonError[RPC_ERROR_CODE].serialize();
                string_t errorMessage = jsonError[RPC_ERROR_MESSAGE].serialize();
                string_t errorPath = jsonError[RPC_ERROR_PATH].serialize();

                LOGS_ERROR << L"Error id: " << id << ", code: " << errorCode << ", message: " << errorMessage << ", path: " << errorPath;
                LOGS_ERROR << L"Full error payload: " << jsonReply.serialize();
            }

            return;
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to parse service reply error";
    }
}

void interactivity_manager_impl::process_get_time_reply(std::shared_ptr<interactive_rpc_message> message, const web::json::value& jsonReply)
{
    try
    {
        if (jsonReply.has_field(RPC_RESULT) && jsonReply.at(RPC_RESULT).has_field(RPC_TIME))
        {
            std::chrono::milliseconds msServerTime = milliseconds(jsonReply.at(RPC_RESULT).at(RPC_TIME).as_number().to_uint64());

            auto currTime = unix_timestamp_in_ms();
            m_latency = (currTime - message->timestamp()) / 2;
            m_serverTimeOffset = currTime - msServerTime - m_latency;

            if (!m_initServerTimeComplete)
            {
                m_initServerTimeComplete = true;
            }
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to parse system time from getTime reply";
    }
}

void interactivity_manager_impl::process_get_groups_reply(const web::json::value& jsonReply)
{
    try
    {
        if (jsonReply.has_field(RPC_RESULT) && jsonReply.at(RPC_RESULT).has_field(RPC_PARAM_GROUPS))
        {
            web::json::array groups = jsonReply.at(RPC_RESULT).at(RPC_PARAM_GROUPS).as_array();

            for (auto iter = groups.begin(); iter != groups.end(); ++iter)
            {
                std::shared_ptr<interactive_group_impl> newGroup = std::shared_ptr<interactive_group_impl>(new interactive_group_impl(*iter));
                m_groupsInternal[newGroup->group_id()] = newGroup;
            }

            // Should only ever be set once, after initialization
            m_initGroupsComplete = true;
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to create groups in process_get_groups_reply";
    }
}

void interactivity_manager_impl::process_create_groups_reply(const web::json::value& jsonReply)
{
    try
    {
        // for now, these are handled the same way
        process_update_groups_reply(jsonReply);
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to update groups in process_create_groups_reply";
    }
}

void interactivity_manager_impl::process_update_groups_reply(const web::json::value& jsonReply)
{
    try
    {
        if (jsonReply.has_field(RPC_RESULT) && jsonReply.at(RPC_RESULT).has_field(RPC_PARAM_GROUPS))
        {
            web::json::array groups = jsonReply.at(RPC_RESULT).at(RPC_PARAM_GROUPS).as_array();

            for (auto iter = groups.begin(); iter != groups.end(); ++iter)
            {
                if ((*iter).has_field(RPC_GROUP_ID))
                {
                    std::shared_ptr<interactive_group_impl> groupToUpdate = m_groupsInternal[(*iter)[RPC_GROUP_ID].as_string()];
                    groupToUpdate->update((*iter));
                }
            }
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to update groups in process_update_groups_reply";
    }
}

void interactivity_manager_impl::process_on_group_create(const web::json::value& onGroupCreateMethod)
{
    try
    {
        if (onGroupCreateMethod.has_field(RPC_PARAMS) && onGroupCreateMethod.at(RPC_PARAMS).has_field(RPC_PARAM_GROUPS))
        {
            web::json::array groups = onGroupCreateMethod.at(RPC_PARAMS).at(RPC_PARAM_GROUPS).as_array();

            for (auto iter = groups.begin(); iter != groups.end(); ++iter)
            {
                if ((*iter).has_field(RPC_GROUP_ID))
                {
                    std::shared_ptr<interactive_group_impl> groupToUpdate = m_groupsInternal[(*iter)[RPC_GROUP_ID].as_string()];
                    if (nullptr != groupToUpdate)
                    {
                        groupToUpdate->update((*iter), false);
                    }
                    else
                    {
                        std::shared_ptr<interactive_group_impl> newGroup = std::shared_ptr<interactive_group_impl>(new interactive_group_impl(*iter));
                        m_groupsInternal[newGroup->scene_id()] = newGroup;
                    }
                }
            }
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to create groups in process_on_group_create";
    }
}

void interactivity_manager_impl::process_on_group_update(const web::json::value& onGroupUpdateMethod)
{
    try
    {
        if (onGroupUpdateMethod.has_field(RPC_PARAMS) && onGroupUpdateMethod.at(RPC_PARAMS).has_field(RPC_PARAM_GROUPS))
        {
            web::json::array groups = onGroupUpdateMethod.at(RPC_PARAMS).at(RPC_PARAM_GROUPS).as_array();

            for (auto iter = groups.begin(); iter != groups.end(); ++iter)
            {
                if ((*iter).has_field(RPC_GROUP_ID))
                {
                    std::shared_ptr<interactive_group_impl> groupToUpdate = m_groupsInternal[(*iter)[RPC_GROUP_ID].as_string()];
                    groupToUpdate->update((*iter));
                }
            }
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to update groups in process_on_group_update";
    }
}

void interactivity_manager_impl::process_update_controls_reply(const web::json::value& jsonReply)
{
    try
    {
        if (jsonReply.has_field(RPC_RESULT) && jsonReply.at(RPC_RESULT).has_field(RPC_SCENE_CONTROLS))
        {
            process_update_controls(jsonReply.at(RPC_RESULT).at(RPC_SCENE_CONTROLS).as_array());
        }
        else
        {
            LOGS_ERROR << "Unexpected json format in process_update_controls_reply";
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to update controls in process_update_controls_reply";
    }
}

void interactivity_manager_impl::process_on_control_update(const web::json::value& onControlUpdateMethod)
{
    try
    {
        if (onControlUpdateMethod.has_field(RPC_PARAMS) && onControlUpdateMethod.at(RPC_PARAMS).has_field(RPC_SCENE_CONTROLS))
        {
            process_update_controls(onControlUpdateMethod.at(RPC_PARAMS).at(RPC_SCENE_CONTROLS).as_array());
        }
        else
        {
            LOGS_ERROR << "Unexpected json format in process_on_control_update";
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to update controls in process_on_control_update";
    }
}

// Update controls based on replies from the service and explicit onUpdateControl RPC method call
void interactivity_manager_impl::process_update_controls(web::json::array controlsToUpdate)
{
    try
    {
        if (controlsToUpdate.size() < 1)
        {
            LOGS_DEBUG << L"Unexpected number of controls.";
        }

        for (auto iter = controlsToUpdate.begin(); iter != controlsToUpdate.end(); ++iter)
        {
            if ((*iter).has_field(RPC_CONTROL_ID))
            {
                std::shared_ptr<interactive_control> controlToUpdate = m_controls[(*iter)[RPC_CONTROL_ID].as_string()];

                if (nullptr == controlToUpdate)
                {
                    LOGS_DEBUG << L"Unexpected update received for control " << (*iter)[RPC_CONTROL_ID].as_string();
                }

                if (interactive_control_type::button == controlToUpdate->control_type())
                {
                    std::shared_ptr<interactive_button_control> buttonToUpdate = std::dynamic_pointer_cast<interactive_button_control>(controlToUpdate);
                    buttonToUpdate->update((*iter), false);
                }
                else if (interactive_control_type::joystick == controlToUpdate->control_type())
                {
                    std::shared_ptr<interactive_joystick_control> joystickToUpdate = std::dynamic_pointer_cast<interactive_joystick_control>(controlToUpdate);
                    joystickToUpdate->update((*iter), false);
                }
            }
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to update groups in process_on_group_update";
    }
}


void interactivity_manager_impl::process_get_scenes_reply(const web::json::value& jsonReply)
{
    try
    {
        if (jsonReply.has_field(RPC_RESULT) && jsonReply.at(RPC_RESULT).has_field(RPC_PARAM_SCENES))
        {
            web::json::array scenes = jsonReply.at(RPC_RESULT).at(RPC_PARAM_SCENES).as_array();

            for (auto iter = scenes.begin(); iter != scenes.end(); ++iter)
            {
                std::shared_ptr<interactive_scene> newScene = std::shared_ptr<interactive_scene>(new interactive_scene());
                newScene->m_impl = std::shared_ptr<interactive_scene_impl>(new interactive_scene_impl());
                newScene->m_impl->init_from_json(*iter);
                m_scenes[newScene->scene_id()] = newScene;
            }

            // Should only ever be set once, after initialization
            m_initScenesComplete = true;
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to create scenes in process_get_scenes_reply";
    }
}

void interactivity_manager_impl::process_update_participants_reply(const web::json::value& jsonReply)
{
    try
    {
        if (jsonReply.has_field(RPC_RESULT) && jsonReply.at(RPC_RESULT).has_field(RPC_PARAM_PARTICIPANTS))
        {
            web::json::array participants = jsonReply.at(RPC_RESULT).at(RPC_PARAM_PARTICIPANTS).as_array();

            for (auto iter = participants.begin(); iter != participants.end(); ++iter)
            {
                if ((*iter).has_field(RPC_USER_ID))
                {
                    std::shared_ptr<interactive_participant> participantToUpdate = m_participants[(*iter)[RPC_USER_ID].as_number().to_int32()];
                    if (nullptr != participantToUpdate)
                    {
                        participantToUpdate->m_impl->update((*iter), false);
                    }
                }
            }
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to update participants in process_update_participants_reply";
    }
}

void interactivity_manager_impl::process_method(const web::json::value& methodJson)
{
    LOGS_DEBUG << "Received an RPC call from the service";
    try
    {
        if (methodJson.has_field(RPC_METHOD))
        {
            string_t rpcMethodName = methodJson.at(RPC_METHOD).as_string();
            if (0 == rpcMethodName.compare(RPC_METHOD_ON_INPUT))
            {
                process_input(methodJson);
            }
            else if (0 == rpcMethodName.compare(RPC_METHOD_PARTICIPANTS_JOINED))
            {
                process_participant_joined(methodJson);
            }
            else if (0 == rpcMethodName.compare(RPC_METHOD_PARTICIPANTS_LEFT))
            {
                process_participant_left(methodJson);
            }
            else if (0 == rpcMethodName.compare(RPC_PARTICIPANTS_ON_UPDATE))
            {
                process_on_participant_update(methodJson);
            }
            else if (0 == rpcMethodName.compare(RPC_METHOD_ON_READY_CHANGED))
            {
                process_on_ready_changed(methodJson);
            }
            else if (0 == rpcMethodName.compare(RPC_METHOD_ON_GROUP_CREATE))
            {
                process_on_group_create(methodJson);
            }
            else if (0 == rpcMethodName.compare(RPC_METHOD_ON_GROUP_UPDATE))
            {
                process_on_group_update(methodJson);
            }
            else if (0 == rpcMethodName.compare(RPC_METHOD_ON_CONTROL_UPDATE))
            {
                process_on_control_update(methodJson);
            }
            else
            {
                LOGS_INFO << "Unexpected or unsupported RPC call: " << methodJson.at(RPC_METHOD).as_string();
            }
        }
        else
        {
            LOGS_INFO << "Method RPC call had no method name";
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to process method RPC call";
    }
}


void interactivity_manager_impl::process_participant_joined(const web::json::value& participantJoinedJson)
{
    LOGS_INFO << "Received a participant joined event";
    try
    {
        if (participantJoinedJson.has_field(RPC_PARAMS) && participantJoinedJson.at(RPC_PARAMS).has_field(RPC_PARAM_PARTICIPANTS))
        {
            web::json::array currParticipants = participantJoinedJson.at(RPC_PARAMS).at(RPC_PARAM_PARTICIPANTS).as_array();
            for (auto iter = currParticipants.begin(); iter != currParticipants.end(); ++iter)
            {
                std::shared_ptr<interactive_participant> currParticipant = std::shared_ptr<interactive_participant>(new interactive_participant());
                currParticipant->m_impl = std::shared_ptr<interactive_participant_impl>(new interactive_participant_impl());

                bool success = currParticipant->m_impl->init_from_json(*iter);

                if (success)
                {
                    currParticipant->m_impl->m_state = interactive_participant_state::joined;
                    m_participants[currParticipant->mixer_id()] = currParticipant;
                    m_participantTomixerId[currParticipant->m_impl->m_sessionId] = currParticipant->mixer_id();

                    m_participantsByGroupId[currParticipant->m_impl->m_groupId].push_back(currParticipant->mixer_id());

                    LOGS_DEBUG << "Participant " << currParticipant->mixer_id() << " joined";

                    std::shared_ptr<interactive_participant_state_change_event_args> args = std::shared_ptr<interactive_participant_state_change_event_args>(new interactive_participant_state_change_event_args(currParticipant, interactive_participant_state::joined));

                    queue_interactive_event_for_client(L"", std::error_code(0, std::generic_category()), interactive_event_type::participant_state_changed, args);
                }
                else
                {
                    queue_interactive_event_for_client(L"Failed to initialize participant, onParticipantJoin", std::make_error_code(std::errc::not_supported), interactive_event_type::error, nullptr);
                }
            }
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to process participant joined RPC call";
    }
}


void interactivity_manager_impl::process_participant_left(const web::json::value& participantLeftJson)
{
    LOGS_INFO << "Received a participant left event";
    try
    {
        if (participantLeftJson.has_field(RPC_PARAMS) && participantLeftJson.at(RPC_PARAMS).has_field(RPC_PARAM_PARTICIPANTS))
        {
            web::json::array currParticipants = participantLeftJson.at(RPC_PARAMS).at(RPC_PARAM_PARTICIPANTS).as_array();
            for (auto iter = currParticipants.begin(); iter != currParticipants.end(); ++iter)
            {
                if (iter->has_field(RPC_USER_ID))
                {
                    uint32_t mixerId = iter->at(RPC_USER_ID).as_number().to_uint32();
                    std::shared_ptr<interactive_participant> currParticipant = m_participants[mixerId];
                    currParticipant->m_impl->m_state = interactive_participant_state::left;

                    // update the states of the group - remove the participant entirely
                    participant_leave_group(currParticipant->mixer_id(), currParticipant->m_impl->m_groupId);

                    LOGS_DEBUG << "Participant " << mixerId << " left";

                    std::shared_ptr<interactive_participant_state_change_event_args> args = std::shared_ptr<interactive_participant_state_change_event_args>(new interactive_participant_state_change_event_args(currParticipant, interactive_participant_state::left));

                    queue_interactive_event_for_client(L"", std::error_code(0, std::generic_category()), interactive_event_type::participant_state_changed, args);
                }
                else
                {
                    LOGS_DEBUG << "Failed to parse id from participants left";
                }
            }
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to process participant left RPC call";
    }
}

void interactivity_manager_impl::process_on_participant_update(const web::json::value& participantChangeJson)
{
    LOGS_INFO << "Received a participant update";
    try
    {
        if (participantChangeJson.has_field(RPC_PARAMS) && participantChangeJson.at(RPC_PARAMS).has_field(RPC_PARAM_PARTICIPANTS))
        {
            web::json::array currParticipants = participantChangeJson.at(RPC_PARAMS).at(RPC_PARAM_PARTICIPANTS).as_array();
            for (auto iter = currParticipants.begin(); iter != currParticipants.end(); ++iter)
            {
                uint32_t participantmixerId = m_participantTomixerId[(*iter)[RPC_SESSION_ID].as_string()];
                std::shared_ptr<interactive_participant> participantToUpdate = m_participants[participantmixerId];
                participantToUpdate->m_impl->update(*iter, false);
            }
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to process participant update RPC call";
    }
}

void interactivity_manager_impl::process_on_ready_changed(const web::json::value& onReadyMethod)
{
    LOGS_INFO << "Received an interactivity state change from service";
    try
    {
        if (onReadyMethod.has_field(RPC_PARAMS) && onReadyMethod.at(RPC_PARAMS).has_field(RPC_PARAM_IS_READY))
        {
            std::lock_guard<std::recursive_mutex> lock(m_lock);

            if (onReadyMethod.at(RPC_PARAMS).at(RPC_PARAM_IS_READY).as_bool() == true)
            {
                set_interactivity_state(interactivity_state::interactivity_enabled);
            }
            else
            {
                set_interactivity_state(interactivity_state::interactivity_disabled);
            }
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to process participant state change RPC call";
    }
}


void interactivity_manager_impl::process_input(const web::json::value& inputMethod)
{
    LOGS_INFO << "Received an input message";

    if (interactivity_state::interactivity_enabled != m_interactivityState)
    {
        LOGS_DEBUG << "Received interactive input from participants while client not in an interactive state.";
        return;
    }

    try
    {
        if (inputMethod.has_field(RPC_PARAMS) && inputMethod.at(RPC_PARAMS).has_field(RPC_PARAM_INPUT))
        {
            auto paramInputJson = inputMethod.at(RPC_PARAMS).at(RPC_PARAM_INPUT);
            if (paramInputJson.has_field(RPC_CONTROL_KIND))
            {
                string_t kind = paramInputJson[RPC_CONTROL_KIND].as_string();

                if (0 == kind.compare(RPC_CONTROL_BUTTON))
                {
                    process_button_input(inputMethod);
                }
                else if (0 == kind.compare(RPC_CONTROL_JOYSTICK))
                {
                    process_joystick_input(inputMethod);
                }
                else
                {
                    LOGS_INFO << "Received unknown input";
                }
            }
            else if (paramInputJson.has_field(RPC_CONTROL_EVENT)) // TODO: service update to add "kind" to control input events
            {
                string_t controlEvent = paramInputJson[RPC_CONTROL_EVENT].as_string();

                if (0 == controlEvent.compare(RPC_INPUT_EVENT_BUTTON_DOWN) || 0 == controlEvent.compare(RPC_INPUT_EVENT_BUTTON_UP))
                {
                    process_button_input(inputMethod);
                }
                else if (0 == controlEvent.compare(RPC_INPUT_EVENT_MOVE))
                {
                    process_joystick_input(inputMethod);
                }
                else
                {
                    LOGS_INFO << "Received unknown input";
                }
            }
        }
        else
        {
            LOGS_INFO << "Poorly formatted input message: " << inputMethod.serialize();
        }
    }
    catch (std::exception e)
    {
        LOGS_INFO << "Failed to process input RPC call";
    }
}


void interactivity_manager_impl::process_button_input(const web::json::value& inputJson)
{
    LOGS_INFO << "Received a button input message";
    try
    {
        if (inputJson.has_field(RPC_PARAMS) &&
            inputJson.at(RPC_PARAMS).has_field(RPC_PARAM_INPUT) &&
            inputJson.at(RPC_PARAMS).at(RPC_PARAM_INPUT).has_field(RPC_CONTROL_ID))
        {
            auto buttonInputJson = inputJson.at(RPC_PARAMS).at(RPC_PARAM_INPUT);
            if (buttonInputJson.has_field(RPC_PARAM_INPUT_EVENT))
            {
                std::lock_guard<std::recursive_mutex> lock(m_lock);

                string_t controlId = buttonInputJson[RPC_CONTROL_ID].as_string();
                string_t transactionId;
                uint32_t cost = 0;
                update_button_state(controlId, inputJson);

                // send event out to title
                std::shared_ptr<interactive_participant> currParticipant;

                bool isPressed = ((0 == buttonInputJson[RPC_PARAM_INPUT_EVENT].as_string().compare(RPC_INPUT_EVENT_BUTTON_DOWN)) ? true : false);

                if (inputJson.at(RPC_PARAMS).has_field(RPC_PARTICIPANT_ID))
                {
                    uint32_t mixerId = m_participantTomixerId[inputJson.at(RPC_PARAMS).at(RPC_PARTICIPANT_ID).as_string()];
                    currParticipant = m_participants[mixerId];
                }
                else
                {
                    LOGS_ERROR << L"Received button input without a participant session id";
                }

                if (isPressed)
                {
                    if (inputJson.at(RPC_PARAMS).has_field(RPC_TRANSACTION_ID))
                    {
                        auto button = m_buttons[controlId];
                        if (button)
                        {
                            cost = button->cost();
                        }

                        transactionId = inputJson.at(RPC_PARAMS).at(RPC_TRANSACTION_ID).as_string();
                    }
                    else
                    {
                        LOGS_ERROR << L"Received button press input without a spark transaction id";
                    }
                }
                
                std::shared_ptr<interactive_button_event_args> args = std::shared_ptr<interactive_button_event_args>(new interactive_button_event_args(controlId, transactionId, cost, currParticipant, isPressed));

                queue_interactive_event_for_client(L"", std::error_code(0, std::generic_category()), interactive_event_type::button, args);
            }
            else
            {
                LOGS_INFO << "Button control input message did not contain a correctly formatted button object: no event property";
            }
        }
        else
        {
            LOGS_INFO << "Button control input message did not contain a correctly formatted button object";
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to process button control input message";
    }
}


void interactivity_manager_impl::process_joystick_input(const web::json::value& joystickInputJson)
{
    if (joystickInputJson.has_field(RPC_PARAMS))
    {
        web::json::value joystickParams = joystickInputJson.at(RPC_PARAMS);
        if (joystickParams.has_field(RPC_PARTICIPANT_ID)
            && joystickParams.has_field(RPC_PARAM_INPUT)
            && joystickParams[RPC_PARAM_INPUT].has_field(RPC_CONTROL_EVENT))
        {
            web::json::value inputData = joystickParams[RPC_PARAM_INPUT];
            string_t controlEvent = inputData[RPC_CONTROL_EVENT].as_string();
            if (0 == controlEvent.compare(RPC_INPUT_EVENT_MOVE))
            {
                if (inputData.has_field(RPC_CONTROL_ID) && inputData.has_field(RPC_JOYSTICK_X) && inputData.has_field(RPC_JOYSTICK_Y))
                {
                    double x = inputData[RPC_JOYSTICK_X].as_double();
                    double y = inputData[RPC_JOYSTICK_Y].as_double();

                    string_t controlId = inputData[RPC_CONTROL_ID].as_string();
                    string_t participantSessionId = joystickParams[RPC_PARTICIPANT_ID].as_string();
                    uint32_t mixerId = m_participantTomixerId[participantSessionId];

                    if (0 == mixerId)
                    {
                        LOGS_INFO << "Unexpected input from participant id " << participantSessionId;
                        return;
                    }

                    std::shared_ptr<interactive_participant> participant = m_participants[mixerId];
                    if (nullptr == participant)
                    {
                        LOGS_INFO << "No participant record for mixer id " << mixerId;
                        return;
                    }

                    // Update joystick state
                    update_joystick_state(controlId, mixerId, x, y);

                    std::shared_ptr<interactive_joystick_event_args> args = std::shared_ptr<interactive_joystick_event_args>(new interactive_joystick_event_args(participant, x, y, controlId));

                    queue_interactive_event_for_client(L"", std::error_code(0, std::generic_category()), interactive_event_type::joystick, args);

                    return;
                }
            }
            else
            {
                LOGS_INFO << "Received poorly formatted joystick input: " << joystickInputJson.serialize();
                return;
            }
        }
        else
        {
            LOGS_INFO << "Received poorly formatted joystick input: " << joystickInputJson.serialize();
            return;
        }
    }
}

void interactivity_manager_impl::send_message(std::shared_ptr<interactive_rpc_message> rpcMessage)
{
    if (interactivity_connection_state::connected != m_connectionState)
    {
        LOGS_INFO << L"Failed to send message (" << rpcMessage->id() << "), not connected to Mixer interactivity experience";
        return;
    }

    try
    {
        if (rpcMessage->m_json.has_field(RPC_DISCARD) && false == rpcMessage->m_json.at(RPC_DISCARD).as_bool())
        {
            m_awaitingReply.push_back(rpcMessage);
        }

        m_webSocketConnection->send(rpcMessage->to_string())
            .then([](pplx::task<void> t)
        {
            try
            {
                t.get();
            }
            catch (std::exception e)
            {
                // Throws this exception on failure to send. It is up to the developer
                // to resend or not.
                LOGS_ERROR << "Failed to send on websocket.";
            }
        });
    }
    catch (std::exception e)
    {
        remove_awaiting_reply(rpcMessage->id());
        LOGS_ERROR << "Failed to serialize and send rpc message";
    }
}


std::shared_ptr<interactive_rpc_message>
interactivity_manager_impl::remove_awaiting_reply(uint32_t messageId)
{
    std::lock_guard<std::recursive_mutex> lock(m_messagesLock);
    std::shared_ptr<interactive_rpc_message> message;

    auto msgToRemove = std::find_if(m_awaitingReply.begin(), m_awaitingReply.end(), [&](const std::shared_ptr<interactive_rpc_message> & msg) {
        return msg->id() == messageId;
    });

    if (msgToRemove != m_awaitingReply.end())
    {
        message = *msgToRemove;
        m_awaitingReply.erase(msgToRemove);

        LOGS_DEBUG << "Removed message sent: " << messageId;
    }
    else
    {
        LOGS_INFO << "Unexpected json reply, no outstanding message awaiting reply matching reply id";
    }

    return message;
}

void
interactivity_manager_impl::add_group_to_map(std::shared_ptr<interactive_group_impl> group)
{
    auto checkForGroup = m_groupsInternal[group->m_groupId];
    if (nullptr == checkForGroup)
    {
        m_groupsInternal[group->m_groupId] = group;

        std::vector<std::shared_ptr<interactive_group_impl>> groupToUpdate;
        groupToUpdate.push_back(group);

        send_create_groups(groupToUpdate);
    }
}

void
interactivity_manager_impl::add_control_to_map(std::shared_ptr<interactive_control> control)
{
    std::lock_guard<std::recursive_mutex> lock(m_lock);
    m_controls[control->control_id()] = control;

    if (control->control_type() == interactive_control_type::joystick)
    {
        std::shared_ptr<interactive_joystick_control> joystick = std::static_pointer_cast <interactive_joystick_control>(control);
        m_joysticks[joystick->control_id()] = joystick;

        LOGS_DEBUG << "Adding joystick to map (" << joystick->control_id() << ")";
    }
    else if (control->control_type() == interactive_control_type::button)
    {
        std::shared_ptr<interactive_button_control> button = std::static_pointer_cast <interactive_button_control>(control);
        m_buttons[button->control_id()] = button;

        LOGS_DEBUG << "Adding button to map (" << button->control_id() << ")";
    }
    return;
}


void
interactivity_manager_impl::update_button_state(string_t buttonId, web::json::value buttonInputParamsJson)
{
    // TODO: lock
    std::shared_ptr<interactive_button_control> button = m_buttons[buttonId];

    if (nullptr == button)
    {
        LOGS_INFO << L"Button not in map: " << buttonId;
    }
    else if (buttonInputParamsJson.has_field(RPC_PARAMS) && (buttonInputParamsJson[RPC_PARAMS].has_field(RPC_PARTICIPANT_ID)))
    {
        string_t participantSessionId = buttonInputParamsJson[RPC_PARAMS][RPC_PARTICIPANT_ID].as_string();

        uint32_t mixerId = m_participantTomixerId[participantSessionId];

        if (0 == mixerId)
        {
            LOGS_INFO << "Unexpected input from participant id " << participantSessionId;
            return;
        }

        std::shared_ptr<interactive_participant> currParticipant = m_participants[mixerId];
        if (nullptr == currParticipant)
        {
            LOGS_INFO << "Unexpected input from participant " << mixerId;            
            return;
        }

        if (currParticipant->input_disabled())
        {
            LOGS_INFO << "Ignoring input from disabled participant " << currParticipant->username();
            return;
        }

        std::shared_ptr<interactive_button_state> newState = std::shared_ptr<interactive_button_state>(new interactive_button_state());
        std::shared_ptr<interactive_button_state> oldStateByParticipant = button->m_buttonStateByMixerId[mixerId];
        if (nullptr == oldStateByParticipant)
        {
            // Create initial state entry for this participant
            button->m_buttonStateByMixerId[mixerId] = newState;
            oldStateByParticipant = button->m_buttonStateByMixerId[mixerId];
        }

        bool wasPressed = oldStateByParticipant->is_pressed();
        bool isPressed = (0 == buttonInputParamsJson[RPC_PARAMS][RPC_PARAM_INPUT][RPC_PARAM_INPUT_EVENT].as_string().compare(RPC_INPUT_EVENT_BUTTON_DOWN));

       if (isPressed)
        {
            if (!wasPressed)
            {
                newState->m_isDown = true;
                newState->m_isPressed = true;
                newState->m_isUp = false;
            }
            else
            {
                newState->m_isDown = false;
                newState->m_isPressed = true;
                newState->m_isUp = false;
            }
        }
        else
        {
            // This means isPressed on the event was false, so it was a mouse up event.
            newState->m_isDown = false;
            newState->m_isPressed = false;
            newState->m_isUp = true;
        }

        button->m_buttonStateByMixerId[mixerId] = newState;

        if (newState->is_down())
        {
            button->m_buttonCount->m_buttonDowns++;
        }
        if (newState->is_pressed())
        {
            button->m_buttonCount->m_buttonPresses++;
        }
        if (newState->is_up())
        {
            button->m_buttonCount->m_buttonUps++;
        }
    }
}


void
interactivity_manager_impl::update_joystick_state(string_t joystickId, uint32_t mixerId, double x, double y)
{
    // TODO: lock
    std::shared_ptr<interactive_joystick_control> joystick = m_joysticks[joystickId];

    if (nullptr == joystick)
    {
        LOGS_INFO << L"Joystick not in map: " << joystickId;
    }
    else
    {
        std::shared_ptr<interactive_participant> currParticipant = m_participants[mixerId];
        if (nullptr == currParticipant)
        {
            LOGS_INFO << "Unexpected input from participant " << mixerId;
            return;
        }

        if (currParticipant->input_disabled())
        {
            LOGS_INFO << "Ignoring input from disabled participant " << currParticipant->username();
            return;
        }

        std::shared_ptr<interactive_joystick_state> newState = std::shared_ptr<interactive_joystick_state>(new interactive_joystick_state(x, y));
        std::shared_ptr<interactive_joystick_state> oldStateByParticipant = joystick->m_joystickStateByMixerId[mixerId];
        joystick->m_joystickStateByMixerId[mixerId] = newState;
        joystick->m_x = x;
        joystick->m_y = y;
    }
}


string_t
interactivity_manager_impl::find_or_create_participant_session_id(_In_ uint32_t mixerId)
{
    string_t participantSessionId;
    static string_t baseMockParticipantSessionId = L"mockParticipantId";
    static uint32_t nextParticipantSessionId = 0;
    
    for (auto iter = m_participantTomixerId.begin(); iter != m_participantTomixerId.end(); iter++)
    {
        if (mixerId == iter->second)
        {
            participantSessionId = iter->first;
        }
    }

    if (participantSessionId.empty())
    {
        participantSessionId = baseMockParticipantSessionId;
        participantSessionId.append(std::to_wstring(nextParticipantSessionId++));
    }

    return participantSessionId;
}

void
interactivity_manager_impl::mock_button_event(_In_ uint32_t mixerId, _In_ string_t buttonId, _In_ bool is_down)
{
    string_t participantSessionId = find_or_create_participant_session_id(mixerId);
    web::json::value mockInputJson = build_mock_button_input(get_next_message_id(), participantSessionId, buttonId, is_down);

    on_socket_message_received(mockInputJson.serialize());
}

#if 0
void
interactivity_manager_impl::mock_participant_join(uint32_t mixerId, string_t username)
{
    uint32_t messageId = get_next_message_id();
    string_t participantSessionId = find_or_create_participant_session_id(mixerId);
    web::json::value mockParticipantJoinJson = build_participant_state_change_mock_data(messageId, mixerId, username, participantSessionId, true /*isJoin*/, false /*discard*/);
    auto mockParticipantJoinMethod = build_mediator_rpc_message(messageId, RPC_METHOD_PARTICIPANTS_JOINED, mockParticipantJoinJson);

    process_method(mockParticipantJoinMethod);
}

void
interactivity_manager_impl::mock_participant_leave(uint32_t mixerId, string_t username)
{
    uint32_t messageId = get_next_message_id();
    string_t participantSessionId = find_or_create_participant_session_id(mixerId);
    web::json::value mockParticipantLeaveJson = build_participant_state_change_mock_data(messageId, mixerId, username, participantSessionId, false /*isJoin*/, false /*discard*/);
    auto mockParticipantLeaveMethod = build_mediator_rpc_message(messageId, RPC_METHOD_PARTICIPANTS_LEFT, mockParticipantLeaveJson);

    process_method(mockParticipantLeaveMethod);
}
#endif

void
interactivity_manager_impl::queue_interactive_event_for_client(string_t errorMessage, std::error_code errorCode, interactive_event_type type, std::shared_ptr<interactive_event_args> args)
{
    std::lock_guard<std::recursive_mutex> lock(m_lock);
    m_eventsForClient.push_back(*(create_interactive_event(errorMessage, errorCode, type, args)));
    return;
}

void MICROSOFT_MIXER_NAMESPACE::interactivity_manager_impl::queue_message_for_service(std::shared_ptr<interactive_rpc_message> message)
{
    m_pendingSend.push(message);
}

NAMESPACE_MICROSOFT_MIXER_END
