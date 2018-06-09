#include "interactive_batch.h"
#include "interactive_session.h"
#include "common.h"

#include <functional>

namespace mixer_internal
{

interactive_batch_entry_internal::interactive_batch_entry_internal()
	: next(nullptr), value(rapidjson::kObjectType)
{
}

interactive_batch_array_internal::interactive_batch_array_internal()
	: value(rapidjson::kArrayType)
{
}

int interactive_batch_begin(interactive_session session, interactive_batch* batchPtr, char *methodName, on_get_batch_params getParams)
{
	if (nullptr == session || nullptr == batchPtr || nullptr == methodName || nullptr == getParams)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	std::auto_ptr<interactive_batch_internal> batchInternal(new interactive_batch_internal());
	batchInternal->session = reinterpret_cast<interactive_session_internal*>(session);
	batchInternal->priority = RPC_PRIORITY_DEFAULT;
	batchInternal->method = methodName;
	batchInternal->getParams = getParams;
	batchInternal->firstEntry = nullptr;
	batchInternal->lastEntry = nullptr;
	batchInternal->document = std::make_shared<rapidjson::Document>();
	*batchPtr = reinterpret_cast<interactive_batch>(batchInternal.release());

	return MIXER_OK;
}

int interactive_batch_add_entry(interactive_batch batch, interactive_batch_entry* entry)
{
	if (nullptr == batch || nullptr == entry)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_internal* batchInternal = reinterpret_cast<interactive_batch_internal*>(batch);
	interactive_batch_entry_internal* entryInternal = new interactive_batch_entry_internal();
	if (nullptr == batchInternal->firstEntry)
	{
		batchInternal->firstEntry = entryInternal;
		batchInternal->lastEntry = entryInternal;
	}
	else
	{
		batchInternal->lastEntry->next = entryInternal;
		batchInternal->lastEntry = entryInternal;
	}

	*entry = reinterpret_cast<interactive_batch_entry>(entryInternal);

	return MIXER_OK;
}

int interactive_batch_iterate_entries(interactive_batch_internal* batch, on_get_entry_params getParams)
{
	if (nullptr == getParams)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_entry_internal* freePtr = nullptr;
	interactive_batch_entry_internal* next = batch->firstEntry;
	while (nullptr != next) {
		getParams(next);
		freePtr = next;
		next = next->next;
		delete freePtr;
	}

	batch->firstEntry = nullptr;
	batch->lastEntry = nullptr;

	return MIXER_OK;
}

int interactive_batch_free(interactive_batch_internal* batch)
{
	if (nullptr == batch)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_entry_internal* freePtr = nullptr;
	interactive_batch_entry_internal* next = batch->firstEntry;
	while (nullptr != next) {
		freePtr = next;
		next = next->next;
		delete freePtr;
	}

	delete batch;

	return MIXER_OK;
}

int interactive_batch_end(interactive_batch_internal* batch)
{
	if (nullptr == batch)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	int result = queue_method(*batch->session, batch->method, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		params.AddMember(RPC_PRIORITY, batch->priority, allocator);
		batch->getParams(batch, allocator, params);
	}, nullptr, batch->document);

	interactive_batch_free(batch);

	return result;
}

int interactive_batch_add_param(interactive_batch batch, interactive_batch_entry entry, const char* name, rapidjson::Value& value)
{
	if (nullptr == batch || nullptr == entry || nullptr == name)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_internal* batchInternal = reinterpret_cast<interactive_batch_internal*>(batch);
	interactive_batch_entry_internal* entryInternal = reinterpret_cast<interactive_batch_entry_internal*>(entry);
	entryInternal->value.AddMember(
		rapidjson::Value(std::string(name), batchInternal->document->GetAllocator()),
		value,
		batchInternal->document->GetAllocator());

	return MIXER_OK;
}

int interactive_batch_array_push(interactive_batch batch, interactive_batch_array arrayItem, rapidjson::Value& value)
{
	if (nullptr == batch || nullptr == arrayItem)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_internal* batchInternal = reinterpret_cast<interactive_batch_internal*>(batch);
	interactive_batch_array_internal* arrayInternal = reinterpret_cast<interactive_batch_array_internal*>(arrayItem);
	arrayInternal->value.PushBack(value, batchInternal->document->GetAllocator());

	return MIXER_OK;
}

}

using namespace mixer_internal;

int interactive_batch_set_priority(interactive_batch batch, int priority)
{
	if (nullptr == batch)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_internal* batchInternal = reinterpret_cast<interactive_batch_internal*>(batch);
	batchInternal->priority = priority;
	return MIXER_OK;
}

int interactive_batch_add_param_null(interactive_batch batch, interactive_batch_entry entry, const char* name)
{
	return interactive_batch_add_param(batch, entry, name, rapidjson::Value(rapidjson::kNullType));
}

int interactive_batch_add_param_str(interactive_batch batch, interactive_batch_entry entry, const char* name, const char* value)
{
	if (nullptr == value)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_internal* batchInternal = reinterpret_cast<interactive_batch_internal*>(batch);
	return interactive_batch_add_param(batch, entry, name, rapidjson::Value(std::string(value), batchInternal->document->GetAllocator()));
}

int interactive_batch_add_param_uint(interactive_batch batch, interactive_batch_entry entry, const char* name, unsigned int value)
{
	return interactive_batch_add_param(batch, entry, name, rapidjson::Value(value));
}

int interactive_batch_add_param_bool(interactive_batch batch, interactive_batch_entry entry, const char* name, bool value)
{
	return interactive_batch_add_param(batch, entry, name, rapidjson::Value(value));
}

int interactive_batch_add_param_object(interactive_batch batch, interactive_batch_entry entry, const char* name, interactive_batch_object_callback callback)
{
	if (nullptr == batch || nullptr == entry)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}
	
	std::auto_ptr<interactive_batch_entry_internal> tempEntry(new interactive_batch_entry_internal());
	callback(batch, reinterpret_cast<interactive_batch_entry>(tempEntry.get()));

	return interactive_batch_add_param(batch, entry, name, tempEntry->value);
}

int interactive_batch_add_param_array(interactive_batch batch, interactive_batch_entry entry, const char* name, interactive_batch_array_callback callback)
{
	if (nullptr == batch || nullptr == entry)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	std::auto_ptr<interactive_batch_array_internal> tempArray(new interactive_batch_array_internal());
	tempArray->value.SetArray();
	callback(batch, reinterpret_cast<interactive_batch_array>(tempArray.get()));

	return interactive_batch_add_param(batch, entry, name, tempArray->value);
}

int interactive_batch_array_push_null(interactive_batch batch, interactive_batch_array arrayItem)
{
	return interactive_batch_array_push(batch, arrayItem, rapidjson::Value(rapidjson::kNullType));
}

int interactive_batch_array_push_str(interactive_batch batch, interactive_batch_array arrayItem, const char* value)
{
	if (nullptr == value)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_internal* batchInternal = reinterpret_cast<interactive_batch_internal*>(batch);
	return interactive_batch_array_push(batch, arrayItem, rapidjson::Value(std::string(value), batchInternal->document->GetAllocator()));
}

int interactive_batch_array_push_uint(interactive_batch batch, interactive_batch_array arrayItem, unsigned int value)
{
	return interactive_batch_array_push(batch, arrayItem, rapidjson::Value(value));
}

int interactive_batch_array_push_bool(interactive_batch batch, interactive_batch_array arrayItem, bool value)
{
	return interactive_batch_array_push(batch, arrayItem, rapidjson::Value(value));
}

int interactive_batch_array_push_object(interactive_batch batch, interactive_batch_array arrayItem, interactive_batch_object_callback callback)
{
	if (nullptr == batch || nullptr == arrayItem)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	std::auto_ptr<interactive_batch_entry_internal> tempEntry(new interactive_batch_entry_internal());
	callback(batch, reinterpret_cast<interactive_batch_entry>(tempEntry.get()));

	return interactive_batch_array_push(batch, arrayItem, tempEntry->value);
}

int interactive_batch_array_push_array(interactive_batch batch, interactive_batch_array arrayItem, interactive_batch_array_callback callback)
{
	if (nullptr == batch || nullptr == arrayItem)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	std::auto_ptr<interactive_batch_array_internal> tempArray(new interactive_batch_array_internal());
	callback(batch, reinterpret_cast<interactive_batch_array>(tempArray.get()));

	return interactive_batch_array_push(batch, arrayItem, tempArray->value);
}
