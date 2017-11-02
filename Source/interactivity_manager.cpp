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

using namespace pplx;
using namespace MICROSOFT_MIXER_NAMESPACE;

NAMESPACE_MICROSOFT_MIXER_BEGIN

static std::mutex s_singletonLock;

interactivity_manager::interactivity_manager()
{
    m_impl = std::shared_ptr<interactivity_manager_impl>(new interactivity_manager_impl());
}

/*
interactivity_manager::~interactivity_manager()
{
    // TODO: lock impl
    m_MIXERIMPl->stop_interactive();
    return;
}
*/

std::shared_ptr<interactivity_manager>
interactivity_manager::get_singleton_instance()
{
    std::lock_guard<std::mutex> guard(s_singletonLock);
    static std::shared_ptr<interactivity_manager> instance;
    if (instance == nullptr)
    {
        instance = std::shared_ptr<interactivity_manager>(new interactivity_manager());
    }

    return instance;
}

bool interactivity_manager::initialize(
    _In_ string_t interactiveVersion,
    _In_ bool goInteractive,
    _In_ string_t sharecode
)
{
    return m_impl->initialize(interactiveVersion, goInteractive, sharecode);
}

#if TV_API | XBOX_UWP
std::shared_ptr<interactive_event>
interactivity_manager::set_local_user(xbox_live_user_t user)
{
    return m_impl->set_local_user(user);
}
#else
std::shared_ptr<interactive_event>
interactivity_manager::set_xtoken(string_t token)
{
    return m_impl->set_xtoken(token);
}

std::shared_ptr<interactive_event>
interactivity_manager::set_oauth_token(string_t token)
{
	return m_impl->set_oauth_token(token);
}
#endif

const std::chrono::milliseconds
interactivity_manager::get_server_time()
{
    return m_impl->get_server_time();
}

bool
interactivity_manager::start_interactive()
{
    return m_impl->start_interactive();
}

bool
interactivity_manager::stop_interactive()
{
    return m_impl->stop_interactive();
}

const interactivity_state
interactivity_manager::interactivity_state()
{
    return m_impl->interactivity_state();
}

std::vector<std::shared_ptr<interactive_group>>
interactivity_manager::groups()
{
    return m_impl->groups();
}

std::shared_ptr<interactive_group>
interactivity_manager::group(_In_ const string_t& group_id)
{
    return m_impl->group(group_id);
}

std::vector<std::shared_ptr<MICROSOFT_MIXER_NAMESPACE::interactive_scene>>
interactivity_manager::scenes()
{
    return m_impl->scenes();
}

std::shared_ptr<interactive_scene>
interactivity_manager::scene(_In_ const string_t& sceneId)
{
    return m_impl->scene(sceneId);
}

std::vector<std::shared_ptr<interactive_participant>>
interactivity_manager::participants()
{
    return m_impl->participants();
}

void
interactivity_manager::set_disabled(_In_ const string_t& control_id, _In_ bool disabled) const
{
    return m_impl->set_disabled(control_id, disabled);
}

void
interactivity_manager::set_progress(_In_ const string_t& control_id, _In_ float progress)
{
    return m_impl->set_progress(control_id, progress);
}

void
interactivity_manager::trigger_cooldown(_In_ const string_t& control_id, _In_ const std::chrono::milliseconds& cooldown) const
{
    return m_impl->trigger_cooldown(control_id, cooldown);
}

void
interactivity_manager::send_rpc_message(const string_t & message, const string_t & parameters) const
{
    return m_impl->send_rpc_message(message, parameters);
}

void
interactivity_manager::capture_transaction(const string_t & transaction_id) const
{
    return m_impl->capture_transaction(transaction_id);
}

std::vector<MICROSOFT_MIXER_NAMESPACE::interactive_event>
interactivity_manager::do_work()
{
    return m_impl->do_work();
}

NAMESPACE_MICROSOFT_MIXER_END