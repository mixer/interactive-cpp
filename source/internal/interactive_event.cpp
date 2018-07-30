#include "interactive_event.h"

namespace mixer_internal
{

bool compare_event_priority::operator()(const std::shared_ptr<interactive_event_internal> left, const std::shared_ptr<interactive_event_internal> right)
{
	return left->type > right->type;
}

interactive_event_internal::interactive_event_internal(interactive_event_type type) : type(type) {}

rpc_method_event::rpc_method_event(std::shared_ptr<rapidjson::Document>&& methodJson) : interactive_event_internal(interactive_event_type_rpc_method), methodJson(methodJson) {}

rpc_reply_event::rpc_reply_event(const unsigned int id, std::shared_ptr<rapidjson::Document>&& replyJson, const method_handler replyHandler) : interactive_event_internal(interactive_event_type_rpc_reply), id(id), replyJson(replyJson), replyHandler(replyHandler) {}

http_request_event::http_request_event(const uint32_t packetId, const std::string& uri, const std::string& verb, const http_headers* headers, const std::string* body) :
	interactive_event_internal(interactive_event_type_http_request), packetId(packetId), uri(uri), verb(verb), headers(nullptr == headers ? http_headers() : *headers), body(nullptr == body ? std::string() : *body)
{
}

http_response_event::http_response_event(http_response&& response, const http_response_handler handler) : interactive_event_internal(interactive_event_type_http_response), response(response), responseHandler(handler) {}

error_event::error_event(const interactive_error error) : interactive_event_internal(interactive_event_type_error), error(error) {}

state_change_event::state_change_event(interactive_state currentState) : interactive_event_internal(interactive_event_type_state_change), currentState(currentState) {}

}