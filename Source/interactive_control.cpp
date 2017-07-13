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

//
// Control builder static methods - in order to maintain correct sync state
// with the service, we want the interactivity_manager_impl to own the various scene
// and control objects.
//

std::shared_ptr<interactive_button_control>
interactive_control_builder::build_button_control(string_t parentSceneId, web::json::value json)
{
    std::shared_ptr<interactive_button_control> button = std::shared_ptr<interactive_button_control>(new interactive_button_control());
    bool success = button->init_from_json(json);

    if (success)
    {
        button->m_parentScene = parentSceneId;
        interactivity_manager::get_singleton_instance()->m_impl->add_control_to_map(button);
    }
    else
    {
        button = nullptr;
        interactivity_manager::get_singleton_instance()->m_impl->queue_interactive_event_for_client(L"Failed to initialize button control from json", std::make_error_code(std::errc::invalid_argument), interactive_event_type::error, nullptr);
    }

    return button;
}

std::shared_ptr<interactive_button_control>
interactive_control_builder::build_button_control()
{
    std::shared_ptr<interactive_button_control> button = std::shared_ptr<interactive_button_control>(new interactive_button_control());
    
    interactivity_manager::get_singleton_instance()->m_impl->add_control_to_map(button);

    return button;
}

std::shared_ptr<interactive_button_control>
interactive_control_builder::build_button_control(
    _In_ string_t parentSceneId,
    _In_ string_t controlId,
    _In_ bool enabled,
    _In_ float progress,
    _In_ std::chrono::milliseconds cooldownDeadline,
    _In_ string_t buttonText,
    _In_ uint32_t sparkCost
)
{
    std::shared_ptr<interactive_button_control> button = std::shared_ptr<interactive_button_control>(new interactive_button_control(parentSceneId, controlId, enabled, progress, cooldownDeadline, buttonText, sparkCost));
    
    interactivity_manager::get_singleton_instance()->m_impl->add_control_to_map(button);

    return button;
}


std::shared_ptr<interactive_joystick_control>
interactive_control_builder::build_joystick_control()
{
    std::shared_ptr<interactive_joystick_control> joystick = std::shared_ptr<interactive_joystick_control>(new interactive_joystick_control());
    
    interactivity_manager::get_singleton_instance()->m_impl->add_control_to_map(joystick);

    return joystick;
}

std::shared_ptr<interactive_joystick_control>
interactive_control_builder::build_joystick_control(string_t parentSceneId, web::json::value json)
{
    std::shared_ptr<interactive_joystick_control> joystick = std::shared_ptr<interactive_joystick_control>(new interactive_joystick_control());
    bool success = joystick->init_from_json(json);

    if (success)
    {
        joystick->m_parentScene = parentSceneId;
        interactivity_manager::get_singleton_instance()->m_impl->add_control_to_map(joystick);
    }
    else
    {
        joystick = nullptr;
        interactivity_manager::get_singleton_instance()->m_impl->queue_interactive_event_for_client(L"Failed to initialize joystick control from json", std::make_error_code(std::errc::invalid_argument), interactive_event_type::error, nullptr);
    }

    return joystick;
}

std::shared_ptr<interactive_joystick_control>
interactive_control_builder::build_joystick_control(
    _In_ string_t parentSceneId,
    _In_ string_t controlId,
    _In_ bool enabled,
    _In_ double x,
    _In_ double y
)
{
    std::shared_ptr<interactive_joystick_control> joystick = std::shared_ptr<interactive_joystick_control>(new interactive_joystick_control(parentSceneId, controlId, enabled, x, y));
    interactivity_manager::get_singleton_instance()->m_impl->add_control_to_map(joystick);

    return joystick;
}

//
// Control base class
//

const interactive_control_type&
interactive_control::control_type() const
{
    return m_type;
}

const string_t&
interactive_control::control_id() const
{
    return m_controlId;
}

const std::map<string_t, string_t>&
interactive_control::meta_properties() const
{
    return m_metaProperties;
}

interactive_control::interactive_control()
{

}

interactive_control::interactive_control(
    _In_ string_t parentScene,
    _In_ string_t controlId,
    _In_ bool disabled
)
{
    m_parentScene = std::move(parentScene);
    m_controlId = std::move(controlId);
    m_disabled = disabled;
}


bool
interactive_button_state::is_pressed()
{
    return m_isPressed;
}

bool
interactive_button_state::is_down()
{
    return m_isDown;
}

bool
interactive_button_state::is_up()
{
    return m_isUp;
}

interactive_button_state::interactive_button_state() :
    m_isUp(false),
    m_isDown(false),
    m_isPressed(false)
{
}

uint32_t
interactive_button_count::count_of_button_presses()
{
    return m_buttonPresses;
}

uint32_t
interactive_button_count::count_of_button_downs()
{
    return m_buttonDowns;
}

uint32_t
interactive_button_count::count_of_button_ups()
{
    return m_buttonUps;
}

void
interactive_button_count::clear()
{
    m_buttonDowns = 0;
    m_buttonPresses = 0;
    m_buttonUps = 0;
}

interactive_button_count::interactive_button_count() :
    m_buttonPresses(0),
    m_buttonDowns(0),
    m_buttonUps(0)
{
}

double
interactive_joystick_state::x()
{
    return m_x;
}

double
interactive_joystick_state::y()
{
    return m_y;
}

interactive_joystick_state::interactive_joystick_state(_In_ double x, _In_ double y) :
    m_x(x),
    m_y(y)
{
}

//
// Button control
//

bool
interactive_button_control::disabled() const
{
    return m_disabled;
}

void
interactive_button_control::set_disabled(bool disabled)
{
    m_disabled = disabled;
    m_interactivityManager->set_disabled(m_controlId, disabled);
}

const string_t&
interactive_button_control::button_text() const
{
    return m_buttonText;
}


uint32_t
interactive_button_control::cost() const
{
    return m_sparkCost;
}

void
interactive_button_control::trigger_cooldown(std::chrono::milliseconds cooldown) const
{
    m_interactivityManager->trigger_cooldown(m_controlId, cooldown);
    return;
}


std::chrono::milliseconds
interactive_button_control::remaining_cooldown() const
{
    auto remaining = m_cooldownDeadline - m_interactivityManager->get_server_time();

    if (remaining.count() < 0)
    { 
        remaining = std::chrono::milliseconds(0);
    }

    return remaining;
}


float
interactive_button_control::progress() const
{
    return m_progress;
}


void
interactive_button_control::set_progress(_In_ float progress)
{
    m_progress = progress;
    m_interactivityManager->set_progress(m_controlId, progress);
    return;
}


uint32_t
interactive_button_control::count_of_button_downs()
{
    return m_buttonCount->count_of_button_downs();
}


uint32_t
interactive_button_control::count_of_button_presses()
{
    return m_buttonCount->count_of_button_presses();
}


uint32_t
interactive_button_control::count_of_button_ups()
{
    return m_buttonCount->count_of_button_ups();
}


bool
interactive_button_control::is_pressed()
{
    return m_buttonCount->count_of_button_ups() > 0;
}


bool
interactive_button_control::is_pressed(_In_ uint32_t mixerId)
{
    bool isPressed = false;
    if (m_buttonStateByMixerId[mixerId] != nullptr)
    {
        isPressed = m_buttonStateByMixerId[mixerId]->is_pressed();
    }
    return isPressed;
}


bool
interactive_button_control::is_down()
{
    return (m_buttonCount->count_of_button_ups() < m_buttonCount->count_of_button_downs());
}

bool
interactive_button_control::is_down(_In_ uint32_t mixerId)
{
    bool isDown = false;
    if (m_buttonStateByMixerId[mixerId] != nullptr)
    {
        isDown = m_buttonStateByMixerId[mixerId]->is_down();
    }
    return isDown;
}


bool
interactive_button_control::is_up()
{
    return (m_buttonCount->count_of_button_ups() > m_buttonCount->count_of_button_downs());
}

bool
interactive_button_control::is_up(_In_ uint32_t mixerId)
{
    bool isUp = false;
    if (m_buttonStateByMixerId[mixerId] != nullptr)
    {
        isUp = m_buttonStateByMixerId[mixerId]->is_up();
    }
    return isUp;
}


bool
interactive_button_control::init_from_json(web::json::value json)
{
    return update(json, true);
}

bool
MICROSOFT_MIXER_NAMESPACE::interactive_button_control::update(web::json::value json, bool overwrite)
{
    string_t errorString;
    bool success = true;

    try
    {
        // Attributes that need to be plumbed through:
        // keyCode
        // position

        if (success && json.has_field(RPC_ETAG))
        {
            m_etag = json[RPC_ETAG].as_string();
        }
        else
        {
            errorString = L"Trying to construct or update a button, no etag";
            success = false;
        }

        if (success && overwrite)
        {
            if (json.has_field(RPC_CONTROL_ID))
            {
                m_controlId = json[RPC_CONTROL_ID].as_string();
            }
            else
            {
                errorString = L"Trying to construct a button, no id";
                success = false;
            }
        }

        if (success && json.has_field(RPC_CONTROL_BUTTON_TEXT))
        {
            m_buttonText = json[RPC_CONTROL_BUTTON_TEXT].as_string();
        }

        if (success && json.has_field(RPC_SPARK_COST))
        {
            m_sparkCost = json[RPC_SPARK_COST].as_integer();
        }

        if (success && json.has_field(RPC_CONTROL_BUTTON_COOLDOWN))
        {
            uint64_t ullDeadline = json[RPC_CONTROL_BUTTON_COOLDOWN].as_number().to_uint64();
            std::chrono::milliseconds msDeadline = std::chrono::milliseconds(ullDeadline);
            m_cooldownDeadline = msDeadline;
        }

        if (success && json.has_field(RPC_DISABLED))
        {
            m_disabled = json[RPC_DISABLED].as_bool();
        }
        else
        {
            if (overwrite)
            {
                errorString = L"Trying to construct or update a button, no disabled field";
                success = false;
            }
        }

        if (success && json.has_field(RPC_METADATA))
        {
            auto metadata = json[RPC_METADATA].as_object();

            for (auto meta : metadata)
            {
                string_t key(meta.first);
                string_t value(meta.second.as_object().at(RPC_VALUE).as_string());

                m_metaProperties.insert_or_assign(key, value);
            }
        }

        if (success && json.has_field(RPC_CONTROL_BUTTON_PROGRESS))
        {
            m_progress = (float)json[RPC_CONTROL_BUTTON_PROGRESS].as_number().to_double();
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed to parse json in interactive_button_control::update()";
    }

    return success;
}

void
interactive_button_control::clear_state()
{
    m_buttonStateByMixerId.clear();
    m_buttonCount->clear();
}

interactive_button_control::interactive_button_control()
{
    m_interactivityManager = interactivity_manager::get_singleton_instance();
    m_disabled = true;
    m_type = interactive_control_type::button;
    m_buttonCount = std::shared_ptr<interactive_button_count>(new interactive_button_count());
}

interactive_button_control::interactive_button_control(
    _In_ string_t parentSceneId,
    _In_ string_t controlId,
    _In_ bool disabled,
    _In_ float progress,
    _In_ std::chrono::milliseconds cooldownDeadline,
    _In_ string_t buttonText,
    _In_ uint32_t sparkCost
)
{
    m_interactivityManager = interactivity_manager::get_singleton_instance();
    m_parentScene = std::move(parentSceneId);
    m_type = interactive_control_type::button;
    m_controlId = std::move(controlId);
    m_disabled = disabled;
    m_progress = progress;
    m_buttonText = std::move(buttonText);
    m_cooldownDeadline = std::move(cooldownDeadline);
    m_sparkCost = sparkCost;
    m_buttonCount = std::shared_ptr<interactive_button_count>(new interactive_button_count());
}

//
// Joystick control
//
  
void
interactive_joystick_control::clear_state()
{
}

double
 interactive_joystick_control::x() const
 {
     return m_x;
 }

 double
     interactive_joystick_control::x(_In_ uint32_t mixerId)
 {
     double x = 0;
     if (m_joystickStateByMixerId[mixerId] != nullptr)
     {
         x = m_joystickStateByMixerId[mixerId]->x();
     }
     return x;
 }

 double
 interactive_joystick_control::y() const
 {
     return m_y;
 }

 double
     interactive_joystick_control::y(_In_ uint32_t mixerId)
 {
     double y = 0;
     if (m_joystickStateByMixerId[mixerId] != nullptr)
     {
         y = m_joystickStateByMixerId[mixerId]->y();
     }
     return y;
 }

interactive_joystick_control::interactive_joystick_control()
{
    m_interactivityManager = interactivity_manager::get_singleton_instance();
    m_disabled = true;
    m_type = interactive_control_type::joystick;
    m_x = 0.0;
    m_y = 0.0;
}

interactive_joystick_control::interactive_joystick_control(
    _In_ string_t parentSceneId,
    _In_ string_t controlId,
    _In_ bool disabled,
    _In_ double x,
    _In_ double y
)
{
    m_interactivityManager = interactivity_manager::get_singleton_instance();
    m_parentScene = std::move(parentSceneId);
    m_type = interactive_control_type::joystick;
    m_controlId = std::move(controlId);
    m_disabled = disabled;
    m_x = x;
    m_y = y;
}

bool
interactive_joystick_control::init_from_json(web::json::value json)
{
    return update(json, true);
}

bool
MICROSOFT_MIXER_NAMESPACE::interactive_joystick_control::update(web::json::value json, bool overwrite)
{
    string_t errorString;
    bool success = true;

    try
    {
        if (success && overwrite)
        {
            if (json.has_field(RPC_CONTROL_ID))
            {
                m_controlId = json[RPC_CONTROL_ID].as_string();
            }
            else
            {
                errorString = L"Trying to construct a joystick, no id";
                success = false;
            }
        }

        if (success && json.has_field(RPC_ETAG))
        {
            m_etag = json[RPC_ETAG].as_string();
        }
        else
        {
            errorString = L"Trying to construct or update joystick, no etag";
            success = false;
        }

        if (success && json.has_field(RPC_DISABLED))
        {
            m_disabled = json[RPC_DISABLED].as_bool();
        }

        if (success && json.has_field(RPC_METADATA))
        {
            auto metadata = json[RPC_METADATA].as_object();

            for (auto meta : metadata)
            {
                string_t key(meta.first);
                string_t value(meta.second.as_object().at(RPC_VALUE).as_string());

                m_metaProperties.insert_or_assign(key, value);
            }
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed in process_get_scenes_reply";
    }

    return success;
}

NAMESPACE_MICROSOFT_MIXER_END
