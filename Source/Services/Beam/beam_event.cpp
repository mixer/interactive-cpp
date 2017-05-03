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

NAMESPACE_MICROSOFT_XBOX_BEAM_BEGIN
static std::mutex s_singletonLock;

beam_event::beam_event(
    std::chrono::milliseconds time,
    std::error_code errorCode,
    string_t errorMessage,
    beam_event_type eventType,
    std::shared_ptr<beam_event_args> eventArgs
    ) :
    m_time(std::move(time)),
    m_errorCode(std::move(errorCode)),
    m_errorMessage(std::move(errorMessage)),
    m_eventType(std::move(eventType)),
    m_eventArgs(std::move(eventArgs))
{
}

const std::chrono::milliseconds&
beam_event::time() const
{
    return m_time;
}


const std::error_code&
beam_event::err() const
{
    return m_errorCode;
}


const string_t&
beam_event::err_message() const
{
    return m_errorMessage;
}

beam_event_type
beam_event::event_type() const
{
    return m_eventType;
}

const std::shared_ptr<beam_event_args>&
beam_event::event_args() const
{
    return m_eventArgs;
}

//
// State change event args
//
const beam_interactivity_state
beam_interactivity_state_change_event_args::new_state() const
{
    return m_newState;
}

beam_interactivity_state_change_event_args::beam_interactivity_state_change_event_args(
    _In_ beam_interactivity_state new_state
    ) :
    beam_event_args(),
    m_newState(new_state)
{
}

//
// Button control event args
//

beam_button_event_args::beam_button_event_args(string_t controlId, std::shared_ptr<beam_participant> participant, bool isPressed)
{
    m_controlId = std::move(controlId);
    m_participant = std::move(participant);
    m_isPressed = isPressed;
}

const std::shared_ptr<xbox::services::beam::beam_participant>& beam_button_event_args::participant() const
{
    return m_participant;
}

const string_t& beam_button_event_args::control_id() const
{
    return m_controlId;
}

bool beam_button_event_args::is_pressed() const
{
    return m_isPressed;
}

//
// Joystick control event args
//

const string_t& beam_joystick_event_args::control_id() const
{
    return m_controlId;
}

double beam_joystick_event_args::x() const
{
    return m_x;
}

double beam_joystick_event_args::y() const
{
    return m_y;
}

const std::shared_ptr<beam_participant>& beam_joystick_event_args::participant() const
{
    return m_participant;
}

beam_joystick_event_args::beam_joystick_event_args(
    std::shared_ptr<beam_participant> participant,
    double x,
    double y,
    string_t control_id
    ) :
    m_participant(std::move(participant)),
    m_x(x),
    m_y(y),
    m_controlId(std::move(control_id))
{
}


//
// Participant state change event args
//

beam_participant_state_change_event_args::beam_participant_state_change_event_args(
    std::shared_ptr<beam_participant> participant,
    beam_participant_state state
    ) :
    m_participant(std::move(participant)),
    m_state(state)
{
}

const std::shared_ptr<beam_participant>&
beam_participant_state_change_event_args::participant() const
{
    return m_participant;
}

const beam_participant_state&
beam_participant_state_change_event_args::state() const
{
    return m_state;
}

NAMESPACE_MICROSOFT_XBOX_BEAM_END
