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

NAMESPACE_MICROSOFT_MIXER_BEGIN
static std::mutex s_singletonLock;

interactive_event::interactive_event(
    std::chrono::milliseconds time,
    std::error_code errorCode,
    string_t errorMessage,
    interactive_event_type eventType,
    std::shared_ptr<interactive_event_args> eventArgs
    ) :
    m_time(std::move(time)),
    m_errorCode(std::move(errorCode)),
    m_errorMessage(std::move(errorMessage)),
    m_eventType(std::move(eventType)),
    m_eventArgs(std::move(eventArgs))
{
}

const std::chrono::milliseconds&
interactive_event::time() const
{
    return m_time;
}


const std::error_code&
interactive_event::err() const
{
    return m_errorCode;
}


const string_t&
interactive_event::err_message() const
{
    return m_errorMessage;
}

interactive_event_type
interactive_event::event_type() const
{
    return m_eventType;
}

const std::shared_ptr<interactive_event_args>&
interactive_event::event_args() const
{
    return m_eventArgs;
}

//
// State change event args
//
const interactivity_state
interactivity_state_change_event_args::new_state() const
{
    return m_newState;
}

interactivity_state_change_event_args::interactivity_state_change_event_args(
    _In_ interactivity_state new_state
    ) :
    interactive_event_args(),
    m_newState(new_state)
{
}

//
// Interactive message event args
//

const string_t& interactive_message_event_args::message() const
{
    return m_message;
}

interactive_message_event_args::interactive_message_event_args(string_t message)
{
    m_message = std::move(message);
}

//
// Button control event args
//

interactive_button_event_args::interactive_button_event_args(string_t controlId, string_t transaction_id, uint32_t cost, std::shared_ptr<interactive_participant> participant, bool isPressed)
{
    m_controlId = std::move(controlId);
    m_transactionId = std::move(transaction_id);
    m_participant = std::move(participant);
    m_cost = cost;
    m_isPressed = isPressed;
}

const std::shared_ptr<MICROSOFT_MIXER_NAMESPACE::interactive_participant>& interactive_button_event_args::participant() const
{
    return m_participant;
}

const string_t& interactive_button_event_args::control_id() const
{
    return m_controlId;
}

const string_t & interactive_button_event_args::transaction_id() const
{
    return m_transactionId;
}

uint32_t interactive_button_event_args::cost() const
{
    return m_cost;
}

bool interactive_button_event_args::is_pressed() const
{
    return m_isPressed;
}

//
// Joystick control event args
//

const string_t& interactive_joystick_event_args::control_id() const
{
    return m_controlId;
}

double interactive_joystick_event_args::x() const
{
    return m_x;
}

double interactive_joystick_event_args::y() const
{
    return m_y;
}

const std::shared_ptr<interactive_participant>& interactive_joystick_event_args::participant() const
{
    return m_participant;
}

interactive_joystick_event_args::interactive_joystick_event_args(
    std::shared_ptr<interactive_participant> participant,
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

interactive_participant_state_change_event_args::interactive_participant_state_change_event_args(
    std::shared_ptr<interactive_participant> participant,
    interactive_participant_state state
    ) :
    m_participant(std::move(participant)),
    m_state(state)
{
}

const std::shared_ptr<interactive_participant>&
interactive_participant_state_change_event_args::participant() const
{
    return m_participant;
}

const interactive_participant_state&
interactive_participant_state_change_event_args::state() const
{
    return m_state;
}

NAMESPACE_MICROSOFT_MIXER_END
