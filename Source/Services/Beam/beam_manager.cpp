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

using namespace pplx;
using namespace XBOX_BEAM_NAMESPACE;

NAMESPACE_MICROSOFT_XBOX_BEAM_BEGIN

static std::mutex s_singletonLock;

beam_manager::beam_manager()
{
    m_impl = std::shared_ptr<beam_manager_impl>(new beam_manager_impl());
}

/*
beam_manager::~beam_manager()
{
    // TODO: lock impl
    m_beamImpl->stop_interactive();
    return;
}
*/

std::shared_ptr<beam_manager>
beam_manager::get_singleton_instance()
{
    std::lock_guard<std::mutex> guard(s_singletonLock);
    static std::shared_ptr<beam_manager> instance;
    if (instance == nullptr)
    {
        instance = std::shared_ptr<beam_manager>(new beam_manager());
    }

    return instance;
}

bool beam_manager::initialize(
    _In_ string_t interactiveVersion,
    _In_ bool goInteractive
)
{
    return m_impl->initialize(interactiveVersion, goInteractive);
}

#if TV_API | XBOX_UWP
std::shared_ptr<beam_event>
beam_manager::add_local_user(xbox_live_user_t user)
{
    return m_impl->add_local_user(user);
}
#else
std::shared_ptr<beam_event>
beam_manager::set_xtoken(string_t token)
{
    return m_impl->set_xtoken(token);
}
#endif

const std::chrono::milliseconds
beam_manager::get_server_time()
{
    return m_impl->get_server_time();
}

bool
beam_manager::start_interactive()
{
    return m_impl->start_interactive();
}

bool
beam_manager::stop_interactive()
{
    return m_impl->stop_interactive();
}

const beam_interactivity_state
beam_manager::interactivity_state()
{
    return m_impl->interactivity_state();
}

std::vector<std::shared_ptr<beam_group>>
beam_manager::groups()
{
    return m_impl->groups();
}

std::shared_ptr<beam_group>
beam_manager::group(_In_ const string_t& group_id)
{
    return m_impl->group(group_id);
}

std::vector<std::shared_ptr<xbox::services::beam::beam_scene>>
beam_manager::scenes()
{
    return m_impl->scenes();
}

std::shared_ptr<beam_scene>
beam_manager::scene(_In_ const string_t& sceneId)
{
    return m_impl->scene(sceneId);
}

std::vector<std::shared_ptr<beam_participant>>
beam_manager::participants()
{
    return m_impl->participants();
}

void
beam_manager::trigger_cooldown(_In_ const string_t& control_id, _In_ const std::chrono::milliseconds& cooldown) const
{
    return m_impl->trigger_cooldown(control_id, cooldown);
}

std::vector<xbox::services::beam::beam_event>
beam_manager::do_work()
{
    return m_impl->do_work();
}

NAMESPACE_MICROSOFT_XBOX_BEAM_END