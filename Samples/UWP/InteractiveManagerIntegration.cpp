// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pch.h"
#include <time.h>
#include "InteractivitySample.h"

using namespace Concurrency;
using namespace Windows::Foundation::Collections;
using namespace Windows::Foundation;
using namespace xbox::services;
using namespace xbox::services::stats::manager;
using namespace MICROSOFT_MIXER_NAMESPACE;

namespace
{
    const int c_debugLog = 202;

    const int c_maxLeaderboards = 10;
    const int c_liveHUD = 1000;
    const int c_sampleUIPanel = 2000;
    const int c_goInteractiveBtn = 2101;
    const int c_placeParticipantBtn = 2102;
    const int c_disbandGroupsBtn = 2103;
	const int c_toggleEnabledBtn = 2104;
    const int c_cooldownBtn = 2106;
	const int c_setProgressBtn = 2107;
    const int c_switchScenesBtn = 2108;
    const int c_voteYesCountLabel = 1004;
    const int c_voteNoCountLabel = 1006;
	const int c_yesBtnDownLabel = 1008;
	const int c_yesBtnPressedLabel = 1009;
	const int c_yesBtnUpLabel = 1010;
	const int c_yesBtnDownByParticipantLabel = 1012;
	const int c_yesBtnPressedByParticipantLabel = 1013;
	const int c_yesBtnUpByParticipantLabel = 1014;
	const int c_yesJoystickXLabel = 1016;
	const int c_yesJoystickYLabel = 1017;
	const int c_yesJoystickXByParticipantLabel = 1019;
	const int c_yesJoystickYByParticipantLabel = 1020;

    const string_t s_interactiveVersion = L"76127";

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
    m_interactivityManager = interactivity_manager::get_singleton_instance();

    m_currentInteractiveState = interactivity_state::not_initialized;
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

	m_toggleEnabledBtn = m_ui->FindControl<ATG::Button>(2000, c_toggleEnabledBtn);
    m_buttons.push_back(m_toggleEnabledBtn);

	m_cooldownBtn = m_ui->FindControl<ATG::Button>(2000, c_cooldownBtn);
	m_buttons.push_back(m_cooldownBtn);

    m_setProgressBtn = m_ui->FindControl<ATG::Button>(2000, c_setProgressBtn);
    m_buttons.push_back(m_setProgressBtn);

    m_switchScenesBtn = m_ui->FindControl<ATG::Button>(2000, c_switchScenesBtn);
    m_buttons.push_back(m_switchScenesBtn);
}

void Sample::AddUserToInteractiveManager(_In_ std::shared_ptr<xbox::services::system::xbox_live_user> user)
{
    stringstream_t source;
    source << _T("Adding user ");
    source << user->gamertag();
    source << _T(" to Interactive Manager");
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
                m_interactivityManager->set_xtoken(token);
            }
        }
        catch (Platform::Exception^ ex)
        {
            this->m_console->WriteLine(ex->Message->Data());
        }
    });
}

void Sample::UpdateInteractivityManager()
{
    // Process events from the stats manager
    // This should be called each frame update

    auto interactiveEvents = m_interactivityManager->do_work();

    for (const auto& evt : interactiveEvents)
    {
        interactive_event_type eventType = evt.event_type();

        switch (eventType)
        {
        case interactive_event_type::interactivity_state_changed:
            HandleInteractivityStateChange(evt);
            break;
        case interactive_event_type::participant_state_changed:
            HandleParticipantStateChange(evt);
            break;
        case interactive_event_type::error:
            HandleInteractivityError(evt);
            break;
        }

        // If you want to handle the interactive input manually:
        if (eventType == interactive_event_type::button || eventType == interactive_event_type::joystick)
        {
            HandleInteractiveControlEvent(evt);
            break;
        }
    }
}

void Sample::InitializeInteractivity()
{
    m_interactivityManager->initialize(s_interactiveVersion, true /*goInteractive*/);
}


void Sample::ToggleInteractivity()
{
    auto currentInteractiveState = m_interactivityManager->interactivity_state();
    if (interactivity_state::not_initialized == currentInteractiveState)
    {
        InitializeInteractivity();
    }
    else if (interactivity_state::interactivity_enabled == currentInteractiveState ||
        interactivity_state::interactivity_pending == currentInteractiveState)
    {
        m_interactivityManager->stop_interactive();
        m_interactivityBtn->SetText(L"Go Interactive");
    }
    else if (interactivity_state::interactivity_disabled == currentInteractiveState)
    {
        m_interactivityManager->start_interactive();
    }
    else
    {
        // Do nothing, interactive state is pending, wait till it completes
    }
}

void Sample::SwitchScenes()
{
    auto groupsList = m_interactivityManager->groups();

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
                    auto sceneToSet = m_interactivityManager->scene(newSceneId);

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
    auto scenesList = m_interactivityManager->scenes();
    auto groupsList = m_interactivityManager->groups();

    if (groupsList.size() == 0 || scenesList.size() == 0)
    {
        m_console->WriteLine(L"Groups and scenes not yet populated - wait for interactivity to initialize.");
        return;
    }

    if (groupsList.size() == 1)
    {
        auto redScene = m_interactivityManager->scene(s_redLasersScene);
        auto blueScene = m_interactivityManager->scene(s_blueLasersScene);

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

        std::shared_ptr<interactive_group> groupRedTeam = std::make_shared<interactive_group>(s_redGroup, redScene);

        std::shared_ptr<interactive_group> groupBlueTeam = std::make_shared<interactive_group>(s_blueGroup, blueScene);

        m_groupsList = m_interactivityManager->groups();

        if (m_groupsList.size() != 3)
        {
            m_console->WriteLine(L"Unexpected error: invalid group size after group initialization");
            return;
        }
    }
}

void Sample::AddParticipantToGroup()
{
    auto interactiveState = m_interactivityManager->interactivity_state();

    if (interactivity_state::not_initialized == interactiveState || interactivity_state::initializing == interactiveState)
    {
        m_console->WriteLine(L"Wait for interactivity to initialize.");
        return;
    }

    auto groupsList = m_interactivityManager->groups();

    if (groupsList.size() == 0)
    {
        m_console->WriteLine(L"Unexpected error - no groups defined");
        return;
    }
    else
    {
        auto defaultGroup = m_interactivityManager->group(s_defaultGroup);
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
            m_console->Write(participant->username().c_str());
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
    auto interactiveState = m_interactivityManager->interactivity_state();

    if (interactivity_state::not_initialized == interactiveState || interactivity_state::initializing == interactiveState)
    {
        m_console->WriteLine(L"Wait for interactivity to initialize.");
        return;
    }

    auto groupsList = m_interactivityManager->groups();

    if (groupsList.size() == 0)
    {
        m_console->WriteLine(L"Unexpected error - no groups defined");
        return;
    }
    else
    {
        auto defaultGroup = m_interactivityManager->group(s_defaultGroup);
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
                    m_console->Write(participant->username().c_str());
                    m_console->Write(L" back to the default group");
                    m_console->Write(L"\n");
                }
            }
        }
    }
}

// For now this method only disables and enables buttons. Soon the SDK will have support for disabling joysticks
// and we can modify this method to disable and enable joysticks as well.
void Sample::ToggleEnabledStateForControls(string_t groupId)
{
    m_console->Write(L"Toggling the enabled / disabled state for all visible buttons for group: ");
    m_console->Write(groupId.c_str());
    m_console->Write(L"\n");

    auto currentScene = m_interactivityManager->group(groupId)->scene();

    auto buttons = currentScene->buttons();
    for (auto & button : buttons)
    {
        if (nullptr != button)
        {
            if (button->disabled())
            {
                button->set_disabled(false);
            }
            else
            {
                button->set_disabled(true);
            }
        }
    }
}

void Sample::TriggerCooldownOnButtons(string_t groupId)
{
    m_console->Write(L"Triggering a 5 second cooldown for all visible buttons for group: ");
    m_console->Write(groupId.c_str());
    m_console->Write(L"\n");

    auto currentScene = m_interactivityManager->group(groupId)->scene();

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

void Sample::SetProgressOnButtons(string_t groupId)
{
	m_console->Write(L"Sets progress to 50% for all visible buttons for group: ");
	m_console->Write(groupId.c_str());
	m_console->Write(L"\n");

	auto currentScene = m_interactivityManager->group(groupId)->scene();

	auto buttons = currentScene->buttons();
	for (auto & button : buttons)
	{
		if (nullptr != button)
		{
			button->set_progress(0.5);
		}
	}
}

void Sample::ProcessInteractiveEvents(std::vector<interactive_event> events)
{
    for (int i = 0; i < events.size(); i++)
    {
        interactive_event event = events[i];
        interactive_event_type eventType = event.event_type();

        switch (eventType)
        {
        case interactive_event_type::interactivity_state_changed:
            HandleInteractivityStateChange(event);
            break;
        case interactive_event_type::participant_state_changed:
            HandleParticipantStateChange(event);
            break;
        case interactive_event_type::error:
            HandleInteractivityError(event);
            break;
        }

        // If you want to handle the interactive input manually:
        if (eventType == interactive_event_type::button || eventType == interactive_event_type::joystick)
        {
            HandleInteractiveControlEvent(event);
            break;
        }
    }
}

void Sample::HandleInteractivityError(interactive_event event)
{
    m_console->Write(L"Interactivity error!\n");
    m_console->Write(event.err_message().c_str());
    m_console->Write(L"\n");
    return;
}

void Sample::HandleInteractivityStateChange(interactive_event event)
{
    std::shared_ptr<interactivity_state_change_event_args> eventArgs = std::dynamic_pointer_cast<interactivity_state_change_event_args>(event.event_args());
    if (eventArgs->new_state() == interactivity_state::interactivity_disabled)
    {
        if (m_currentInteractiveState == interactivity_state::initializing)
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
    else if (eventArgs->new_state() == interactivity_state::interactivity_pending)
    {
        m_console->WriteLine(L"Interactivity pending");
        m_interactivityBtn->SetText(L"Pending...");
    }
    else if (eventArgs->new_state() == interactivity_state::interactivity_enabled)
    {
        m_console->WriteLine(L"Interactivity enabled");
        m_interactivityBtn->SetText(L"Stop Interactive");
    }
    m_currentInteractiveState = eventArgs->new_state();
    return;
}

void Sample::HandleParticipantStateChange(interactive_event event)
{
    if (interactive_event_type::participant_state_changed == event.event_type())
    {
        std::shared_ptr<interactive_participant_state_change_event_args> participantStateArgs = std::dynamic_pointer_cast<interactive_participant_state_change_event_args>(event.event_args());

        if (nullptr == participantStateArgs)
        {
            m_console->WriteLine(L"Invalid participant state change");
            return;
        }

        if (interactive_participant_state::joined == participantStateArgs->state())
        {
            m_console->Write(participantStateArgs->participant()->username().c_str());
            m_console->Write(L" is now participating\n");
            return;
        }
        else if (interactive_participant_state::left == participantStateArgs->state())
        {
            m_console->Write(participantStateArgs->participant()->username().c_str());
            m_console->Write(L" is no longer participating :(\n");
            return;
        }
    }
    return;
}

void Sample::HandleInteractiveControlEvent(interactive_event event)
{
    if (interactive_event_type::button == event.event_type())
    {
        std::shared_ptr<interactive_button_event_args> buttonArgs = std::dynamic_pointer_cast<interactive_button_event_args>(event.event_args());
        HandleButtonEvents(buttonArgs);
    }
    else if (interactive_event_type::joystick == event.event_type())
    {
        std::shared_ptr<interactive_joystick_event_args> joystickArgs = std::dynamic_pointer_cast<interactive_joystick_event_args>(event.event_args());
        HandleJoystickEvents(joystickArgs);
    }
    return;
}

void Sample::HandleButtonEvents(std::shared_ptr<interactive_button_event_args> buttonEventArgs)
{
    if (nullptr != buttonEventArgs)
    {
        string_t buttonId = buttonEventArgs->control_id();
        if (true == buttonEventArgs->is_pressed())
        {
            if (!buttonEventArgs->transaction_id().empty())
            {
                m_interactivityManager->capture_transaction(buttonEventArgs->transaction_id());
            }

            m_console->Write(L"Button ");
            m_console->Write(buttonId.c_str());
            m_console->Write(L" is down (charged ");
            m_console->Write(buttonEventArgs->participant()->username().c_str());
            m_console->Write(L" ");
            m_console->Write(std::to_wstring(buttonEventArgs->cost()).c_str());
            m_console->Write(L" spark(s))\n");
        }
        else
        {
            m_console->Write(L"Button ");
            m_console->Write(buttonId.c_str());
            m_console->Write(L" is up (");
            m_console->Write(buttonEventArgs->participant()->username().c_str());
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
        m_console->WriteLine(L"Invalid interactive_button_event_args");
    }
}

void Sample::HandleJoystickEvents(std::shared_ptr<interactive_joystick_event_args> joystickEventArgs)
{
    m_console->Write(L"Received joystick event for control ");
    m_console->Write(joystickEventArgs->control_id().c_str());
    m_console->Write(L":\n");
    m_console->Write(L"    participant: ");
    m_console->Write(joystickEventArgs->participant()->username().c_str());
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
