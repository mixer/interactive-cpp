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


uint32_t
beam_participant::beam_id() const
{
    return m_impl->beam_id();
}

const string_t&
beam_participant::beam_username() const
{
    return m_impl->beam_username();
}

uint32_t
beam_participant::beam_level() const
{
    return m_impl->beam_level();
}

const beam_participant_state
beam_participant::state() const
{
    return m_impl->state();
}

void
beam_participant::set_group(std::shared_ptr<beam_group> group)
{
    return m_impl->set_group(group);
}

const std::shared_ptr<beam_group>
beam_participant::group()
{
    return m_impl->group();
}

const std::chrono::milliseconds&
beam_participant::last_input_at() const
{
    return m_impl->last_input_at();
}

const std::chrono::milliseconds&
beam_participant::connected_at() const
{
    return m_impl->connected_at();
}

bool
beam_participant::input_disabled() const
{
    return m_impl->input_disabled();
}


beam_participant::beam_participant(
    unsigned int beamId,
    string_t username,
    uint32_t beamLevel,
    string_t groupId,
    std::chrono::milliseconds lastInputAt,
    std::chrono::milliseconds connectedAt,
    bool disabled
    )
{
    m_impl = std::make_shared<beam_participant_impl>(beamId, username, L"", beamLevel, groupId, lastInputAt, connectedAt, disabled);
}


beam_participant::beam_participant()
{
    m_impl = std::make_shared<beam_participant_impl>();
}

//
// Backing implementation
//

uint32_t
beam_participant_impl::beam_id() const
{
    return m_beamId;
}

const string_t&
beam_participant_impl::beam_username() const
{
    return m_username;
}

uint32_t
beam_participant_impl::beam_level() const
{
    return m_beamLevel;
}

const beam_participant_state
beam_participant_impl::state() const
{
    return m_state;
}

void
beam_participant_impl::set_group(std::shared_ptr<beam_group> group)
{
    string_t oldGroup = m_groupId;
    m_groupIdUpdated = true;
    m_groupId = group->group_id();
    m_beamManager->m_impl->move_participant_group(m_beamId, oldGroup, m_groupId);
}

const std::shared_ptr<beam_group>
beam_participant_impl::group()
{
    return m_beamManager->group(m_groupId);
}

const std::chrono::milliseconds&
beam_participant_impl::last_input_at() const
{
    return m_lastInputAt;
}

const std::chrono::milliseconds&
beam_participant_impl::connected_at() const
{
    return m_connectedAt;
}

bool
beam_participant_impl::input_disabled() const
{
    return m_disabled;
}

#if 0
std::shared_ptr<beam_button_control> xbox::services::beam::beam_participant_impl::button(const string_t & control_id)
{
    return std::shared_ptr<beam_button_control>();
}

std::vector<std::shared_ptr<beam_button_control>> xbox::services::beam::beam_participant_impl::buttons()
{
    return std::vector<std::shared_ptr<beam_button_control>>();
}

std::shared_ptr<beam_joystick_control> xbox::services::beam::beam_participant_impl::joystick(const string_t & control_id)
{
    return std::shared_ptr<beam_joystick_control>();
}

std::vector<std::shared_ptr<beam_joystick_control>> xbox::services::beam::beam_participant_impl::joysticks()
{
    return std::vector<std::shared_ptr<beam_joystick_control>>();
}
#endif


beam_participant_impl::beam_participant_impl(
    unsigned int beamId,
    string_t username,
    string_t sessionId,
    uint32_t beamLevel,
    string_t groupId,
    std::chrono::milliseconds lastInputAt,
    std::chrono::milliseconds connectedAt,
    bool disabled
) :
    m_beamId(std::move(beamId)),
    m_username(std::move(username)),
    m_sessionId(std::move(sessionId)),
    m_beamLevel(std::move(beamLevel)),
    m_groupId(std::move(groupId)),
    m_lastInputAt(std::move(lastInputAt)),
    m_connectedAt(std::move(connectedAt)),
    m_disabled(std::move(disabled)),
    m_groupIdUpdated(false),
    m_disabledUpdated(false),
    m_stateUpdated(false)
{
    m_beamManager = beam_manager::get_singleton_instance();
}

beam_participant_impl::beam_participant_impl() :
    m_groupId(RPC_GROUP_DEFAULT),
    m_disabled(false),
    m_groupIdUpdated(false),
    m_disabledUpdated(false),
    m_stateUpdated(false)
{
    m_beamManager = beam_manager::get_singleton_instance();
}

bool
beam_participant_impl::update(web::json::value json, bool overwrite)
{
    string_t errorString;
    bool success = true;

    try
    {
        if (success)
        {
            if (json.has_field(RPC_BEAM_ID))
            {
                m_beamId = json[RPC_BEAM_ID].as_number().to_uint32();
            }
            else if (overwrite)
            {
                errorString = L"Trying to construct participant, no id";
                success = false;
            }
        }

        if (success)
        {
            if (json.has_field(RPC_BEAM_USERNAME))
            {
                m_username = json[RPC_BEAM_USERNAME].as_string();
            }
            else if (overwrite)
            {
                errorString = L"Trying to construct participant, no username";
                success = false;
            }
        }

        if (success)
        {
            if (json.has_field(RPC_SESSION_ID))
            {
                m_sessionId = json[RPC_SESSION_ID].as_string();
            }
            else if (overwrite)
            {
                errorString = L"Trying to construct participant, no sessionId";
                success = false;
            }
        }

        if (success)
        {
            if (json.has_field(RPC_PART_CONNECTED))
            {
                auto connectedAtUnix = json[RPC_PART_CONNECTED].as_number().to_uint64();
                std::chrono::milliseconds connectedAtChrono(connectedAtUnix);

                m_connectedAt = connectedAtChrono;
            }
            else if (overwrite)
            {
                errorString = L"Trying to construct participant, no connectedAt timestamp";
                success = false;
            }
        }

        if (success)
        {
            if (json.has_field(RPC_PART_LAST_INPUT))
            {
                auto inputAtUnix = json[RPC_PART_LAST_INPUT].as_number().to_uint64();
                std::chrono::milliseconds inputAtChrono(inputAtUnix);

                m_connectedAt = inputAtChrono;
            }
            else if (overwrite)
            {
                errorString = L"Trying to construct participant, no lastInputAt timestamp";
                success = false;
            }
        }

        if (success)
        {
            if (json.has_field(RPC_ETAG))
            {
                if (0 == m_etag.compare(json[RPC_ETAG].as_string()))
                {
                    LOGS_ERROR << L"New etag same as old (" << m_etag << L")";
                }
                m_etag = json[RPC_ETAG].as_string();
            }
            else if (overwrite)
            {
                errorString = L"Trying to construct participant, no etag";
                success = false;
            }
        }

        if (success)
        {
            if (json.has_field(RPC_BEAM_LEVEL))
            {
                m_beamLevel = json[RPC_BEAM_LEVEL].as_number().to_uint32();
            }
            else if (overwrite)
            {
                errorString = L"Trying to construct participant, no beam level";
                success = false;
            }
        }

        if (success)
        {
            if (json.has_field(RPC_GROUP_ID))
            {
                m_groupId = json[RPC_GROUP_ID].as_string();
            }
            else if (overwrite)
            {
                errorString = L"Trying to construct participant, no beam group";
                success = false;
            }
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to parse participant json";
    }

    if (!success)
    {
        LOGS_ERROR << errorString;
    }

    return success;
}

bool xbox::services::beam::beam_participant_impl::init_from_json(web::json::value json)
{
    return update(json, true);
}


NAMESPACE_MICROSOFT_XBOX_BEAM_END

