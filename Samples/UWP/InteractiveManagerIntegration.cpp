// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pch.h"
#include <time.h>
#include "Interactivity.h"

using namespace Concurrency;
using namespace Windows::Foundation::Collections;
using namespace Windows::Foundation;
using namespace xbox::services;
using namespace xbox::services::stats::manager;
using namespace xbox::services::beam;

namespace
{
    const int c_debugLog = 202;

    const int c_maxLeaderboards = 10;
    const int c_liveHUD = 1000;
    const int c_sampleUIPanel = 2000;
    const int c_goInteractiveBtn = 2101;
    const int c_placeParticipantBtn = 2102;
    const int c_disbandGroupsBtn = 2103;
    const int c_cooldownRedBtn = 2104;
    const int c_cooldownBlueBtn = 2105;
    const int c_switchScenesBtn = 2106;
    const int c_voteYesCountLabel = 1004;
    const int c_voteNoCountLabel = 1006;

    const string_t s_interactiveVersion = L"19005";

    const string_t s_defaultGroup = L"default";
    const string_t s_redGroup = L"redGroup";
    const string_t s_blueGroup = L"blueGroup";

    const string_t s_defaultScene = L"default";
    const string_t s_redMinesScene = L"red_mines";
    const string_t s_redLasersScene = L"red_lasers";
    const string_t s_blueMinesScene = L"blue_mines";
    const string_t s_blueLasersScene = L"blue_lasers";
}

void Sample::InitializeInteractiveManager()
{
    m_beamManager = beam_manager::get_singleton_instance();

    m_currentInteractiveState = beam_interactivity_state::not_initialized;
    m_voteYesCount = 0;
    m_voteNoCount = 0;

    // Create the map of groups to scenes - this should be kept in sync with the scenes created in the Interactive Studio
    std::vector<string_t> defaultScenes;
    defaultScenes.push_back(s_defaultScene);

    std::vector<string_t> redScenes;
    redScenes.push_back(s_redMinesScene);
    redScenes.push_back(s_redLasersScene);

    std::vector<string_t> blueScenes;
    blueScenes.push_back(s_blueMinesScene);
    blueScenes.push_back(s_blueLasersScene);

    m_groupToSceneMap[s_defaultGroup] = defaultScenes;
    m_groupToSceneMap[s_redGroup] = redScenes;
    m_groupToSceneMap[s_blueGroup] = blueScenes;

    // Initialize controls and labels
    m_voteYesCountLabel = m_ui->FindControl<ATG::TextLabel>(2000, c_voteYesCountLabel);
    m_voteNoCountLabel = m_ui->FindControl<ATG::TextLabel>(2000, c_voteNoCountLabel);

    m_interactivityBtn = m_ui->FindControl<ATG::Button>(2000, c_goInteractiveBtn);
    m_buttons.push_back(m_interactivityBtn);

    m_disbandGroupsBtn = m_ui->FindControl<ATG::Button>(2000, c_disbandGroupsBtn);
    m_buttons.push_back(m_disbandGroupsBtn);

    m_cooldownRedControlsBtn = m_ui->FindControl<ATG::Button>(2000, c_cooldownRedBtn);
    m_buttons.push_back(m_cooldownRedControlsBtn);

    m_cooldownBlueControlsBtn = m_ui->FindControl<ATG::Button>(2000, c_cooldownBlueBtn);
    m_buttons.push_back(m_cooldownBlueControlsBtn);

    m_switchScenesBtn = m_ui->FindControl<ATG::Button>(2000, c_switchScenesBtn);
    m_buttons.push_back(m_switchScenesBtn);
}

void Sample::AddUserToInteractiveManager(_In_ std::shared_ptr<xbox::services::system::xbox_live_user> user)
{
    stringstream_t source;
    source << _T("Adding user ");
    source << user->gamertag();
    source << _T(" to StatsManager");
    m_console->WriteLine(source.str().c_str());

    m_user = user;

    auto task = m_user->get_token_and_signature(L"GET", L"https://mixer.com", L"")
    .then([this](XBOX_LIVE_NAMESPACE::xbox_live_result<XBOX_LIVE_NAMESPACE::system::token_and_signature_result> result)
    {
        try
        {
            string_t token = result.payload().token();

            if (!token.empty())
            {
                m_beamManager->set_xtoken(token);
            }
        }
        catch (Platform::Exception^ ex)
        {
            this->m_console->WriteLine(ex->Message->Data());
        }
    });
}

void Sample::UpdateBeamManager()
{
    // Process events from the stats manager
    // This should be called each frame update

    auto beamEvents = m_beamManager->do_work();

    for (const auto& evt : beamEvents)
    {
        beam_event_type eventType = evt.event_type();

        switch (eventType)
        {
        case beam_event_type::interactivity_state_changed:
            HandleInteractivityStateChange(evt);
            break;
        case beam_event_type::participant_state_changed:
            HandleParticipantStateChange(evt);
            break;
        case beam_event_type::error:
            HandleInteractivityError(evt);
            break;
        }

        // If you want to handle the interactive input manually:
        if (eventType == beam_event_type::button || eventType == beam_event_type::joystick)
        {
            HandleInteractiveControlEvent(evt);
            break;
        }
    }
}

void Sample::InitializeInteractivity()
{
    m_beamManager->initialize(s_interactiveVersion, false /*goInteractive*/);
}


void Sample::ToggleInteractivity()
{
    auto currentInteractiveState = m_beamManager->interactivity_state();
    if (beam_interactivity_state::not_initialized == currentInteractiveState)
    {
        InitializeInteractivity();
    }
    else if (beam_interactivity_state::interactivity_enabled == currentInteractiveState ||
        beam_interactivity_state::interactivity_pending == currentInteractiveState)
    {
        m_beamManager->stop_interactive();
        m_interactivityBtn->SetText(L"Go Interactive");
    }
    else if (beam_interactivity_state::interactivity_disabled == currentInteractiveState)
    {
        m_beamManager->start_interactive();
    }
    else
    {
        // Do nothing, interactive state is pending, wait till it completes
    }
}

void Sample::SwitchScenes()
{
    auto groupsList = m_beamManager->groups();

    for (auto iter = groupsList.begin(); iter != groupsList.end(); iter++)
    {
        auto currentGroup = (*iter);
        if (0 != currentGroup->group_id().compare(s_defaultGroup))
        {
            auto currentGroupsSceneList = m_groupToSceneMap[currentGroup->group_id()];
            string_t currentGroupSceneId = currentGroup->scene()->scene_id();

            for (int i = 0; i < currentGroupsSceneList.size(); i++)
            {
                // Find the current scene, then go to the next one, wrapping around
                if (0 == currentGroupSceneId.compare(currentGroupsSceneList[i]))
                {
                    i++;
                    if (i == currentGroupsSceneList.size())
                    {
                        i = 0;
                    }

                    string_t newSceneId = currentGroupsSceneList[i];
                    auto sceneToSet = m_beamManager->scene(newSceneId);

                    m_console->Write(L"Updating scene for group ");
                    m_console->Write(currentGroup->group_id().c_str());
                    m_console->Write(L" from ");
                    m_console->Write(currentGroupSceneId.c_str());
                    m_console->Write(L" to ");
                    m_console->Write(newSceneId.c_str());
                    m_console->Write(L"\n");

                    currentGroup->set_scene(sceneToSet);
                    break;
                }
            }
        }
    }
}

void Sample::InitializeGroupsAndScenes()
{
    auto scenesList = m_beamManager->scenes();
    auto groupsList = m_beamManager->groups();

    if (groupsList.size() == 0 || scenesList.size() == 0)
    {
        m_console->WriteLine(L"Groups and scenes not yet populated - wait for interactivity to initialize.");
        return;
    }

    if (groupsList.size() == 1)
    {
        auto redScene = m_beamManager->scene(s_redLasersScene);
        auto blueScene = m_beamManager->scene(s_blueLasersScene);

        if (nullptr == redScene)
        {
            m_console->Write(L"Unexpected error: \"");
            m_console->Write(s_redLasersScene.c_str());
            m_console->Write(L"\" does not exist\n");
            return;
        }

        if (nullptr == blueScene)
        {
            m_console->Write(L"Unexpected error: \"");
            m_console->Write(s_blueLasersScene.c_str());
            m_console->Write(L"\" does not exist\n");
            return;
        }

        std::shared_ptr<beam_group> groupRedTeam = std::make_shared<beam_group>(s_redGroup, redScene);

        std::shared_ptr<beam_group> groupBlueTeam = std::make_shared<beam_group>(s_blueGroup, blueScene);

        m_groupsList = m_beamManager->groups();

        if (m_groupsList.size() != 3)
        {
            m_console->WriteLine(L"Unexpected error: invalid group size after group initialization");
            return;
        }
    }
}

void Sample::AddParticipantToGroup()
{
    auto beamState = m_beamManager->interactivity_state();

    if (beam_interactivity_state::not_initialized == beamState || beam_interactivity_state::initializing == beamState)
    {
        m_console->WriteLine(L"Wait for interactivity to initialize.");
        return;
    }

    auto groupsList = m_beamManager->groups();

    if (groupsList.size() == 0)
    {
        m_console->WriteLine(L"Unexpected error - no groups defined");
        return;
    }
    else
    {
        auto defaultGroup = m_beamManager->group(s_defaultGroup);
        auto remainingParticipants = defaultGroup->participants();

        if (remainingParticipants.size() > 0)
        {
            auto smallestGroup = (*groupsList.begin());
            for (auto iter = groupsList.begin(); iter != groupsList.end(); iter++)
            {
                auto currentGroup = (*iter);
                if (0 == currentGroup->group_id().compare(s_defaultGroup))
                {
                    iter++;
                    if (iter == groupsList.end())
                    {
                        break;
                    }
                    currentGroup = (*iter);
                }

                if (currentGroup != smallestGroup)
                {
                    if (currentGroup->participants().size() < smallestGroup->participants().size())
                    {
                        smallestGroup = currentGroup;
                    }
                }
            }

            auto participant = remainingParticipants[0];
            participant->set_group(smallestGroup);

            string_t newSceneId = smallestGroup->scene()->scene_id();
            m_console->Write(L"Added participant ");
            m_console->Write(participant->beam_username().c_str());
            m_console->Write(L" to group ");
            m_console->Write(smallestGroup->group_id().c_str());
            m_console->Write(L" (displaying scene ");
            m_console->Write(smallestGroup->scene()->scene_id().c_str());
            m_console->Write(L")\n");
        }
        else
        {
            m_console->WriteLine(L"No more participants in the default group");
        }
    }
}

void Sample::DisbandGroups()
{
    m_console->WriteLine(L"Disbanding groups");
    auto beamState = m_beamManager->interactivity_state();

    if (beam_interactivity_state::not_initialized == beamState || beam_interactivity_state::initializing == beamState)
    {
        m_console->WriteLine(L"Wait for interactivity to initialize.");
        return;
    }

    auto groupsList = m_beamManager->groups();

    if (groupsList.size() == 0)
    {
        m_console->WriteLine(L"Unexpected error - no groups defined");
        return;
    }
    else
    {
        auto defaultGroup = m_beamManager->group(s_defaultGroup);
        for (auto groupsIter = groupsList.begin(); groupsIter != groupsList.end(); groupsIter++)
        {
            auto currentGroup = (*groupsIter);
            if (0 == currentGroup->group_id().compare(s_defaultGroup))
            {
                groupsIter++;
                currentGroup = (*groupsIter);
            }

            auto participantsToMove = currentGroup->participants();
            if (participantsToMove.size() > 0)
            {
                for (auto participantsIter = participantsToMove.begin(); participantsIter != participantsToMove.end(); participantsIter++)
                {
                    auto participant = (*participantsIter);
                    participant->set_group(defaultGroup);

                    m_console->Write(L"Moved participant ");
                    m_console->Write(participant->beam_username().c_str());
                    m_console->Write(L" back to the default group");
                    m_console->Write(L"\n");
                }
            }
        }
    }
}

void Sample::TriggerCooldownOnButtons(string_t groupId)
{
    m_console->Write(L"Triggering a 5 second cooldown for all visible buttons for group: ");
    m_console->Write(groupId.c_str());
    m_console->Write(L"\n");

    auto currentScene = m_beamManager->group(groupId)->scene();

    auto buttons = currentScene->buttons();

    for (auto & button : buttons)
    {
        if (nullptr != button)
        {
            std::chrono::milliseconds cooldownTimer(5 * 1000);
            button->trigger_cooldown(cooldownTimer);
        }
    }
}

void Sample::ProcessInteractiveEvents(std::vector<beam_event> events)
{
    for (int i = 0; i < events.size(); i++)
    {
        beam_event event = events[i];
        beam_event_type eventType = event.event_type();

        switch (eventType)
        {
        case beam_event_type::interactivity_state_changed:
            HandleInteractivityStateChange(event);
            break;
        case beam_event_type::participant_state_changed:
            HandleParticipantStateChange(event);
            break;
        case beam_event_type::error:
            HandleInteractivityError(event);
            break;
        }

        // If you want to handle the interactive input manually:
        if (eventType == beam_event_type::button || eventType == beam_event_type::joystick)
        {
            HandleInteractiveControlEvent(event);
            break;
        }
    }
}

void Sample::HandleInteractivityError(beam_event event)
{
    m_console->Write(L"Interactivity error!\n");
    m_console->Write(event.err_message().c_str());
    m_console->Write(L"\n");
    return;
}

void Sample::HandleInteractivityStateChange(beam_event event)
{
    std::shared_ptr<beam_interactivity_state_change_event_args> eventArgs = std::dynamic_pointer_cast<beam_interactivity_state_change_event_args>(event.event_args());
    if (eventArgs->new_state() == beam_interactivity_state::interactivity_disabled)
    {
        if (m_currentInteractiveState == beam_interactivity_state::initializing)
        {
            m_console->WriteLine(L"Interactivity initialized");

            InitializeGroupsAndScenes();
        }
        else
        {
            m_console->WriteLine(L"Interactivity stopped");
        }
        m_interactivityBtn->SetText(L"Go Interactive");
    }
    else if (eventArgs->new_state() == beam_interactivity_state::interactivity_pending)
    {
        m_console->WriteLine(L"Interactivity pending");
        m_interactivityBtn->SetText(L"Pending...");
    }
    else if (eventArgs->new_state() == beam_interactivity_state::interactivity_enabled)
    {
        m_console->WriteLine(L"Interactivity enabled");
        m_interactivityBtn->SetText(L"Stop Interactive");
    }
    m_currentInteractiveState = eventArgs->new_state();
    return;
}

void Sample::HandleParticipantStateChange(beam_event event)
{
    if (beam_event_type::participant_state_changed == event.event_type())
    {
        std::shared_ptr<beam_participant_state_change_event_args> participantStateArgs = std::dynamic_pointer_cast<beam_participant_state_change_event_args>(event.event_args());

        if (nullptr == participantStateArgs)
        {
            m_console->WriteLine(L"Invalid participant state change");
            return;
        }

        if (beam_participant_state::joined == participantStateArgs->state())
        {
            m_console->Write(participantStateArgs->participant()->beam_username().c_str());
            m_console->Write(L" is now participating\n");
            return;
        }
        else if (beam_participant_state::left == participantStateArgs->state())
        {
            m_console->Write(participantStateArgs->participant()->beam_username().c_str());
            m_console->Write(L" is no longer participating :(\n");
            return;
        }
    }
    return;
}

void Sample::HandleInteractiveControlEvent(beam_event event)
{
    if (beam_event_type::button == event.event_type())
    {
        std::shared_ptr<beam_button_event_args> buttonArgs = std::dynamic_pointer_cast<beam_button_event_args>(event.event_args());
        HandleButtonEvents(buttonArgs);
    }
    else if (beam_event_type::joystick == event.event_type())
    {
        std::shared_ptr<beam_joystick_event_args> joystickArgs = std::dynamic_pointer_cast<beam_joystick_event_args>(event.event_args());
        HandleJoystickEvents(joystickArgs);
    }
    return;
}

void Sample::HandleButtonEvents(std::shared_ptr<beam_button_event_args> buttonEventArgs)
{
    if (nullptr != buttonEventArgs)
    {
        string_t buttonId = buttonEventArgs->control_id();
        if (true == buttonEventArgs->is_pressed())
        {
            m_console->Write(L"Button ");
            m_console->Write(buttonId.c_str());
            m_console->Write(L" is down (");
            m_console->Write(buttonEventArgs->participant()->beam_username().c_str());
            m_console->Write(L")\n");
        }
        else
        {
            m_console->Write(L"Button ");
            m_console->Write(buttonId.c_str());
            m_console->Write(L" is up (");
            m_console->Write(buttonEventArgs->participant()->beam_username().c_str());
            m_console->Write(L")\n");

            if (0 == buttonId.compare(L"btnVoteYes"))
            {
                m_voteYesCount++;
                m_voteYesCountLabel->SetText(std::to_wstring(m_voteYesCount).c_str());
            }
            else if (0 == buttonId.compare(L"btnVoteNo"))
            {
                m_voteNoCount++;
                m_voteNoCountLabel->SetText(std::to_wstring(m_voteNoCount).c_str());
            }
        }
    }
    else
    {
        m_console->WriteLine(L"Invalid beam_button_event_args");
    }
}

void Sample::HandleJoystickEvents(std::shared_ptr<beam_joystick_event_args> joystickEventArgs)
{
    m_console->Write(L"Received joystick event for control ");
    m_console->Write(joystickEventArgs->control_id().c_str());
    m_console->Write(L":\n");
    m_console->Write(L"    participant: ");
    m_console->Write(joystickEventArgs->participant()->beam_username().c_str());
    m_console->Write(L"\n");
    m_console->Write(L"              x: ");
    m_console->Write(std::to_wstring(joystickEventArgs->x()).c_str());
    m_console->Write(L"\n");
    m_console->Write(L"              y: ");
    m_console->Write(std::to_wstring(joystickEventArgs->y()).c_str());
    m_console->Write(L"\n");
}

void Sample::SetAllButtonsEnabled(bool enabled)
{
    for (auto iter = m_buttons.begin(); iter != m_buttons.end(); iter++)
    {
        auto currButton = (*iter);
        currButton->SetEnabled(enabled);
    }
}
