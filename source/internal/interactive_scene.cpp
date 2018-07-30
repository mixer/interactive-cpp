#include "interactive_session.h"
#include "common.h"

namespace mixer_internal
{

int update_control_pointers(interactive_session_internal& session, const char* sceneId)
{
	std::unique_lock<std::shared_mutex> scenesLock(session.scenesMutex);
	// Iterate through each scene and set up a pointer to each control.
	int sceneIndex = 0;
	for (auto& scene : session.scenesRoot[RPC_PARAM_SCENES].GetArray())
	{
		std::string scenePointer = "/" + std::string(RPC_PARAM_SCENES) + "/" + std::to_string(sceneIndex++);
		auto thisSceneId = scene[RPC_SCENE_ID].GetString();
		if (nullptr != sceneId)
		{
			if (0 != strcmp(sceneId, thisSceneId))
			{
				continue;
			}
		}

		auto controlsArray = scene.FindMember(RPC_PARAM_CONTROLS);
		if (controlsArray != scene.MemberEnd() && controlsArray->value.IsArray())
		{
			int controlIndex = 0;
			for (auto& control : controlsArray->value.GetArray())
			{
				interactive_control_pointer ctrl(thisSceneId, scenePointer + "/" + std::string(RPC_PARAM_CONTROLS) + "/" + std::to_string(controlIndex++));
				session.controls.emplace(control[RPC_CONTROL_ID].GetString(), std::move(ctrl));
			}
		}

		session.scenes.emplace(thisSceneId, scenePointer);
	}

	return MIXER_OK;
}

int cache_scenes(interactive_session_internal& session)
{
	DEBUG_INFO("Caching scenes.");

	RETURN_IF_FAILED(queue_method(session, RPC_METHOD_GET_SCENES, nullptr, [](interactive_session_internal& session, rapidjson::Document& doc) -> int
	{
		if (session.shutdownRequested)
		{
			return MIXER_OK;
		}

		// Critical Section: Get the scenes array from the result and set up pointers to scenes and controls.
		{
			std::unique_lock<std::shared_mutex> l(session.scenesMutex);
			session.controls.clear();
			session.scenes.clear();
			session.scenesRoot.RemoveAllMembers();

			// Copy just the scenes array portion of the reply into the cached scenes root.
			rapidjson::Value scenesArray(rapidjson::kArrayType);
			rapidjson::Value replyScenesArray = doc[RPC_RESULT][RPC_PARAM_SCENES].GetArray();
			scenesArray.CopyFrom(replyScenesArray, session.scenesRoot.GetAllocator());
			session.scenesRoot.AddMember(RPC_PARAM_SCENES, scenesArray, session.scenesRoot.GetAllocator());
		}

		RETURN_IF_FAILED(update_control_pointers(session));
		if (!session.scenesCached)
		{
			session.scenesCached = true;
			return bootstrap(session);
		}

		return MIXER_OK;
	}));

	return MIXER_OK;
}

}

int interactive_get_scenes(interactive_session session, on_scene_enumerate onScene)
{
	if (nullptr == session || nullptr == onScene)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	// Validate connection state.
	if (interactive_connected > sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	std::vector<interactive_object_internal> scenes;
	// Critical Section: Enumerate the scenes and create interactive_scene objects for each.
	{
		std::shared_lock<std::shared_mutex> l(sessionInternal->scenesMutex);
		for (auto sceneItr = sessionInternal->scenes.begin(); sessionInternal->scenes.end() != sceneItr; ++sceneItr)
		{
			// Construct an interactive_scene object to pass to the caller.
			scenes.push_back({ sceneItr->first });
		}
	}

	for (const interactive_object_internal& sceneInternal : scenes)
	{
		interactive_scene scene;
		scene.id = sceneInternal.id.c_str();
		scene.idLength = sceneInternal.id.length();
		onScene(sessionInternal->callerContext, sessionInternal, &scene);
	}

	return MIXER_OK;
}

int interactive_scene_get_groups(interactive_session session, const char* sceneId, on_group_enumerate onGroup)
{
	if (nullptr == session || nullptr == onGroup)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	if (interactive_connected > sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	std::vector<interactive_group_internal> groups;
	// Critical Section: Enumerate all groups for the specified scene.
	{
		std::shared_lock<std::shared_mutex> l(sessionInternal->scenesMutex);
		auto sceneItr = sessionInternal->scenes.find(sceneId);
		if (sessionInternal->scenes.end() == sceneItr)
		{
			return MIXER_ERROR_OBJECT_NOT_FOUND;
		}

		// Find the cached scene and enumerate all groups.
		rapidjson::Value* sceneVal = rapidjson::Pointer(sceneItr->second.c_str()).Get(sessionInternal->scenesRoot);
		if (sceneVal->HasMember(RPC_PARAM_GROUPS) && (*sceneVal)[RPC_PARAM_GROUPS].IsArray() && !(*sceneVal)[RPC_PARAM_GROUPS].Empty())
		{
			for (auto& groupObj : (*sceneVal)[RPC_PARAM_GROUPS].GetArray())
			{
				groups.push_back({ groupObj[RPC_GROUP_ID].GetString(), sceneId });
			}
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

int interactive_scene_get_controls(interactive_session session, const char* sceneId, on_control_enumerate onControl)
{
	if (nullptr == session || nullptr == onControl)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	if (interactive_connected > sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	std::vector<interactive_control_internal> controls;
	// Critical Section: Enumerate all controls on the scene.
	{
		std::shared_lock<std::shared_mutex> l(sessionInternal->scenesMutex);
		auto sceneItr = sessionInternal->scenes.find(sceneId);
		if (sessionInternal->scenes.end() == sceneItr)
		{
			return MIXER_ERROR_OBJECT_NOT_FOUND;
		}

		// Find the cached scene and enumerate all controls.
		rapidjson::Value* sceneVal = rapidjson::Pointer(sceneItr->second.c_str()).Get(sessionInternal->scenesRoot);
		if (sceneVal->HasMember(RPC_PARAM_CONTROLS) && (*sceneVal)[RPC_PARAM_CONTROLS].IsArray() && !(*sceneVal)[RPC_PARAM_CONTROLS].Empty())
		{
			for (auto& controlObj : (*sceneVal)[RPC_PARAM_CONTROLS].GetArray())
			{
				if (!controlObj.IsObject())
				{
					continue;
				}

				if (!controlObj.HasMember(RPC_CONTROL_ID) || !controlObj[RPC_CONTROL_ID].IsString())
				{
					continue;
				}

				controls.push_back({ controlObj[RPC_CONTROL_ID].GetString(), controlObj[RPC_CONTROL_KIND].GetString() });
			}
		}
	}

	for (const interactive_control_internal& controlInternal : controls)
	{
		interactive_control control;
		control.id = controlInternal.id.c_str();
		control.idLength = controlInternal.id.length();
		control.kind = controlInternal.kind.c_str();
		control.kindLength = controlInternal.kind.length();
		onControl(sessionInternal->callerContext, sessionInternal, &control);
	}

	return MIXER_OK;
}