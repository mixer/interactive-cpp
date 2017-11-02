//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#pragma once
#include "interactivity.h"
#include "interactivity_types.h"
#include <chrono>

//
// property names for the json RPC protocol
//

// top-level
#define RPC_ETAG                _XPLATSTR("etag")
#define RPC_ID                  _XPLATSTR("id")
#define RPC_DISCARD             _XPLATSTR("discard")
#define RPC_RESULT              _XPLATSTR("result")
#define RPC_TYPE                _XPLATSTR("type")
#define RPC_REPLY               _XPLATSTR("reply")
#define RPC_METHOD              _XPLATSTR("method")
#define RPC_PARAMS              _XPLATSTR("params")
#define RPC_METADATA            _XPLATSTR("meta")
#define RPC_ERROR               _XPLATSTR("error")
#define RPC_ERROR_CODE          _XPLATSTR("code")
#define RPC_ERROR_MESSAGE       _XPLATSTR("message")
#define RPC_ERROR_PATH          _XPLATSTR("path")
#define RPC_DISABLED            _XPLATSTR("disabled")

// RPC methods and replies
#define RPC_METHOD_READY                _XPLATSTR("ready")   // equivalent to "start_interactive" or "goInteractive")
#define RPC_METHOD_ON_READY_CHANGED     _XPLATSTR("onReady") // called by server to both game and participant clients
#define RPC_METHOD_ON_GROUP_CREATE      _XPLATSTR("onGroupCreate")
#define RPC_METHOD_ON_GROUP_UPDATE      _XPLATSTR("onGroupUpdate")
#define RPC_METHOD_ON_CONTROL_UPDATE    _XPLATSTR("onControlUpdate") 
#define RPC_METHOD_GET_TIME             _XPLATSTR("getTime")
#define RPC_TIME                        _XPLATSTR("time")
#define RPC_PARAM_IS_READY              _XPLATSTR("isReady")
#define RPC_METHOD_GIVE_INPUT           _XPLATSTR("giveInput")

#define RPC_GET_PARTICIPANTS            _XPLATSTR("getAllParticipants")
#define RPC_GET_PARTICIPANTS_BLOCK_SIZE 100 // returns up to 100 participants
#define RPC_METHOD_PARTICIPANTS_JOINED  _XPLATSTR("onParticipantJoin")
#define RPC_METHOD_PARTICIPANTS_LEFT    _XPLATSTR("onParticipantLeave")
#define RPC_PARTICIPANTS_ON_UPDATE      _XPLATSTR("onParticipantUpdate")
#define RPC_METHOD_PARTICIPANTS_ACTIVE  _XPLATSTR("getActiveParticipants")
#define RPC_METHOD_PARTICIPANTS_UPDATE  _XPLATSTR("updateParticipants")
#define RPC_PARAM_PARTICIPANTS          _XPLATSTR("participants")
#define RPC_PARAM_PARTICIPANT           _XPLATSTR("participant")
#define RPC_PARAM_PARTICIPANTS_ACTIVE_THRESHOLD _XPLATSTR("threshold") // unix milliseconds timestamp

#define RPC_METHOD_GET_SCENES           _XPLATSTR("getScenes")
#define RPC_METHOD_UPDATE_SCENES        _XPLATSTR("updateScenes")
#define RPC_PARAM_SCENES                _XPLATSTR("scenes")
#define RPC_METHOD_GET_GROUPS           _XPLATSTR("getGroups")
#define RPC_METHOD_UPDATE_GROUPS        _XPLATSTR("updateGroups") // updating the sceneID associated with the group is how you set the current scene
#define RPC_PARAM_GROUPS                _XPLATSTR("groups")
#define RPC_METHOD_CREATE_CONTROLS      _XPLATSTR("createControls")
#define RPC_METHOD_UPDATE_CONTROLS      _XPLATSTR("updateControls")
#define RPC_PARAM_CONTROLS              _XPLATSTR("controls")
#define RPC_METHOD_UPDATE_PARTICIPANTS  _XPLATSTR("updateParticipants")

#define RPC_METHOD_ON_INPUT         _XPLATSTR("giveInput")
#define RPC_PARAM_INPUT             _XPLATSTR("input")
#define RPC_PARAM_CONTROL           _XPLATSTR("control")
#define RPC_PARAM_INPUT_EVENT       _XPLATSTR("event")
#define RPC_INPUT_EVENT_BUTTON_DOWN _XPLATSTR("mousedown")
#define RPC_INPUT_EVENT_BUTTON_UP   _XPLATSTR("mouseup")
#define RPC_INPUT_EVENT_MOVE        _XPLATSTR("move")
#define RPC_INPUT_EVENT_MOVE_X      _XPLATSTR("x")
#define RPC_INPUT_EVENT_MOVE_Y      _XPLATSTR("y")
#define RPC_PARAM_TRANSACTION       _XPLATSTR("transaction")
#define RPC_PARAM_TRANSACTION_ID    _XPLATSTR("transactionID")
#define RPC_SPARK_COST              _XPLATSTR("cost")
#define RPC_METHOD_CAPTURE          _XPLATSTR("capture")

// groups
#define RPC_METHOD_CREATE_GROUPS    _XPLATSTR("createGroups")
#define RPC_METHOD_UPDATE_GROUPS    _XPLATSTR("updateGroups")
#define RPC_GROUP_ID                _XPLATSTR("groupID")
#define RPC_GROUP_DEFAULT           _XPLATSTR("default")

// scenes
#define RPC_SCENE_ID                _XPLATSTR("sceneID")
#define RPC_SCENE_DEFAULT           _XPLATSTR("default")
#define RPC_SCENE_CONTROLS          _XPLATSTR("controls")

// controls
#define RPC_CONTROL_ID              _XPLATSTR("controlID")
#define RPC_TRANSACTION_ID          _XPLATSTR("transactionID")
#define RPC_CONTROL_KIND            _XPLATSTR("kind")
#define RPC_CONTROL_BUTTON          _XPLATSTR("button")
#define RPC_CONTROL_BUTTON_TYPE     _XPLATSTR("button") // repeat, but for clarity - this corresponds to the javascript mouse event button types (0-4)
#define RPC_CONTROL_BUTTON_TEXT     _XPLATSTR("text")
#define RPC_CONTROL_BUTTON_PROGRESS _XPLATSTR("progress")
#define RPC_CONTROL_BUTTON_COOLDOWN _XPLATSTR("cooldown")
#define RPC_CONTROL_JOYSTICK        _XPLATSTR("joystick")
#define RPC_CONTROL_EVENT           _XPLATSTR("event")
#define RPC_CONTROL_UP              _XPLATSTR("mouseup")
#define RPC_CONTROL_DOWN            _XPLATSTR("mousedown")
#define RPC_JOYSTICK_MOVE           _XPLATSTR("move")
#define RPC_JOYSTICK_X              _XPLATSTR("x")
#define RPC_JOYSTICK_Y              _XPLATSTR("y")
#define RPC_VALUE                   _XPLATSTR("value")

// participants
#define RPC_SESSION_ID      _XPLATSTR("sessionID")
#define RPC_USER_ID         _XPLATSTR("userID")
#define RPC_PARTICIPANT_ID  _XPLATSTR("participantID")
#define RPC_XUID            _XPLATSTR("xuid")
#define RPC_USERNAME        _XPLATSTR("username")
#define RPC_LEVEL           _XPLATSTR("level")
#define RPC_PART_LAST_INPUT _XPLATSTR("lastInputAt")
#define RPC_PART_CONNECTED  _XPLATSTR("connectedAt")
#define RPC_GROUP_ID        _XPLATSTR("groupID")
#define RPC_GROUP_DEFAULT   _XPLATSTR("default")

namespace Microsoft
{
namespace mixer
{


static std::chrono::milliseconds unix_timestamp_in_ms()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
}

static std::shared_ptr<interactive_rpc_message> build_mediator_rpc_message(uint32_t messageId, const string_t& methodName, web::json::value params, bool discard)
{
	web::json::value messageJson;

	messageJson[RPC_TYPE] = web::json::value::string(RPC_METHOD);
	messageJson[RPC_ID] = web::json::value::number(messageId);
	messageJson[RPC_METHOD] = web::json::value::string(methodName);
	messageJson[RPC_PARAMS] = params;
	messageJson[RPC_DISCARD] = web::json::value::boolean(discard);

	std::shared_ptr<interactive_rpc_message> message = std::shared_ptr<interactive_rpc_message>(new interactive_rpc_message(messageId, messageJson, unix_timestamp_in_ms()));

	return message;
}

static web::json::value build_participant_state_change_mock_data(uint32_t messageId, uint32_t mixerId, string_t username, string_t participantSessionId, bool isJoin, bool discard)
{
	web::json::value messageJson;
	web::json::value params;
	web::json::value participants;
	web::json::value participant;

	participant[RPC_ID] = web::json::value::number(mixerId);
	participant[RPC_USERNAME] = web::json::value::string(username);
	participant[RPC_SESSION_ID] = web::json::value::string(participantSessionId);
	participant[RPC_ETAG] = web::json::value::string(_XPLATSTR("0"));
	participant[RPC_GROUP_ID] = web::json::value::string(RPC_GROUP_DEFAULT);
	participant[RPC_PART_LAST_INPUT] = web::json::value::number(0);
	participant[RPC_LEVEL] = web::json::value::number(0);

	if (isJoin)
	{
		participant[RPC_PART_CONNECTED] = web::json::value::number(static_cast<int64_t>(unix_timestamp_in_ms().count()));
	}

	participants[0] = participant;

	params[RPC_PARAM_PARTICIPANTS] = participants;

	messageJson[RPC_TYPE] = web::json::value::string(RPC_METHOD);
	messageJson[RPC_ID] = web::json::value::number(messageId);
	messageJson[RPC_METHOD] = isJoin ? web::json::value::string(RPC_METHOD_PARTICIPANTS_JOINED) : web::json::value::string(RPC_METHOD_PARTICIPANTS_LEFT);
	messageJson[RPC_PARAMS] = params;
	messageJson[RPC_DISCARD] = web::json::value::boolean(discard);

	return messageJson;
}

//
// Base input message JSON object - not including the control-type-specific info like [x,y] coordinates or control id
//
static web::json::value build_mock_input_base(uint32_t messageId, string_t methodName, bool discard, string_t participantSessionId)
{
	web::json::value mockInput;
	mockInput[RPC_TYPE] = web::json::value::string(RPC_METHOD);
	mockInput[RPC_METHOD] = web::json::value::string(methodName);
	mockInput[RPC_DISCARD] = web::json::value::boolean(discard);
	mockInput[RPC_ID] = web::json::value::number(messageId);

	web::json::value params;
	params[RPC_ID] = web::json::value::string(participantSessionId);

	mockInput[RPC_PARAMS] = params;

	return mockInput;
}

static web::json::value build_mock_button_input(uint32_t messageId, string_t participantSessionId, string_t controlId, bool isDown)
{
	web::json::value mockInput = build_mock_input_base(messageId, RPC_METHOD_GIVE_INPUT, false, participantSessionId);

	web::json::value inputJson;
	inputJson[RPC_CONTROL_ID] = web::json::value::string(controlId);
	inputJson[RPC_PARAM_INPUT_EVENT] = isDown ? web::json::value::string(RPC_INPUT_EVENT_BUTTON_UP) : web::json::value::string(RPC_INPUT_EVENT_BUTTON_DOWN);
	mockInput[RPC_PARAMS][RPC_PARAM_INPUT] = inputJson;

	return mockInput;
}

static web::json::value builde_mock_joystick_input(uint32_t messageId, string_t participantSessionId, string_t controlId, double x, double y)
{
	web::json::value mockInput = build_mock_input_base(messageId, RPC_METHOD_GIVE_INPUT, false, participantSessionId);

	web::json::value inputJson;
	inputJson[RPC_CONTROL_ID] = web::json::value::string(controlId);
	inputJson[RPC_JOYSTICK_X] = x;
	inputJson[RPC_JOYSTICK_Y] = y;
	mockInput[RPC_PARAMS][RPC_PARAM_INPUT] = inputJson;

	return mockInput;
}
}
}