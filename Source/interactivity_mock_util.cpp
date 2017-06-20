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
#include "interactivity.h"
#include "interactivity_internal.h"
#include "json_helper.h"

NAMESPACE_MICROSOFT_MIXER_BEGIN
static std::mutex s_singletonLock;

std::shared_ptr<interactivity_mock_util>
interactivity_mock_util::get_singleton_instance()
{
    std::lock_guard<std::mutex> guard(s_singletonLock);
    static std::shared_ptr<interactivity_mock_util> instance;
    if (instance == nullptr)
    {
        instance = std::shared_ptr<interactivity_mock_util>(new interactivity_mock_util());
    }

    return instance;
}

void
interactivity_mock_util::set_oauth_token(_In_ string_t token)
{
    string_t fullTokenString = L"Bearer ";
    fullTokenString.append(token);
	m_interactiveManagerImpl->m_accessToken = fullTokenString;
}

void
interactivity_mock_util::mock_button_event(_In_ uint32_t beamId, _In_ string_t buttonId, _In_ bool is_down)
{
	m_interactiveManagerImpl->mock_button_event(beamId, buttonId, is_down);
}

void interactivity_mock_util::mock_participant_join(_In_ uint32_t participantId, _In_ string_t participantUsername)
{
	m_interactiveManagerImpl->mock_participant_join(participantId, participantUsername);
}

void interactivity_mock_util::mock_participant_leave(_In_ uint32_t participantId, _In_ string_t participantUsername)
{
	m_interactiveManagerImpl->mock_participant_join(participantId, participantUsername);
}

interactivity_mock_util::interactivity_mock_util()
{
	m_interactiveManagerImpl = interactivity_manager::get_singleton_instance()->m_impl;
}

NAMESPACE_MICROSOFT_MIXER_END