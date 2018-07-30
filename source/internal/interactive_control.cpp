#include "interactive_session.h"
#include "common.h"

#include <type_traits>

namespace mixer_internal
{

interactive_control_internal::interactive_control_internal(std::string id, std::string kind) : interactive_object_internal(std::move(id)), kind(std::move(kind)) {}

interactive_control_pointer::interactive_control_pointer(std::string sceneId, std::string cachePointer) : sceneId(std::move(sceneId)), cachePointer(std::move(cachePointer)) {}

// Assumes caller holds read lock on scenes mutex.
int get_control_scene_id(interactive_session_internal& session, const char* controlId, std::string& sceneId)
{
	auto itr = session.controls.find(controlId);
	if (itr == session.controls.end())
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::string controlPtr = itr->second.cachePointer;

	// The controlPtr is prefixed with a scene pointer, parse it to find the scene this control belongs to.
	size_t controlOffset = controlPtr.find("controls", 0);
	std::string scenePtr = controlPtr.substr(0, controlOffset - 1);

	// Get the scene id.
	rapidjson::Value* scene = rapidjson::Pointer(rapidjson::StringRef(scenePtr.c_str(), scenePtr.length())).Get(session.scenesRoot);
	sceneId = (*scene)[RPC_SCENE_ID].GetString();

	return MIXER_OK;
}

// Assumes caller holds read lock on scenes mutex.
int get_scene_object_prop_count(interactive_session_internal& session, const char* pointer, size_t* count)
{
	if (nullptr == pointer || nullptr == count)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	*count = 0;
	rapidjson::Value* value = nullptr;
	value = rapidjson::Pointer(rapidjson::StringRef(pointer)).Get(session.scenesRoot);
	if (nullptr == value)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	*count = value->MemberCount();
	return MIXER_OK;
}

// Assumes caller holds read lock on scenes mutex.
int get_scene_object_prop_data(interactive_session_internal& session, const char* pointer, size_t index, char* propName, size_t* propNameLength, interactive_property_type* propType)
{
	if (nullptr == pointer || nullptr == propNameLength || nullptr == propType)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	if (*propNameLength > 0 && nullptr == propName)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	*propType = interactive_property_type::interactive_unknown_t;

	rapidjson::Value* value = rapidjson::Pointer(rapidjson::StringRef(pointer)).Get(session.scenesRoot);
	if (nullptr == value)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	// Verify that this index is valid for this object.
	if (value->MemberCount() <= index)
	{
		return MIXER_ERROR_PROPERTY_NOT_FOUND;
	}

	// Move iterator to property index.
	auto itr = value->MemberBegin();
	for (size_t i = 0; i < index; ++i)
	{
		++itr;
	}

	// Verify the caller's buffer is large enough to hold the contents.
	if (*propNameLength < itr->name.GetStringLength())
	{
		*propNameLength = itr->name.GetStringLength() + 1;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	memcpy(propName, itr->name.GetString(), *propNameLength - 1);
	propName[*propNameLength - 1] = '\0';

	// Determine the type of this property.
	if (itr->value.IsString())
	{
		*propType = interactive_property_type::interactive_string_t;
	}
	else if (itr->value.IsInt())
	{
		*propType = interactive_property_type::interactive_int_t;
	}
	else if (itr->value.IsBool())
	{
		*propType = interactive_property_type::interactive_bool_t;
	}
	else if (itr->value.IsFloat())
	{
		*propType = interactive_property_type::interactive_float_t;
	}
	else if (itr->value.IsArray())
	{
		*propType = interactive_property_type::interactive_array_t;
	}
	else if (itr->value.IsObject())
	{
		*propType = interactive_property_type::interactive_object_t;
	}

	return MIXER_OK;
}

void parse_control(rapidjson::Value& controlJson, interactive_control& control)
{
	control.id = controlJson[RPC_CONTROL_ID].GetString();
	control.idLength = controlJson[RPC_CONTROL_ID].GetStringLength();
	if (controlJson.HasMember(RPC_CONTROL_KIND))
	{
		control.kind = controlJson[RPC_CONTROL_KIND].GetString();
		control.kindLength = controlJson[RPC_CONTROL_KIND].GetStringLength();
	}
}

int cache_new_control(interactive_session_internal& session, const char* sceneId, interactive_control& control, rapidjson::Value& controlJson)
{
	// Critical Section: Add a control to the scenes cache.
	{
		std::unique_lock<std::shared_mutex> scenesLock(session.scenesMutex);
		if (session.controls.find(control.id) != session.controls.end())
		{
			return MIXER_ERROR_OBJECT_EXISTS;
		}

		auto sceneItr = session.scenes.find(sceneId);
		if (sceneItr == session.scenes.end())
		{
			return MIXER_ERROR_OBJECT_NOT_FOUND;
		}

		std::string scenePtr = sceneItr->second;
		rapidjson::Value* scene = rapidjson::Pointer(rapidjson::StringRef(scenePtr.c_str(), scenePtr.length())).Get(session.scenesRoot);

		rapidjson::Document::AllocatorType& allocator = session.scenesRoot.GetAllocator();
		auto controlsItr = scene->FindMember(RPC_PARAM_CONTROLS);
		rapidjson::Value* controls;
		if (controlsItr == scene->MemberEnd() || !controlsItr->value.IsArray())
		{
			controls = &scene->AddMember(RPC_PARAM_CONTROLS, rapidjson::Value(rapidjson::kArrayType), allocator);
		}
		else
		{
			controls = &controlsItr->value;
		}

		rapidjson::Value myControlJson(rapidjson::kObjectType);
		myControlJson.CopyFrom(controlJson, session.scenesRoot.GetAllocator());
		controls->PushBack(myControlJson, allocator);
	}

	RETURN_IF_FAILED(update_control_pointers(session, sceneId));

	return MIXER_OK;
}

int update_cached_control(interactive_session_internal& session, interactive_control& control, rapidjson::Value& controlJson)
{
	// Critical Section: Replace a control in the scenes cache.
	{
		std::unique_lock<std::shared_mutex> scenesLock(session.scenesMutex);
		auto itr = session.controls.find(control.id);
		if (itr == session.controls.end())
		{
			return MIXER_ERROR_OBJECT_NOT_FOUND;
		}

		std::string controlPtr = itr->second.cachePointer;
		rapidjson::Value myControlJson(rapidjson::kObjectType);
		myControlJson.CopyFrom(controlJson, session.scenesRoot.GetAllocator());
		rapidjson::Pointer(rapidjson::StringRef(controlPtr.c_str(), controlPtr.length()))
			.Swap(session.scenesRoot, myControlJson);
	}

	return MIXER_OK;
}

int delete_cached_control(interactive_session_internal& session, const char* sceneId, interactive_control& control)
{
	// Critical Section: Erase the control if it exists on the scene.
	{
		std::unique_lock<std::shared_mutex> scenesLock(session.scenesMutex);
		if (session.controls.find(control.id) == session.controls.end())
		{
			// This control doesn't exist, ignore this deletion.
			return MIXER_OK;
		}

		auto sceneItr = session.scenes.find(sceneId);
		if (sceneItr == session.scenes.end())
		{
			return MIXER_ERROR_OBJECT_NOT_FOUND;
		}

		// Find the controls array on the scene.
		std::string scenePtr = sceneItr->second;
		rapidjson::Value* scene = rapidjson::Pointer(rapidjson::StringRef(scenePtr.c_str(), scenePtr.length())).Get(session.scenesRoot);
		auto controlsItr = scene->FindMember(RPC_PARAM_CONTROLS);
		if (controlsItr == scene->MemberEnd() || !controlsItr->value.IsArray())
		{
			// If the scene has no controls on it, ignore this deletion.
			return MIXER_OK;
		}

		// Erase the value from the array.
		rapidjson::Value* controls = &controlsItr->value;
		for (rapidjson::Value* controlItr = controls->Begin(); controlItr != controls->End(); ++controlItr)
		{
			if (0 == strcmp(controlItr->GetObject()[RPC_CONTROL_ID].GetString(), control.id))
			{
				controls->Erase(controlItr);
				break;
			}
		}
	}

	RETURN_IF_FAILED(update_control_pointers(session, sceneId));

	return MIXER_OK;
}

int safe_get_value(rapidjson::Value* jsonValue, int* value)
{
	if (!jsonValue->IsInt())
	{
		return MIXER_ERROR_INVALID_PROPERTY_TYPE;
	}

	*value = jsonValue->GetInt();
	return MIXER_OK;
}

int safe_get_value(rapidjson::Value* jsonValue, long long* value)
{
	if (!jsonValue->IsInt64())
	{
		return MIXER_ERROR_INVALID_PROPERTY_TYPE;
	}

	*value = jsonValue->GetInt64();
	return MIXER_OK;
}

int safe_get_value(rapidjson::Value* jsonValue, bool* value)
{
	if (!jsonValue->IsBool())
	{
		return MIXER_ERROR_INVALID_PROPERTY_TYPE;
	}

	*value = jsonValue->GetBool();
	return MIXER_OK;
}

int safe_get_value(rapidjson::Value* jsonValue, float* value)
{
	if (!jsonValue->IsFloat())
	{
		return MIXER_ERROR_INVALID_PROPERTY_TYPE;
	}

	*value = jsonValue->GetFloat();
	return MIXER_OK;
}

int safe_get_value(rapidjson::Value* jsonValue, std::string* value)
{
	if (!jsonValue->IsString())
	{
		return MIXER_ERROR_INVALID_PROPERTY_TYPE;
	}

	*value = jsonValue->GetString();
	return MIXER_OK;
}

template<typename T>
int interactive_control_get_property(interactive_session session, const char* controlId, const char* key, T* prop)
{
	if (nullptr == session || nullptr == controlId|| nullptr == key || nullptr == prop)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	// Validate connection state.
	if (interactive_connected > sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	if (sessionInternal->shutdownRequested)
	{
		return MIXER_OK;
	}

	std::shared_lock<std::shared_mutex> scenesReadLock(sessionInternal->scenesMutex);
	auto controlItr = sessionInternal->controls.find(std::string(controlId));
	if (controlItr == sessionInternal->controls.end())
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::string controlPointer = controlItr->second.cachePointer + "/" + std::string(key);
	rapidjson::Value* controlValue = rapidjson::Pointer(controlPointer.c_str()).Get(sessionInternal->scenesRoot);
	if (nullptr == controlValue)
	{
		return MIXER_ERROR_PROPERTY_NOT_FOUND;
	}

	return safe_get_value(controlValue, prop);
}

template <typename T>
int interactive_control_set_property(interactive_session session, const char* controlId, const char* key, T prop)
{
	if (nullptr == controlId || nullptr == key)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	if (interactive_connected > sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	// Find the control
	std::shared_lock<std::shared_mutex> l(sessionInternal->scenesMutex);
	auto controlItr = sessionInternal->controls.find(controlId);
	if (sessionInternal->controls.end() == controlItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	RETURN_IF_FAILED(queue_method(*sessionInternal, RPC_METHOD_UPDATE_CONTROLS, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		params.AddMember(RPC_SCENE_ID, controlItr->second.sceneId, allocator);
		rapidjson::Value controlsArray(rapidjson::kArrayType);
		// Create a control object with an id.
		rapidjson::Value controlObj(rapidjson::kObjectType);
		controlObj.AddMember(RPC_CONTROL_ID, std::string(controlId), allocator);

		// Add a null value to the object with the given key.
		rapidjson::Value keyVal(rapidjson::kStringType);
		keyVal.SetString(std::string(key), allocator);
		controlObj.AddMember(keyVal, prop, allocator);

		controlsArray.PushBack(controlObj, allocator);
		params.AddMember(RPC_PARAM_CONTROLS, controlsArray, allocator);
	}, nullptr));

	return MIXER_OK;
}

template <typename T>
int interactive_control_get_meta_property(interactive_session session, const char* controlId, const char* key, T* property)
{
	std::string metaKey = std::string(RPC_METADATA) + "/" + std::string(key) + "/value";
	return interactive_control_get_property<T>(session, controlId, metaKey.c_str(), property);
}

}

using namespace mixer_internal;

int interactive_control_trigger_cooldown(interactive_session session, const char* controlId, const unsigned long long cooldownMs)
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

	std::string controlSceneId;
	// Critical Section: Get the control's scene id.
	{
		std::shared_lock<std::shared_mutex> scenesReadLock(sessionInternal->scenesMutex);
		RETURN_IF_FAILED(get_control_scene_id(*sessionInternal, controlId, controlSceneId));
	}

	long long cooldownTimestamp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()).time_since_epoch().count() - sessionInternal->serverTimeOffsetMs + cooldownMs;
	RETURN_IF_FAILED(queue_method(*sessionInternal, RPC_METHOD_UPDATE_CONTROLS, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		params.AddMember(RPC_SCENE_ID, controlSceneId, allocator);
		params.AddMember("priority", 1, allocator);

		rapidjson::Value controls(rapidjson::kArrayType);
		rapidjson::Value control(rapidjson::kObjectType);
		control.AddMember(RPC_CONTROL_ID, std::string(controlId), allocator);
		control.AddMember(RPC_CONTROL_BUTTON_COOLDOWN, cooldownTimestamp, allocator);
		controls.PushBack(std::move(control), allocator);
		params.AddMember(RPC_PARAM_CONTROLS, std::move(controls), allocator);
	}, nullptr));

	return MIXER_OK;
}

int interactive_control_get_property_count(interactive_session session, const char* controlId, size_t* count)
{
	if (nullptr == session || nullptr == controlId || nullptr == count)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	*count = 0;
	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	// Validate connection state.
	if (interactive_connected > sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	std::shared_lock<std::shared_mutex> scenesReadLock(sessionInternal->scenesMutex);
	auto controlItr = sessionInternal->controls.find(controlId);
	if (controlItr == sessionInternal->controls.end())
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	return get_scene_object_prop_count(*sessionInternal, controlItr->second.cachePointer.c_str(), count);
}

int interactive_control_get_meta_property_count(interactive_session session, const char* controlId, size_t* count)
{
	if (nullptr == session || nullptr == controlId || nullptr == count)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	*count = 0;
	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	// Validate connection state.
	if (interactive_connected > sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	std::shared_lock<std::shared_mutex> scenesReadLock(sessionInternal->scenesMutex);
	auto controlItr = sessionInternal->controls.find(std::string(controlId));
	if (controlItr == sessionInternal->controls.end())
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::string metaPropPointer = controlItr->second.cachePointer + "/" + RPC_METADATA;
	return get_scene_object_prop_count(*sessionInternal, metaPropPointer.c_str(), count);
}

int interactive_control_get_property_data(interactive_session session, const char* controlId, size_t index, char* propName, size_t* propNameLength, interactive_property_type* propType)
{
	if (nullptr == session || nullptr == controlId || nullptr == propNameLength || nullptr == propType)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	*propType = interactive_property_type::interactive_unknown_t;
	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	// Validate connection state.
	if (interactive_connected > sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	std::shared_lock<std::shared_mutex> scenesReadLock(sessionInternal->scenesMutex);
	auto controlItr = sessionInternal->controls.find(controlId);
	if (controlItr == sessionInternal->controls.end())
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	const std::string& controlPointer = controlItr->second.cachePointer;
	return get_scene_object_prop_data(*sessionInternal, controlPointer.c_str(), index, propName, propNameLength, propType);
}

int interactive_control_get_meta_property_data(interactive_session session, const char* controlId, size_t index, char* propName, size_t* propNameLength, interactive_property_type* propType)
{
	if (nullptr == session || nullptr == controlId || nullptr == propNameLength || nullptr == propType)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	*propType = interactive_property_type::interactive_unknown_t;
	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	// Validate connection state.
	if (interactive_connected > sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	std::shared_lock<std::shared_mutex> scenesReadLock(sessionInternal->scenesMutex);
	auto controlItr = sessionInternal->controls.find(std::string(controlId));
	if (controlItr == sessionInternal->controls.end())
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::string controlPointer = controlItr->second.cachePointer + "/" + RPC_METADATA;
	interactive_property_type valueType = interactive_property_type::interactive_unknown_t;
	int err = get_scene_object_prop_data(*sessionInternal, controlPointer.c_str(), index, propName, propNameLength, &valueType);
	if (MIXER_OK == err)
	{
		// Metadata properties are stored in a sub-"value" for some reason.
		controlPointer = controlPointer + "/" + propName;
		char value[6]; // To store "value"
		size_t valueLength = sizeof(value);
		return get_scene_object_prop_data(*sessionInternal, controlPointer.c_str(), 0, value, &valueLength, propType);
	}

	return err;
}

int interactive_control_get_property_int(interactive_session session, const char* controlId, const char* key, int* property)
{
	return interactive_control_get_property<int>(session, controlId, key, property);
}

int interactive_control_get_property_int64(interactive_session session, const char* controlId, const char* key, long long* property)
{
	return interactive_control_get_property<long long>(session, controlId, key, property);
}

int interactive_control_get_property_bool(interactive_session session, const char* controlId, const char* key, bool* property)
{
	return interactive_control_get_property<bool>(session, controlId, key, property);
}

int interactive_control_get_property_float(interactive_session session, const char* controlId, const char* key, float* property)
{
	return interactive_control_get_property<float>(session, controlId, key, property);
}

int interactive_control_get_property_string(interactive_session session, const char* controlId, const char* key, char* property, size_t* propertyLength)
{
	if (*propertyLength > 0 && nullptr == property)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	std::string stringProp;
	RETURN_IF_FAILED(interactive_control_get_property<std::string>(session, controlId, key, &stringProp));

	if (*propertyLength < stringProp.length() + 1)
	{
		*propertyLength = stringProp.length() + 1;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	if (0 != stringProp.length())
	{
		memcpy(property, stringProp.c_str(), *propertyLength - 1);
	}

	property[*propertyLength - 1] = '\0';

	return MIXER_OK;
}

int interactive_control_set_property_null(interactive_session session, const char* controlId, const char* key)
{
	rapidjson::Value val(rapidjson::kNullType);
	return interactive_control_set_property<rapidjson::Value&>(session, controlId, key, val);
}

int interactive_control_set_property_int(interactive_session session, const char* controlId, const char* key, int property)
{
	return interactive_control_set_property<int>(session, controlId, key, property);
}

int interactive_control_set_property_int64(interactive_session session, const char* controlId, const char* key, long long property)
{
	return interactive_control_set_property<long long>(session, controlId, key, property);
}

int interactive_control_set_property_bool(interactive_session session, const char* controlId, const char* key, bool property)
{
	return interactive_control_set_property<bool>(session, controlId, key, property);
}

int interactive_control_set_property_float(interactive_session session, const char* controlId, const char* key, float property)
{
	return interactive_control_set_property<float>(session, controlId, key, property);
}

int interactive_control_set_property_string(interactive_session session, const char* controlId, const char* key, const char* property)
{
	return interactive_control_set_property<std::string>(session, controlId, key, property);
}

int interactive_control_get_meta_property_int(interactive_session session, const char* controlId, const char* key, int* property)
{
	return interactive_control_get_meta_property(session, controlId, key, property);
}

int interactive_control_get_meta_property_int64(interactive_session session, const char* controlId, const char* key, long long* property)
{
	return interactive_control_get_meta_property(session, controlId, key, property);
}

int interactive_control_get_meta_property_bool(interactive_session session, const char* controlId, const char* key, bool* property)
{
	return interactive_control_get_meta_property(session, controlId, key, property);
}

int interactive_control_get_meta_property_float(interactive_session session, const char* controlId, const char* key, float* property)
{
	return interactive_control_get_meta_property(session, controlId, key, property);
}

int interactive_control_get_meta_property_string(interactive_session session, const char* controlId, const char* key, char* property, size_t* propertyLength)
{	
	if (*propertyLength > 0 && nullptr == property)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	std::string stringProp;
	RETURN_IF_FAILED(interactive_control_get_meta_property(session, controlId, key, &stringProp));
	

	if (nullptr == property || *propertyLength < stringProp.length() + 1)
	{
		*propertyLength = stringProp.length() + 1;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	if (0 != stringProp.length())
	{
		memcpy(property, stringProp.c_str(), *propertyLength - 1);
	}

	property[*propertyLength - 1] = '\0';

	return MIXER_OK;
}