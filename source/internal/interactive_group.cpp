#include "interactive_session.h"
#include "common.h"

namespace mixer_internal
{

interactive_group_internal::interactive_group_internal(std::string id, std::string scene) : interactive_object_internal(std::move(id)), scene(std::move(scene)) {}

int cache_groups(interactive_session_internal& session)
{
	DEBUG_INFO("Caching groups.");
	RETURN_IF_FAILED(queue_method(session, RPC_METHOD_GET_GROUPS, nullptr, [](interactive_session_internal& session, rapidjson::Document& reply) -> int
	{
		scenes_by_group scenesByGroup;
		rapidjson::Value& groups = reply[RPC_RESULT][RPC_PARAM_GROUPS];
		for (auto& group : groups.GetArray())
		{
			std::string groupId = group[RPC_GROUP_ID].GetString();
			std::string sceneId;
			if (group.HasMember(RPC_SCENE_ID))
			{
				sceneId = group[RPC_SCENE_ID].GetString();
			}

			scenesByGroup.emplace(groupId, sceneId);
		}

		// Critical Section: Cache groups and their scenes
		{
			std::unique_lock<std::shared_mutex> l(session.scenesMutex);
			session.scenesByGroup.swap(scenesByGroup);
		}

		if (!session.groupsCached)
		{
			session.groupsCached = true;
			return bootstrap(session);
		}

		return MIXER_OK;
	}));
	
	return MIXER_OK;
}

}

using namespace mixer_internal;

int interactive_get_groups(interactive_session session, on_group_enumerate onGroup)
{
	if (nullptr == session || nullptr == onGroup)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	// Validate connection state.
	if (interactive_connected > sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	std::vector<interactive_group_internal> groups;
	// Critical Section: Enumerate all groups
	{
		std::shared_lock<std::shared_mutex> l(sessionInternal->scenesMutex);
		for (auto& sceneByGroup : sessionInternal->scenesByGroup)
		{	
			groups.emplace_back(sceneByGroup.first, sceneByGroup.second);
		}
	}

	for (const interactive_group_internal& groupInternal : groups)
	{
		interactive_group group;
		group.id = groupInternal.id.c_str();
		group.idLength = groupInternal.id.length();
		group.sceneId = groupInternal.scene.c_str();
		group.sceneIdLength = groupInternal.scene.length();
		onGroup(sessionInternal->callerContext, sessionInternal, &group);
	}

	return MIXER_OK;
}

int interactive_create_group(interactive_session session, const char* groupId, const char* sceneId)
{
	if (nullptr == session || nullptr == groupId)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	if (interactive_connected > sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	std::string groupIdStr(groupId);
	std::string sceneIdStr = nullptr == sceneId ? RPC_SCENE_DEFAULT : sceneId;
	RETURN_IF_FAILED(queue_method(*sessionInternal, RPC_METHOD_CREATE_GROUPS, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		rapidjson::Value groups(rapidjson::kArrayType);
		rapidjson::Value group(rapidjson::kObjectType);
		group.AddMember(RPC_GROUP_ID, std::move(groupIdStr), allocator);
		group.AddMember(RPC_SCENE_ID, std::move(sceneIdStr), allocator);
		groups.PushBack(std::move(group), allocator);
		params.AddMember(RPC_PARAM_GROUPS, std::move(groups), allocator);
	}, nullptr));

	return MIXER_OK;
}

int interactive_group_set_scene(interactive_session session, const char* groupId, const char* sceneId)
{
	if (nullptr == session || nullptr == groupId || nullptr == sceneId)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	if (interactive_connected > sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	std::string groupIdStr(groupId);
	std::string sceneIdStr(sceneId);
	RETURN_IF_FAILED(queue_method(*sessionInternal, RPC_METHOD_UPDATE_GROUPS, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		rapidjson::Value groups(rapidjson::kArrayType);
		rapidjson::Value group(rapidjson::kObjectType);
		group.AddMember(RPC_GROUP_ID, std::move(groupIdStr), allocator);
		group.AddMember(RPC_SCENE_ID, std::move(sceneIdStr), allocator);
		groups.PushBack(std::move(group), allocator);
		params.AddMember(RPC_PARAM_GROUPS, std::move(groups), allocator);
	}, nullptr));

	return MIXER_OK;
}