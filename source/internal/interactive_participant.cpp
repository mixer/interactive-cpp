#include "interactive_session.h"
#include "interactive_batch.h"
#include "common.h"

namespace mixer_internal
{

void parse_participant(rapidjson::Value& participantJson, interactive_participant& participant)
{
	participant.id = participantJson[RPC_SESSION_ID].GetString();
	participant.idLength = participantJson[RPC_SESSION_ID].GetStringLength();
	participant.userId = participantJson[RPC_USER_ID].GetUint();
	participant.userName = participantJson[RPC_USERNAME].GetString();
	participant.usernameLength = participantJson[RPC_USERNAME].GetStringLength();
	participant.level = participantJson[RPC_LEVEL].GetUint();
	participant.lastInputAtMs = participantJson[RPC_PART_LAST_INPUT].GetUint64();
	participant.connectedAtMs = participantJson[RPC_PART_CONNECTED].GetUint64();
	participant.disabled = participantJson[RPC_DISABLED].GetBool();
	participant.groupId = participantJson[RPC_GROUP_ID].GetString();
	participant.groupIdLength = participantJson[RPC_GROUP_ID].GetStringLength();
}

}

using namespace mixer_internal;

int interactive_get_participants(interactive_session session, on_participant_enumerate onParticipant)
{
	if (nullptr == session || nullptr == onParticipant)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	// Validate connection state.
	if (interactive_disconnected == sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	for (auto& participantById : sessionInternal->participants)
	{
		interactive_participant participant;
		parse_participant(*participantById.second, participant);
		onParticipant(sessionInternal->callerContext, sessionInternal, &participant);
	}

	return MIXER_OK;
}

int interactive_participant_set_group(interactive_session session, const char* participantId, const char* groupId)
{
	if (nullptr == session || nullptr == participantId || nullptr == groupId)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	RETURN_IF_FAILED(queue_method(*sessionInternal, RPC_METHOD_UPDATE_PARTICIPANTS, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		rapidjson::Value participants(rapidjson::kArrayType);
		rapidjson::Value participant(rapidjson::kObjectType);
		participant.AddMember(RPC_SESSION_ID, std::string(participantId), allocator);
		participant.AddMember(RPC_GROUP_ID, std::string(groupId), allocator);
		participants.PushBack(participant, allocator);
		params.AddMember(RPC_PARAM_PARTICIPANTS, participants, allocator);
		params.AddMember("priority", 0, allocator);
	}, nullptr));

	return MIXER_OK;
}

int interactive_participant_get_user_id(interactive_session session, const char* participantId, unsigned int* userId)
{
	if (nullptr == session || nullptr == participantId || nullptr == userId)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	// Validate connection state.
	if (interactive_disconnected == sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	auto participantItr = sessionInternal->participants.find(std::string(participantId));
	if (sessionInternal->participants.end() == participantItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::shared_ptr<rapidjson::Document> participantDoc = participantItr->second;
	*userId = (*participantDoc)[RPC_USER_ID].GetUint();
	return MIXER_OK;
}

int interactive_participant_get_user_name(interactive_session session, const char* participantId, char* userName, size_t* userNameLength)
{
	if (nullptr == session || nullptr == participantId || nullptr == userNameLength)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	// Validate connection state.
	if (interactive_disconnected == sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	auto participantItr = sessionInternal->participants.find(std::string(participantId));
	if (sessionInternal->participants.end() == participantItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::shared_ptr<rapidjson::Document> participantDoc = participantItr->second;

	size_t actualLength = (*participantDoc)[RPC_USERNAME].GetStringLength();
	if (nullptr == userName || *userNameLength < actualLength + 1)
	{
		*userNameLength = actualLength + 1;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	memcpy(userName, (*participantDoc)[RPC_USERNAME].GetString(), actualLength);
	userName[actualLength] = 0;
	*userNameLength = actualLength + 1;
	return MIXER_OK;
}

int interactive_participant_get_level(interactive_session session, const char* participantId, unsigned int* level)
{
	if (nullptr == session || nullptr == participantId || nullptr == level)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	// Validate connection state.
	if (interactive_disconnected == sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	auto participantItr = sessionInternal->participants.find(std::string(participantId));
	if (sessionInternal->participants.end() == participantItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::shared_ptr<rapidjson::Document> participantDoc = participantItr->second;
	*level = (*participantDoc)[RPC_LEVEL].GetUint();
	return MIXER_OK;
}

int interactive_participant_get_last_input_at(interactive_session session, const char* participantId, unsigned long long* lastInputAt)
{
	if (nullptr == session || nullptr == participantId || nullptr == lastInputAt)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	// Validate connection state.
	if (interactive_disconnected == sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	auto participantItr = sessionInternal->participants.find(std::string(participantId));
	if (sessionInternal->participants.end() == participantItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::shared_ptr<rapidjson::Document> participantDoc = participantItr->second;
	*lastInputAt = (*participantDoc)[RPC_PART_LAST_INPUT].GetUint64();
	return MIXER_OK;
}

int interactive_participant_get_connected_at(interactive_session session, const char* participantId, unsigned long long* connectedAt)
{
	if (nullptr == session || nullptr == participantId || nullptr == connectedAt)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	// Validate connection state.
	if (interactive_disconnected == sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	auto participantItr = sessionInternal->participants.find(std::string(participantId));
	if (sessionInternal->participants.end() == participantItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::shared_ptr<rapidjson::Document> participantDoc = participantItr->second;
	*connectedAt = (*participantDoc)[RPC_PART_CONNECTED].GetUint64();
	return MIXER_OK;
}

int interactive_participant_is_disabled(interactive_session session, const char* participantId, bool* isDisabled)
{
	if (nullptr == session || nullptr == participantId || nullptr == isDisabled)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	// Validate connection state.
	if (interactive_disconnected == sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	auto participantItr = sessionInternal->participants.find(std::string(participantId));
	if (sessionInternal->participants.end() == participantItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::shared_ptr<rapidjson::Document> participantDoc = participantItr->second;
	*isDisabled = (*participantDoc)[RPC_DISABLED].GetBool();
	return MIXER_OK;
}

int interactive_participant_get_group(interactive_session session, const char* participantId, char* group, size_t* groupLength)
{
	if (nullptr == session || nullptr == participantId || nullptr == groupLength)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	// Validate connection state.
	if (interactive_disconnected == sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	auto participantItr = sessionInternal->participants.find(std::string(participantId));
	if (sessionInternal->participants.end() == participantItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::shared_ptr<rapidjson::Document> participantDoc = participantItr->second;
	size_t actualLength = (*participantDoc)[RPC_GROUP_ID].GetStringLength();
	if (nullptr == group || *groupLength < actualLength + 1)
	{
		*groupLength = actualLength + 1;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	memcpy(group, (*participantDoc)[RPC_GROUP_ID].GetString(), actualLength);
	group[actualLength] = 0;
	*groupLength = actualLength + 1;
	return MIXER_OK;
}

int interactive_participant_batch_add(interactive_batch batch, interactive_batch_entry* entry, const char* participantId)
{
	RETURN_IF_FAILED(interactive_batch_add_entry(batch, entry, RPC_PARAM_PARTICIPANTS));

	RETURN_IF_FAILED(interactive_batch_add_param_str(&entry->obj, RPC_SESSION_ID, participantId));

	return MIXER_OK;
}

int interactive_participant_batch_commit(interactive_batch batch)
{
	if (nullptr == batch)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_batch_internal* batchInternal = reinterpret_cast<interactive_batch_internal*>(batch);
	return interactive_batch_commit(batchInternal);
}

int interactive_participant_get_param_string(interactive_session session, const char * participantId, const char *paramName, char* value, size_t* valueLength)
{
	if (nullptr == session || nullptr == participantId || nullptr == valueLength)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	// Validate connection state.
	if (interactive_disconnected == sessionInternal->state)
	{
		return MIXER_ERROR_NOT_CONNECTED;
	}

	auto participantItr = sessionInternal->participants.find(std::string(participantId));
	if (sessionInternal->participants.end() == participantItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::shared_ptr<rapidjson::Document> participantDoc = participantItr->second;

	size_t actualLength = (*participantDoc)[paramName].GetStringLength();
	if (nullptr == value || *valueLength < actualLength + 1)
	{
		*valueLength = actualLength + 1;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	memcpy(value, (*participantDoc)[paramName].GetString(), actualLength);
	value[actualLength] = 0;
	*valueLength = actualLength + 1;
	return MIXER_OK;
}