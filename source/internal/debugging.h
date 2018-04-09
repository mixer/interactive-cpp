#pragma once
#include "interactivity.h"

namespace mixer
{

static on_debug_msg g_dbgCallback = nullptr;
static interactive_debug_level g_dbgLevel = interactive_debug_level::debug_none;

#define _DEBUG_IF_LEVEL(x, y) do { if (y <= g_dbgLevel && nullptr != g_dbgCallback) { g_dbgCallback(y, std::string(x).c_str(), std::string(x).length()); } } while (0)
#define DEBUG_ERROR(x) _DEBUG_IF_LEVEL(x, interactive_debug_level::debug_error)
#define DEBUG_WARNING(x) _DEBUG_IF_LEVEL(x, interactive_debug_level::debug_warning)
#define DEBUG_INFO(x) _DEBUG_IF_LEVEL(x, interactive_debug_level::debug_info)
#define DEBUG_TRACE(x) _DEBUG_IF_LEVEL(x, interactive_debug_level::debug_trace)

}

