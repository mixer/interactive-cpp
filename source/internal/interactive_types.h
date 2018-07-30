#pragma once

#include "rapidjson\document.h"
#include <string>
#include <map>
#include <functional>
#include <queue>

namespace mixer_internal
{

struct interactive_session_internal;
struct interactive_event_internal;

struct interactive_object_internal
{
	const std::string id;
	interactive_object_internal(std::string id);
};

struct interactive_control_pointer
{
	const std::string sceneId;
	const std::string cachePointer;
	interactive_control_pointer(std::string sceneId, std::string cachePointer);
};

struct interactive_control_internal : public interactive_object_internal
{
	const std::string kind;
	interactive_control_internal(std::string id, std::string kind);
};

struct interactive_group_internal : public interactive_object_internal
{
	const std::string scene;
	interactive_group_internal(std::string id, std::string scene);
};

struct compare_event_priority;
typedef std::priority_queue<std::shared_ptr<interactive_event_internal>, std::vector<std::shared_ptr<interactive_event_internal>>, compare_event_priority> interactive_event_queue;

typedef std::map<std::string, std::string> scenes_by_id;
typedef std::map<std::string, std::string> scenes_by_group;
typedef std::map<std::string, interactive_control_pointer> controls_by_id;
typedef std::map<std::string, std::shared_ptr<rapidjson::Document>> participants_by_id;
typedef std::function<int(interactive_session_internal&, rapidjson::Document&)> method_handler;
typedef std::map<std::string, method_handler> method_handlers_by_method;
typedef std::map<unsigned int, std::pair<bool, method_handler>> reply_handlers_by_id;
typedef std::function<int(const http_response&)> http_response_handler;

}