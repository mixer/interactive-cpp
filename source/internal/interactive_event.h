#pragma once
#include "common.h"
#include "rapidjson\document.h"
#include "http_client.h"
#include "interactive_types.h"

namespace mixer_internal
{

// Interactive event types in priority order.
enum interactive_event_type
{
	interactive_event_type_error,
	interactive_event_type_state_change,
	interactive_event_type_http_response,
	interactive_event_type_http_request,
	interactive_event_type_rpc_reply,
	interactive_event_type_rpc_method,
};

struct interactive_event_internal
{
	const interactive_event_type type;
	interactive_event_internal(const interactive_event_type type);
};

struct compare_event_priority
{
	bool operator()(const std::shared_ptr<interactive_event_internal> left, const std::shared_ptr<interactive_event_internal> right);
};

struct rpc_method_event : interactive_event_internal
{	
	const std::shared_ptr<rapidjson::Document> methodJson;
	rpc_method_event(std::shared_ptr<rapidjson::Document>&& methodJson);
};

struct rpc_reply_event : interactive_event_internal
{
	const unsigned int id;
	const std::shared_ptr<rapidjson::Document> replyJson;
	const method_handler replyHandler;
	rpc_reply_event(const unsigned int id, std::shared_ptr<rapidjson::Document>&& replyJson, const method_handler handler);
};

struct http_request_event : interactive_event_internal
{
	const uint32_t packetId;
	const std::string uri;
	const std::string verb;
	const std::map<std::string, std::string> headers;
	const std::string body;
	http_request_event(const uint32_t packetId, const std::string& uri, const std::string& verb, const http_headers* headers, const std::string* body);
};

struct http_response_event : interactive_event_internal
{
	const http_response response;
	const http_response_handler responseHandler;
	http_response_event(http_response&&, const http_response_handler);
};

typedef std::pair<const mixer_result_code, const std::string> interactive_error;

struct error_event : interactive_event_internal
{
	const interactive_error error;
	error_event(const interactive_error error);
};

struct state_change_event : interactive_event_internal
{	
	const interactive_state currentState;
	state_change_event(interactive_state currentState);
};

}