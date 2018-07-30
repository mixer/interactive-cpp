#include "interactive_session.h"
#include "common.h"

namespace mixer_internal
{

interactive_session_internal::interactive_session_internal()
	: callerContext(nullptr), isReady(false), state(interactive_disconnected), shutdownRequested(false),packetId(0), 
	sequenceId(0), wsOpen(false), onInput(nullptr), onError(nullptr), onStateChanged(nullptr), onParticipantsChanged(nullptr), 
	onUnhandledMethod(nullptr), onControlChanged(nullptr), onTransactionComplete(nullptr), serverTimeOffsetMs(0), serverTimeOffsetCalculated(false), scenesCached(false), groupsCached(false),
	getTimeRequestId(0xffffffff)
{
	scenesRoot.SetObject();
}

interactive_object_internal::interactive_object_internal(std::string id) : id(std::move(id)) {}

void
interactive_session_internal::enqueue_outgoing_event(std::shared_ptr<interactive_event_internal>&& ev)
{
	std::unique_lock<std::mutex> outgoingLock(this->outgoingMutex);
	this->outgoingEvents.emplace(ev);
}

void
interactive_session_internal::enqueue_incoming_event(std::shared_ptr<interactive_event_internal>&& ev)
{
	std::unique_lock<std::mutex> incomingLock(this->incomingMutex);
	this->incomingEvents.emplace(ev);
}

void interactive_session_internal::handle_ws_open(const websocket& socket, const std::string& message)
{
	(socket);
	DEBUG_INFO("Websocket opened: " + message);
	// Notify the outgoing thread.
	this->wsOpen = true;
}

void interactive_session_internal::handle_ws_message(const websocket& socket, const std::string& message)
{
	(socket);
	DEBUG_TRACE("Websocket message received: " + message);
	if (this->shutdownRequested)
	{
		return;
	}

	// Parse the message to determine packet type.
	std::shared_ptr<rapidjson::Document> messageJson = std::make_shared<rapidjson::Document>();
	if (!messageJson->Parse(message.c_str(), message.length()).HasParseError())
	{
		if (!messageJson->HasMember(RPC_TYPE))
		{
			// Message does not conform to protocol, ignore it.
			DEBUG_WARNING("Incoming RPC packet missing type parameter.");
			return;
		}

		std::string type = (*messageJson)[RPC_TYPE].GetString();
		if (0 == type.compare(RPC_METHOD))
		{	
			this->enqueue_incoming_event(std::make_shared<rpc_method_event>(std::move(messageJson)));
		}
		else if (0 == type.compare(RPC_REPLY))
		{
			unsigned int id = (*messageJson)[RPC_ID].GetUint();
			method_handler handlerFunc = nullptr;
			bool executeImmediately = false;
			// Critical Section: Check if there is a registered reply handler and if it's marked for immediate execution.
			{
				std::unique_lock<std::mutex> incomingLock(this->incomingMutex);
				auto replyHandlerItr = this->replyHandlersById.find(id);
				if (replyHandlerItr != this->replyHandlersById.end())
				{
					executeImmediately = replyHandlerItr->second.first;
					handlerFunc.swap(replyHandlerItr->second.second);
					this->replyHandlersById.erase(replyHandlerItr);
				}
			}

			if (nullptr != handlerFunc)
			{
				if (executeImmediately)
				{
					handlerFunc(*this, *messageJson);
				}
				else
				{
					this->enqueue_incoming_event(std::make_shared<rpc_reply_event>(id, std::move(messageJson), handlerFunc));
				}
			}
		}
	}
	else
	{
		DEBUG_ERROR("Failed to parse websocket message: " + message);
	}
}

void interactive_session_internal::handle_ws_close(const websocket& socket, const unsigned short code, const std::string& message)
{
	(socket);
	DEBUG_INFO("Websocket closed: " + message + " (" + std::to_string(code) + ")");
}

int get_interactive_hosts(interactive_session_internal& session, std::vector<std::string>& interactiveHosts)
{	
	DEBUG_INFO("Retrieving interactive hosts.");
	http_response response;
	static std::string hostsUri = "https://mixer.com/api/v1/interactive/hosts";
	// Critical Section: Http request.
	{
		std::unique_lock<std::mutex> httpLock(session.httpMutex);
		RETURN_IF_FAILED(session.http->make_request(hostsUri, "GET", nullptr, "", response));
	}

	if (200 != response.statusCode)
	{
		std::string errorMessage = "Failed to acquire interactive host servers.";
		DEBUG_ERROR(std::to_string(response.statusCode) + " " + errorMessage);
		session.enqueue_incoming_event(std::make_shared<error_event>(interactive_error(MIXER_ERROR_NO_HOST, std::move(errorMessage))));

		return MIXER_ERROR_NO_HOST;
	}

	rapidjson::Document doc;
	if (doc.Parse(response.body.c_str()).HasParseError() || !doc.IsArray())
	{
		return MIXER_ERROR_JSON_PARSE;
	}

	for (auto itr = doc.Begin(); itr != doc.End(); ++itr)
	{
		auto addressItr = itr->FindMember("address");
		if (addressItr != itr->MemberEnd())
		{
			interactiveHosts.push_back(addressItr->value.GetString());
			DEBUG_TRACE("Host found: " + std::string(addressItr->value.GetString(), addressItr->value.GetStringLength()));
		}
	}

	return MIXER_OK;
}

#define DEFAULT_CONNECTION_RETRY_FREQUENCY_S 1
#define MAX_CONNECTION_RETRY_FREQUENCY_S 8

void interactive_session_internal::run_incoming_thread()
{	
	auto onWsOpen = std::bind(&interactive_session_internal::handle_ws_open, this, std::placeholders::_1, std::placeholders::_2);
	auto onWsMessage = std::bind(&interactive_session_internal::handle_ws_message, this, std::placeholders::_1, std::placeholders::_2);
	auto onWsClose = std::bind(&interactive_session_internal::handle_ws_close, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

	// Interactive hosts in retry order.
	std::vector<std::string> hosts;
	std::vector<std::string>::iterator hostItr;
	static unsigned int connectionRetryFrequency = DEFAULT_CONNECTION_RETRY_FREQUENCY_S;

	while (!shutdownRequested)
	{
		// Query interactive hosts if they have not been populated.
		if (hosts.empty())
		{
			int err = get_interactive_hosts(*this, hosts);
			if (err)
			{	
				std::this_thread::sleep_for(std::chrono::seconds(connectionRetryFrequency));
				continue;
			}

			hostItr = hosts.begin();
		}

		// Connect long running websocket.
		DEBUG_INFO("Connecting to websocket: " + *hostItr);
		this->ws->add_header("X-Protocol-Version", "2.0");
		this->ws->add_header("Authorization", this->authorization);
		this->ws->add_header("X-Interactive-Version", this->versionId);
		if (!this->shareCode.empty())
		{
			this->ws->add_header("X-Interactive-Sharecode", this->shareCode);
		}

		int err = this->ws->open(*hostItr, onWsOpen, onWsMessage, nullptr, onWsClose);
		if (this->shutdownRequested)
		{
			break;
		}

		if (err)
		{
			std::string errorMessage;
			if (!this->wsOpen)
			{
				errorMessage = "Failed to open websocket: " + *hostItr;
				DEBUG_ERROR(std::to_string(err) + " " + errorMessage);
				enqueue_incoming_event(std::make_shared<error_event>(interactive_error(MIXER_ERROR_WS_CONNECT_FAILED, std::move(errorMessage))));
			}
			else
			{
				this->wsOpen = false;
				errorMessage = "Lost connection to websocket: " + *hostItr;
				// Since there was a successful connection, reset the connection retry frequency and hosts.
				connectionRetryFrequency = DEFAULT_CONNECTION_RETRY_FREQUENCY_S;
				enqueue_incoming_event(std::make_shared<error_event>(interactive_error(MIXER_ERROR_WS_CLOSED, errorMessage)));

				// When the websocket closes, interactive state is fully reset. Clear any pending methods.
				// Critical Section: Clear websocket methods.
				{
					std::lock_guard<std::mutex> outgoingLock(this->outgoingMutex);
					std::queue<std::shared_ptr<interactive_event_internal>> cleanOutgoingEvents;
					while (!this->outgoingEvents.empty())
					{
						auto ev = this->outgoingEvents.front();
						if (ev->type != interactive_event_type_rpc_method)
						{
							cleanOutgoingEvents.emplace(std::move(ev));
						}
						this->outgoingEvents.pop();
					}

					if (!cleanOutgoingEvents.empty())
					{
						this->outgoingEvents.swap(cleanOutgoingEvents);
					}
				}

				// Reset bootstraps
				this->serverTimeOffsetCalculated = false;
				this->groupsCached = false;
				this->scenesCached = false;

				enqueue_incoming_event(std::make_shared<state_change_event>(interactive_connecting));
			}
		}
		
		++hostItr;

		// Once all the hosts have been tried, clear it and get a new list of hosts.
		if (hostItr == hosts.end())
		{
			hosts = {};
			std::this_thread::sleep_for(std::chrono::seconds(connectionRetryFrequency));
			connectionRetryFrequency = std::min<unsigned int>(MAX_CONNECTION_RETRY_FREQUENCY_S, connectionRetryFrequency *= 2);
		}
	}
}

void interactive_session_internal::run_outgoing_thread()
{
	std::queue<std::shared_ptr<interactive_event_internal>> processingEvents;
	// Run this thread continuously until shutdown is requested.
	while (!shutdownRequested)
	{
		if (!processingEvents.empty())
		{
			// If the processing queue still contains messages after a loop executes, there must have been an error. Throttle retries by sleeping this thread whenever this happens.
			std::this_thread::sleep_for(std::chrono::seconds(DEFAULT_CONNECTION_RETRY_FREQUENCY_S));
		}
		else
		{	
			// Critical section: Check if there are any queued methods or requests that need to be sent.
			std::unique_lock<std::mutex> lock(outgoingMutex);
			if (this->outgoingEvents.empty())
			{
				outgoingCV.wait(lock);
				// Since this thread just woke up, check if it has been signalled to stop.
				if (shutdownRequested)
				{
					break;
				}
			}

			processingEvents.swap(this->outgoingEvents);
		}

		// Process all http requests.
		while (!processingEvents.empty() && !shutdownRequested)
		{
			auto ev = processingEvents.front();
			switch (ev->type)
			{
			case interactive_event_type_http_request:
			{
				auto request = reinterpret_cast<std::shared_ptr<http_request_event>&>(ev);
				http_response response;
				int err = http->make_request(request->uri, request->verb, request->headers.empty() ? nullptr : &request->headers, request->body, response);
				if (err)
				{
					std::string errorMessage = "Failed to '" + request->verb + "' to " + request->uri;
					DEBUG_ERROR(std::to_string(err) + " " + errorMessage);
					enqueue_incoming_event(std::make_shared<error_event>(interactive_error(MIXER_ERROR_HTTP, std::move(errorMessage))));
				}
				else
				{
					// This request was successfully sent, remove it from the queue.
					processingEvents.pop();

					DEBUG_TRACE("HTTP response received: (" + std::to_string(response.statusCode) + ") " + response.body);
					// Critical Section: Find the response handler for this request.
					http_response_handler handler = nullptr;
					{
						std::unique_lock<std::mutex> incomingLock(this->incomingMutex);
						auto responseHandlerItr = this->httpResponseHandlers.find(request->packetId);
						if (responseHandlerItr != this->httpResponseHandlers.end())
						{
							handler = std::move(responseHandlerItr->second);
							this->httpResponseHandlers.erase(responseHandlerItr);
						}
					}

					if (nullptr != handler)
					{
						enqueue_incoming_event(std::make_shared<http_response_event>(std::move(response), handler));
					}
				}

				break;
			}
			case interactive_event_type_rpc_method:
			{	
				if (!this->wsOpen)
				{
					// If the websocket is not open, skip processing this event.
					break;
				}

				auto methodEvent = reinterpret_cast<std::shared_ptr<rpc_method_event>&>(ev);
				std::string packet = jsonStringify(*(methodEvent->methodJson));
				DEBUG_TRACE("Sending websocket message: " + packet);

				// Critical Section: Only one thread may send a websocket message at a time.
				int err = 0;
				{
					std::unique_lock<std::mutex> sendLock(this->websocketMutex);
					err = this->ws->send(packet);
				}

				if (err)
				{
					std::string errorMessage = "Failed to send websocket message.";
					DEBUG_ERROR(std::to_string(err) + " " + errorMessage);
					enqueue_incoming_event(std::make_shared<error_event>(interactive_error(MIXER_ERROR_WS_SEND_FAILED, std::move(errorMessage))));

					// An error here implies that the connection is broken.
					// Break out of the websocket method loop so that http requests are not starved and retry.
					break;
				}
				else
				{
					// Method sent successfully.
					processingEvents.pop();
				}
				break;
			}
			default:
			{
				assert(false);
				break;
			}
			}
		}
	}
}

}