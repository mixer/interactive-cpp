#include "win_http_api.h"
#include "win_dll.h"
namespace mixer_internal
{

static win_dll winHttpDll{ "winhttp.dll" };
win_http_api::win_http_api() : 
	win_http_open(winHttpDll["WinHttpOpen"]),
	win_http_close_handle(winHttpDll["WinHttpCloseHandle"]),
	win_http_connect(winHttpDll["WinHttpConnect"]),
	win_http_websocket_complete_upgrade(winHttpDll["WinHttpWebSocketCompleteUpgrade"]),
	win_http_set_timeouts(winHttpDll["WinHttpSetTimeouts"]),
	win_http_websocket_close(winHttpDll["WinHttpWebSocketClose"]),
	win_http_send_request(winHttpDll["WinHttpSendRequest"]),
	win_http_websocket_receive(winHttpDll["WinHttpWebSocketReceive"]),
	win_http_set_options(winHttpDll["WinHttpSetOption"]),
	win_http_open_request(winHttpDll["WinHttpOpenRequest"]),
	win_http_open_read_data(winHttpDll["WinHttpReadData"]),
	win_http_websocket_send(winHttpDll["WinHttpWebSocketSend"]),
	win_http_query_headers(winHttpDll["WinHttpQueryHeaders"]),
	win_http_add_request_headers(winHttpDll["WinHttpAddRequestHeaders"]),
	win_http_websocket_query_close_status(winHttpDll["WinHttpWebSocketQueryCloseStatus"]),
	win_http_receive_response(winHttpDll["WinHttpReceiveResponse"]),
	win_http_read_data(winHttpDll["WinHttpReadData"]) {};
}