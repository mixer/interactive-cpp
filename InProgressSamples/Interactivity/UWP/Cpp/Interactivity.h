//--------------------------------------------------------------------------------------
// Interactivity.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "SampleGUI.h"
#include "DeviceResources.h"
#include "LiveResources.h"
#include "StepTimer.h"
#include "beam.h"

using namespace XBOX_LIVE_NAMESPACE;


// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample : public DX::IDeviceNotify
{
public:

    Sample();

    // Initialization and management
    void Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);

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
    // void GetAllAchievements();

    void SetupUI();
    void Update(DX::StepTimer const& timer);
    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;
    std::unique_ptr<DirectX::Mouse>         m_mouse;

    // UI
    std::unique_ptr<ATG::UIManager>         m_ui;
    std::unique_ptr<DX::TextConsole>       m_console;

    // Xbox Live objects
    std::unique_ptr<ATG::LiveResources>     m_liveResources;
    //std::shared_ptr<beam_manager>           m_beamManager;
    beam_manager m_beamManager;

    DirectX::GamePad::ButtonStateTracker    m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker m_keyboardButtons;
};