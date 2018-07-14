#pragma once
#include "interactivity.h"
#include "interactive_session.h"
#include "rapidjson\document.h"

#include <list>

namespace mixer_internal
{

struct interactive_batch_internal;
struct interactive_batch_internal
{
	interactive_session_internal* session;
	std::string method;
	std::shared_ptr<rapidjson::Document> document;
	std::list<interactive_batch_entry> entries;
};

int interactive_batch_begin(interactive_session session, interactive_batch* batchPtr, char *methodName);
int interactive_batch_add_entry(interactive_batch batch, interactive_batch_entry* entry, const char * paramsKey);
int interactive_batch_free(interactive_batch_internal* batch);
int interactive_batch_commit(interactive_batch_internal* batch);
int interactive_batch_add_param(interactive_batch_internal* batch, rapidjson::Pointer& ptr, rapidjson::Value& value);
int interactive_batch_array_push(interactive_batch_array* array, rapidjson::Value& value);
int interactive_batch_array_push(interactive_batch_array* array, rapidjson::Pointer& ptr, rapidjson::Value& value);

}