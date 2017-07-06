// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include "SampleGUI.h"
#include "DeviceResources.h"
#include "LiveResources.h"
#include "StepTimer.h"

#include "namespaces.h"
#define _MIXERIMP_EXPORT
#include "interactivity_types.h"
#include "interactivity.h"

using namespace MICROSOFT_MIXER_NAMESPACE;

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample : public DX::IDeviceNotify, public std::enable_shared_from_this<Sample>
{
public:

    Sample();

    // Initialization and management
    void Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);
    void HandleSignin(
        _In_ std::shared_ptr<xbox::services::system::xbox_live_user> user,
        _In_ xbox::services::system::sign_in_status result
        );
    void HandleSignout(_In_ std::shared_ptr<xbox::services::system::xbox_live_user> user);

    void SetupDefaultUser(Windows::System::User^ systemUser);

    // Basic render loop
    void Tick();
    void Render();

    // Rendering helpers
    void Clear();

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
    void ValidateDevice();

    // Properties
    void GetDefaultSize( int& width, int& height ) const;

private:

    // Interactivity Methods

    void SetupUI();
    void Update(DX::StepTimer const& timer);
    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void InitializeInteractiveManager();
    void InitializeInteractivity();
    void AddUserToInteractiveManager(_In_ std::shared_ptr<xbox::services::system::xbox_live_user> user);
    void UpdateInteractivityManager();
    void ToggleInteractivity();
    void SwitchScenes();
    void InitializeGroupsAndScenes();
    void AddParticipantToGroup();
    void DisbandGroups();
    void ToggleEnabledStateForControls(string_t groupId);
    void TriggerCooldownOnButtons(string_t groupId);
	void SetProgressOnButtons(string_t groupId);
    void ProcessInteractiveEvents(std::vector<interactive_event> events);
    void HandleInteractivityError(interactive_event event);
    void HandleInteractivityStateChange(interactive_event event);
    void HandleParticipantStateChange(interactive_event event);
    void HandleInteractiveControlEvent(interactive_event event);
    void HandleButtonEvents(std::shared_ptr<interactive_button_event_args> buttonEventArgs);
    void HandleJoystickEvents(std::shared_ptr<interactive_joystick_event_args> joystickEventArgs);
    void SetAllButtonsEnabled(bool disabled);
	void RefreshControlInputState();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    DirectX::GamePad::ButtonStateTracker    m_gamePadButtons[DirectX::GamePad::MAX_PLAYER_COUNT];
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;
    std::unique_ptr<DirectX::Mouse>         m_mouse;

    // UI
    std::shared_ptr<ATG::UIManager>         m_ui;
    std::unique_ptr<DX::TextConsole>        m_console;

    // Xbox Live objects
    std::unique_ptr<ATG::LiveResources>     m_liveResources;
    function_context                        m_signInContext;
    function_context                        m_signOutContext;
    std::shared_ptr<xbox::services::system::xbox_live_user> m_user;

    // Interactivity
    std::shared_ptr<MICROSOFT_MIXER_NAMESPACE::interactivity_manager> m_interactivityManager;
    MICROSOFT_MIXER_NAMESPACE::interactivity_state      m_currentInteractiveState;
    std::vector<std::shared_ptr<interactive_scene>>            m_scenesList;
    std::vector<std::shared_ptr<interactive_group>>            m_groupsList;
    uint32_t                                            m_voteYesCount;
    uint32_t                                            m_voteNoCount;
    std::map<string_t, std::vector<string_t>>           m_groupToSceneMap;

    DirectX::Keyboard::KeyboardStateTracker m_keyboardButtons;
    std::vector<ATG::Button*>               m_buttons;
    ATG::Button*                            m_interactivityBtn;
    ATG::Button*                            m_disbandGroupsBtn;
    ATG::Button*                            m_toggleEnabledBtn;
    ATG::Button*                            m_cooldownBtn;
    ATG::Button*                            m_setProgressBtn;
    ATG::Button*                            m_switchScenesBtn;
    ATG::Button*                            m_simulateUserChangeBtn;
    ATG::TextLabel*                         m_voteYesCountLabel;
    ATG::TextLabel*                         m_voteNoCountLabel;
    ATG::TextLabel*                         m_yesButtonStateDownLabel;
    ATG::TextLabel*                         m_yesButtonStatePressedLabel;
    ATG::TextLabel*                         m_yesButtonStateUpLabel;
    ATG::TextLabel*                         m_yesButtonStateByParticipantDownLabel;
    ATG::TextLabel*                         m_yesButtonStateByParticipantPressedLabel;
    ATG::TextLabel*                         m_yesButtonStateByParticipantUpLabel;
    ATG::TextLabel*                         m_joystickXReadingLabel;
    ATG::TextLabel*                         m_joystickYReadingLabel;
    ATG::TextLabel*                         m_joystickXByParticipantReadingLabel;
    ATG::TextLabel*                         m_joystickYByParticipantReadingLabel;
};