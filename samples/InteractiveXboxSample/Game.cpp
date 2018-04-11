//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

using namespace DirectX;
using namespace Windows::Xbox::Input;
using namespace Windows::Foundation::Collections;

using Microsoft::WRL::ComPtr;

Game::Game() :
    m_window(nullptr),
    m_outputWidth(1920),
    m_outputHeight(1080),
    m_featureLevel(D3D_FEATURE_LEVEL_11_1),
    m_frame(0)
{
}

// STL includes
#include <chrono>
#include <thread>
#include <iostream>
#include <codecvt>
#include <string>

std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

#define CLIENT_ID		"f0d20e2d263b75894f5cdaabc8a344b99b1ea6f9ecb7fa4f"
#define INTERACTIVE_ID	"135704"
#define SHARE_CODE		"xe7dpqd5"

// Display a short code to the user in order to obtain a reusable token for future connections.
int authorize(std::string& authorization)
{
	int err = 0;
	char shortCode[7];
	size_t shortCodeLength = sizeof(shortCode);
	char shortCodeHandle[1024];
	size_t shortCodeHandleLength = sizeof(shortCodeHandle);
	err = interactive_auth_get_short_code(CLIENT_ID, shortCode, &shortCodeLength, shortCodeHandle, &shortCodeHandleLength);
	if (err) return err;
	
	std::cout << "Visit " << "https://www.mixer.com/go?code=" << shortCode;

	// Wait for OAuth token response.
	char refreshTokenBuffer[1024];
	size_t refreshTokenLength = sizeof(refreshTokenBuffer);
	err = interactive_auth_wait_short_code(CLIENT_ID, shortCodeHandle, refreshTokenBuffer, &refreshTokenLength);
	if (err) return err;

	/*
	*	TODO:	This is where you would serialize the refresh token for future use in a way that is associated with the current user.
	*			Future calls would then only need to check if the token is stale, refresh it if so, and then parse the new authorization header.
	*/

	// Extract the authorization header from the refresh token.
	char authBuffer[1024];
	size_t authBufferLength = sizeof(authBuffer);
	err = interactive_auth_parse_refresh_token(refreshTokenBuffer, authBuffer, &authBufferLength);
	if (err) return err;

	authorization = std::string(authBuffer, authBufferLength);
	return 0;
}


// Initialize the Direct3D resources required to run.
void Game::Initialize(IUnknown* window)
{
    m_window = window;

    CreateDevice();

    CreateResources();

    // TODO: Change the timer settings if you want something other than the default variable timestep mode.
    // e.g. for 60 FPS fixed timestep update logic, call:
    /*
    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / 60);
    */

	int err = 0;

	// Get an authorization token for the user to pass to the connect function.
	std::string authorization;
	err = authorize(authorization);
	if (err)
	{
		throw err;
		return;
	}

	// Connect to the user's interactive channel, using the interactive project specified by the version ID.
	err = interactive_open_session(authorization.c_str(), INTERACTIVE_ID, SHARE_CODE, true, &m_interactiveSession);
	if (err) throw err;

	err = interactive_set_session_context(m_interactiveSession, this);
	if (err) throw err;

	// Register a callback for button presses.
	err = interactive_register_input_handler(m_interactiveSession, [](void* context, interactive_session session, const interactive_input* input)
	{
		Game* game = (Game*)context;
		if (input_type_button == input->type && interactive_button_action::down == input->buttonData.action)
		{
			// Capture the transaction on button down to deduct sparks
			int err = interactive_capture_transaction(session, input->transactionId);
			if (!err)
			{
				game->m_controlsById[input->transactionId] = input->control.id;
			}

			
		}
	});
	if (err) throw err;

	err = interactive_register_transaction_complete_handler(m_interactiveSession, [](void* context, interactive_session session, const char* transactionId, size_t transactionIdLength, unsigned int errorCode, const char* errorMessage, size_t errorMessageLength)
	{
		UNREFERENCED_PARAMETER(session);
		UNREFERENCED_PARAMETER(transactionIdLength);
		UNREFERENCED_PARAMETER(errorMessageLength);
		Game* game = (Game*)context;
		if (errorCode)
		{
			std::cerr << errorMessage << "(" << std::to_string(errorCode) << ")" << std::endl;
		}
		else
		{
			// Transaction was captured, now execute the most super awesome interactive functionality!
			std::string controlId = game->m_controlsById[transactionId];
			if (0 == strcmp("GiveHealth", controlId.c_str()))
			{
				std::cout << "Giving health to the player!" << std::endl;
			}
		}

		game->m_controlsById.erase(transactionId);
	});
}

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
    PIXBeginEvent(EVT_COLOR_FRAME, L"Frame %I64u", m_frame);

    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();

    PIXEndEvent();
    m_frame++;
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(EVT_COLOR_UPDATE, L"Update");

    // Allow the game to exit by pressing the view button on a controller.
    // This is just a helper for development.
    IVectorView<IGamepad^>^ gamepads = Gamepad::Gamepads;

    for (unsigned i = 0; i < gamepads->Size; i++)
    {
        IGamepadReading^ reading = gamepads->GetAt(i)->GetCurrentReading();
        if (reading->IsViewPressed)
        {
            Windows::ApplicationModel::Core::CoreApplication::Exit();
        }
    }

    float elapsedTime = float(timer.GetElapsedSeconds());

    // TODO: Add your game logic here.
    elapsedTime;

	int err = interactive_run(m_interactiveSession, 1);
	if (err)
	{
		std::cerr << "Failed to process interactive event, error: " << std::to_string(err);
	}

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    Clear();

    PIXBeginEvent(m_d3dContext.Get(), EVT_COLOR_RENDER, L"Render");

    // TODO: Add your rendering code here.

    PIXEndEvent(m_d3dContext.Get());

    Present();
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    PIXBeginEvent(m_d3dContext.Get(), PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    XMVECTORF32 clearColor;
    clearColor.v = XMColorSRGBToRGB(Colors::CornflowerBlue);
    m_d3dContext->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
    m_d3dContext->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    m_d3dContext->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());

    // Set the viewport.
    CD3D11_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(m_outputWidth), static_cast<float>(m_outputHeight));
    m_d3dContext->RSSetViewports(1, &viewport);

    PIXEndEvent(m_d3dContext.Get());
}

// Presents the back buffer contents to the screen.
void Game::Present()
{
    PIXBeginEvent(m_d3dContext.Get(), EVT_COLOR_FRAME, L"Present");

    // The first argument instructs DXGI to block until VSync, putting the application
    // to sleep until the next VSync. This ensures we don't waste any cycles rendering
    // frames that will never be displayed to the screen.
    DX::ThrowIfFailed(m_swapChain->Present(1, 0));

    // Xbox One apps do not need to handle DXGI_ERROR_DEVICE_REMOVED or DXGI_ERROR_DEVICE_RESET.

    PIXEndEvent(m_d3dContext.Get());
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnSuspending()
{
    m_d3dContext->Suspend(0);

    // TODO: Save game progress using the ConnectedStorage API.
}

void Game::OnResuming()
{
    m_d3dContext->Resume();
    m_timer.ResetElapsedTime();

    // TODO: Handle changes in users and input devices.
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDevice()
{
    D3D11X_CREATE_DEVICE_PARAMETERS params = {};
    params.Version = D3D11_SDK_VERSION;

#ifdef _DEBUG
    // Enable the debug layer.
    params.Flags = D3D11_CREATE_DEVICE_DEBUG;
#elif defined(PROFILE)
    // Enable the instrumented driver.
    params.Flags = D3D11_CREATE_DEVICE_INSTRUMENTED;
#endif

    // Create the Direct3D 11 API device object and a corresponding context.
    DX::ThrowIfFailed(D3D11XCreateDeviceX(
        &params,
        m_d3dDevice.ReleaseAndGetAddressOf(),
        m_d3dContext.ReleaseAndGetAddressOf()
        ));

    // Check for 4k swapchain support
    D3D11X_GPU_HARDWARE_CONFIGURATION hwConfig = {};
    m_d3dDevice->GetGpuHardwareConfiguration(&hwConfig);
    if (hwConfig.HardwareVersion >= D3D11X_HARDWARE_VERSION_XBOX_ONE_X)
    {
        m_outputWidth = 3840;
        m_outputHeight = 2160;
    }

    // TODO: Initialize device dependent objects here (independent of window size).
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateResources()
{
    UINT backBufferWidth = static_cast<UINT>(m_outputWidth);
    UINT backBufferHeight = static_cast<UINT>(m_outputHeight);
    DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;
    UINT backBufferCount = 2;

    // If the swap chain already exists, resize it, otherwise create one.
    if (m_swapChain)
    {
        DX::ThrowIfFailed(m_swapChain->ResizeBuffers(backBufferCount, backBufferWidth, backBufferHeight, backBufferFormat, 0));

        // Xbox One apps do not need to handle DXGI_ERROR_DEVICE_REMOVED or DXGI_ERROR_DEVICE_RESET.
    }
    else
    {
        // First, retrieve the underlying DXGI Device from the D3D Device.
        ComPtr<IDXGIDevice1> dxgiDevice;
        DX::ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

        // Identify the physical adapter (GPU or card) this device is running on.
        ComPtr<IDXGIAdapter> dxgiAdapter;
        DX::ThrowIfFailed(dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf()));

        // And obtain the factory object that created it.
        ComPtr<IDXGIFactory2> dxgiFactory;
        DX::ThrowIfFailed(dxgiAdapter->GetParent(IID_GRAPHICS_PPV_ARGS(dxgiFactory.GetAddressOf())));

        // Create a descriptor for the swap chain.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = backBufferWidth;
        swapChainDesc.Height = backBufferHeight;
        swapChainDesc.Format = backBufferFormat;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = backBufferCount;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        swapChainDesc.Flags = DXGIX_SWAP_CHAIN_MATCH_XBOX360_AND_PC;

        // Create a SwapChain from a CoreWindow.
        DX::ThrowIfFailed(dxgiFactory->CreateSwapChainForCoreWindow(
            m_d3dDevice.Get(),
            m_window,
            &swapChainDesc,
            nullptr,
            m_swapChain.GetAddressOf()
            ));
    }

    // Obtain the backbuffer for this window which will be the final 3D rendertarget.
    ComPtr<ID3D11Texture2D> backBuffer;
    DX::ThrowIfFailed(m_swapChain->GetBuffer(0, IID_GRAPHICS_PPV_ARGS(backBuffer.GetAddressOf())));

    // Create a view interface on the rendertarget to use on bind.
    DX::ThrowIfFailed(m_d3dDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTargetView.ReleaseAndGetAddressOf()));

    // Allocate a 2-D surface as the depth/stencil buffer and
    // create a DepthStencil view on this surface to use on bind.
    CD3D11_TEXTURE2D_DESC depthStencilDesc(depthBufferFormat, backBufferWidth, backBufferHeight, 1, 1, D3D11_BIND_DEPTH_STENCIL);

    ComPtr<ID3D11Texture2D> depthStencil;
    DX::ThrowIfFailed(m_d3dDevice->CreateTexture2D(&depthStencilDesc, nullptr, depthStencil.GetAddressOf()));

    CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
    DX::ThrowIfFailed(m_d3dDevice->CreateDepthStencilView(depthStencil.Get(), &depthStencilViewDesc, m_depthStencilView.ReleaseAndGetAddressOf()));

    // TODO: Initialize windows-size dependent objects here.
}
#pragma endregion
