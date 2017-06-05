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

NAMESPACE_MICROSOFT_MIXER_BEGIN

enum class mixer_web_socket_connection_state : int32_t
{
    disconnected,
    activated,
    connecting,
    connected,
};

NAMESPACE_MICROSOFT_MIXER_END