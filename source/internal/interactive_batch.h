#pragma once
#include "interactivity.h"
#include "interactive_session.h"
#include "rapidjson\document.h"
#include <functional>

namespace mixer_internal
{

struct interactive_batch_internal;
struct interactive_batch_entry_internal;
typedef std::function<void(interactive_batch_entry_internal*)> on_get_entry_params;
typedef std::function<void(interactive_batch_internal*, rapidjson::Document::AllocatorType& allocator, rapidjson::Value& value)> on_get_batch_params;

struct interactive_batch_internal
{
	interactive_session_internal* session;
	const char* method;
	int priority;
	on_get_batch_params getParams;
	std::shared_ptr<rapidjson::Document> document;
	interactive_batch_entry_internal* firstEntry;
	interactive_batch_entry_internal* lastEntry;
	const char *param;
};

struct interactive_batch_entry_internal
{
	interactive_batch_entry_internal();

	interactive_batch_entry_internal* next;
	rapidjson::Value value;
};

struct interactive_batch_array_internal
{
	interactive_batch_array_internal();

	rapidjson::Value value;
};

int interactive_batch_begin(interactive_session session, interactive_batch* batchPtr, char *methodName, on_get_batch_params getParams);
int interactive_batch_add_entry(interactive_batch batch, interactive_batch_entry* entry);
int interactive_batch_iterate_entries(interactive_batch_internal* batch, on_get_entry_params getParams);
int interactive_batch_free(interactive_batch_internal* batch);
int interactive_batch_end(interactive_batch_internal* batch);
int interactive_batch_add_param(interactive_batch batch, interactive_batch_entry entry, char* name, rapidjson::Value& value);
int interactive_batch_array_push(interactive_batch batch, interactive_batch_array arrayItem, rapidjson::Value& value);

}