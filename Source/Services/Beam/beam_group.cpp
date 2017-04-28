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

beam_group::beam_group(_In_ string_t groupId)
{
    m_beamManager = beam_manager::get_singleton_instance();
    m_impl = std::make_shared<beam_group_impl>(groupId);
    m_beamManager->m_impl->add_group_to_map(m_impl);
}

beam_group::beam_group(
    _In_ string_t groupId,
    _In_ std::shared_ptr<beam_scene> scene
)
{
    m_beamManager = beam_manager::get_singleton_instance();
    m_impl = std::make_shared<beam_group_impl>(groupId);
    m_impl->m_sceneId = scene->scene_id();
    m_beamManager->m_impl->add_group_to_map(m_impl);
}

beam_group::beam_group()
{
    m_beamManager = beam_manager::get_singleton_instance();
    m_impl = std::make_shared<beam_group_impl>();
    m_beamManager->m_impl->add_group_to_map(m_impl);
}

const string_t&
beam_group::group_id() const
{
    return m_impl->m_groupId;
}

std::shared_ptr<beam_scene>
beam_group::scene()
{
    return m_impl->scene();
}

void beam_group::set_scene(std::shared_ptr<beam_scene> currentScene)
{
    m_impl->set_scene(currentScene);
}

const std::vector<std::shared_ptr<beam_participant>>
beam_group::participants()
{
    return m_impl->participants();
}

/// beam_group_impl

beam_group_impl::beam_group_impl(string_t groupId) :
    m_groupId(std::move(groupId)),
    m_sceneId(RPC_SCENE_DEFAULT),
    m_sceneChanged(false),
    m_participantsChanged(false)
{
    m_beamManager = beam_manager::get_singleton_instance();
}

beam_group_impl::beam_group_impl(
    string_t groupId,
    string_t etag,
    string_t sceneId
) :
    m_groupId(std::move(groupId)),
    m_etag(std::move(etag)),
    m_sceneId(std::move(sceneId)),
    m_sceneChanged(false),
    m_participantsChanged(false)
{
    m_beamManager = beam_manager::get_singleton_instance();
}

beam_group_impl::beam_group_impl(web::json::value json)
{
    m_beamManager = beam_manager::get_singleton_instance();
    update(json, true);
}

string_t
beam_group_impl::group_id() const
{
    return m_groupId;
}

string_t
beam_group_impl::etag() const
{
    return m_etag;
}

string_t
beam_group_impl::scene_id() 
{
    return m_sceneId;
}

const std::vector<std::shared_ptr<beam_participant>>
beam_group_impl::participants()
{
    std::vector<std::shared_ptr<beam_participant>> participants;

    auto participantIds = m_beamManager->m_impl->m_participantsByGroupId[m_groupId];

    if (0 != participantIds.size())
    {
        participants.reserve(participantIds.size());
        for (auto iter = participantIds.begin(); iter != participantIds.end(); iter++)
        {
            uint32_t participantId = (*iter);
            participants.push_back(m_beamManager->m_impl->m_participants[participantId]);
        }
    }

    return participants;
}

void
beam_group_impl::set_scene(std::shared_ptr<beam_scene> currentScene)
{
    auto oldScene = m_sceneId;
    m_sceneId = currentScene->scene_id();
    m_sceneChanged = true;
    auto result = m_beamManager->m_impl->try_set_current_scene(m_sceneId, m_groupId);

    if (result && result->event_type() == beam_event_type::error)
    {
        m_sceneChanged = false;
        m_sceneId = oldScene;
    }
}

std::shared_ptr<beam_scene>
beam_group_impl::scene() const
{
    return m_beamManager->scene(m_sceneId);
}

bool
beam_group_impl::update(web::json::value json, bool overwrite)
{
    string_t errorString;
    bool success = true;

    std::lock_guard<std::recursive_mutex> lock(m_lock);

    try
    {
        if (success && json.has_field(RPC_ETAG))
        {
            m_etag = json[RPC_ETAG].as_string();
        }
        else if (overwrite)
        {
            errorString = L"Trying to construct group, no etag";
            success = false;
        }

        if (success && json.has_field(RPC_SCENE_ID))
        {
            m_sceneId = json[RPC_SCENE_ID].as_string();
        }
        else if (overwrite)
        {
            errorString = L"Trying to construct group, no sceneID";
            success = false;
        }

        if (success && json.has_field(RPC_GROUP_ID))
        {
            m_groupId = json[RPC_GROUP_ID].as_string();
        }
        else if (overwrite)
        {
            errorString = L"Trying to construct group, no groupID";
            success = false;
        }
    }
    catch (std::exception e)
    {
        LOGS_ERROR << "Failed beam_group::in init_from_json";
    }

    return success;
}

NAMESPACE_MICROSOFT_XBOX_BEAM_END