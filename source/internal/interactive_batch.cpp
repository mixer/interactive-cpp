#include "interactive_batch.h"
#include "interactive_session.h"
#include "common.h"

#include <functional>
#include <sstream>

#include "rapidjson/pointer.h"

#define OBJECT_BATCH(o) ((o)->_a)
#define OBJECT_POINTER(o) ((o)->_b)

namespace mixer_internal
{

inline interactive_batch_internal* cast_batch(interactive_batch batch)
{
	return reinterpret_cast<interactive_batch_internal*>(batch);
}

inline interactive_batch_internal* object_get_batch(interactive_batch_object* obj)
{
	return reinterpret_cast<interactive_batch_internal*>(OBJECT_BATCH(obj));
}

inline rapidjson::Value* object_get_value(interactive_batch_object* obj)
{
	return reinterpret_cast<rapidjson::Value*>(OBJECT_POINTER(obj));
}

inline interactive_batch_internal* array_get_batch(interactive_batch_array* obj)
{
	return reinterpret_cast<interactive_batch_internal*>(OBJECT_BATCH(obj));
}

inline rapidjson::Value* array_get_value(interactive_batch_array* obj)
{
	return reinterpret_cast<rapidjson::Value*>(OBJECT_POINTER(obj));
}

int interactive_batch_begin(interactive_session session, char *methodName, interactive_batch_type type, interactive_batch* batchPtr)
{
	if (nullptr == session || nullptr == batchPtr || nullptr == methodName)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	std::auto_ptr<interactive_batch_internal> batchInternal(new interactive_batch_internal());
	batchInternal->session = reinterpret_cast<interactive_session_internal*>(session);
	batchInternal->method = methodName;
	batchInternal->type = type;
	batchInternal->document = std::make_shared<rapidjson::Document>();
	*batchPtr = reinterpret_cast<interactive_batch>(batchInternal.release());

	interactive_batch_set_priority(*batchPtr, RPC_PRIORITY_DEFAULT);

	return MIXER_OK;
}

int interactive_batch_add_entry(interactive_batch batch, const char * paramsKey, const char* entryId, interactive_batch_entry* entry)
{
	if (nullptr == batch || nullptr == entry)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_internal* batchInternal = cast_batch(batch);
	std::stringstream ss;
	ss << "/" << RPC_PARAMS << "/" << paramsKey;
	size_t pos = batchInternal->entryIds.size();
	if (0 == pos)
	{
		rapidjson::Pointer(ss.str().c_str())
			.Set(*batchInternal->document, rapidjson::Value(rapidjson::kArrayType));
	}

	auto result = batchInternal->entryIds.emplace(entryId);
	if (!result.second)
	{
		return MIXER_ERROR_DUPLICATE_ENTRY;
	}

	ss << "/" << pos;
	OBJECT_BATCH(&entry->obj) = batchInternal;
	OBJECT_POINTER(&entry->obj) = 
		&rapidjson::Pointer(ss.str().c_str())
			.Set(*batchInternal->document, rapidjson::Value(rapidjson::kObjectType));

	return MIXER_OK;
}

int interactive_batch_free(interactive_batch_internal* batch)
{
	if (nullptr == batch)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	delete batch;

	return MIXER_OK;
}

int interactive_batch_commit(interactive_batch_internal* batch)
{
	if (nullptr == batch)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	return queue_method_prebuilt(*batch->session, batch->method.c_str(), nullptr, batch->document);
}

int interactive_batch_add_param(interactive_batch_internal* batch, rapidjson::Pointer& ptr, rapidjson::Value& value)
{
	if (nullptr == batch)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	ptr.Set(*batch->document, value);

	return MIXER_OK;
}

int interactive_batch_add_param(interactive_batch_object* obj, const char *paramName, rapidjson::Value& value)
{
	if (nullptr == obj || nullptr == paramName)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	object_get_value(obj)->AddMember(
		rapidjson::Value(std::string(paramName).c_str(), object_get_batch(obj)->document->GetAllocator()),
		value,
		object_get_batch(obj)->document->GetAllocator());

	return MIXER_OK;
}

int interactive_batch_array_push(interactive_batch_array* array, rapidjson::Value& value)
{
	if (nullptr == array)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	array_get_value(array)->PushBack(value, array_get_batch(array)->document->GetAllocator());

	return MIXER_OK;
}

int interactive_batch_array_push(interactive_batch_array* array, rapidjson::Pointer& ptr, rapidjson::Value& value)
{
	if (nullptr == array)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_internal* batch = array_get_batch(array);
	ptr.Set(*batch->document.get(), value);

	return MIXER_OK;
}

}

using namespace mixer_internal;

int interactive_batch_close(interactive_batch batch)
{
	if (nullptr == batch)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	return interactive_batch_free(cast_batch(batch));
}

int interactive_batch_set_priority(interactive_batch batch, int priority)
{
	if (nullptr == batch)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_internal* batchInternal = cast_batch(batch);
	std::stringstream ss;
	ss << "/" << RPC_PARAMS << "/" << RPC_PRIORITY;
	rapidjson::Pointer(ss.str().c_str()).Set(*batchInternal->document, priority);
	return MIXER_OK;
}

int interactive_batch_add_param_null(interactive_batch_object* obj, const char* name)
{
	return interactive_batch_add_param(
		obj,
		name,
		rapidjson::Value(rapidjson::kNullType).Move());
}

int interactive_batch_add_param_str(interactive_batch_object* obj, const char* name, const char* value)
{
	if (nullptr == obj || nullptr == value)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_internal* batch = object_get_batch(obj);
	if (nullptr == batch)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	return interactive_batch_add_param(
		obj,
		name,
		rapidjson::Value(std::string(value).c_str(), batch->document->GetAllocator()).Move());
}

int interactive_batch_add_param_uint(interactive_batch_object* obj, const char* name, unsigned int value)
{
	return interactive_batch_add_param(
		obj,
		name,
		rapidjson::Value(value).Move());
}

int interactive_batch_add_param_bool(interactive_batch_object* obj, const char* name, bool value)
{
	return interactive_batch_add_param(
		obj,
		name,
		rapidjson::Value(value).Move());
}

int interactive_batch_add_param_object(interactive_batch_object* obj, const char* name, interactive_batch_object* paramObj)
{
	if (nullptr == obj || nullptr == name || nullptr == paramObj)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_internal* batch = object_get_batch(obj);
	auto jsonObj = object_get_value(obj)->GetObject();
	jsonObj.AddMember(
		rapidjson::Value(std::string(name).c_str(), object_get_batch(obj)->document->GetAllocator()).Move(),
		rapidjson::Value(rapidjson::kObjectType).Move(),
		object_get_batch(obj)->document->GetAllocator());
	OBJECT_BATCH(paramObj) = batch;
	OBJECT_POINTER(paramObj) = &jsonObj.FindMember(name)->value;

	return MIXER_OK;
}

int interactive_batch_add_param_array(interactive_batch_object* obj, const char* name, interactive_batch_array* paramArr)
{
	if (nullptr == obj || nullptr == name || nullptr == paramArr)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_internal* batch = object_get_batch(obj);
	auto jsonObj = object_get_value(obj)->GetObject();
	jsonObj.AddMember(
		rapidjson::Value(std::string(name).c_str(), object_get_batch(obj)->document->GetAllocator()).Move(),
		rapidjson::Value(rapidjson::kArrayType).Move(),
		object_get_batch(obj)->document->GetAllocator());
	OBJECT_BATCH(paramArr) = batch;
	OBJECT_POINTER(paramArr) = &jsonObj.FindMember(name)->value;

	return MIXER_OK;
}

int interactive_batch_array_push_null(interactive_batch_array* array)
{
	return interactive_batch_array_push(array, rapidjson::Value(rapidjson::kNullType).Move());
}

int interactive_batch_array_push_str(interactive_batch_array* array, const char* value)
{
	if (nullptr == value)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_internal* batch = array_get_batch(array);
	if (nullptr == batch)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	return interactive_batch_array_push(array, rapidjson::Value(std::string(value).c_str(), batch->document->GetAllocator()).Move());
}

int interactive_batch_array_push_uint(interactive_batch_array* array, unsigned int value)
{
	return interactive_batch_array_push(array, rapidjson::Value(value).Move());
}

int interactive_batch_array_push_bool(interactive_batch_array* array, bool value)
{
	return interactive_batch_array_push(array, rapidjson::Value(value).Move());
}

int interactive_batch_array_push_object(interactive_batch_array* array, interactive_batch_object* pushObj)
{
	if (nullptr == array || nullptr == pushObj)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_internal* batch = array_get_batch(array);
	auto arr = array_get_value(array)->GetArray();
	arr.PushBack(
		rapidjson::Value(rapidjson::kObjectType).Move(),
		array_get_batch(array)->document->GetAllocator());
	OBJECT_BATCH(pushObj) = batch;
	OBJECT_POINTER(pushObj) = &arr[arr.Size() - 1];

	return MIXER_OK;
}

int interactive_batch_array_push_array(interactive_batch_array* array, interactive_batch_array* pushArr)
{
	if (nullptr == array || nullptr == pushArr)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_internal* batch = array_get_batch(array);
	auto arr = array_get_value(array)->GetArray();
	arr.PushBack(
		rapidjson::Value(rapidjson::kArrayType).Move(),
		array_get_batch(array)->document->GetAllocator());
	OBJECT_BATCH(pushArr) = batch;
	OBJECT_POINTER(pushArr) = &arr[arr.Size() - 1];

	return MIXER_OK;
}
