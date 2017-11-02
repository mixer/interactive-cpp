//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "pch.h"
#include "mixer_debug.h"

NAMESPACE_MICROSOFT_MIXER_BEGIN

static on_debug_msg g_dbgCallback = nullptr;
static mixer_debug_level g_dbgLevel = mixer_debug_level::none;

void mixer_config_debug_level(const mixer_debug_level dbgLevel)
{
	g_dbgLevel = dbgLevel;
}

void mixer_config_debug(const mixer_debug_level dbgLevel, const on_debug_msg dbgCallback)
{
	g_dbgLevel = dbgLevel;
	g_dbgCallback = dbgCallback;
}

void mixer_debug(const mixer_debug_level level, const string_t& message)
{
	if (nullptr == g_dbgCallback || level > g_dbgLevel)
	{
		return;
	}

	g_dbgCallback(level, message);
}

string_t convertStr(const char* str)
{
	if (nullptr == str || 0 == *str)
	{
		return string_t();
	}

#if _WIN32 || UNICODE
	size_t bufferSize = strlen(str) + 1;
	string_t wcs;
	wcs.resize(bufferSize);
	mbstowcs_s(&bufferSize, &wcs[0], bufferSize, str, bufferSize);
	return wcs;
#else
	return string_t(str);
#endif
}

NAMESPACE_MICROSOFT_MIXER_END