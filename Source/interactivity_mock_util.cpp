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
#include "beam.h"
#include "beam_internal.h"
#include "json_helper.h"

NAMESPACE_MICROSOFT_XBOX_BEAM_BEGIN
static std::mutex s_singletonLock;

std::shared_ptr<beam_mock_util>
    beam_mock_util::get_singleton_instance()
{
    std::lock_guard<std::mutex> guard(s_singletonLock);
    static std::shared_ptr<beam_mock_util> instance;
    if (instance == nullptr)
    {
        instance = std::shared_ptr<beam_mock_util>(new beam_mock_util());
    }

    return instance;
}

void
beam_mock_util::set_oauth_token(_In_ string_t token)
{
    string_t fullTokenString = L"Bearer ";
    fullTokenString.append(token);
    m_beamManagerImpl->m_accessToken = fullTokenString;
}

void
beam_mock_util::mock_button_event(_In_ uint32_t beamId, _In_ string_t buttonId, _In_ bool is_down)
{
    m_beamManagerImpl->mock_button_event(beamId, buttonId, is_down);
}

void beam_mock_util::mock_participant_join(_In_ uint32_t participantId, _In_ string_t participantUsername)
{
    m_beamManagerImpl->mock_participant_join(participantId, participantUsername);
}

void beam_mock_util::mock_participant_leave(_In_ uint32_t participantId, _In_ string_t participantUsername)
{
    m_beamManagerImpl->mock_participant_join(participantId, participantUsername);
}

beam_mock_util::beam_mock_util()
{
    m_beamManagerImpl = beam_manager::get_singleton_instance()->m_impl;
}

NAMESPACE_MICROSOFT_XBOX_BEAM_END