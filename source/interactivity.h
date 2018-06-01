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

/** @mainpage Mixer Interactive C++ SDK Documentation

This is the API documentation for the Mixer C++ SDK. For a detailed walkthrough of specific features and usage visit https://github.com/mixer/interactive-cpp/wiki.

@ref Interactivity "API Documentation"
*/

/** @defgroup Interactivity Mixer Interactive C++ SDK
* @{
*/

extern "C" {

	/** @name Structures
	*   @{
	*/
	struct interactive_object
	{
		const char* id;
		size_t idLength;
	};
	/** @} */

	/** @name Authorization
	*   @{
	*/
	/// <summary>
	/// Get a short code that can be used to obtain an OAuth token from <c>https://www.mixer.com/go?code=SHORTCODE</c>.
	/// </summary>
	/// <remarks>
	/// This is a blocking function that waits on network IO.
	/// </remarks>
	int interactive_auth_get_short_code(const char* clientId, const char* clientSecret, char* shortCode, size_t* shortCodeLength, char* shortCodeHandle, size_t* shortCodeHandleLength);

	/// <summary>
	/// Wait for a <c>shortCode</c> to be authorized or rejected after presenting the OAuth short code web page. The resulting <c>refreshToken</c>
	/// should be securely serialized and linked to the current user.
	/// </summary>
	/// <remarks>
	/// This is a blocking function that waits on network IO.
	/// </remarks>
	int interactive_auth_wait_short_code(const char* clientId, const char* clientSecret, const char* shortCodeHandle, char* refreshToken, size_t* refreshTokenLength);

	/// <summary>
	/// Determine if a <c>refreshToken</c> returned by <c>interactive_auth_wait_short_code</c> is stale. A token is stale if it has exceeded its half-life.
	/// </summary>
	int interactive_auth_is_token_stale(const char* token, bool* isStale);

	/// <summary>
	/// Refresh a stale <c>refreshToken<c>.
	/// </summary>
	/// <remarks>
	/// This is a blocking function that waits on network IO.
	/// </remarks>
	int interactive_auth_refresh_token(const char* clientId, const char* clientSecret, const char* staleToken, char* refreshToken, size_t* refreshTokenLength);

	/// <summary>
	/// Parse a <c>refreshToken</c> to get the authorization header that should be passed to <c>interactive_open_session()</c>.
	/// </summary>
	int interactive_auth_parse_refresh_token(const char* token, char* authorization, size_t* authorizationLength);
	/** @} */

	/** @name Session
	*   @{
	*/
	/// <summary>
	/// An opaque handle to an interactive session.
	/// </summary>
	typedef void* interactive_session;	

	/// <summary>
	/// Open an <c>interactive_session</c>.
	/// <param name="session">A pointer to an <c>interactive_session</c> that will be allocated internally.</param>
	/// <remarks>
	/// All calls to <c>interactive_open_session</c> must eventually be followed by a call to <c>interactive_close_session</c> to free the handle.
	/// </remarks>
	/// </summary>
	int interactive_open_session(interactive_session* session);

	/// <summary>
	/// Open an interactive session. All calls to <c>interactive_open_session</c> must eventually be followed by a call to <c>interactive_close_session</c> to avoid a memory leak.
	/// </summary>
	/// <param name="session">An interactive session handle opened with <c>interactive_open_session</c>.</param>
	/// <param name="auth">The authorization header that is passed to the service. This should either be a OAuth Bearer token or an XToken.</param>
	/// <param name="versionId">The id of the interative project that should be started.</param>
	/// <param name="shareCode">An optional parameter that is used when starting an interactive project that the user does not have implicit access to. This is usually required unless a project has been published.</param>
	/// <param name="setReady">Specifies if the session should set the interactive ready state during connection. If false, this can be manually toggled later with <c>interactive_set_ready</c></param>
	/// <remarks>
	/// This is a blocking function that waits on network IO, it is not recommended to call this from the UI thread.
	/// </remarks>
	int interactive_connect(interactive_session session, const char* auth, const char* versionId, const char* shareCode, bool setReady);

	/// <summary>
	/// Set the ready state for specified session. No participants will be able to see interactive scenes or give input
	/// until the interactive session is ready.
	/// </summary>
	/// <remarks>
	/// This is a blocking function that waits on network IO.
	/// </remarks>
	int interactive_set_ready(interactive_session session, bool isReady);

	/// <summary>
	/// This function processes the specified number of events from the interactive service and calls back on registered event handlers.
	/// </summary>
	/// <remarks>
	/// This should be called often, at least once per frame, so that interactive input is processed in a timely manner.
	/// </remarks>
	int interactive_run(interactive_session session, unsigned int maxEventsToProcess);

	enum interactive_state
	{
		interactive_disconnected,
		interactive_not_ready,
		interactive_ready
	};

	/// <summary>
	/// Get the current <c>interactive_state</c> for the specified session.
	/// </summary>
	int interactive_get_state(interactive_session session, interactive_state* state);

	/// <summary>
	/// Set a session context that will be passed to every event callback. This context pointer is not read or written by this library, it's purely to enable your code to track state between calls and callbacks if necessary.
	/// </summary>
	int interactive_set_session_context(interactive_session session, void* context);

	/// <summary>
	/// Get the previously set session context. Context will be nullptr on return if no context has been set.
	/// </summary>
	int interactive_get_session_context(interactive_session session, void** context);

	enum interactive_throttle_type
	{
		throttle_global,
		throttle_input,
		throttle_participant_join,
		throttle_participant_leave
	};

	/// <summary>
	/// Set a throttle for server to client messages on this interactive session. 
	/// <remarks>
	/// There is a global throttle on all interactive sessions of 30 megabits and 10 megabits per second by default.
	/// </remarks>
	/// </summary>
	int interactive_set_bandwidth_throttle(interactive_session session, interactive_throttle_type throttleType, unsigned int maxBytes, unsigned int bytesPerSecond);

	/// <summary>
	/// Capture a transaction to charge a participant the input's spark cost. This should be called before
	/// taking further action on input as the participant may not have enough sparks or the transaction may have expired.
	/// Register an <c>on_transaction_complete</c> handler to execute actions on the participant's behalf.
	/// </summary>
	int interactive_capture_transaction(interactive_session session, const char* transactionId);

	/// <summary>
	/// Disconnect from an interactive session and clean up memory.
	/// </summary>
	/// <remarks>
	/// <para>This must not be called from inside an event handler as the lifetime of registered event handlers are assumed to outlive the session. Only call this when there is no thread processing events via <c>interactive_run</c></para>
	/// <para>This is a blocking function that waits on outstanding network IO, ensuring all operations are completed before returning. It is not recommended to call this function from the UI thread.</para>
	/// </remarks>
	void interactive_close_session(interactive_session session);
	/** @} */

	/** @name Controls
	*   @{
	*/

	struct interactive_control : public interactive_object
	{
		const char* kind;
		size_t kindLength;
	};


	// Known control properties
	#define CONTROL_PROP_DISABLED "disabled"
	#define CONTROL_PROP_POSITION "position"

	#define BUTTON_PROP_KEY_CODE "keyCode"
	#define BUTTON_PROP_TEXT "text"
	#define BUTTON_PROP_TOOLTIP "tooltip"
	#define BUTTON_PROP_COST "cost"
	#define BUTTON_PROP_PROGRESS "progress"
	#define BUTTON_PROP_COOLDOWN "cooldown"

	#define JOYSTICK_PROP_SAMPLE_RATE "sampleRate"
	#define JOYSTICK_PROP_ANGLE "angle"
	#define JOYSTICK_PROP_INTENSITY "intensity"

	typedef enum interactive_property_type
	{
		interactive_unknown_t,
		interactive_int_t,
		interactive_bool_t,
		interactive_float_t,
		interactive_string_t,
		interactive_array_t,
		interactive_object_t
	} interactive_property_type;

	typedef void(*on_control_enumerate)(void* context, interactive_session session, interactive_control* control);

	/// <summary>
	/// Trigger a cooldown on a control for the specified number of milliseconds.
	/// </summary>
	int interactive_control_trigger_cooldown(interactive_session session, const char* controlId, const unsigned long long cooldownMs);

	/// <summary>
	/// Get the number of properties on the control.
	/// </summary>
	int interactive_control_get_property_count(interactive_session session, const char* controlId, size_t* count);

	/// <summary>
	/// Get the name and type of the property at the given index.
	/// </summary>
	int interactive_control_get_property_data(interactive_session session, const char* controlId, size_t index, char* propName, size_t* propNameLength, interactive_property_type* type);

	/// <summary>
	/// Get the number of meta properties on the control.
	/// </summary>
	int interactive_control_get_meta_property_count(interactive_session session, const char* controlId, size_t* count);

	/// <summary>
	/// Get the name and type of the meta property at the given index.
	/// </summary>
	int interactive_control_get_meta_property_data(interactive_session session, const char* controlId, size_t index, char* propName, size_t* propNameLength, interactive_property_type* type);

	/// <summary>
	/// Get an <c>int</c> property value by name.
	/// </summary>
	int interactive_control_get_property_int(interactive_session session, const char* controlId, const char* key, int* property);

	/// <summary>
	/// Get a <c>long long</c> property value by name.
	/// </summary>
	int interactive_control_get_property_int64(interactive_session session, const char* controlId, const char* key, long long* property);

	/// <summary>
	/// Get a <c>bool</c> property value by name.
	/// </summary>
	int interactive_control_get_property_bool(interactive_session session, const char* controlId, const char* key, bool* property);

	/// <summary>
	/// Get a <c>float</c> property value by name.
	/// </summary>
	int interactive_control_get_property_float(interactive_session session, const char* controlId, const char* key, float* property);

	/// <summary>
	/// Get a <c>char*</c> property value by name.
	/// </summary>
	int interactive_control_get_property_string(interactive_session session, const char* controlId, const char* key, char* property, size_t* propertyLength);

	/// <summary>
	/// Get an <c>int</c> meta property value by name.
	/// </summary>
	int interactive_control_get_meta_property_int(interactive_session session, const char* controlId, const char* key, int* property);

	/// <summary>
	/// Get a <c>long long</c> meta property value by name.
	/// </summary>
	int interactive_control_get_meta_property_int64(interactive_session session, const char* controlId, const char* key, long long* property);

	/// <summary>
	/// Get a <c>bool</c> meta property value by name.
	/// </summary>
	int interactive_control_get_meta_property_bool(interactive_session session, const char* controlId, const char* key, bool* property);

	/// <summary>
	/// Get a <c>float</c> meta property value by name.
	/// </summary>
	int interactive_control_get_meta_property_float(interactive_session session, const char* controlId, const char* key, float* property);

	/// <summary>
	/// Get a <c>char*</c> meta property value by name.
	/// </summary>
	int interactive_control_get_meta_property_string(interactive_session session, const char* controlId, const char* key, char* property, size_t* propertyLength);
	/** @} */

	/** @name Groups
	*   @{
	*/

	struct interactive_group : public interactive_object
	{
		const char* sceneId;
		size_t sceneIdLength;
	};

	/// <summary>
	/// Callback for <c>interactive_get_groups</c>.
	/// </summary>
	typedef void(*on_group_enumerate)(void* context, interactive_session session, interactive_group* group);

	/// <summary>
	/// Get the interactive groups for the specified session.
	/// </summary>
	int interactive_get_groups(interactive_session session, on_group_enumerate onGroup);

	/// <summary>
	/// Create a new group for the specified session with the specified scene for participants in this group. If no scene is specified it will be set
	/// to the default scene.
	/// </summary>
	int interactive_create_group(interactive_session session, const char* groupId, const char* sceneId);

	/// <summary>
	/// Set a group's scene for the specified session. Use this and <c>interative_set_participant_group</c> to manage which scenes participants see.
	/// </summary>
	int interactive_group_set_scene(interactive_session session, const char* groupId, const char* sceneId);
	/** @} */

	/** @name Scenes
	*   @{
	*/

	struct interactive_scene : public interactive_object
	{
	};

	/// <summary>
	/// Callback for <c>interactive_get_scenes</c>
	/// </summary>
	typedef void(*on_scene_enumerate)(void* context, interactive_session session, interactive_scene* scene);

	/// <summary>
	/// Get all scenes for the specified session.
	/// </summary>
	int interactive_get_scenes(interactive_session session, on_scene_enumerate onScene);

	/// <summary>
	/// Get each group that this scene belongs to for the specified session.
	/// </summary>
	int interactive_scene_get_groups(interactive_session session, const char* sceneId, on_group_enumerate onGroup);

	/// <summary>
	/// Get a scene's controls for the specified session.
	/// </summary>
	int interactive_scene_get_controls(interactive_session session, const char* sceneId, on_control_enumerate onControl);
	/** @} */

	/** @name Events and Handlers
	*   @{
	*/
	/// <summary>
	/// Callback for any errors that may occur during the session.
	/// </summary>
	/// <remarks>
	/// This is the only callback function that may be called from a thread other than the thread that calls <c>interactive_run</c>.
	/// </remarks>
	typedef void(*on_error)(void* context, interactive_session session, int errorCode, const char* errorMessage, size_t errorMessageLength);

	/// <summary>
	/// Callback when the interactive state changes between disconnected, ready, and not ready.
	/// </summary>
	typedef void(*on_state_changed)(void* context, interactive_session session, interactive_state previousState, interactive_state newState);

	enum interactive_input_type
	{
		input_type_key,
		input_type_click,
		input_type_move,
		input_type_custom
	};

	enum interactive_button_action
	{
		interactive_button_action_up,
		interactive_button_action_down
	};

	struct interactive_input
	{
		interactive_control control;
		interactive_input_type type;
		const char* participantId;
		size_t participantIdLength;
		const char* jsonData;
		size_t jsonDataLength;
		const char* transactionId;
		size_t transactionIdLength;
		struct buttonData
		{
			interactive_button_action action;
		} buttonData;
		struct coordinateData
		{
			float x;
			float y;
		} coordinateData;
	};

	/// <summary>
	/// Callback when an interactive participant gives input during the session.
	/// </summary>
	typedef void(*on_input)(void* context, interactive_session session, const interactive_input* input);

	struct interactive_participant : public interactive_object
	{
		unsigned int userId;
		const char* userName;
		size_t usernameLength;
		unsigned int level;
		unsigned long long lastInputAtMs;
		unsigned long long connectedAtMs;
		bool disabled;
		const char* groupId;
		size_t groupIdLength;
	};

	enum interactive_participant_action
	{
		participant_join,
		participant_leave,
		participant_update
	};

	/// <summary>
	/// Callback when an interactive participant changes.
	/// </summary>
	typedef void(*on_participants_changed)(void* context, interactive_session session, interactive_participant_action action, const interactive_participant* participant);

	/// <summary>
	/// Callback when a transaction completes.
	/// </summary>
	typedef void(*on_transaction_complete)(void* context, interactive_session session, const char* transactionId, size_t transactionIdLength, unsigned int error, const char* errorMessage, size_t errorMessageLength);

	/// <summary>
	/// Callback when any method is called that is not handled by the existing callbacks. This may be useful for more advanced scenarios or future protocol changes that may not have existed in this version of the library.
	/// </summary>
	typedef void(*on_unhandled_method)(void* context, interactive_session session, const char* methodJson, size_t methodJsonLength);

	/// <summary>
	/// Set the handler function for errors. This function may be called by a background thread so be careful when updating UI.
	/// </summary>
	int interactive_set_error_handler(interactive_session session, on_error onError);

	/// <summary>
	/// Set the handler function for state changes. This function is called by your own thread during <c>interactive_run</c>
	/// </summary>
	int interactive_set_state_changed_handler(interactive_session session, on_state_changed onStateChanged);

	/// <summary>
	/// Set the handler function for interactive input. This function is called by your own thread during <c>interactive_run</c>
	/// </summary>
	int interactive_set_input_handler(interactive_session session, on_input onInput);

	/// <summary>
	/// Set the handler function for participants changing. This function is called by your own thread during <c>interactive_run</c>
	/// </summary>
	int interactive_set_participants_changed_handler(interactive_session session, on_participants_changed onParticipantsChanged);

	/// <summary>
	/// Set the handler function for spark transaction completion. This function is called by your own thread during <c>interactive_run</c>
	/// </summary>
	int interactive_set_transaction_complete_handler(interactive_session session, on_transaction_complete onTransactionComplete);

	/// <summary>
	/// Set the handler function for unhandled methods. This may be useful for more advanced scenarios or future protocol changes that may not have existed in this version of the library. This function is called by your own thread during <c>interactive_run</c>
	/// </summary>
	int interactive_set_unhandled_method_handler(interactive_session session, on_unhandled_method onUnhandledMethod);
	/** @} */

	/** @name Participants
	*   @{
	*/

	typedef void(*on_participant_enumerate)(void* context, interactive_session session, interactive_participant* participant);

	/// <summary>
	/// Get all participants (viewers) for the specified session.
	/// </summary>
	int interactive_get_participants(interactive_session session, on_participant_enumerate onParticipant);

	/// <summary>
	/// Change the participant's group Use this along with <c>interactive_group_set_scene</c> to configure which scene a participant sees. All participants join the 'default' (case sensitive) group when joining a session.
	/// </summary>
	int interactive_participant_set_group(interactive_session session, const char* participantId, const char* groupId);

	/// <summary>
	/// Get the participant's user id.
	/// </summary>
	int interactive_participant_get_user_id(interactive_session session, const char* participantId, unsigned int* userId);

	/// <summary>
	/// Get the participant's user name.
	/// </summary>
	int interactive_participant_get_user_name(interactive_session session, const char* participantId, char* userName, size_t* userNameLength);

	/// <summary>
	/// Get the participant's level.
	/// </summary>
	int interactive_participant_get_level(interactive_session session, const char* participantId, unsigned int* level);

	/// <summary>
	/// Get the participant's last input time as a unix millisecond timestamp.
	/// </summary>
	int interactive_participant_get_last_input_at(interactive_session session, const char* participantId, unsigned long long* lastInputAt);

	/// <summary>
	/// Get the participant's unix millisecond timestamp when they connected to the session.
	/// </summary>
	int interactive_participant_get_connected_at(interactive_session session, const char* participantId, unsigned long long* connectedAt);

	/// <summary>
	/// Is the participant's input disabled?
	/// </summary>
	int interactive_participant_is_disabled(interactive_session session, const char* participantId, bool* isDisabled);

	/// <summary>
	/// Get the participant's group name.
	/// </summary>
	int interactive_participant_get_group(interactive_session session, const char* participantId, char* group, size_t* groupLength);
	/** @} */

	/** @name Manual Protocol Integration
	*   @{
	*/
	/// <summary>
	/// Send a method to the interactive session. This may be used to interface with the interactive protocol directly and implement functionality 
	/// that this SDK does not provide out of the box.
	/// </summary>
	/// <remarks>
	/// This is a blocking function that waits on network IO.
	/// </remarks>
	int interactive_send_method(interactive_session session, const char* method, const char* paramsJson, bool discardReply, unsigned int* id);

	/// <summary>
	/// Recieve a reply for a method with the specified id. This may be used to interface with the interactive protocol directly and implement functionality
	/// that this SDK does not provide out of the box.
	/// </summary>
	/// <remarks>
	/// This is a blocking function that waits on network IO.
	/// </remarks>
	int interactive_receive_reply(interactive_session session, unsigned int id, unsigned int timeoutMs, char* replyJson, size_t* replyJsonLength);
	/** @} */

	/** @name Debugging
	*   @{
	*/
	enum interactive_debug_level
	{
		interactive_debug_none = 0,
		interactive_debug_error,
		interactive_debug_warning,
		interactive_debug_info,
		interactive_debug_trace
	};

	/// <summary>
	/// Callback whenever a debug event happens.
	/// </summary>
	typedef void(*on_debug_msg)(const interactive_debug_level dbgMsgType, const char* dbgMsg, size_t dbgMsgSize);

	/// <summary>
	/// Configure the debug verbosity for all interactive sessions in the current process.
	/// </summary>
	void interactive_config_debug_level(const interactive_debug_level dbgLevel);

	/// <summary>
	/// Configure the debug verbosity and set the debug callback function for all interactive sessions in the current process.
	/// </summary>
	void interactive_config_debug(const interactive_debug_level dbgLevel, on_debug_msg dbgCallback);
	/** @} */

	/** @name Error Codes
	*   @{
	*/
	typedef enum mixer_result_code
	{
		MIXER_OK,
		MIXER_ERROR,
		MIXER_ERROR_AUTH,
		MIXER_ERROR_AUTH_DENIED,
		MIXER_ERROR_AUTH_INVALID_TOKEN,
		MIXER_ERROR_BUFFER_SIZE,
		MIXER_ERROR_CANCELLED,
		MIXER_ERROR_HTTP,
		MIXER_ERROR_INIT,
		MIXER_ERROR_INVALID_CALLBACK,
		MIXER_ERROR_INVALID_CLIENT_ID,
		MIXER_ERROR_INVALID_OPERATION,
		MIXER_ERROR_INVALID_POINTER,
		MIXER_ERROR_INVALID_PROPERTY_TYPE,
		MIXER_ERROR_INVALID_VERSION_ID,
		MIXER_ERROR_JSON_PARSE,
		MIXER_ERROR_METHOD_CREATE,
		MIXER_ERROR_NO_HOST,
		MIXER_ERROR_NO_REPLY,
		MIXER_ERROR_OBJECT_NOT_FOUND,
		MIXER_ERROR_PROPERTY_NOT_FOUND,
		MIXER_ERROR_TIMED_OUT,
		MIXER_ERROR_UNKNOWN_METHOD,
		MIXER_ERROR_UNRECOGNIZED_DATA_FORMAT,
		MIXER_ERROR_WS_CLOSED,
		MIXER_ERROR_WS_CONNECT_FAILED,
		MIXER_ERROR_WS_DISCONNECT_FAILED,
		MIXER_ERROR_WS_READ_FAILED,
		MIXER_ERROR_WS_SEND_FAILED,
		MIXER_ERROR_NOT_CONNECTED
	} mixer_result_code;
	/** @} */

}

/** @} */
