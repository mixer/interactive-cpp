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
#include "beam.h"
#include "beam_types.h"
#include <chrono>

//
// property names for the json RPC protocol
// TODO: for cross-platform, may have to define these values differently
//

// top-level
#define RPC_ETAG                L"etag"
#define RPC_ID                  L"id"
#define RPC_DISCARD             L"discard"
#define RPC_RESULT              L"result"
#define RPC_TYPE                L"type"
#define RPC_REPLY               L"reply"
#define RPC_METHOD              L"method"
#define RPC_PARAMS              L"params"
#define RPC_METADATA            L"meta"
#define RPC_ERROR               L"error"
#define RPC_ERROR_CODE          L"code"
#define RPC_ERROR_MESSAGE       L"message"
#define RPC_ERROR_PATH          L"path"
#define RPC_DISABLED            L"disabled"

// RPC methods and replies
#define RPC_METHOD_READY                L"ready"   // equivalent to "start_interactive" or "goInteractive"
#define RPC_METHOD_ON_READY_CHANGED     L"onReady" // called by server to both game and participant clients
#define RPC_METHOD_ON_GROUP_CREATE      L"onGroupCreate"
#define RPC_METHOD_ON_GROUP_UPDATE      L"onGroupUpdate"
#define RPC_METHOD_ON_CONTROL_UPDATE    L"onControlUpdate" 
#define RPC_METHOD_GET_TIME             L"getTime"
#define RPC_TIME                        L"time"
#define RPC_PARAM_IS_READY              L"isReady"
#define RPC_METHOD_GIVE_INPUT           L"giveInput"

#define RPC_GET_PARTICIPANTS            L"getAllParticipants"
#define RPC_GET_PARTICIPANTS_BLOCK_SIZE 100 // returns up to 100 participants
#define RPC_METHOD_PARTICIPANTS_JOINED  L"onParticipantJoin"
#define RPC_METHOD_PARTICIPANTS_LEFT    L"onParticipantLeave"
#define RPC_PARTICIPANTS_ON_UPDATE      L"onParticipantUpdate"
#define RPC_METHOD_PARTICIPANTS_ACTIVE  L"getActiveParticipants"
#define RPC_METHOD_PARTICIPANTS_UPDATE  L"updateParticipants"
#define RPC_PARAM_PARTICIPANTS          L"participants"
#define RPC_PARAM_PARTICIPANT           L"participant"
#define RPC_PARAM_PARTICIPANTS_ACTIVE_THRESHOLD L"threshold" // unix milliseconds timestamp

#define RPC_METHOD_GET_SCENES           L"getScenes"
#define RPC_METHOD_UPDATE_SCENES        L"updateScenes"
#define RPC_PARAM_SCENES                L"scenes"
#define RPC_METHOD_GET_GROUPS           L"getGroups"
#define RPC_METHOD_UPDATE_GROUPS        L"updateGroups" // updating the sceneID associated with the group is how you set the current scene
#define RPC_PARAM_GROUPS                L"groups"
#define RPC_METHOD_CREATE_CONTROLS      L"createControls"
#define RPC_METHOD_UPDATE_CONTROLS      L"updateControls"
#define RPC_PARAM_CONTROLS              L"controls"
#define RPC_METHOD_UPDATE_PARTICIPANTS  L"updateParticipants"

#define RPC_METHOD_ON_INPUT         L"giveInput"
#define RPC_PARAM_INPUT             L"input"
#define RPC_PARAM_CONTROL           L"control"
#define RPC_PARAM_INPUT_EVENT       L"event"
#define RPC_INPUT_EVENT_BUTTON_DOWN L"mousedown"
#define RPC_INPUT_EVENT_BUTTON_UP   L"mouseup"
#define RPC_INPUT_EVENT_MOVE        L"move"
#define RPC_INPUT_EVENT_MOVE_X      L"x"
#define RPC_INPUT_EVENT_MOVE_Y      L"y"
#define RPC_PARAM_TRANSACTION       L"transaction"
#define RPC_PARAM_TRANSACTION_ID    L"transactionID"
#define RPC_SPARK_COST              L"cost"
#define RPC_METHOD_CAPTURE          L"capture"

// groups
#define RPC_METHOD_CREATE_GROUPS    L"createGroups"
#define RPC_METHOD_UPDATE_GROUPS    L"updateGroups"
#define RPC_GROUP_ID                L"groupID"
#define RPC_GROUP_DEFAULT           L"default"

// scenes
#define RPC_SCENE_ID                L"sceneID"
#define RPC_SCENE_DEFAULT           L"default"
#define RPC_SCENE_CONTROLS          L"controls"

// controls
#define RPC_CONTROL_ID              L"controlID"
#define RPC_CONTROL_KIND            L"kind"
#define RPC_CONTROL_BUTTON          L"button"
#define RPC_CONTROL_BUTTON_TYPE     L"button" // repeat, but for clarity - this corresponds to the javascript mouse event button types (0-4)
#define RPC_CONTROL_BUTTON_TEXT     L"text"
#define RPC_CONTROL_BUTTON_PROGRESS L"progress"
#define RPC_CONTROL_BUTTON_COOLDOWN L"cooldown"
#define RPC_CONTROL_JOYSTICK        L"joystick"
#define RPC_CONTROL_EVENT           L"event"
#define RPC_CONTROL_UP              L"mouseup"
#define RPC_CONTROL_DOWN            L"mousedown"
#define RPC_JOYSTICK_MOVE           L"move"
#define RPC_JOYSTICK_X              L"x"
#define RPC_JOYSTICK_Y              L"y"

// participants
#define RPC_SESSION_ID      L"sessionID"
#define RPC_BEAM_ID         L"userID"
#define RPC_PARTICIPANT_ID  L"participantID"
#define RPC_XUID            L"xuid"
#define RPC_BEAM_USERNAME   L"username"
#define RPC_BEAM_LEVEL      L"level"
#define RPC_PART_LAST_INPUT L"lastInputAt"
#define RPC_PART_CONNECTED  L"connectedAt"
#define RPC_GROUP_ID        L"groupID"
#define RPC_GROUP_DEFAULT   L"default"

namespace xbox { namespace services { namespace beam {


    static std::chrono::milliseconds unix_timestamp_in_ms()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    }

    static std::shared_ptr<beam_rpc_message> build_mediator_rpc_message(uint32_t messageId, const string_t& methodName, web::json::value params, bool discard)
    {
        web::json::value messageJson;

        messageJson[RPC_TYPE] = web::json::value::string(RPC_METHOD);
        messageJson[RPC_ID] = web::json::value::number(messageId);
        messageJson[RPC_METHOD] = web::json::value::string(methodName);
        messageJson[RPC_PARAMS] = params;
        messageJson[RPC_DISCARD] = web::json::value::boolean(discard);

        std::shared_ptr<beam_rpc_message> message = std::shared_ptr<beam_rpc_message>(new beam_rpc_message(messageId, messageJson, unix_timestamp_in_ms()));

        return message;
    }

    static web::json::value build_participant_state_change_mock_data(uint32_t messageId, uint32_t beamId, string_t beamUsername, string_t participantSessionId, bool isJoin, bool discard)
    {
        web::json::value messageJson;
        web::json::value params;
        web::json::value participants;
        web::json::value participant;

        participant[RPC_BEAM_ID] = web::json::value::number(beamId);
        participant[RPC_BEAM_USERNAME] = web::json::value::string(beamUsername);
        participant[RPC_SESSION_ID] = web::json::value::string(participantSessionId);
        participant[RPC_ETAG] = web::json::value::string(L"0");
        participant[RPC_GROUP_ID] = web::json::value::string(RPC_GROUP_DEFAULT);
        participant[RPC_PART_LAST_INPUT] = web::json::value::number(0);
        participant[RPC_BEAM_LEVEL] = web::json::value::number(0);

        if (isJoin)
        {
            participant[RPC_PART_CONNECTED] = web::json::value::number(unix_timestamp_in_ms().count());
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
        mockInput[RPC_TYPE] = web::json::value::string(L"method");
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
}}}