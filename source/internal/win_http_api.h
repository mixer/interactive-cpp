#pragma once

#include "win_dll.h"
#include "winhttp.h"

namespace mixer_internal
{

class win_http_api
{
public:
	win_http_api();
	decltype(WinHttpOpen) *win_http_open;
	decltype(WinHttpCloseHandle) *win_http_close_handle;
	decltype(WinHttpConnect) *win_http_connect;
	decltype(WinHttpWebSocketCompleteUpgrade) *win_http_websocket_complete_upgrade;
	decltype(WinHttpSetTimeouts) *win_http_set_timeouts;
	decltype(WinHttpWebSocketClose) *win_http_websocket_close;
	decltype(WinHttpSendRequest) *win_http_send_request;
	decltype(WinHttpWebSocketReceive) *win_http_websocket_receive;
	decltype(WinHttpSetOption) *win_http_set_options;
	decltype(WinHttpOpenRequest) *win_http_open_request;
	decltype(WinHttpReadData) *win_http_open_read_data;
	decltype(WinHttpWebSocketSend) *win_http_websocket_send;
	decltype(WinHttpQueryHeaders) *win_http_query_headers;
	decltype(WinHttpAddRequestHeaders) *win_http_add_request_headers;
	decltype(WinHttpWebSocketQueryCloseStatus) *win_http_websocket_query_close_status;
	decltype(WinHttpReceiveResponse) *win_http_receive_response;
	decltype(WinHttpReadData) *win_http_read_data;
};
}