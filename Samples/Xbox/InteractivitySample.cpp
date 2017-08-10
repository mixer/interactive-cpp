// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pch.h"
#include "StatsSample.h"
#include "InteractivitySample.h"
#include "ATGColors.h"

using namespace DirectX;
using namespace MICROSOFT_MIXER_NAMESPACE;

namespace
{
    const int c_liveHUD                                     = 1000;
    const int c_sampleUIPanel                               = 2000;
    const int c_goInteractiveBtn                            = 2101;
    const int c_placeParticipantBtn                         = 2102;
    const int c_disbandGroupsBtn                            = 2103;
    const int c_toggleEnabledStateBtn                       = 2104;
    const int c_cooldownBtn                                 = 2106;
    const int c_setProgressBtn                              = 2107;
    const int c_switchScenesBtn                             = 2108;
    const int c_sendMessageBtn                              = 2109;
    const int c_healthButtonLabel                           = 1004;
    const int c_healthButtonStateDownLabel                  = 1008;
    const int c_healthButtonStatePressedLabel               = 1009;
    const int c_healthButtonStateUpLabel                    = 1010;
    const int c_healthButtonStateByParticipantDownLabel     = 1012;
    const int c_healthButtonStateByParticipantPressedLabel  = 1013;
    const int c_healthButtonStateByParticipantUpLabel       = 1014;
    const int c_joystickXReadingLabel                       = 1016;
    const int c_joystickYReadingLabel                       = 1017;
    const int c_joystickXByParticipantReadingLabel          = 1019;
    const int c_joystickYByParticipantReadingLabel          = 1020;

    // TODO: Replace the Project Version ID with your Project Version ID from Interactive Studio
    const string_t s_interactiveVersion     = L"76127";

    const string_t s_defaultGroup           = L"default";
    const string_t s_group1                 = L"group1";

    const string_t s_defaultScene           = L"default";
    const string_t s_scene1                 = L"Scene1";
}

Sample::Sample() :
    m_frame(0)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
    m_liveResources = std::make_unique<ATG::LiveResources>();

    ATG::UIConfig uiconfig;
    m_ui = std::make_unique<ATG::UIManager>(uiconfig);
    m_currentInteractiveState = interactivity_state::not_initialized;
    m_interactivityManager = interactivity_manager::get_singleton_instance();
    m_health = 0;
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
    //Register the Stats Sample ETW+ Provider
    EventRegisterXDKS_0301D082();

    m_gamePad = std::make_unique<GamePad>();

    m_ui->LoadLayout(L".\\Assets\\SampleUI.csv", L".\\Assets");

    m_healthButtonLabel = m_ui->FindControl<ATG::TextLabel>(2000, c_healthButtonLabel);

    m_healthButtonStateDownLabel = m_ui->FindControl<ATG::TextLabel>(2000, c_healthButtonStateDownLabel);
    m_healthButtonStatePressedLabel = m_ui->FindControl<ATG::TextLabel>(2000, c_healthButtonStatePressedLabel);
    m_healthButtonStateUpLabel = m_ui->FindControl<ATG::TextLabel>(2000, c_healthButtonStateUpLabel);

    m_healthButtonStateByParticipantDownLabel = m_ui->FindControl<ATG::TextLabel>(2000, c_healthButtonStateByParticipantDownLabel);
    m_healthButtonStateByParticipantPressedLabel = m_ui->FindControl<ATG::TextLabel>(2000, c_healthButtonStateByParticipantPressedLabel);
    m_healthButtonStateByParticipantUpLabel = m_ui->FindControl<ATG::TextLabel>(2000, c_healthButtonStateByParticipantUpLabel);

    m_joystickXReadingLabel = m_ui->FindControl<ATG::TextLabel>(2000, c_joystickXReadingLabel);
    m_joystickYReadingLabel = m_ui->FindControl<ATG::TextLabel>(2000, c_joystickYReadingLabel);

    m_joystickXByParticipantReadingLabel = m_ui->FindControl<ATG::TextLabel>(2000, c_joystickXByParticipantReadingLabel);
    m_joystickYByParticipantReadingLabel = m_ui->FindControl<ATG::TextLabel>(2000, c_joystickYByParticipantReadingLabel);

    // Set up references to buttons in the main scene
    m_interactivityBtn = m_ui->FindControl<ATG::Button>(2000, c_goInteractiveBtn);
    m_buttons.push_back(m_interactivityBtn);

    m_disbandGroupsBtn = m_ui->FindControl<ATG::Button>(2000, c_disbandGroupsBtn);
    m_buttons.push_back(m_disbandGroupsBtn);

    m_cooldownControlsBtn = m_ui->FindControl<ATG::Button>(2000, c_cooldownBtn);
    m_buttons.push_back(m_cooldownControlsBtn);

    m_setProgressBtn = m_ui->FindControl<ATG::Button>(2000, c_setProgressBtn);
    m_buttons.push_back(m_setProgressBtn);

    m_switchScenesBtn = m_ui->FindControl<ATG::Button>(2000, c_switchScenesBtn);
    m_buttons.push_back(m_switchScenesBtn);

    m_sendMessageBtn = m_ui->FindControl<ATG::Button>(2000, c_sendMessageBtn);
    m_buttons.push_back(m_sendMessageBtn);

    m_liveResources->Initialize(m_ui, m_ui->FindPanel<ATG::Overlay>(c_sampleUIPanel));
    m_deviceResources->SetWindow(window);

    m_deviceResources->CreateDeviceResources();  
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
    
    SetupUI();
}

#pragma region UI Methods
void Sample::SetupUI()
{
    using namespace ATG;

    // Toggle interactivity
    m_ui->FindControl<Button>(c_sampleUIPanel, c_goInteractiveBtn)->SetCallback([this](IPanel*, IControl*)
    {
        ToggleInteractivity();
    });

    // Add another participant to one of the groups
    m_ui->FindControl<Button>(c_sampleUIPanel, c_placeParticipantBtn)->SetCallback([this](IPanel*, IControl*)
    {
        AddParticipantToGroup();
    });

    // Move all participants back to the default group
    m_ui->FindControl<Button>(c_sampleUIPanel, c_disbandGroupsBtn)->SetCallback([this](IPanel*, IControl*)
    {
        DisbandGroups();
    });

    // Disable buttons on the default group
    m_ui->FindControl<Button>(c_sampleUIPanel, c_toggleEnabledStateBtn)->SetCallback([this](IPanel*, IControl*)
    {
        ToggleButtonsEnabledState(s_defaultGroup);
    });

    // Trigger cooldowns on the default group
    m_ui->FindControl<Button>(c_sampleUIPanel, c_cooldownBtn)->SetCallback([this](IPanel*, IControl*)
    {
        TriggerCooldownOnButtons(s_defaultGroup);
    });

    // Set progress on the buttons in the default scene
    m_ui->FindControl<Button>(c_sampleUIPanel, c_setProgressBtn)->SetCallback([this](IPanel*, IControl*)
    {
        SetProgress(s_defaultGroup);
    });

    // Rotate through scenes defined for the groups
    m_ui->FindControl<Button>(c_sampleUIPanel, c_switchScenesBtn)->SetCallback([this](IPanel*, IControl*)
    {
        SwitchScenes();
    });

    // SEnds a 
    m_ui->FindControl<Button>(c_sampleUIPanel, c_sendMessageBtn)->SetCallback([this](IPanel*, IControl*)
    {
        SendMessage();
    });
}
#pragma endregion

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frame %I64u", m_frame);

    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();

    PIXEndEvent();
    m_frame++;
}

// Updates the world.
void Sample::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float elapsedTime = float(timer.GetElapsedSeconds());

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (!m_ui->Update(elapsedTime, pad))
        {
            if (pad.IsViewPressed())
            {
                Windows::ApplicationModel::Core::CoreApplication::Exit();
            }
            if (pad.IsMenuPressed())
            {
                Windows::Xbox::UI::SystemUI::ShowAccountPickerAsync(nullptr,Windows::Xbox::UI::AccountPickerOptions::None);
            }
        }
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    RefreshControlInputState();

    std::vector<interactive_event> events = m_interactivityManager->do_work();
    ProcessInteractiveEvents(events);

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Sample::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // Prepare the render target to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    PIXEndEvent(context);

    m_ui->Render();
    m_console->Render();
    // Show the new frame.
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit();
    PIXEndEvent(context);
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto renderTarget = m_deviceResources->GetBackBufferRenderTargetView();

    context->ClearRenderTargetView(renderTarget, ATG::Colors::Background);

    context->OMSetRenderTargets(1, &renderTarget, nullptr);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    PIXEndEvent(context);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Sample::OnSuspending()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Suspend(0);
}

void Sample::OnResuming()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Resume();
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
    m_ui->Reset();
    m_liveResources->Refresh();
}

#pragma endregion


#pragma region Interactivity Methods

void Sample::InitializeInteractivity()
{
    auto user = m_liveResources->GetUser();

    if (nullptr != user)
    {
        m_interactivityManager->set_local_user(user);

        m_interactivityManager->initialize(s_interactiveVersion, true /*goInteractive*/);
    }
    else
    {
        m_console->WriteLine(L"ERROR: no signed in local user");
    }
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
        std::shared_ptr<interactive_scene> currentScene = currentGroup->scene();
        std::shared_ptr<interactive_scene> sceneToSet = nullptr;

        if (0 == currentScene->scene_id().compare(s_defaultScene))
        {
            sceneToSet = m_interactivityManager->scene(s_scene1);
        }
        else if (0 == currentScene->scene_id().compare(s_scene1))
        {
            sceneToSet = m_interactivityManager->scene(s_defaultScene);
        }

        if (sceneToSet != nullptr)
        {
            m_console->Write(L"Updating scenes.");
            m_console->Write(L"\n");

            currentGroup->set_scene(sceneToSet);
        }
    }
}

void Sample::SendMessage()
{
    // This is a sample message that sets a bandwidth throttle.
    const string_t message = L"{\"type\": \"method\","\
                                L"\"id\": 123,"\
                                L"\"method\": \"setBandwidthThrottle\","\
                                L"    \"discard\": true,"\
                                L"    \"params\": {"\
                                L"        \"giveInput\": {"\
                                L"            \"capacity\": 10000000,"\
                                L"            \"drainRate\": 3000000"\
                                L"        },"\
                                L"        \"onParticipantJoin\": {"\
                                L"            \"capacity\": 0,"\
                                L"            \"drainRate\": 0"\
                                L"        },"\
                                L"        \"onParticipantLeave\": null"\
                                L"    }"\
                                L"}";
    m_interactivityManager->send_message(message);
}

void Sample::SimulateUserChange()
{
    SetAllButtonsEnabled(false);

    m_interactivityManager->stop_interactive();
    m_interactivityBtn->SetText(L"Go Interactive");

    while (true)
    {
        auto currentInteractiveState = m_interactivityManager->interactivity_state();

        if (interactivity_state::interactivity_disabled == currentInteractiveState)
        {
            OutputDebugStringW(L"Interactivity now disabled\n");
            break;
        }

        Sleep(100);
    }

    InitializeInteractivity();

    while (true)
    {
        auto currentInteractiveState = m_interactivityManager->interactivity_state();

        if (interactivity_state::interactivity_disabled == currentInteractiveState)
        {
            OutputDebugStringW(L"Interactivity now back to disabled\n");
            break;
        }

        Sleep(100);
    }

    m_interactivityManager->start_interactive();

    SetAllButtonsEnabled(true);
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
        auto scene1 = m_interactivityManager->scene(s_scene1);

        if (nullptr == scene1)
        {
            m_console->Write(L"Unexpected error: \"");
            m_console->Write(s_scene1.c_str());
            m_console->Write(L"\" does not exist\n");
            return;
        }

        std::shared_ptr<interactive_group> group1 = std::make_shared<interactive_group>(s_group1, scene1);
        m_groupsList = m_interactivityManager->groups();
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

void Sample::ToggleButtonsEnabledState(string_t groupId)
{
    m_console->Write(L"Toggling disabled / enabled all visible buttons for group: ");
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

void Sample::SetProgress(string_t groupId)
{
    m_console->Write(L"Set progress to 50% for all visible buttons for group: ");
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
        case interactive_event_type::unknown:
            std::shared_ptr<interactive_message_event_args> messageEventArgs = std::dynamic_pointer_cast<interactive_message_event_args>(event.event_args());
            m_console->Write(messageEventArgs->message().c_str());
            m_console->Write(L"\n");
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

            if (0 == buttonId.compare(L"GiveHealth"))
            {
                m_health++;
                m_healthButtonLabel->SetText(std::to_wstring(m_health).c_str());
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
void Sample::RefreshControlInputState()
{
    bool isDown = false;
    bool isPressed = false;
    bool isUp = false;

    bool isDownByMixerID = false;
    bool isPressedByMixerID = false;
    bool isUpByMixerID = false;

    if (m_interactivityManager->interactivity_state() >= interactivity_state::initialized)
    {
        LPCWSTR healthButtonID = L"GiveHealth";
        auto defaultGroup = m_interactivityManager->group(L"default");
        auto foo = defaultGroup->participants();
        auto defaultScene = m_interactivityManager->scene(L"default");
        auto healthButton = defaultScene->button(healthButtonID).get();
        
        // We know there will be at least one participant (the broadcaster)
        // so we'll use their ID to test getting input by Mixer ID.
        uint32_t mixerId = 0;
        if (m_interactivityManager->participants().size() > 0)
        {
            mixerId = m_interactivityManager->participants()[0]->mixer_id();
        }

        isDown = healthButton->is_up();
        isPressed = healthButton->is_pressed();
        isUp = healthButton->is_down();

        isUpByMixerID = healthButton->is_up(mixerId);
        isPressedByMixerID = healthButton->is_pressed(mixerId);
        isDownByMixerID = healthButton->is_down(mixerId);
        
        auto joystick = defaultScene->joystick(L"Joystick");
        m_joystickXReadingLabel->SetText(joystick->x().ToString()->Data());
        m_joystickYReadingLabel->SetText(joystick->y().ToString()->Data());

        m_joystickXByParticipantReadingLabel->SetText(joystick->x(mixerId).ToString()->Data());
        m_joystickYByParticipantReadingLabel->SetText(joystick->y(mixerId).ToString()->Data());
    }

    // Button state
    if (isDown)
    {
        m_healthButtonStateDownLabel->SetText(L"True");
    }
    else
    {
        m_healthButtonStateDownLabel->SetText(L"False");
    }
    if (isPressed)
    {
        m_healthButtonStatePressedLabel->SetText(L"True");
    }
    else
    {
        m_healthButtonStatePressedLabel->SetText(L"False");
    }
    if (isUp)
    {
        m_healthButtonStateUpLabel->SetText(L"True");
    }
    else
    {
        m_healthButtonStateUpLabel->SetText(L"False");
    }

    // Button state by participant
    if (isUpByMixerID)
    {
        m_healthButtonStateByParticipantUpLabel->SetText(L"True");
    }
    else
    {
        m_healthButtonStateByParticipantUpLabel->SetText(L"False");
    }
    if (isPressedByMixerID)
    {
        m_healthButtonStateByParticipantPressedLabel->SetText(L"True");
    }
    else
    {
        m_healthButtonStateByParticipantPressedLabel->SetText(L"False");
    }
    if (isDownByMixerID)
    {
        m_healthButtonStateByParticipantDownLabel->SetText(L"True");
    }
    else
    {
        m_healthButtonStateByParticipantDownLabel->SetText(L"False");
    }
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());
    m_console = std::make_unique<DX::TextConsole>(context,L"Courier_16.spritefont");

    m_ui->RestoreDevice(context);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    RECT fullscreen = m_deviceResources->GetOutputSize();
    static const RECT consoleDisplay = { 960, 150, 1838, 825 };

    m_ui->SetWindow(fullscreen);
    m_console->SetWindow(consoleDisplay);
}

void Sample::SetAllButtonsEnabled(bool enabled)
{
    for (auto iter = m_buttons.begin(); iter != m_buttons.end(); iter++)
    {
        auto currButton = (*iter);
        currButton->SetEnabled(enabled);
    }
}
#pragma endregion
