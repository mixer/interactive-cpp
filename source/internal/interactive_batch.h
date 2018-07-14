#pragma once
#include "interactivity.h"
#include "interactive_session.h"
#include "rapidjson\document.h"

#include <unordered_set>

namespace mixer_internal
{

enum interactive_batch_type
{
	INTERACTIVE_BATCH_TYPE_CONTROL,
	INTERACTIVE_BATCH_TYPE_PARTICIPANT,
	INTERACTIVE_BATCH_TYPE_SCENE
};

struct interactive_batch_op_internal;
struct interactive_batch_op_internal
{
	interactive_session_internal* session;
	std::string method;
	std::shared_ptr<rapidjson::Document> document;
	std::unordered_set<std::string> entryIds;
	interactive_batch_type type;
};

int interactive_batch_begin(interactive_session session, char *methodName, interactive_batch_type type, interactive_batch_op* batchPtr);
int interactive_batch_add_entry(interactive_batch_op batch, const char * paramsKey, const char* entryId, interactive_batch_entry* entry);
int interactive_batch_free(interactive_batch_op_internal* batch);
int interactive_batch_commit(interactive_batch_op_internal* batch);
int interactive_batch_add_param(interactive_batch_op_internal* batch, rapidjson::Pointer& ptr, rapidjson::Value& value);
int interactive_batch_array_push(interactive_batch_array* array, rapidjson::Value& value);
int interactive_batch_array_push(interactive_batch_array* array, rapidjson::Pointer& ptr, rapidjson::Value& value);

}