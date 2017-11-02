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
#include "mixer_web_socket_client.h"
#include "mixer_web_socket_connection_state.h"

NAMESPACE_MICROSOFT_MIXER_BEGIN

class web_socket_connection : public std::enable_shared_from_this<web_socket_connection>
{
public:
	web_socket_connection(
		_In_ string_t bearerToken,
		_In_ string_t interactiveVersion,
		_In_ string_t sharecode,
		_In_ string_t protocolVersion
	);

	// ensure_connected() will try to connect if you didn't start one. 
	void ensure_connected();

	pplx::task<void> send(
		_In_ const string_t& message
	);

	pplx::task<void> close();

	//set a manual URI
	void set_uri(_In_ web::uri uri);

	// current connection state of the web_socket_connection
	mixer_web_socket_connection_state state();

	void set_received_handler(
		_In_ std::function<void(string_t)> handler
	);

	void set_connection_state_change_handler(
		_In_ std::function<void(mixer_web_socket_connection_state oldState, mixer_web_socket_connection_state newState)> handler
	);

	pplx::task<void>& connection_task() { return m_connectingTask; }

private:

	//Internal connection logic
	pplx::task<void> connect_async();

	void set_state_helper(_In_ mixer_web_socket_connection_state newState);

	// Close callback. It could because of client call, network issue or service termination
	void on_close(uint16_t code, string_t reason);

	const string_t convert_web_socket_connection_state_to_string(_In_ mixer_web_socket_connection_state state);

	std::shared_ptr<mixer_web_socket_client> m_client;
	web::uri m_uri;
	string_t m_subProtocol;
	string_t m_bearerToken;
	string_t m_interactiveVersion;
	string_t m_sharecode;
	string_t m_protocolVersion;

	std::recursive_mutex m_stateLocker;
	mixer_web_socket_connection_state m_state;

	pplx::task<void> m_connectingTask;

	std::function<void(mixer_web_socket_connection_state oldState, mixer_web_socket_connection_state newState)> m_externalStateChangeHandler;

	int m_connectionAttempts;

	bool m_closeCallbackSet;
	bool m_connectionActive;
	bool m_closeRequested;
};

NAMESPACE_MICROSOFT_MIXER_END
