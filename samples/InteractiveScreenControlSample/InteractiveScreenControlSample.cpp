#include "stdafx.h"
#include "../../source/interactivity.h"
#include "../../source/internal/rapidjson/Document.h"

#include <windows.h>
#include <shellapi.h>

// STL includes
#include <chrono>
#include <thread>
#include <iostream>
#include <map>
#include <string>

#define CLIENT_ID		"f0d20e2d263b75894f5cdaabc8a344b99b1ea6f9ecb7fa4f"
#define INTERACTIVE_ID	"247834"
#define SHARE_CODE		"v8t3hlaa"

#define MIXER_DEBUG 0

std::map<std::string, std::string> controlsByTransaction;

// Display an OAuth consent page to the user.
int authorize(std::string& authorization);

// Handle interactive input.
void handle_interactive_input(void* context, interactive_session session, const interactive_input* input);

int main()
{
	int err = 0;

#if MIXER_DEBUG
	interactive_config_debug(interactive_debug_trace, [](const interactive_debug_level dbgMsgType, const char* dbgMsg, size_t dbgMsgSize)
	{
		std::cout << dbgMsg << std::endl;
	});
#endif

	// Get an authorization token for the user to pass to the connect function.
	std::string authorization;
	err = authorize(authorization);
	if (err) return err;

	// Connect to the user's interactive channel, using the interactive project specified by the version ID.
	interactive_session session;
	err = interactive_open_session(authorization.c_str(), INTERACTIVE_ID, SHARE_CODE, false, &session);
	if (err) return err;

	// Register a callback for button presses.
	err = interactive_set_input_handler(session, handle_interactive_input);
	if (err) return err;

	// Now notify participants that interactive is ready.
	err = interactive_set_ready(session, true);
	if (err) return err;

	// Simulate game update loop. All previously registered session callbacks will be called from this thread.
	for (;;)
	{
		// This call processes any waiting messages from the interactive service. If there are no messages this returns immediately.
		err = interactive_run(session, 1);
		if (err) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}

	interactive_close_session(session);

	return err;
}

int authorize(std::string& authorization)
{
	int err = 0;
	char shortCode[7];
	size_t shortCodeLength = _countof(shortCode);
	char shortCodeHandle[1024];
	size_t shortCodeHandleLength = _countof(shortCodeHandle);

	// Get an OAuth short code from the user. For more information about OAuth see: https://oauth.net/2/
	err = interactive_auth_get_short_code(CLIENT_ID, nullptr, shortCode, &shortCodeLength, shortCodeHandle, &shortCodeHandleLength);
	if (err) return err;

	// Pop the browser for the user to approve access.
	std::string authUrl = std::string("https://www.mixer.com/go?code=") + shortCode;
	ShellExecuteA(0, 0, authUrl.c_str(), nullptr, nullptr, SW_SHOW);

	// Wait for OAuth token response.
	char refreshTokenBuffer[1024];
	size_t refreshTokenLength = _countof(refreshTokenBuffer);
	err = interactive_auth_wait_short_code(CLIENT_ID, nullptr, shortCodeHandle, refreshTokenBuffer, &refreshTokenLength);
	if (err)
	{
		if (MIXER_ERROR_TIMED_OUT == err)
		{
			std::cout << "Authorization timed out, user did not approve access within the time limit." << std::endl;
		}
		else if (MIXER_ERROR_AUTH_DENIED == err)
		{
			std::cout << "User denied access." << std::endl;
		}

		return err;
	}

	/*
	*	TODO:	This is where you would serialize the refresh token locally or on your own service for future use in a way that is associated with the current user.
	*			Future calls would then only need to check if the token is stale, refresh it if so, and then parse the new authorization header.
	*/

	// Extract the authorization header from the refresh token.
	char authBuffer[1024];
	size_t authBufferLength = _countof(authBuffer);
	err = interactive_auth_parse_refresh_token(refreshTokenBuffer, authBuffer, &authBufferLength);
	if (err) return err;

	// Set the authorization out parameter.
	authorization = std::string(authBuffer, authBufferLength);
	return 0;
}

int get_participant_name(interactive_session session, const char* participantId, std::string& participantName)
{
	// Get the participant's name.
	size_t participantNameLength = 0;

	// First call with a nullptr to get the required size for the user's name, MIXER_ERROR_BUFFER_SIZE is the expected return value.
	int err = interactive_participant_get_user_name(session, participantId, nullptr, &participantNameLength);
	if (MIXER_ERROR_BUFFER_SIZE != err)
	{
		return err;
	}

	// Resize the string to the correct size and call it again.
	participantName.resize(participantNameLength);
	err = interactive_participant_get_user_name(session, participantId, (char*)participantName.data(), &participantNameLength);
	// STL strings don't need a trailing null character.
	participantName = participantName.erase(participantNameLength - 1);

	return 0;
}

void handle_interactive_input(void* context, interactive_session session, const interactive_input* input)
{
	// Get the participant's Mixer name to give them attribution.
	std::string participantName;
	int err = get_participant_name(session, input->participantId, participantName);
	if (err)
	{
		std::cerr << "Failed to get participant user name (" << std::to_string(err) << ")" << std::endl;
		return;
	}

	// Now handle the input based on input type.
	if ((input_type_key == input->type || input_type_click == input->type) && interactive_button_action_down == input->buttonData.action)
	{	
		float x = input->coordinateData.x;
		float y = input->coordinateData.y;
		std::cout << participantName << " clicked x: " + std::to_string(x) << " y: " << std::to_string(y) << std::endl;
	}
	else if (input_type_move == input->type)
	{
		// Show which user moved the joystick.
		float x = input->coordinateData.x;
		float y = input->coordinateData.y;
		std::cout << participantName << " moved pointer to x: " + std::to_string(x) << " y: " << std::to_string(y) << std::endl;
	}
}
