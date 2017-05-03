// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "SampleGUI.h"
#include "LiveResources.h"

#include "namespaces.h"
#define _BEAMIMP_EXPORT
#include "beam_types.h"
#include "beam.h"

using namespace xbox::services::beam;

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
    void ToggleInteractivity();
    void SwitchScenes();
    void InitializeGroupsAndScenes();
    void AddParticipantToGroup();
    void DisbandGroups();
    void TriggerCooldownOnButtons(string_t groupId);
    void ProcessInteractiveEvents(std::vector<beam_event> events);
    void HandleInteractivityError(beam_event event);
    void HandleInteractivityStateChange(beam_event event);
    void HandleParticipantStateChange(beam_event event);
    void HandleInteractiveControlEvent(beam_event event);
    void HandleButtonEvents(std::shared_ptr<beam_button_event_args> buttonEventArgs);
    void HandleJoystickEvents(std::shared_ptr<beam_joystick_event_args> joystickEventArgs);

    void SetupUI();
    void Update(DX::StepTimer const& timer);
    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

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
    std::shared_ptr<xbox::services::beam::beam_manager> m_beamManager;
    xbox::services::beam::beam_interactivity_state      m_currentInteractiveState;
    std::vector<std::shared_ptr<beam_scene>>            m_scenesList;
    std::vector<std::shared_ptr<beam_group>>            m_groupsList;
    uint32_t                                            m_voteYesCount;
    uint32_t                                            m_voteNoCount;
    std::map<string_t, std::vector<string_t>>           m_groupToSceneMap;

    // UI Objects
    std::unique_ptr<ATG::UIManager>     m_ui;
    std::unique_ptr<DX::TextConsole>    m_console; 
    ATG::Button*                        m_interactivityBtn;
    ATG::Button*                        m_disbandGroupsBtn;
    ATG::Button*                        m_cooldownRedControlsBtn;
    ATG::Button*                        m_cooldownBlueControlsBtn;
    ATG::Button*                        m_switchScenesBtn;
    ATG::TextLabel*                     m_voteYesCountLabel;
    ATG::TextLabel*                     m_voteNoCountLabel;
};
