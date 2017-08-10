// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "SampleGUI.h"
#include "LiveResources.h"

#include "namespaces.h"
#define _MIXERIMP_EXPORT
#include "interactivity_types.h"
#include "interactivity.h"

using namespace MICROSOFT_MIXER_NAMESPACE;

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample
{
public:

    Sample();

    // Initialization and management
    void Initialize(IUnknown* window);

    // Basic game loop
    void Tick();
    void Render();

    // Rendering helpers
    void Clear();

    // Messages
    void OnSuspending();
    void OnResuming();
    
private:

    // Interactivity Methods
    void InitializeInteractivity();
    void ToggleInteractivity();
    void SwitchScenes();
    void SendMessage();
    void SimulateUserChange();
    void InitializeGroupsAndScenes();
    void AddParticipantToGroup();
    void DisbandGroups();
    void ToggleButtonsEnabledState(string_t groupId);
    void TriggerCooldownOnButtons(string_t groupId);
    void SetProgress(string_t groupId);
    void ProcessInteractiveEvents(std::vector<interactive_event> events);
    void HandleInteractivityError(interactive_event event);
    void HandleInteractivityStateChange(interactive_event event);
    void HandleParticipantStateChange(interactive_event event);
    void HandleInteractiveControlEvent(interactive_event event);
    void HandleButtonEvents(std::shared_ptr<interactive_button_event_args> buttonEventArgs);
    void HandleJoystickEvents(std::shared_ptr<interactive_joystick_event_args> joystickEventArgs);
    void RefreshControlInputState();

    void SetupUI();
    void Update(DX::StepTimer const& timer);
    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    void SetAllButtonsEnabled(bool disabled);

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    uint64_t                                    m_frame;
    DX::StepTimer                               m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;

    // Xbox Live objects
    std::unique_ptr<ATG::LiveResources>         m_liveResources;

    // Interactivity objects
    std::shared_ptr<interactivity_manager> m_interactivityManager;
    interactivity_state      m_currentInteractiveState;
    std::vector<std::shared_ptr<interactive_scene>>            m_scenesList;
    std::vector<std::shared_ptr<interactive_group>>            m_groupsList;
    uint32_t                                            m_health;
    std::map<string_t, std::vector<string_t>>           m_groupToSceneMap;

    // UI Objects
    std::unique_ptr<ATG::UIManager>     m_ui;
    std::unique_ptr<DX::TextConsole>    m_console;
    std::vector<ATG::Button*>           m_buttons;
    ATG::Button*                        m_interactivityBtn;
    ATG::Button*                        m_disbandGroupsBtn;
    ATG::Button*                        m_cooldownControlsBtn;
    ATG::Button*                        m_setProgressBtn;
    ATG::Button*                        m_switchScenesBtn;
    ATG::Button*                        m_sendMessageBtn;
    ATG::Button*                        m_simulateUserChangeBtn;
    ATG::TextLabel*                     m_healthButtonLabel;
    ATG::TextLabel*                     m_healthButtonStateDownLabel;
    ATG::TextLabel*                     m_healthButtonStatePressedLabel;
    ATG::TextLabel*                     m_healthButtonStateUpLabel;
    ATG::TextLabel*                     m_healthButtonStateByParticipantDownLabel;
    ATG::TextLabel*                     m_healthButtonStateByParticipantPressedLabel;
    ATG::TextLabel*                     m_healthButtonStateByParticipantUpLabel;
    ATG::TextLabel*                     m_joystickXReadingLabel;
    ATG::TextLabel*                     m_joystickYReadingLabel;
    ATG::TextLabel*                     m_joystickXByParticipantReadingLabel;
    ATG::TextLabel*                     m_joystickYByParticipantReadingLabel;
};
