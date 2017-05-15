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
#include "beam.h"
#include "beam_internal.h"
#include "json_helper.h"

using namespace std::chrono;

const string_t requestServerUri = L"https://beam.pro/api/v1/interactive/hosts";
const string_t localServiceUri = L"ws://127.0.0.1:3000/gameClient";
const string_t protocolVersion = L"2.0";

NAMESPACE_MICROSOFT_XBOX_BEAM_BEGIN

std::shared_ptr<beam_event> create_beam_event(string_t errorMessage, std::error_code errorCode, beam_event_type type, std::shared_ptr<beam_event_args> args)
{
    std::shared_ptr<beam_event> event = std::make_shared<beam_event>(unix_timestamp_in_ms(), errorCode, errorMessage, type, args);
    return event;
}


string_t beam_rpc_message::to_string()
{
    return m_json.serialize();
}

uint32_t beam_rpc_message::id() { return m_id; }

std::chrono::milliseconds beam_rpc_message::timestamp()
{
    return m_timestamp;
}

beam_rpc_message::beam_rpc_message(uint32_t id, web::json::value jsonMessage, std::chrono::milliseconds timestamp) :
    m_retries(0),
    m_id(id),
    m_json(std::move(jsonMessage)),
    m_timestamp(std::move(timestamp))
{
}

beam_manager_impl::beam_manager_impl() :
    m_connectionState(beam_interactivity_connection_state::disconnected),
    m_interactivityState(beam_interactivity_state::not_initialized),
    m_initRetryAttempt(0),
    m_maxInitRetries(10),
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

    m_initializingTask = pplx::create_task([] {});
    m_processMessagesTask = pplx::create_task([] {});
}

beam_manager_impl::~beam_manager_impl()
{
    close_websocket();
}

void beam_manager_impl::close_websocket()
{
    std::shared_ptr<XBOX_BEAM_NAMESPACE::web_socket_connection> socketToClean;
    {
        std::lock_guard<std::recursive_mutex> lock(m_lock);
        socketToClean = m_webSocketConnection;
        m_webSocketConnection = nullptr;
        m_connectionState = beam_interactivity_connection_state::disconnected;
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
beam_manager_impl::initialize(
    _In_ string_t interactiveVersion,
    _In_ bool goInteractive
)
{
    if (beam_interactivity_state::not_initialized != m_interactivityState)
    {
        return true;
    }

    m_interactiveVersion = interactiveVersion;

    // Create long-running initialization task
    std::weak_ptr<beam_manager_impl> thisWeakPtr = shared_from_this();

    if (m_initializingTask.is_done())
    {
        m_initializingTask = pplx::create_task([thisWeakPtr, interactiveVersion, goInteractive]()
        {
            std::shared_ptr<beam_manager_impl> pThis;
            pThis = thisWeakPtr.lock();
            if (nullptr != pThis)
            {
                pThis->init_worker(interactiveVersion, goInteractive);
            }
        });
    }
    else
    {
        LOGS_DEBUG << "beam_manager initialization already in progress";
        return false;
    }

    // Create long-running message processing task
    if (false == m_processing)
    {
        m_processMessagesTask = pplx::create_task([thisWeakPtr]()
        {
            std::shared_ptr<beam_manager_impl> pThis;
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
std::shared_ptr<beam_event>
beam_manager_impl::add_local_user(xbox_live_user_t user)
{
    std::lock_guard<std::recursive_mutex> lock(m_lock);

    auto duration = std::chrono::steady_clock::now().time_since_epoch();
    std::chrono::milliseconds currTime = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    std::shared_ptr<beam_event> event = nullptr;
    
    if (beam_interactivity_state::interactivity_enabled == m_interactivityState || beam_interactivity_state::interactivity_pending == m_interactivityState)
    {
        event = std::make_shared<xbox::services::beam::beam_event>(currTime, std::error_code(0, std::generic_category()), L"Cannot change local user while in an interactive state", beam_event_type::error, nullptr);
    }
    else if (beam_interactivity_state::initializing == m_interactivityState)
    {
        event = std::make_shared<xbox::services::beam::beam_event>(currTime, std::error_code(0, std::generic_category()), L"Cannot change local user while initialization is in progress", beam_event_type::error, nullptr);
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
            set_interactivity_state(beam_interactivity_state::not_initialized);
        }
    }

    return event;
}
#endif

void
beam_manager_impl::init_worker(
    _In_ string_t interactiveVersion,
    _In_ bool goInteractive
)
{
    bool success = true;
    std::shared_ptr<beam_event> errorEvent;

    set_interactivity_state(beam_interactivity_state::initializing);

    success = get_auth_token(errorEvent);

    if (success)
    {
        success = get_interactive_host();
        if (!success)
        {
            errorEvent = create_beam_event(L"beam_manager::initialize failed", std::make_error_code(std::errc::operation_canceled), beam_event_type::error, nullptr);
        }
    }

    if (success)
    {
        initialize_websockets_helper();
        initialize_server_state_helper();

        int retryAttempt = 0;
        std::chrono::milliseconds retryInterval(100);
        while (retryAttempt < m_maxInitRetries)
        {
            if (m_initScenesComplete && m_initGroupsComplete && m_initServerTimeComplete)
            {
                set_interactivity_state(beam_interactivity_state::interactivity_disabled);
                break;
            }

            std::this_thread::sleep_for(retryInterval);

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
        if (m_interactivityState == beam_interactivity_state::interactivity_disabled)
        {
            start_interactive();
        }
    }
    
    if (!success)
    {
        LOGS_DEBUG << L"Failed to initialize beam_manager.";
        set_interactivity_state(beam_interactivity_state::not_initialized);

        if (nullptr == errorEvent)
        {
            errorEvent = create_beam_event(L"beam_manager::initialize failed", std::make_error_code(std::errc::operation_canceled), beam_event_type::error, nullptr);
        }

        {
            std::lock_guard<std::recursive_mutex> lock(m_lock);
            m_eventsForClient.push_back(*errorEvent);
        }
    }

    return;
}


void xbox::services::beam::beam_manager_impl::process_messages_worker()
{
    static const int chunkSize = 10;
    while (m_processing)
    {
        {
            std::lock_guard<std::recursive_mutex> lock(m_messagesLock);
            for (int i = 0; i < chunkSize && !m_unhandledFromService.empty(); i++)
            {
                std::shared_ptr<beam_rpc_message> currentMessage = m_unhandledFromService.front();
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

        if (m_webSocketConnection && m_webSocketConnection->state() == beam_web_socket_connection_state::connected)
        {
            std::lock_guard<std::recursive_mutex> lock(m_messagesLock);
            for (int i = 0; i < chunkSize && !m_pendingSend.empty(); i++)
            {
                std::shared_ptr<beam_rpc_message> currentMessage = m_pendingSend.front();
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
                std::shared_ptr<beam_rpc_message> currentMessage = m_awaitingReply[i];

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
beam_manager_impl::get_auth_token(_Out_ std::shared_ptr<beam_event> &errorEvent)
{
#if TV_API | XBOX_UWP
    if (0 == m_localUsers.size())
    {
        errorEvent = create_beam_event(L"No local users registered, cannot complete initialization", std::make_error_code(std::errc::operation_canceled), beam_event_type::error, nullptr);
        return false;
    }
    else
    {
        string_t beamUri = L"https://beam.pro";
        string_t authRequestHeaders = L"";
        auto platformHttp = ref new Platform::String(L"POST");
        auto platformUrl = ref new Platform::String(beamUri.c_str());
        auto platformHeaders = ref new Platform::String(authRequestHeaders.c_str());

        pplx::task<Windows::Xbox::System::GetTokenAndSignatureResult^> asyncTask;

        std::weak_ptr<beam_manager_impl> thisWeakPtr = shared_from_this();
        asyncTask = pplx::create_task([thisWeakPtr, platformHttp, platformUrl, platformHeaders]()
        {
            std::shared_ptr<beam_manager_impl> pThis;
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
                errorEvent = create_beam_event(L"Failed to retrieve token from local user", std::make_error_code(std::errc::operation_canceled), beam_event_type::error, nullptr);
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
    // For now, the auth code needs to be set by a call to beam_mock_util::set_oauth_token, eventually will require a call to get the shortcode auth
    if (m_accessToken.empty())
    {
        LOGS_INFO << "OAuth token empty";
        errorEvent = create_beam_event(L"OAuth token empty", std::make_error_code(std::errc::operation_canceled), beam_event_type::error, nullptr);
        return false;
    }
#endif

    return true;
}

bool
beam_manager_impl::get_interactive_host()
{
    bool success = true;

    try
    {
        web::http::client::http_client hostsRequestClient(requestServerUri);
        auto result = hostsRequestClient.request(L"GET");
        auto response = result.get();
        if (response.status_code() == web::http::status_codes::OK)
        {
            std::weak_ptr<beam_manager_impl> thisWeakPtr = shared_from_this();
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
beam_manager_impl::initialize_websockets_helper()
{
    if (m_interactivityState == beam_interactivity_state::initializing)
    {
        if (m_webSocketConnection != nullptr)
        {
            close_websocket();
        }

        m_webSocketConnection = std::make_shared<XBOX_BEAM_NAMESPACE::web_socket_connection>(m_accessToken, m_interactiveVersion, protocolVersion);

        // We will reset these event handlers on destructor, so it's safe to pass in 'this' here.
        std::weak_ptr<beam_manager_impl> thisWeakPtr = shared_from_this();
        m_webSocketConnection->set_connection_state_change_handler([thisWeakPtr](beam_web_socket_connection_state oldState, beam_web_socket_connection_state newState)
        {
            std::shared_ptr<beam_manager_impl> pThis(thisWeakPtr.lock());
            if (pThis != nullptr)
            {
                pThis->on_socket_connection_state_change(oldState, newState);
            }
        });

        m_webSocketConnection->set_received_handler([thisWeakPtr](string_t message)
        {
            std::shared_ptr<beam_manager_impl> pThis(thisWeakPtr.lock());
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
beam_manager_impl::get_next_message_id()
{
    static uint32_t nextMessageId = 0;
    return nextMessageId++;
}


bool
beam_manager_impl::initialize_server_state_helper()
{
    while (m_initRetryAttempt < m_maxInitRetries)
    {
        if (beam_interactivity_connection_state::connected == m_connectionState)
        {
            break;
        }

        LOGS_INFO << "Init task waiting on websocket connection... attempt " << ++m_initRetryAttempt;

        std::this_thread::sleep_for(m_initRetryInterval);

        m_initRetryInterval = min((3 * m_initRetryInterval), std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::minutes(1)));
    }

    if (m_initRetryAttempt < m_maxInitRetries)
    {
        // request service time, for synchronization purposes
        std::shared_ptr<beam_rpc_message> getTimeMessage = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_GET_TIME, web::json::value(), false);
        queue_message_for_service(getTimeMessage);

        // request groups
        std::shared_ptr<beam_rpc_message> getGroupsMessage = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_GET_GROUPS, web::json::value(), false);
        queue_message_for_service(getGroupsMessage);

        // request scenes
        std::shared_ptr<beam_rpc_message> getScenesMessage = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_GET_SCENES, web::json::value(), false);
        queue_message_for_service(getScenesMessage);

        return true;
    }
    else
    {
        LOG_ERROR("Beam manager initialization failed - websocket connection timed out");
        return false;
    }
}

const std::chrono::milliseconds
beam_manager_impl::get_server_time()
{
    std::chrono::milliseconds serverTime = unix_timestamp_in_ms() - m_serverTimeOffset;

    return serverTime;
}

const beam_interactivity_state
beam_manager_impl::interactivity_state()
{
    return m_interactivityState;
}

bool
beam_manager_impl::start_interactive()
{
    std::lock_guard<std::recursive_mutex> lock(m_lock);

    if (!m_webSocketConnection || (m_webSocketConnection && m_webSocketConnection->state() != beam_web_socket_connection_state::connected))
    {
        queue_beam_event_for_client(L"Websocket connection has not been established, please wait for the initialized state", std::make_error_code(std::errc::not_connected), beam_event_type::error, nullptr);
        return false;
    }

    if (m_interactivityState == beam_interactivity_state::not_initialized)
    {
        queue_beam_event_for_client(L"Interactivity not initialized. Must call beam_manager::initialize before requesting start_interactive", std::make_error_code(std::errc::not_connected), beam_event_type::error, nullptr);
        return false;
    }

    if (m_interactivityState == beam_interactivity_state::initializing)
    {
        queue_beam_event_for_client(L"Interactivity initialization is pending. Please wait for initialize to complete", std::make_error_code(std::errc::not_connected), beam_event_type::error, nullptr);
        return false;
    }

    set_interactivity_state(beam_interactivity_state::interactivity_pending);

    web::json::value params;
    params[RPC_PARAM_IS_READY] = web::json::value::boolean(true);
    std::shared_ptr<beam_rpc_message> sendInteractiveReadyMessage = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_READY, params, true);
    queue_message_for_service(sendInteractiveReadyMessage);

    return true;
}

bool
beam_manager_impl::stop_interactive()
{
    std::lock_guard<std::recursive_mutex> lock(m_lock);

    if (m_interactivityState == beam_interactivity_state::not_initialized || m_interactivityState == beam_interactivity_state::interactivity_disabled)
    {
        return true;
    }

    set_interactivity_state(beam_interactivity_state::interactivity_pending);

    web::json::value params;
    params[RPC_PARAM_IS_READY] = web::json::value::boolean(false);
    std::shared_ptr<beam_rpc_message> sendInteractiveReadyMessage = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_READY, params, false);
    queue_message_for_service(sendInteractiveReadyMessage);

    return true;
}

const std::shared_ptr<beam_participant>&
beam_manager_impl::participant(_In_ uint32_t beamId)
{
    return m_participants[beamId];
}


std::vector<std::shared_ptr<beam_participant>>
beam_manager_impl::participants()
{
    std::vector<std::shared_ptr<beam_participant>> participantsCopy;
    participantsCopy.reserve(m_participants.size());

    for (auto iter = m_participants.begin(); iter != m_participants.end(); iter++)
    {
        participantsCopy.push_back(iter->second);
    }

    return participantsCopy;
}


std::vector<std::shared_ptr<beam_group>>
beam_manager_impl::groups()
{
    std::vector<std::shared_ptr<beam_group>> groupsCopy;
    groupsCopy.reserve(m_groupsInternal.size());

    for (auto iter = m_groupsInternal.begin(); iter != m_groupsInternal.end(); iter++)
    {
        auto groupImpl = iter->second;
        std::shared_ptr<beam_group> newGroup = std::make_shared<beam_group>(groupImpl->group_id());
        newGroup->m_impl = groupImpl;
        groupsCopy.push_back(newGroup);
    }

    return groupsCopy;
}

std::shared_ptr<beam_group>
beam_manager_impl::group(_In_ const string_t& group_id)
{
    auto groupImpl = m_groupsInternal[group_id];
    std::shared_ptr<beam_group> groupPtr = std::make_shared<beam_group>(groupImpl->group_id());
    groupPtr->m_impl = groupImpl;
    return groupPtr;
}

const std::shared_ptr<beam_scene>&
beam_manager_impl::scene(_In_ const string_t& sceneId)
{
    return m_scenes[sceneId];
}

std::vector<std::shared_ptr<beam_scene>>
beam_manager_impl::scenes()
{
    std::vector<std::shared_ptr<beam_scene>> scenesCopy;
    scenesCopy.reserve(m_scenes.size());

    for (auto iter = m_scenes.begin(); iter != m_scenes.end(); iter++)
    {
        scenesCopy.push_back(iter->second);
    }

    return scenesCopy;
}


std::shared_ptr<beam_event>
beam_manager_impl::try_set_current_scene(_In_ const string_t& scene_id, _In_ const string_t& group_id)
{
    std::shared_ptr<beam_event> result;

    if (m_interactivityState == beam_interactivity_state::not_initialized || m_interactivityState == beam_interactivity_state::initializing)
    {
        result = create_beam_event(L"Interactivity not yet initialized.", std::make_error_code(std::errc::not_connected), beam_event_type::error, nullptr);
        return result;
    }

    if (m_scenes.end() == m_scenes.find(scene_id))
    {
        result = create_beam_event(L"Unknown scene ID specified.", std::make_error_code(std::errc::no_such_file_or_directory), beam_event_type::error, nullptr);
        return result;
    }

    auto group = m_groupsInternal[group_id];
    std::vector<std::shared_ptr<beam_group_impl>> groupToUpdate;
    groupToUpdate.push_back(group);

    send_update_groups(groupToUpdate);

    return result;
}

std::shared_ptr<beam_button_control> xbox::services::beam::beam_manager_impl::button(const string_t & control_id)
{
    return m_buttons[control_id];
}


void 
beam_manager_impl::send_update_participants(std::vector<uint32_t> participantIds)
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
        currParticipantJson[RPC_BEAM_USERNAME] = web::json::value::string(currParticipant->m_impl->m_username);
        currParticipantJson[RPC_SESSION_ID] = web::json::value::string(currParticipant->m_impl->m_sessionId);
        currParticipantJson[RPC_ETAG] = web::json::value::string(currParticipant->m_impl->m_etag);

        participantsToUpdateJson[participantIndex] = currParticipantJson;
        participantIndex++;
    }

    params[RPC_PARAM_PARTICIPANTS] = participantsToUpdateJson;

    std::shared_ptr<beam_rpc_message> sendCreateGroupMsg = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_UPDATE_PARTICIPANTS, params, false);

    queue_message_for_service(sendCreateGroupMsg);

    return;
}

void
beam_manager_impl::send_create_groups(std::vector<std::shared_ptr<beam_group_impl>> groupsToCreate)
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

    std::shared_ptr<beam_rpc_message> sendCreateGroupMsg = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_CREATE_GROUPS, params, false);

    queue_message_for_service(sendCreateGroupMsg);

    return;
}

void
beam_manager_impl::send_update_groups(std::vector<std::shared_ptr<beam_group_impl>> groupsToUpdate)
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

    std::shared_ptr<beam_rpc_message> sendUpdateGroupsMsg = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_UPDATE_GROUPS, params, false);

    queue_message_for_service(sendUpdateGroupsMsg);

    return;
}

void
beam_manager_impl::move_participant_group(uint32_t participantId, string_t oldGroupId, string_t newGroupId)
{
    std::shared_ptr<beam_participant> participant = m_participants[participantId];

    if (nullptr == participant)
    {
        LOGS_INFO << L"Failed to find specified participant " << participantId;
        return;
    }

    std::shared_ptr<beam_group_impl> oldGroup = m_groupsInternal[oldGroupId];
    std::shared_ptr<beam_group_impl> newGroup = m_groupsInternal[newGroupId];

    auto oldGroupParticipants = m_participantsByGroupId[oldGroup->m_groupId];
    oldGroupParticipants.erase(std::remove(oldGroupParticipants.begin(), oldGroupParticipants.end(), participantId), oldGroupParticipants.end());
    m_participantsByGroupId[oldGroup->m_groupId] = oldGroupParticipants;

    auto newGroupParticipants = m_participantsByGroupId[newGroup->m_groupId];
    newGroupParticipants.push_back(participantId);
    m_participantsByGroupId[newGroup->m_groupId] = newGroupParticipants;

    std::vector<uint32_t> participantsToUpdate;
    participantsToUpdate.push_back(participant->beam_id());
    send_update_participants(participantsToUpdate);
}

// This should only be used when a participant leaves the channel.
void
beam_manager_impl::participant_leave_group(uint32_t participantId, string_t groupId)
{
    std::shared_ptr<beam_participant> participant = m_participants[participantId];

    if (nullptr == participant)
    {
        LOGS_INFO << L"Failed to find specified participant " << participantId;
        return;
    }

    auto oldGroupParticipants = m_participantsByGroupId[groupId];
    oldGroupParticipants.erase(std::remove(oldGroupParticipants.begin(), oldGroupParticipants.end(), participantId), oldGroupParticipants.end());
    m_participantsByGroupId[groupId] = oldGroupParticipants;

    std::shared_ptr<beam_group_impl> oldGroup = m_groupsInternal[groupId];
    std::vector<std::shared_ptr<beam_group_impl>> groupsToUpdate;
    groupsToUpdate.push_back(oldGroup);

    send_update_groups(groupsToUpdate);
}

std::vector<std::shared_ptr<beam_button_control>>
beam_manager_impl::buttons()
{
    std::vector<std::shared_ptr<beam_button_control>> buttonsCopy;
    buttonsCopy.reserve(m_buttons.size());

    for (auto iter = m_buttons.begin(); iter != m_buttons.end(); iter++)
    {
        buttonsCopy.push_back(iter->second);
    }

    return buttonsCopy;
}

std::shared_ptr<beam_joystick_control> xbox::services::beam::beam_manager_impl::joystick(const string_t & control_id)
{
    return m_joysticks[control_id];
}

std::vector<std::shared_ptr<beam_joystick_control>>
beam_manager_impl::joysticks()
{
    std::vector<std::shared_ptr<beam_joystick_control>> joysticksCopy;
    joysticksCopy.reserve(m_joysticks.size());

    for (auto iter = m_joysticks.begin(); iter != m_joysticks.end(); iter++)
    {
        joysticksCopy.push_back(iter->second);
    }

    return joysticksCopy;
}


void
beam_manager_impl::trigger_cooldown(_In_ const string_t& control_id, _In_ const std::chrono::milliseconds& cooldown)
{
    std::shared_ptr<beam_control> control = m_controls[control_id];
    if (nullptr != control)
    {
        std::shared_ptr<beam_button_control> buttonControl = std::dynamic_pointer_cast<beam_button_control>(control);

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

            std::shared_ptr<beam_rpc_message> updateControlMessage = build_mediator_rpc_message(get_next_message_id(), RPC_METHOD_UPDATE_CONTROLS, params, false);
            queue_message_for_service(updateControlMessage);
        }
    }
}

std::vector<beam_event>
beam_manager_impl::do_work()
{
    std::vector<beam_event> eventsForClient;
    std::lock_guard<std::recursive_mutex> lock(m_lock);

    if (m_eventsForClient.size() > 0)
    {
        if (m_webSocketConnection != nullptr && m_webSocketConnection->state() != beam_web_socket_connection_state::disconnected)
        {
            m_webSocketConnection->ensure_connected();
        }

        eventsForClient = std::move(m_eventsForClient);
        m_eventsForClient.clear();

        for (auto iter = m_controls.begin(); iter != m_controls.end(); iter++)
        {
            iter->second->clear_state();
        }
    }

    return eventsForClient;
}

void
beam_manager_impl::set_interactivity_state(beam_interactivity_state newState)
{
    if (m_interactivityState == newState)
    {
        return;
    }

    bool validStateChange = true;
    beam_interactivity_state oldState = m_interactivityState;
    std::shared_ptr<beam_interactivity_state_change_event_args> eventArgs;

    switch (newState)
    {
    case beam_interactivity_state::not_initialized:
        // can revert to not_initialized from any state
        break;
    case beam_interactivity_state::initializing:
        if (m_interactivityState != beam_interactivity_state::not_initialized)
        {
            validStateChange = false;
        }
        break;
    case beam_interactivity_state::interactivity_disabled:
        if (m_interactivityState == beam_interactivity_state::not_initialized || m_interactivityState == beam_interactivity_state::interactivity_enabled)
        {
            validStateChange = false;
        }
        break;
    case beam_interactivity_state::interactivity_pending:
        if (m_interactivityState != beam_interactivity_state::interactivity_disabled && m_interactivityState != beam_interactivity_state::interactivity_enabled)
        {
            validStateChange = false;
        }
        break;
    case beam_interactivity_state::interactivity_enabled:
        if (m_interactivityState != beam_interactivity_state::interactivity_pending)
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
     
        std::shared_ptr<beam_interactivity_state_change_event_args> args = std::make_shared<beam_interactivity_state_change_event_args>(m_interactivityState);
        queue_beam_event_for_client(L"", std::error_code(0, std::generic_category()), beam_event_type::interactivity_state_changed, args);

        LOGS_DEBUG << L"Interactivity state change from " << oldState << L" to " << newState;
    }
    else
    {
        LOGS_ERROR << L"Unexpected interactivity state change from " << oldState << L" to " << newState;
    }
}

void
beam_manager_impl::on_socket_connection_state_change(
    _In_ beam_web_socket_connection_state oldState,
    _In_ beam_web_socket_connection_state newState
)
{
    if (oldState == newState)
    {
        return;
    }

    if (beam_web_socket_connection_state::activated == newState)
    {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(m_lock);
    {
        if (newState == beam_web_socket_connection_state::disconnected)
        {
            LOGS_INFO << "Websocket disconnected";
            m_connectionState = beam_interactivity_connection_state::disconnected;
        }

        // On connecting, set subscriptions state accordingly.
        if (newState == beam_web_socket_connection_state::connecting)
        {
            LOGS_INFO << "Websocket connecting";
            m_connectionState = beam_interactivity_connection_state::connecting;
        }

        // Socket reconnected, re-subscribe everything
        if (newState == beam_web_socket_connection_state::connected)
        {
            LOGS_INFO << "Websocket connected";
            m_connectionState = beam_interactivity_connection_state::connected;
        }
    }

    return;
}


void
beam_manager_impl::on_socket_message_received(
    _In_ const string_t& message
)
{
    std::shared_ptr<beam_rpc_message> rpcMessage = std::shared_ptr<beam_rpc_message>(new beam_rpc_message(get_next_message_id(), web::json::value::parse(message), unix_timestamp_in_ms()));
    m_unhandledFromService.push(rpcMessage);
}

void beam_manager_impl::process_reply(const web::json::value& jsonReply)
{
    LOGS_INFO << "Received a reply from the service";

    try
    {
        if (jsonReply.has_field(L"id"))
        {
            std::lock_guard<std::recursive_mutex> lock(m_messagesLock);
            std::shared_ptr<beam_rpc_message> message = remove_awaiting_reply(jsonReply.at(RPC_ID).as_integer());

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

void beam_manager_impl::process_reply_error(const web::json::value& jsonReply)
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

void beam_manager_impl::process_get_time_reply(std::shared_ptr<beam_rpc_message> message, const web::json::value& jsonReply)
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

void beam_manager_impl::process_get_groups_reply(const web::json::value& jsonReply)
{
    try
    {
        if (jsonReply.has_field(RPC_RESULT) && jsonReply.at(RPC_RESULT).has_field(RPC_PARAM_GROUPS))
        {
            web::json::array groups = jsonReply.at(RPC_RESULT).at(RPC_PARAM_GROUPS).as_array();

            for (auto iter = groups.begin(); iter != groups.end(); ++iter)
            {
                std::shared_ptr<beam_group_impl> newGroup = std::shared_ptr<beam_group_impl>(new beam_group_impl(*iter));
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

void beam_manager_impl::process_create_groups_reply(const web::json::value& jsonReply)
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

void beam_manager_impl::process_update_groups_reply(const web::json::value& jsonReply)
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
                    std::shared_ptr<beam_group_impl> groupToUpdate = m_groupsInternal[(*iter)[RPC_GROUP_ID].as_string()];
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

void beam_manager_impl::process_on_group_create(const web::json::value& onGroupCreateMethod)
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
                    std::shared_ptr<beam_group_impl> groupToUpdate = m_groupsInternal[(*iter)[RPC_GROUP_ID].as_string()];
                    if (nullptr != groupToUpdate)
                    {
                        groupToUpdate->update((*iter), false);
                    }
                    else
                    {
                        std::shared_ptr<beam_group_impl> newGroup = std::shared_ptr<beam_group_impl>(new beam_group_impl(*iter));
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

void beam_manager_impl::process_on_group_update(const web::json::value& onGroupUpdateMethod)
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
                    std::shared_ptr<beam_group_impl> groupToUpdate = m_groupsInternal[(*iter)[RPC_GROUP_ID].as_string()];
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

void beam_manager_impl::process_update_controls_reply(const web::json::value& jsonReply)
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

void beam_manager_impl::process_on_control_update(const web::json::value& onControlUpdateMethod)
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
void beam_manager_impl::process_update_controls(web::json::array controlsToUpdate)
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
                std::shared_ptr<beam_control> controlToUpdate = m_controls[(*iter)[RPC_CONTROL_ID].as_string()];

                if (nullptr == controlToUpdate)
                {
                    LOGS_DEBUG << L"Unexpected update received for control " << (*iter)[RPC_CONTROL_ID].as_string();
                }

                if (beam_control_type::button == controlToUpdate->control_type())
                {
                    std::shared_ptr<beam_button_control> buttonToUpdate = std::dynamic_pointer_cast<beam_button_control>(controlToUpdate);
                    buttonToUpdate->update((*iter), false);
                }
                else if (beam_control_type::joystick == controlToUpdate->control_type())
                {
                    std::shared_ptr<beam_joystick_control> joystickToUpdate = std::dynamic_pointer_cast<beam_joystick_control>(controlToUpdate);
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


void beam_manager_impl::process_get_scenes_reply(const web::json::value& jsonReply)
{
    try
    {
        if (jsonReply.has_field(RPC_RESULT) && jsonReply.at(RPC_RESULT).has_field(RPC_PARAM_SCENES))
        {
            web::json::array scenes = jsonReply.at(RPC_RESULT).at(RPC_PARAM_SCENES).as_array();

            for (auto iter = scenes.begin(); iter != scenes.end(); ++iter)
            {
                std::shared_ptr<beam_scene> newScene = std::shared_ptr<beam_scene>(new beam_scene());
                newScene->m_impl = std::shared_ptr<beam_scene_impl>(new beam_scene_impl());
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

void beam_manager_impl::process_update_participants_reply(const web::json::value& jsonReply)
{
    try
    {
        if (jsonReply.has_field(RPC_RESULT) && jsonReply.at(RPC_RESULT).has_field(RPC_PARAM_PARTICIPANTS))
        {
            web::json::array participants = jsonReply.at(RPC_RESULT).at(RPC_PARAM_PARTICIPANTS).as_array();

            for (auto iter = participants.begin(); iter != participants.end(); ++iter)
            {
                if ((*iter).has_field(RPC_BEAM_ID))
                {
                    std::shared_ptr<beam_participant> participantToUpdate = m_participants[(*iter)[RPC_BEAM_ID].as_number().to_int32()];
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

void beam_manager_impl::process_method(const web::json::value& methodJson)
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


void beam_manager_impl::process_participant_joined(const web::json::value& participantJoinedJson)
{
    LOGS_INFO << "Received a participant joined event";
    try
    {
        if (participantJoinedJson.has_field(RPC_PARAMS) && participantJoinedJson.at(RPC_PARAMS).has_field(RPC_PARAM_PARTICIPANTS))
        {
            web::json::array currParticipants = participantJoinedJson.at(RPC_PARAMS).at(RPC_PARAM_PARTICIPANTS).as_array();
            for (auto iter = currParticipants.begin(); iter != currParticipants.end(); ++iter)
            {
                std::shared_ptr<beam_participant> currParticipant = std::shared_ptr<beam_participant>(new beam_participant());
                currParticipant->m_impl = std::shared_ptr<beam_participant_impl>(new beam_participant_impl());

                bool success = currParticipant->m_impl->init_from_json(*iter);

                if (success)
                {
                    currParticipant->m_impl->m_state = beam_participant_state::joined;
                    m_participants[currParticipant->beam_id()] = currParticipant;
                    m_participantToBeamId[currParticipant->m_impl->m_sessionId] = currParticipant->beam_id();

                    m_participantsByGroupId[currParticipant->m_impl->m_groupId].push_back(currParticipant->beam_id());

                    LOGS_DEBUG << "Participant " << currParticipant->beam_id() << " joined";

                    std::shared_ptr<beam_participant_state_change_event_args> args = std::shared_ptr<beam_participant_state_change_event_args>(new beam_participant_state_change_event_args(currParticipant, beam_participant_state::joined));

                    queue_beam_event_for_client(L"", std::error_code(0, std::generic_category()), beam_event_type::participant_state_changed, args);
                }
                else
                {
                    queue_beam_event_for_client(L"Failed to initialize participant, onParticipantJoin", std::make_error_code(std::errc::not_supported), beam_event_type::error, nullptr);
                }
            }
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to process participant joined RPC call";
    }
}


void beam_manager_impl::process_participant_left(const web::json::value& participantLeftJson)
{
    LOGS_INFO << "Received a participant left event";
    try
    {
        if (participantLeftJson.has_field(RPC_PARAMS) && participantLeftJson.at(RPC_PARAMS).has_field(RPC_PARAM_PARTICIPANTS))
        {
            web::json::array currParticipants = participantLeftJson.at(RPC_PARAMS).at(RPC_PARAM_PARTICIPANTS).as_array();
            for (auto iter = currParticipants.begin(); iter != currParticipants.end(); ++iter)
            {
                if (iter->has_field(RPC_BEAM_ID))
                {
                    uint32_t beamId = iter->at(RPC_BEAM_ID).as_number().to_uint32();
                    std::shared_ptr<beam_participant> currParticipant = m_participants[beamId];
                    currParticipant->m_impl->m_state = beam_participant_state::left;

                    // update the states of the group - remove the participant entirely
                    participant_leave_group(currParticipant->beam_id(), currParticipant->m_impl->m_groupId);

                    LOGS_DEBUG << "Participant " << beamId << " left";

                    std::shared_ptr<beam_participant_state_change_event_args> args = std::shared_ptr<beam_participant_state_change_event_args>(new beam_participant_state_change_event_args(currParticipant, beam_participant_state::left));

                    queue_beam_event_for_client(L"", std::error_code(0, std::generic_category()), beam_event_type::participant_state_changed, args);
                }
                else
                {
                    LOGS_DEBUG << "Failed to parse beam id from participants left";
                }
            }
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to process participant left RPC call";
    }
}

void beam_manager_impl::process_on_participant_update(const web::json::value& participantChangeJson)
{
    LOGS_INFO << "Received a participant update";
    try
    {
        if (participantChangeJson.has_field(RPC_PARAMS) && participantChangeJson.at(RPC_PARAMS).has_field(RPC_PARAM_PARTICIPANTS))
        {
            web::json::array currParticipants = participantChangeJson.at(RPC_PARAMS).at(RPC_PARAM_PARTICIPANTS).as_array();
            for (auto iter = currParticipants.begin(); iter != currParticipants.end(); ++iter)
            {
                uint32_t participantBeamId = m_participantToBeamId[(*iter)[RPC_SESSION_ID].as_string()];
                std::shared_ptr<beam_participant> participantToUpdate = m_participants[participantBeamId];
                participantToUpdate->m_impl->update(*iter, false);
            }
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to process participant update RPC call";
    }
}

void beam_manager_impl::process_on_ready_changed(const web::json::value& onReadyMethod)
{
    LOGS_INFO << "Received an interactivity state change from service";
    try
    {
        if (onReadyMethod.has_field(RPC_PARAMS) && onReadyMethod.at(RPC_PARAMS).has_field(RPC_PARAM_IS_READY))
        {
            std::lock_guard<std::recursive_mutex> lock(m_lock);

            if (onReadyMethod.at(RPC_PARAMS).at(RPC_PARAM_IS_READY).as_bool() == true)
            {
                set_interactivity_state(beam_interactivity_state::interactivity_enabled);
            }
            else
            {
                set_interactivity_state(beam_interactivity_state::interactivity_disabled);
            }
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to process participant state change RPC call";
    }
}


void beam_manager_impl::process_input(const web::json::value& inputMethod)
{
    LOGS_INFO << "Received an input message";

    if (beam_interactivity_state::interactivity_enabled != m_interactivityState)
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


void beam_manager_impl::process_button_input(const web::json::value& inputJson)
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

                string_t controId = buttonInputJson[RPC_CONTROL_ID].as_string();
                update_button_state(controId, buttonInputJson);

                // send event out to title
                std::shared_ptr<beam_participant> currParticipant;

                if (inputJson.at(RPC_PARAMS).has_field(RPC_PARTICIPANT_ID))
                {
                    uint32_t beamId = m_participantToBeamId[inputJson.at(RPC_PARAMS).at(RPC_PARTICIPANT_ID).as_string()];
                    currParticipant = m_participants[beamId];
                }
                else
                {
                    LOGS_ERROR << L"Received button input without a participant session id";
                }

                bool isPressed = ((0 == buttonInputJson[RPC_PARAM_INPUT_EVENT].as_string().compare(RPC_INPUT_EVENT_BUTTON_DOWN)) ? true : false);
                
                std::shared_ptr<beam_button_event_args> args = std::shared_ptr<beam_button_event_args>(new beam_button_event_args(controId, currParticipant, isPressed));

                queue_beam_event_for_client(L"", std::error_code(0, std::generic_category()), beam_event_type::button, args);
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


void beam_manager_impl::process_joystick_input(const web::json::value& joystickInputJson)
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
                    uint32_t beamId = m_participantToBeamId[participantSessionId];

                    if (0 == beamId)
                    {
                        LOGS_INFO << "Unexpected input from participant id " << participantSessionId;
                        return;
                    }

                    std::shared_ptr<beam_participant> participant = m_participants[beamId];
                    if (nullptr == participant)
                    {
                        LOGS_INFO << "No participant record for beam id " << beamId;
                        return;
                    }

                    std::shared_ptr<beam_joystick_event_args> args = std::shared_ptr<beam_joystick_event_args>(new beam_joystick_event_args(participant, x, y, controlId));

                    queue_beam_event_for_client(L"", std::error_code(0, std::generic_category()), beam_event_type::joystick, args);

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

void beam_manager_impl::send_message(std::shared_ptr<beam_rpc_message> rpcMessage)
{
    if (beam_interactivity_connection_state::connected != m_connectionState)
    {
        LOGS_INFO << L"Failed to send message (" << rpcMessage->id() << "), not connected to beam interactivity service";
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
                // Throws this exception on failure to send, our retry logic once the websocket comes back online will resend
                LOGS_ERROR << "Failed to send on websocket.";
            }
        });
    }
    catch (std::exception e)
    {
        remove_awaiting_reply(rpcMessage->id());
        LOGS_ERROR << "Failed to serialize and send beam rpc message";
    }
}


std::shared_ptr<beam_rpc_message>
beam_manager_impl::remove_awaiting_reply(uint32_t messageId)
{
    std::lock_guard<std::recursive_mutex> lock(m_messagesLock);
    std::shared_ptr<beam_rpc_message> message;

    auto msgToRemove = std::find_if(m_awaitingReply.begin(), m_awaitingReply.end(), [&](const std::shared_ptr<beam_rpc_message> & msg) {
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
beam_manager_impl::add_group_to_map(std::shared_ptr<beam_group_impl> group)
{
    auto checkForGroup = m_groupsInternal[group->m_groupId];
    if (nullptr == checkForGroup)
    {
        m_groupsInternal[group->m_groupId] = group;

        std::vector<std::shared_ptr<beam_group_impl>> groupToUpdate;
        groupToUpdate.push_back(group);

        send_create_groups(groupToUpdate);
    }
}

void
beam_manager_impl::add_control_to_map(std::shared_ptr<beam_control> control)
{
    std::lock_guard<std::recursive_mutex> lock(m_lock);
    m_controls[control->control_id()] = control;

    if (control->control_type() == beam_control_type::joystick)
    {
        std::shared_ptr<beam_joystick_control> joystick = std::static_pointer_cast <beam_joystick_control>(control);
        m_joysticks[joystick->control_id()] = joystick;

        LOGS_DEBUG << "Adding joystick to map (" << joystick->control_id() << ")";
    }
    else if (control->control_type() == beam_control_type::button)
    {
        std::shared_ptr<beam_button_control> button = std::static_pointer_cast <beam_button_control>(control);
        m_buttons[button->control_id()] = button;

        LOGS_DEBUG << "Adding button to map (" << button->control_id() << ")";
    }
    return;
}


void
beam_manager_impl::update_button_state(string_t buttonId, web::json::value buttonInputParamsJson)
{
    // TODO: lock
    std::shared_ptr<beam_button_control> button = m_buttons[buttonId];

    if (nullptr == button)
    {
        LOGS_INFO << L"Button not in map: " << buttonId;
    }
    else if (buttonInputParamsJson.has_field(RPC_PARAMS) && (buttonInputParamsJson[RPC_PARAMS].has_field(RPC_PARTICIPANT_ID)))
    {
        string_t participantSessionId = buttonInputParamsJson[RPC_PARAMS][RPC_PARTICIPANT_ID].as_string();

        uint32_t beamId = m_participantToBeamId[participantSessionId];

        if (0 == beamId)
        {
            LOGS_INFO << "Unexpected input from participant id " << participantSessionId;
            return;
        }

        std::shared_ptr<beam_participant> currParticipant = m_participants[beamId];
        if (nullptr == currParticipant)
        {
            LOGS_INFO << "Unexpected input from participant " << currParticipant->beam_username();
            return;
        }

        if (currParticipant->input_disabled())
        {
            LOGS_INFO << "Ignoring input from disabled participant " << currParticipant->beam_username();
            return;
        }

        std::shared_ptr<beam_button_state> newState = std::shared_ptr<beam_button_state>(new beam_button_state());
        std::shared_ptr<beam_button_state> oldStateByParticipant = button->m_buttonStateByBeamId[beamId];
        if (nullptr == oldStateByParticipant)
        {
            // Create initial state entry for this participant
            button->m_buttonStateByBeamId[beamId] = newState;
            oldStateByParticipant = button->m_buttonStateByBeamId[beamId];
        }

        bool wasPressed = oldStateByParticipant->is_pressed();
        bool isPressed = (0 == buttonInputParamsJson[RPC_PARAMS][RPC_PARAM_INPUT][RPC_PARAM_INPUT_EVENT].as_string().compare(RPC_INPUT_EVENT_BUTTON_DOWN));

        if (!wasPressed &&
            isPressed)
        {
            newState->m_isDown = true;
            newState->m_isPressed = false;
            newState->m_isUp = false;
        }
        else if (wasPressed &&
            !isPressed)
        {
            newState->m_isDown = false;
            newState->m_isPressed = false;
            newState->m_isUp = true;
        }
        else if (wasPressed &&
            isPressed)
        {
            newState->m_isDown = true;
            newState->m_isPressed = true;
            newState->m_isUp = false;
        }
        else if (!wasPressed &&
            !isPressed)
        {
            newState->m_isDown = false;
            newState->m_isPressed = false;
            newState->m_isUp = true;
        }

        button->m_buttonStateByBeamId[beamId] = newState;

        if (newState->is_down())
        {
            button->m_buttonCount->m_buttonDowns++;
        }
        else if (newState->is_pressed())
        {
            button->m_buttonCount->m_buttonPresses++;
        }
        else if (newState->is_up())
        {
            button->m_buttonCount->m_buttonUps++;
        }
    }
}


string_t
beam_manager_impl::find_or_create_participant_session_id(_In_ uint32_t beamId)
{
    string_t participantSessionId;
    static string_t baseMockParticipantSessionId = L"mockParticipantId";
    static uint32_t nextParticipantSessionId = 0;
    
    for (auto iter = m_participantToBeamId.begin(); iter != m_participantToBeamId.end(); iter++)
    {
        if (beamId == iter->second)
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
beam_manager_impl::mock_button_event(_In_ uint32_t beamId, _In_ string_t buttonId, _In_ bool is_down)
{
    string_t participantSessionId = find_or_create_participant_session_id(beamId);
    web::json::value mockInputJson = build_mock_button_input(get_next_message_id(), participantSessionId, buttonId, is_down);

    on_socket_message_received(mockInputJson.serialize());
}

#if 0
void
beam_manager_impl::mock_participant_join(uint32_t beamId, string_t beamUsername)
{
    uint32_t messageId = get_next_message_id();
    string_t participantSessionId = find_or_create_participant_session_id(beamId);
    web::json::value mockParticipantJoinJson = build_participant_state_change_mock_data(messageId, beamId, beamUsername, participantSessionId, true /*isJoin*/, false /*discard*/);
    auto mockParticipantJoinMethod = build_mediator_rpc_message(messageId, RPC_METHOD_PARTICIPANTS_JOINED, mockParticipantJoinJson);

    process_method(mockParticipantJoinMethod);
}

void
beam_manager_impl::mock_participant_leave(uint32_t beamId, string_t beamUsername)
{
    uint32_t messageId = get_next_message_id();
    string_t participantSessionId = find_or_create_participant_session_id(beamId);
    web::json::value mockParticipantLeaveJson = build_participant_state_change_mock_data(messageId, beamId, beamUsername, participantSessionId, false /*isJoin*/, false /*discard*/);
    auto mockParticipantLeaveMethod = build_mediator_rpc_message(messageId, RPC_METHOD_PARTICIPANTS_LEFT, mockParticipantLeaveJson);

    process_method(mockParticipantLeaveMethod);
}
#endif

void
beam_manager_impl::queue_beam_event_for_client(string_t errorMessage, std::error_code errorCode, beam_event_type type, std::shared_ptr<beam_event_args> args)
{
    std::lock_guard<std::recursive_mutex> lock(m_lock);
    m_eventsForClient.push_back(*(create_beam_event(errorMessage, errorCode, type, args)));
    return;
}

void xbox::services::beam::beam_manager_impl::queue_message_for_service(std::shared_ptr<beam_rpc_message> message)
{
    m_pendingSend.push(message);
}

NAMESPACE_MICROSOFT_XBOX_BEAM_END
