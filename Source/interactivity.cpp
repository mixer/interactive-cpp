#include "common.cpp"
#include "http_client.cpp"
#include "interactive_auth.cpp"
#include "interactive_control.cpp"
#include "interactive_group.cpp"
#include "interactive_participant.cpp"
#include "interactive_scene.cpp"
#include "interactive_session.cpp"
#include "interactive_session_internal.cpp"
#if _DURANGO || WINAPI_PARTITION_APP
#include "simplewebsocketcpp/simplewebsocketUWP.cpp"
#elif _WIN32
#include "win_http_client.cpp"
#include "simplewebsocketcpp/simplewebsocketwin.cpp"
#endif