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

const string_t&
beam_scene::scene_id() const
{
    return m_impl->scene_id();
}

const std::vector<string_t>
beam_scene::groups()
{
    return m_impl->groups();
}

const std::vector<std::shared_ptr<beam_button_control>>
beam_scene::buttons()
{
    return m_impl->buttons();
}

const std::shared_ptr<beam_button_control>
beam_scene::button(_In_ const string_t& controlId)
{
    return m_impl->button(controlId);
}

const std::vector<std::shared_ptr<beam_joystick_control>>
beam_scene::joysticks()
{
    return m_impl->joysticks();
}

const std::shared_ptr<beam_joystick_control>
beam_scene::joystick(_In_ const string_t& controlId)
{
    return m_impl->joystick(controlId);
}

beam_scene::beam_scene() {
    m_beamManager = beam_manager::get_singleton_instance();
    m_impl = std::make_shared<beam_scene_impl>();
}

beam_scene::beam_scene(
    _In_ string_t sceneId,
    _In_ bool enabled,
    _In_ const std::vector<const string_t&>& controls
)
{
    m_beamManager = beam_manager::get_singleton_instance();
    m_impl = std::make_shared<beam_scene_impl>(sceneId, L"" /*etag filled in by service call*/, enabled, controls);
}

//
// Backing implementation
//

const string_t&
beam_scene_impl::scene_id() const
{
    return m_sceneId;
}

const bool&
beam_scene_impl::disabled() const
{
    return m_disabled;
}

void
beam_scene_impl::set_disabled(bool disabled)
{
    m_disabled = disabled;
}

const std::vector<string_t>
beam_scene_impl::groups()
{
    std::vector<string_t> groups;
    groups.reserve(m_groupIds.size());

    for (auto iter = m_groupIds.begin(); iter != m_groupIds.end(); iter++)
    {
        string_t currGroupId = (*iter);
        groups.push_back(currGroupId);
    }

    return groups;
}

const std::vector<std::shared_ptr<beam_button_control>>
beam_scene_impl::buttons()
{
    std::vector<std::shared_ptr<beam_button_control>> buttons;
    buttons.reserve(m_buttonIds.size());

    for (auto iter = m_buttonIds.begin(); iter != m_buttonIds.end(); iter++)
    {
        string_t currButtonId = (*iter);
        buttons.push_back(m_beamManager->m_impl->m_buttons[currButtonId]);
    }

    return buttons;
}

const std::shared_ptr<beam_button_control>
beam_scene_impl::button(_In_ const string_t& controlId)
{
    std::shared_ptr<beam_button_control> currControl = nullptr;
    for (auto iter = m_buttonIds.begin(); iter != m_buttonIds.end(); iter++)
    {
        string_t currButtonId = (*iter);
        if (0 == currButtonId.compare(controlId))
        {
            currControl = m_beamManager->m_impl->m_buttons[currButtonId];
            break;
        }
    }

    return currControl;
}


const std::vector<std::shared_ptr<beam_joystick_control>>
beam_scene_impl::joysticks()
{
    std::vector<std::shared_ptr<beam_joystick_control>> joysticks;
    joysticks.reserve(m_joystickIds.size());

    for (auto iter = m_joystickIds.begin(); iter != m_joystickIds.end(); iter++)
    {
        string_t currControlId = (*iter);
        joysticks.push_back(m_beamManager->m_impl->m_joysticks[currControlId]);
    }

    return joysticks;
}


const std::shared_ptr<beam_joystick_control>
beam_scene_impl::joystick(_In_ const string_t& controlId)
{
    std::shared_ptr<beam_joystick_control> currControl = nullptr;
    for (auto iter = m_joystickIds.begin(); iter != m_joystickIds.end(); iter++)
    {
        string_t currControlId = (*iter);
        if (0 == currControlId.compare(controlId))
        {
            currControl = m_beamManager->m_impl->m_joysticks[currControlId];
            break;
        }
    }

    return currControl;
}

void
beam_scene_impl::init_from_json(web::json::value json)
{
    string_t errorString;
    bool success = true;

    try
    {
        if (json.has_field(RPC_SCENE_ID))
        {
            m_sceneId = json[RPC_SCENE_ID].as_string();
        }
        else
        {
            errorString = L"Trying to construct scene, no scene id";
            success = false;
        }

        if (success && json.has_field(RPC_ETAG))
        {
            m_etag = json[RPC_ETAG].as_string();
        }
        else
        {
            errorString = L"Trying to construct scene, no etag";
            success = false;
        }

        if (success && json.has_field(RPC_PARAM_CONTROLS))
        {
            try
            {
                web::json::array controls = json.at(RPC_PARAM_CONTROLS).as_array();

                for (auto iter = controls.begin(); iter != controls.end(); ++iter)
                {
                    auto control = *iter;
                    if (0 == control[RPC_CONTROL_KIND].as_string().compare(RPC_CONTROL_BUTTON))
                    {
                        std::shared_ptr<beam_button_control> newButton = beam_control_builder::build_button_control(m_sceneId, control);

                        if (nullptr != newButton)
                        {
                            m_buttonIds.push_back(newButton->control_id());
                        }
                    }
                    else if (0 == control[RPC_CONTROL_KIND].as_string().compare(RPC_CONTROL_JOYSTICK))
                    {
                        std::shared_ptr<beam_joystick_control> newJoystick = beam_control_builder::build_joystick_control(m_sceneId, control);

                        if (nullptr != newJoystick)
                        {
                            m_joystickIds.push_back(newJoystick->control_id());
                        }
                    }
                    else
                    {
                        LOGS_ERROR << "Unknown control type: " << control[RPC_CONTROL_KIND].as_string();
                    }
                }
            }
            catch (std::exception e)
            {
                LOGS_ERROR << "Failed to create scenes in process_get_scenes_reply";
            }
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed in process_get_scenes_reply";
    }

    if (!success)
    {
        LOGS_ERROR << errorString;
        // TODO: return beam_error
    }
}

beam_scene_impl::beam_scene_impl()
{
    m_beamManager = beam_manager::get_singleton_instance();
}

beam_scene_impl::beam_scene_impl(
    _In_ string_t sceneId,
    _In_ string_t etag,
    _In_ bool disabled,
    _In_ const std::vector<const string_t&>& controls
) :
    m_sceneId(std::move(sceneId)),
    m_etag(std::move(etag)),
    m_disabled(std::move(disabled))
{
    m_beamManager = beam_manager::get_singleton_instance();
}

NAMESPACE_MICROSOFT_XBOX_BEAM_END

