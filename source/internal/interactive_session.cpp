#include "interactive_session.h"
#include "common.h"
#include "interactive_event.h"
#include <functional>

namespace mixer_internal
{

typedef std::function<void(rapidjson::Document::AllocatorType& allocator, rapidjson::Value& value)> on_get_params;

int create_method_json(interactive_session_internal& session, const std::string& method, on_get_params getParams, bool discard, unsigned int* id, std::shared_ptr<rapidjson::Document>& methodDoc)
{
	std::shared_ptr<rapidjson::Document> doc(std::make_shared<rapidjson::Document>());
	doc->SetObject();
	rapidjson::Document::AllocatorType& allocator = doc->GetAllocator();

	unsigned int packetID = session.packetId++;
	doc->AddMember(RPC_ID, packetID, allocator);
	doc->AddMember(RPC_METHOD, method, allocator);
	doc->AddMember(RPC_DISCARD, discard, allocator);
	doc->AddMember(RPC_SEQUENCE, session.sequenceId, allocator);

	// Get the parameters from the caller.
	rapidjson::Value params(rapidjson::kObjectType);
	if (getParams)
	{
		getParams(allocator, params);
	}
	doc->AddMember(RPC_PARAMS, params, allocator);

	if (nullptr != id)
	{
		*id = packetID;
	}
	methodDoc = doc;
	return MIXER_OK;
}

// Queue a method to be sent out on the websocket. If handleImmediately is set to true, the handler will be called by the websocket receive thread rather than put on the reply queue.
int queue_method(interactive_session_internal& session, const std::string& method, on_get_params getParams, method_handler onReply, const bool handleImmediately)
{
	std::shared_ptr<rapidjson::Document> methodDoc;
	unsigned int packetId = 0;
	RETURN_IF_FAILED(create_method_json(session, method, getParams, nullptr == onReply, &packetId, methodDoc));
	DEBUG_TRACE(std::string("Queueing method: ") + jsonStringify(*methodDoc));
	if (onReply)
	{
		std::unique_lock<std::mutex> incomingLock(session.incomingMutex);
		session.replyHandlersById[packetId] = std::pair<bool, method_handler>(handleImmediately, onReply);
	}

	// Synchronize write access to the queue.
	std::shared_ptr<rpc_method_event> methodEvent = std::make_shared<rpc_method_event>(std::move(methodDoc));
	std::unique_lock<std::mutex> queueLock(session.outgoingMutex);
	session.outgoingEvents.emplace(methodEvent);
	session.outgoingCV.notify_one();

	return MIXER_OK;
}

int queue_request(interactive_session_internal& session, const std::string uri, const std::string& verb, const http_headers* headers, const std::string* body, http_response_handler onResponse)
{
	std::shared_ptr<http_request_event> requestEvent = std::make_shared<http_request_event>(session.packetId++, uri, verb, headers, body);

	if (nullptr != onResponse)
	{
		session.httpResponseHandlers[requestEvent->packetId] = onResponse;
	}

	// Queue the request, synchronizing access.
	{
		std::unique_lock<std::mutex> outgoingLock(session.outgoingMutex);
		session.outgoingEvents.emplace(requestEvent);
		session.outgoingCV.notify_one();
	}

	return MIXER_OK;
}

int check_reply_errors(interactive_session_internal& session, rapidjson::Document& reply)
{
	if (reply.HasMember(RPC_SEQUENCE) && !reply[RPC_SEQUENCE].IsNull())
	{
		session.sequenceId = reply[RPC_SEQUENCE].GetInt();
	}

	if (reply.HasMember(RPC_ERROR) && !reply[RPC_ERROR].IsNull())
	{
		int errCode = reply[RPC_ERROR][RPC_ERROR_CODE].GetInt();
		if (session.onError)
		{
			std::string errMessage = reply[RPC_ERROR][RPC_ERROR_MESSAGE].GetString();
			session.onError(session.callerContext, &session, errCode, errMessage.c_str(), errMessage.length());
		}

		return errCode;
	}

	return MIXER_OK;
}

int send_ready_message(interactive_session_internal& session, bool ready = true)
{
	return queue_method(session, RPC_METHOD_READY, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		params.AddMember(RPC_PARAM_IS_READY, ready, allocator);
	}, nullptr);
}

int bootstrap(interactive_session_internal& session);

int update_server_time_offset(interactive_session_internal& session)
{
	// Calculate the server time offset.
	DEBUG_INFO("Requesting server time to calculate client offset.");
	
	int err = queue_method(session, RPC_METHOD_GET_TIME, nullptr, [](interactive_session_internal& session, rapidjson::Document& doc) -> int
	{
		// Note: This reply handler is executed immediately by the background websocket thread.
		// Take care not to call callbacks that the user may expect on their own thread, the debug callback being the only exception.
		if (!doc.HasMember(RPC_RESULT) || !doc[RPC_RESULT].HasMember(RPC_TIME))
		{
			DEBUG_ERROR("Unexpected reply format for server time reply");
			return MIXER_ERROR_UNRECOGNIZED_DATA_FORMAT;
		}

		auto receivedTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now());
		auto latency = (receivedTime - session.getTimeSent) / 2;
		unsigned long long serverTime = doc[RPC_RESULT][RPC_TIME].GetUint64();
		auto offset = receivedTime.time_since_epoch() - latency - std::chrono::milliseconds(serverTime);
		session.serverTimeOffsetMs = offset.count();
		DEBUG_INFO("Server time offset: " + std::to_string(session.serverTimeOffsetMs));

		// Continue bootstrapping the session.
		session.serverTimeOffsetCalculated = true;
		return bootstrap(session);;
	}, true);
	if (err)
	{
		DEBUG_ERROR("Method "  RPC_METHOD_GET_TIME " failed: " + std::to_string(err));
		return err;
	}

	session.getTimeSent = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now());

	return MIXER_OK;
}

/*
Bootstrapping Interactive

A few things must happen before the interactive connection is ready for the client to use.

- First, the server time offset needs to be calculated in order to properly synchronize the client's clock for setting control cooldowns.
- Second, in order to give the caller information about the interactive world, that data must be fetched from the server and cached.

Once this information is requested, the bootstraping state is checked on each reply. If all items are complete the caller is informed via a state change event and the client is ready.
*/
int bootstrap(interactive_session_internal& session)
{
	DEBUG_TRACE("Checking bootstrap state.");
	assert(interactive_connecting == session.state);

	if (!session.serverTimeOffsetCalculated)
	{
		RETURN_IF_FAILED(update_server_time_offset(session));
	}
	else if (!session.scenesCached)
	{
		RETURN_IF_FAILED(cache_scenes(session));
	}
	else if (!session.groupsCached)
	{
		RETURN_IF_FAILED(cache_groups(session));
	}
	else
	{
		DEBUG_TRACE("Bootstrapping complete.");
		interactive_state prevState = session.state;
		session.state = interactive_connected;

		if (session.onStateChanged)
		{
			session.onStateChanged(session.callerContext, &session, prevState, session.state);
		}

		if (session.isReady)
		{
			return send_ready_message(session);
		}
	}

	return MIXER_OK;
}

int handle_hello(interactive_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	if (session.shutdownRequested)
	{
		return MIXER_OK;
	}

	return bootstrap(session);
}

int handle_input(interactive_session_internal& session, rapidjson::Document& doc)
{
	if (!session.onInput)
	{
		// No input handler, return.
		return MIXER_OK;
	}

	interactive_input inputData;
	memset(&inputData, 0, sizeof(inputData));
	std::string inputJson = jsonStringify(doc[RPC_PARAMS]);
	inputData.jsonData = inputJson.c_str();
	inputData.jsonDataLength = inputJson.length();
	rapidjson::Value& input = doc[RPC_PARAMS][RPC_PARAM_INPUT];
	inputData.control.id = input[RPC_CONTROL_ID].GetString();
	inputData.control.idLength = input[RPC_CONTROL_ID].GetStringLength();

	if (doc[RPC_PARAMS].HasMember(RPC_PARTICIPANT_ID))
	{
		inputData.participantId = doc[RPC_PARAMS][RPC_PARTICIPANT_ID].GetString();
		inputData.participantIdLength = doc[RPC_PARAMS][RPC_PARTICIPANT_ID].GetStringLength();
	}

	// Locate the cached control data.
	auto itr = session.controls.find(inputData.control.id);
	if (itr == session.controls.end())
	{
		int errCode = MIXER_ERROR_OBJECT_NOT_FOUND;
		if (session.onError)
		{
			std::string errMessage = "Input received for unknown control.";
			session.onError(session.callerContext, &session, errCode, errMessage.c_str(), errMessage.length());
		}

		return errCode;
	}

	rapidjson::Value* control = rapidjson::Pointer(itr->second.cachePointer.c_str()).Get(session.scenesRoot);
	if (nullptr == control)
	{
		int errCode = MIXER_ERROR_OBJECT_NOT_FOUND;
		if (session.onError)
		{
			std::string errMessage = "Internal failure: Failed to find control in cached json data.";
			session.onError(session.callerContext, &session, errCode, errMessage.c_str(), errMessage.length());
		}

		return errCode;
	}

	inputData.control.kind = control->GetObject()[RPC_CONTROL_KIND].GetString();
	inputData.control.kindLength = control->GetObject()[RPC_CONTROL_KIND].GetStringLength();
	if (doc[RPC_PARAMS].HasMember(RPC_PARAM_TRANSACTION_ID))
	{
		inputData.transactionId = doc[RPC_PARAMS][RPC_PARAM_TRANSACTION_ID].GetString();
		inputData.transactionIdLength = doc[RPC_PARAMS][RPC_PARAM_TRANSACTION_ID].GetStringLength();
	}

	std::string inputEvent = input[RPC_PARAM_INPUT_EVENT].GetString();
	if (0 == inputEvent.compare(RPC_INPUT_EVENT_MOVE))
	{
		inputData.type = input_type_move;
		inputData.coordinateData.x = input[RPC_INPUT_EVENT_MOVE_X].GetFloat();
		inputData.coordinateData.y = input[RPC_INPUT_EVENT_MOVE_Y].GetFloat();
	}
	else if (0 == inputEvent.compare(RPC_INPUT_EVENT_KEY_DOWN) ||
		0 == inputEvent.compare(RPC_INPUT_EVENT_KEY_UP))
	{
		inputData.type = input_type_key;
		interactive_button_action action = interactive_button_action_up;
		if (0 == inputEvent.compare(RPC_INPUT_EVENT_KEY_DOWN))
		{
			action = interactive_button_action_down;
		}

		inputData.buttonData.action = action;
	}
	else if (0 == inputEvent.compare(RPC_INPUT_EVENT_MOUSE_DOWN) ||
		0 == inputEvent.compare(RPC_INPUT_EVENT_MOUSE_UP))
	{
		inputData.type = input_type_click;
		interactive_button_action action = interactive_button_action_up;
		if (0 == inputEvent.compare(RPC_INPUT_EVENT_MOUSE_DOWN))
		{
			action = interactive_button_action_down;
		}

		inputData.buttonData.action = action;

		if (input.HasMember(RPC_INPUT_EVENT_MOVE_X))
		{
			inputData.coordinateData.x = input[RPC_INPUT_EVENT_MOVE_X].GetFloat();
		}
		if (input.HasMember(RPC_INPUT_EVENT_MOVE_Y))
		{
			inputData.coordinateData.y = input[RPC_INPUT_EVENT_MOVE_Y].GetFloat();
		}
	}
	else
	{
		inputData.type = input_type_custom;
	}

	session.onInput(session.callerContext, &session, &inputData);

	return MIXER_OK;
}

int handle_participants_change(interactive_session_internal& session, rapidjson::Document& doc, interactive_participant_action action)
{
	if (!doc.HasMember(RPC_PARAMS) || !doc[RPC_PARAMS].HasMember(RPC_PARAM_PARTICIPANTS))
	{
		return MIXER_ERROR_UNRECOGNIZED_DATA_FORMAT;
	}

	rapidjson::Value& participants = doc[RPC_PARAMS][RPC_PARAM_PARTICIPANTS];
	for (auto itr = participants.Begin(); itr != participants.End(); ++itr)
	{
		interactive_participant participant;
		parse_participant(*itr, participant);

		switch (action)
		{
		case participant_join:
		case participant_update:
		{
			std::shared_ptr<rapidjson::Document> participantDoc(std::make_shared<rapidjson::Document>());
			participantDoc->CopyFrom(*itr, participantDoc->GetAllocator());
			session.participants[participant.id] = participantDoc;
			break;
		}
		case participant_leave:
		default:
		{
			session.participants.erase(participant.id);
			break;
		}
		}

		if (session.onParticipantsChanged)
		{
			session.onParticipantsChanged(session.callerContext, &session, action, &participant);
		}
	}

	return MIXER_OK;
}

int handle_participants_join(interactive_session_internal& session, rapidjson::Document& doc)
{
	return handle_participants_change(session, doc, participant_join);
}

int handle_participants_leave(interactive_session_internal& session, rapidjson::Document& doc)
{
	return handle_participants_change(session, doc, participant_leave);
}

int handle_participants_update(interactive_session_internal& session, rapidjson::Document& doc)
{
	return handle_participants_change(session, doc, participant_update);
}

int handle_ready(interactive_session_internal& session, rapidjson::Document& doc)
{
	if (!doc.HasMember(RPC_PARAMS) || !doc[RPC_PARAMS].HasMember(RPC_PARAM_IS_READY))
	{
		return MIXER_ERROR_UNRECOGNIZED_DATA_FORMAT;
	}

	bool isReady = doc[RPC_PARAMS][RPC_PARAM_IS_READY].GetBool();
	// Only change state and notify if the ready state is different.
	if (isReady && interactive_ready != session.state || !isReady && interactive_connected != session.state)
	{
		interactive_state previousState = session.state;
		session.state = isReady ? interactive_ready : interactive_connected;
		if (session.onStateChanged)
		{
			session.onStateChanged(session.callerContext, &session, previousState, session.state);
		}
	}

	return MIXER_OK;
}

int handle_group_changed(interactive_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	return cache_groups(session);
}

int handle_scene_changed(interactive_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	return cache_scenes(session);
}

int handle_control_changed(interactive_session_internal& session, rapidjson::Document& doc)
{
	if (!doc.HasMember(RPC_PARAMS)
		|| !doc[RPC_PARAMS].HasMember(RPC_PARAM_CONTROLS) || !doc[RPC_PARAMS][RPC_PARAM_CONTROLS].IsArray()
		|| !doc[RPC_PARAMS].HasMember(RPC_SCENE_ID) || !doc[RPC_PARAMS][RPC_SCENE_ID].IsString())
	{
		return MIXER_ERROR_UNRECOGNIZED_DATA_FORMAT;
	}

	const char * sceneId = doc[RPC_PARAMS][RPC_SCENE_ID].GetString();
	rapidjson::Value& controls = doc[RPC_PARAMS][RPC_PARAM_CONTROLS];
	for (auto itr = controls.Begin(); itr != controls.End(); ++itr)
	{
		interactive_control control;
		memset(&control, 0, sizeof(interactive_control));
		parse_control(*itr, control);

		interactive_control_event eventType = interactive_control_updated;
		if (0 == strcmp(doc[RPC_METHOD].GetString(), RPC_METHOD_ON_CONTROL_UPDATE))
		{
			RETURN_IF_FAILED(update_cached_control(session, control, *itr));
		}
		else if (0 == strcmp(doc[RPC_METHOD].GetString(), RPC_METHOD_ON_CONTROL_CREATE))
		{
			eventType = interactive_control_created;
			RETURN_IF_FAILED(cache_new_control(session, sceneId, control, *itr));
		}
		else if (0 == strcmp(doc[RPC_METHOD].GetString(), RPC_METHOD_ON_CONTROL_DELETE))
		{
			eventType = interactive_control_deleted;
			RETURN_IF_FAILED(delete_cached_control(session, sceneId, control));
		}
		else
		{
			return MIXER_ERROR_UNKNOWN_METHOD;
		}

		if (session.onControlChanged)
		{
			session.onControlChanged(session.callerContext, &session, eventType, &control);
		}
	}

	return MIXER_OK;
}

int route_method(interactive_session_internal& session, rapidjson::Document& doc)
{
	std::string method = doc[RPC_METHOD].GetString();
	auto itr = session.methodHandlers.find(method);
	if (itr != session.methodHandlers.end())
	{
		return itr->second(session, doc);
	}
	else
	{
		DEBUG_WARNING("Unhandled method type: " + method);
		if (session.onUnhandledMethod)
		{
			std::string methodJson = jsonStringify(doc);
			session.onUnhandledMethod(session.callerContext, &session, methodJson.c_str(), methodJson.length());
		}
	}

	return MIXER_OK;
}

void register_method_handlers(interactive_session_internal& session)
{
	session.methodHandlers.emplace(RPC_METHOD_HELLO, handle_hello);
	session.methodHandlers.emplace(RPC_METHOD_ON_READY_CHANGED, handle_ready);
	session.methodHandlers.emplace(RPC_METHOD_ON_INPUT, handle_input);
	session.methodHandlers.emplace(RPC_METHOD_ON_PARTICIPANT_JOIN, handle_participants_join);
	session.methodHandlers.emplace(RPC_METHOD_ON_PARTICIPANT_LEAVE, handle_participants_leave);
	session.methodHandlers.emplace(RPC_METHOD_ON_PARTICIPANT_UPDATE, handle_participants_update);
	session.methodHandlers.emplace(RPC_METHOD_ON_GROUP_UPDATE, handle_group_changed);
	session.methodHandlers.emplace(RPC_METHOD_ON_GROUP_CREATE, handle_group_changed);
	session.methodHandlers.emplace(RPC_METHOD_ON_CONTROL_UPDATE, handle_control_changed);
	session.methodHandlers.emplace(RPC_METHOD_UPDATE_SCENES, handle_scene_changed);
}

}

using namespace mixer_internal;

int interactive_open_session(interactive_session* sessionPtr)
{
	if (nullptr == sessionPtr)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	auto session = std::make_unique< interactive_session_internal>();

	// Register method handlers
	register_method_handlers(*session);

	// Initialize Http and Websocket clients
	session->http = http_factory::make_http_client();
	session->ws = websocket_factory::make_websocket();

	*sessionPtr = session.release();
	return MIXER_OK;
}

int interactive_connect(interactive_session session, const char* auth, const char* versionId, const char* shareCode, bool setReady)
{
	if (nullptr == auth || nullptr == versionId)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	// Validate parameters
	if (0 == strlen(auth) || 0 == strlen(versionId))
	{
		return MIXER_ERROR_INVALID_VERSION_ID;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	if (interactive_disconnected != sessionInternal->state)
	{
		return MIXER_ERROR_INVALID_STATE;
	}

	sessionInternal->isReady = setReady;
	sessionInternal->authorization = auth;
	sessionInternal->versionId = versionId;
	sessionInternal->shareCode = shareCode;
	
	sessionInternal->state = interactive_connecting;
	if (sessionInternal->onStateChanged)
	{
		sessionInternal->onStateChanged(sessionInternal->callerContext, sessionInternal, interactive_disconnected, sessionInternal->state);
		if (sessionInternal->shutdownRequested)
		{
			return MIXER_ERROR_CANCELLED;
		}
	}

	// Create thread to open websocket and receive messages.
	sessionInternal->incomingThread = std::thread(std::bind(&interactive_session_internal::run_incoming_thread, sessionInternal));

	// Create thread to send messages over the open websocket.
	sessionInternal->outgoingThread = std::thread(std::bind(&interactive_session_internal::run_outgoing_thread, sessionInternal));

	return MIXER_OK;
}

int interactive_set_session_context(interactive_session session, void* context)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->callerContext = context;

	return MIXER_OK;
}

int interactive_get_session_context(interactive_session session, void** context)
{
	if (nullptr == session || nullptr == context)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	*context = sessionInternal->callerContext;
	return MIXER_OK;
}

int interactive_set_bandwidth_throttle(interactive_session session, interactive_throttle_type throttleType, unsigned int maxBytes, unsigned int bytesPerSecond)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	if (interactive_connected > sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	std::string throttleMethod;
	switch (throttleType)
	{
	case throttle_input:
		throttleMethod = RPC_METHOD_ON_INPUT;
		break;
	case throttle_participant_join:
		throttleMethod = RPC_METHOD_ON_PARTICIPANT_JOIN;
		break;
	case throttle_participant_leave:
		throttleMethod = RPC_METHOD_ON_PARTICIPANT_LEAVE;
		break;
	case throttle_global:
	default:
		throttleMethod = "*";
		break;
	}

	RETURN_IF_FAILED(queue_method(*sessionInternal, RPC_METHOD_SET_THROTTLE, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		rapidjson::Value method(rapidjson::kObjectType);
		method.SetObject();
		method.AddMember(RPC_PARAM_CAPACITY, maxBytes, allocator);
		method.AddMember(RPC_PARAM_DRAIN_RATE, bytesPerSecond, allocator);
		rapidjson::Value throttleVal(rapidjson::kStringType);
		throttleVal.SetString(throttleMethod, allocator);
		params.AddMember(throttleVal, method, allocator);
	}, check_reply_errors));

	return MIXER_OK;
}

int interactive_run(interactive_session session, unsigned int maxEventsToProcess)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	if (0 == maxEventsToProcess)
	{
		return MIXER_OK;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	if (sessionInternal->shutdownRequested)
	{
		return MIXER_ERROR_CANCELLED;
	}

	// Create a local queue and populate it with the top elements from the incoming event queue to minimize locking time.
	interactive_event_queue processingQueue;
	{
		std::lock_guard<std::mutex> incomingLock(sessionInternal->incomingMutex);
		for (unsigned int i = 0; i < maxEventsToProcess && i < sessionInternal->incomingEvents.size(); ++i)
		{
			processingQueue.emplace(std::move(sessionInternal->incomingEvents.top()));
			sessionInternal->incomingEvents.pop();
		}
	}

	// Process all the events in the local queue.
	while (!processingQueue.empty())
	{
		auto ev = processingQueue.top();
		switch (ev->type)
		{
		case interactive_event_type_error:
		{
			auto errorEvent = reinterpret_cast<std::shared_ptr<error_event>&>(ev);
			if (sessionInternal->onError)
			{
				sessionInternal->onError(sessionInternal->callerContext, sessionInternal, errorEvent->error.first, errorEvent->error.second.c_str(), errorEvent->error.second.length());
				if (sessionInternal->shutdownRequested)
				{
					return MIXER_OK;
				}
			}
			break;
		}
		case interactive_event_type_state_change:
		{
			auto stateChangeEvent = reinterpret_cast<std::shared_ptr<state_change_event>&>(ev);
			interactive_state previousState = sessionInternal->state;
			sessionInternal->state = stateChangeEvent->currentState;
			if (sessionInternal->onStateChanged)
			{
				sessionInternal->onStateChanged(sessionInternal->callerContext, sessionInternal, previousState, sessionInternal->state);
			}
			break;
		}
		case interactive_event_type_rpc_reply:
		{
			auto replyEvent = reinterpret_cast<std::shared_ptr<rpc_reply_event>&>(ev);
			replyEvent->replyHandler(*sessionInternal, *replyEvent->replyJson);
			break;
		}
		case interactive_event_type_http_response:
		{
			auto httpResponseEvent = reinterpret_cast<std::shared_ptr<http_response_event>&>(ev);
			httpResponseEvent->responseHandler(httpResponseEvent->response);
			break;
		}
		case interactive_event_type_rpc_method:
		{
			auto rpcMethodEvent = reinterpret_cast<std::shared_ptr<rpc_method_event>&>(ev);
			if (rpcMethodEvent->methodJson->HasMember(RPC_SEQUENCE))
			{
				sessionInternal->sequenceId = (*rpcMethodEvent->methodJson)[RPC_SEQUENCE].GetInt();
			}

			RETURN_IF_FAILED(route_method(*sessionInternal, *rpcMethodEvent->methodJson));
			break;
		}
		default:
			break;
		}

		processingQueue.pop();
		if (sessionInternal->shutdownRequested)
		{
			return MIXER_OK;
		}
	}

	return MIXER_OK;
}

int interactive_get_state(interactive_session session, interactive_state* state)
{
	if (nullptr == session || nullptr == state)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	*state = sessionInternal->state;

	return MIXER_OK;
}

int interactive_get_user(interactive_session session, on_interactive_user onUser)
{
	if (nullptr == session || nullptr == onUser)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	if (sessionInternal->authorization.empty())
	{
		DEBUG_ERROR("Broadcasting check attempted without an authorization string set.");
		return MIXER_ERROR_AUTH;
	}

	std::string currentUserUrl = "https://mixer.com/api/v1/users/current";

	http_headers headers;
	headers["Authorization"] = sessionInternal->authorization;
	http_response response;
	memset(&response, 0, sizeof(http_response));
	int httpErr = sessionInternal->http->make_request(currentUserUrl, "GET", &headers, "", response);
	if (0 != httpErr)
	{
		DEBUG_ERROR(std::to_string(httpErr) + " Failed to GET /api/v1/users/current");
		return MIXER_ERROR_HTTP;
	}

	rapidjson::Document responseDoc;
	responseDoc.Parse(response.body);
	if (responseDoc.HasParseError())
	{
		return MIXER_ERROR_JSON_PARSE;
	}

	// Validate the response.
	if (!responseDoc.HasMember("id") || !responseDoc.HasMember("username") ||
		!responseDoc.HasMember("level") || !responseDoc.HasMember("experience") ||
		!responseDoc.HasMember("sparks") || !responseDoc.HasMember("avatarUrl") ||
		!responseDoc.HasMember("channel") || !responseDoc["channel"].IsObject() ||
		!responseDoc["channel"].HasMember("online"))
	{
		return MIXER_ERROR_UNRECOGNIZED_DATA_FORMAT;
	}

	interactive_user user;
	memset(&user, 0, sizeof(interactive_user));
	if (responseDoc.HasMember("avatarUrl") && responseDoc["avatarUrl"].IsString())
	{
		user.avatarUrl = responseDoc["avatarUrl"].GetString();
	}
	if (responseDoc.HasMember("experience") && responseDoc["experience"].IsUint())
	{
		user.experience = responseDoc["experience"].GetUint();
	}
	if (responseDoc.HasMember("id") && responseDoc["id"].IsUint())
	{
		user.id = responseDoc["id"].GetUint();
	}
	if (responseDoc.HasMember("channel") && responseDoc["channel"].IsObject() &&
		responseDoc["channel"].HasMember("online") && responseDoc["channel"]["online"].IsBool())
	{
		user.isBroadcasting = responseDoc["channel"]["online"].GetBool();
	}
	if (responseDoc.HasMember("level") && responseDoc["level"].IsUint())
	{
		user.level = responseDoc["level"].GetUint();
	}
	if (responseDoc.HasMember("sparks") && responseDoc["sparks"].IsUint())
	{
		user.sparks = responseDoc["sparks"].GetUint();
	}
	if (responseDoc.HasMember("username") && responseDoc["username"].IsString())
	{
		user.userName = responseDoc["username"].GetString();
	}

	onUser(sessionInternal->callerContext, session, &user);
	return MIXER_OK;
}

int interactive_set_ready(interactive_session session, bool isReady)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	if (interactive_connected > sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	return send_ready_message(*sessionInternal, isReady);
}

int interactive_capture_transaction(interactive_session session, const char* transactionId)
{
	if (nullptr == session || nullptr == transactionId)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	std::string transactionIdStr(transactionId);
	if (transactionIdStr.empty())
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	if (interactive_connected > sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}
	
	RETURN_IF_FAILED(queue_method(*sessionInternal, RPC_METHOD_CAPTURE, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		params.AddMember(RPC_PARAM_TRANSACTION_ID, transactionIdStr, allocator);
	}, [transactionIdStr](interactive_session_internal& session, rapidjson::Document& replyDoc)
	{
		if (session.onTransactionComplete)
		{
			unsigned int err = 0;
			std::string errMessage;
			if (replyDoc.HasMember(RPC_ERROR))
			{
				if (replyDoc[RPC_ERROR].IsObject() && replyDoc[RPC_ERROR].HasMember(RPC_ERROR_CODE))
				{
					err = replyDoc[RPC_ERROR][RPC_ERROR_CODE].GetUint();
				}
				if (replyDoc[RPC_ERROR].IsObject() && replyDoc[RPC_ERROR].HasMember(RPC_ERROR_MESSAGE))
				{
					errMessage = replyDoc[RPC_ERROR][RPC_ERROR_MESSAGE].GetString();
				}
			}

			session.onTransactionComplete(session.callerContext, &session, transactionIdStr.c_str(), transactionIdStr.length(), err, errMessage.c_str(), errMessage.length());
		}

		return MIXER_OK;
	}));

	return MIXER_OK;
}

int interactive_queue_method(interactive_session session, const char* method, const char* paramsJson, on_method_reply onReply)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	if (interactive_connected > sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	rapidjson::Document paramsDoc;
	if (paramsDoc.Parse(paramsJson).HasParseError())
	{
		return MIXER_ERROR_JSON_PARSE;
	}

	method_handler replyHandler;
	if (nullptr != onReply)
	{
		replyHandler = [onReply](interactive_session_internal& session, rapidjson::Document& replyJson)
		{
			std::string replyJsonStr = jsonStringify(replyJson);
			onReply(session.callerContext, &session, replyJsonStr.c_str(), replyJsonStr.length());
			return MIXER_OK;
		};
	}

	RETURN_IF_FAILED(queue_method(*sessionInternal, method, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		params.CopyFrom(paramsDoc, allocator);
	}, replyHandler));

	return MIXER_OK;
}

void interactive_close_session(interactive_session session)
{
	if (nullptr != session)
	{
		interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

		// Mark the session as inactive and close the websocket. This will notify any functions in flight to exit at their earliest convenience.
		sessionInternal->shutdownRequested = true;
		if (nullptr != sessionInternal->ws.get())
		{
			sessionInternal->ws->close();
		}

		// Notify the outgoing websocket thread to shutdown.
		{
			std::unique_lock<std::mutex> outgoingLock(sessionInternal->outgoingMutex);
			sessionInternal->outgoingCV.notify_all();
		}

		// Wait for both threads to terminate.
		if (sessionInternal->incomingThread.joinable())
		{
			sessionInternal->incomingThread.join();
		}
		if (sessionInternal->outgoingThread.joinable())
		{
			sessionInternal->outgoingThread.join();
		}

		// Clean up the session memory.
		delete sessionInternal;
	}
}

// Handler registration
int interactive_set_error_handler(interactive_session session, on_error onError)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onError = onError;

	return MIXER_OK;
}

int interactive_set_state_changed_handler(interactive_session session, on_state_changed onStateChanged)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onStateChanged = onStateChanged;

	return MIXER_OK;
}

int interactive_set_input_handler(interactive_session session, on_input onInput)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onInput = onInput;

	return MIXER_OK;
}

int interactive_set_participants_changed_handler(interactive_session session, on_participants_changed onParticipantsChanged)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onParticipantsChanged = onParticipantsChanged;

	return MIXER_OK;
}

int interactive_set_transaction_complete_handler(interactive_session session, on_transaction_complete onTransactionComplete)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onTransactionComplete = onTransactionComplete;

	return MIXER_OK;
}

int interactive_set_control_changed_handler(interactive_session session, on_control_changed onControlChanged)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onControlChanged = onControlChanged;

	return MIXER_OK;
}

int interactive_set_unhandled_method_handler(interactive_session session, on_unhandled_method onUnhandledMethod)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onUnhandledMethod = onUnhandledMethod;

	return MIXER_OK;
}

// Debugging

void interactive_config_debug_level(const interactive_debug_level dbgLevel)
{
	g_dbgInteractiveLevel = dbgLevel;
}

void interactive_config_debug(const interactive_debug_level dbgLevel, const on_debug_msg dbgCallback)
{
	g_dbgInteractiveLevel = dbgLevel;
	g_dbgInteractiveCallback = dbgCallback;
}
