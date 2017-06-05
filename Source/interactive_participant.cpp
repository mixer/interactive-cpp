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


uint32_t
interactive_participant::mixer_id() const
{
    return m_impl->mixer_id();
}

const string_t&
interactive_participant::username() const
{
    return m_impl->username();
}

uint32_t
interactive_participant::level() const
{
    return m_impl->level();
}

const interactive_participant_state
interactive_participant::state() const
{
    return m_impl->state();
}

void
interactive_participant::set_group(std::shared_ptr<interactive_group> group)
{
    return m_impl->set_group(group);
}

const std::shared_ptr<interactive_group>
interactive_participant::group()
{
    return m_impl->group();
}

const std::chrono::milliseconds&
interactive_participant::last_input_at() const
{
    return m_impl->last_input_at();
}

const std::chrono::milliseconds&
interactive_participant::connected_at() const
{
    return m_impl->connected_at();
}

bool
interactive_participant::input_disabled() const
{
    return m_impl->input_disabled();
}


interactive_participant::interactive_participant(
    unsigned int mixerId,
    string_t username,
    uint32_t level,
    string_t groupId,
    std::chrono::milliseconds lastInputAt,
    std::chrono::milliseconds connectedAt,
    bool disabled
    )
{
    m_impl = std::make_shared<interactive_participant_impl>(mixerId, username, L"", level, groupId, lastInputAt, connectedAt, disabled);
}


interactive_participant::interactive_participant()
{
    m_impl = std::make_shared<interactive_participant_impl>();
}

//
// Backing implementation
//

uint32_t
interactive_participant_impl::mixer_id() const
{
    return m_mixerId;
}

const string_t&
interactive_participant_impl::username() const
{
    return m_username;
}

uint32_t
interactive_participant_impl::level() const
{
    return m_level;
}

const interactive_participant_state
interactive_participant_impl::state() const
{
    return m_state;
}

void
interactive_participant_impl::set_group(std::shared_ptr<interactive_group> group)
{
    string_t oldGroup = m_groupId;
    m_groupIdUpdated = true;
    m_groupId = group->group_id();
    m_interactivityManager->m_impl->move_participant_group(m_mixerId, oldGroup, m_groupId);
}

const std::shared_ptr<interactive_group>
interactive_participant_impl::group()
{
    return m_interactivityManager->group(m_groupId);
}

const std::chrono::milliseconds&
interactive_participant_impl::last_input_at() const
{
    return m_lastInputAt;
}

const std::chrono::milliseconds&
interactive_participant_impl::connected_at() const
{
    return m_connectedAt;
}

bool
interactive_participant_impl::input_disabled() const
{
    return m_disabled;
}

#if 0
std::shared_ptr<interactive_button_control> MICROSOFT_MIXER_NAMESPACE::interactive_participant_impl::button(const string_t & control_id)
{
    return std::shared_ptr<interactive_button_control>();
}

std::vector<std::shared_ptr<interactive_button_control>> MICROSOFT_MIXER_NAMESPACE::interactive_participant_impl::buttons()
{
    return std::vector<std::shared_ptr<interactive_button_control>>();
}

std::shared_ptr<interactive_joystick_control> MICROSOFT_MIXER_NAMESPACE::interactive_participant_impl::joystick(const string_t & control_id)
{
    return std::shared_ptr<interactive_joystick_control>();
}

std::vector<std::shared_ptr<interactive_joystick_control>> MICROSOFT_MIXER_NAMESPACE::interactive_participant_impl::joysticks()
{
    return std::vector<std::shared_ptr<interactive_joystick_control>>();
}
#endif


interactive_participant_impl::interactive_participant_impl(
    unsigned int mixerId,
    string_t username,
    string_t sessionId,
    uint32_t level,
    string_t groupId,
    std::chrono::milliseconds lastInputAt,
    std::chrono::milliseconds connectedAt,
    bool disabled
) :
    m_mixerId(std::move(mixerId)),
    m_username(std::move(username)),
    m_sessionId(std::move(sessionId)),
    m_level(std::move(level)),
    m_groupId(std::move(groupId)),
    m_lastInputAt(std::move(lastInputAt)),
    m_connectedAt(std::move(connectedAt)),
    m_disabled(std::move(disabled)),
    m_groupIdUpdated(false),
    m_disabledUpdated(false),
    m_stateUpdated(false)
{
    m_interactivityManager = interactivity_manager::get_singleton_instance();
}

interactive_participant_impl::interactive_participant_impl() :
    m_groupId(RPC_GROUP_DEFAULT),
    m_disabled(false),
    m_groupIdUpdated(false),
    m_disabledUpdated(false),
    m_stateUpdated(false)
{
    m_interactivityManager = interactivity_manager::get_singleton_instance();
}

bool
interactive_participant_impl::update(web::json::value json, bool overwrite)
{
    string_t errorString;
    bool success = true;

    try
    {
        if (success)
        {
            if (json.has_field(RPC_USER_ID))
            {
                m_mixerId = json[RPC_USER_ID].as_number().to_uint32();
            }
            else if (overwrite)
            {
                errorString = L"Trying to construct participant, no id";
                success = false;
            }
        }

        if (success)
        {
            if (json.has_field(RPC_USERNAME))
            {
                m_username = json[RPC_USERNAME].as_string();
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
            if (json.has_field(RPC_LEVEL))
            {
                m_level = json[RPC_LEVEL].as_number().to_uint32();
            }
            else if (overwrite)
            {
                errorString = L"Trying to construct participant, no level";
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
                errorString = L"Trying to construct participant, no group";
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

bool MICROSOFT_MIXER_NAMESPACE::interactive_participant_impl::init_from_json(web::json::value json)
{
    return update(json, true);
}


NAMESPACE_MICROSOFT_MIXER_END

