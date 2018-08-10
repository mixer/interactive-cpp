//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

#include <ppltasks.h>

using namespace DirectX;
using namespace Windows::Xbox::Input;
using namespace Windows::Foundation::Collections;
using namespace concurrency;
using namespace Windows::Xbox::System;

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
#include <sstream>
#include <iomanip>

std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

#define CLIENT_ID		"f0d20e2d263b75894f5cdaabc8a344b99b1ea6f9ecb7fa4f"
#define INTERACTIVE_ID	"135704"
#define SHARE_CODE		"xe7dpqd5"

#define MIXER_DEBUG 0

static std::chrono::time_point<std::chrono::high_resolution_clock> s_gameStartTime = std::chrono::high_resolution_clock::now();

void DebugPrint(const std::string& dbgStr)
{
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - s_gameStartTime);
	std::stringstream formattedMessage;
	auto hours = (elapsed.count() / (60 * 60 * 1000)) % 24;
	auto mins = (elapsed.count() / (60 * 1000)) % 60;
	auto seconds = (elapsed.count() / 1000) % 60;
	auto milliseconds = elapsed.count() % 1000;

	formattedMessage
		<< std::setfill('0') << std::setw(2) << hours << ":"
		<< std::setfill('0') << std::setw(2) << mins << ":"
		<< std::setfill('0') << std::setw(2) << seconds << ":"
		<< std::setfill('0') << std::setw(4) << milliseconds << " "
		<< dbgStr << std::endl;

	OutputDebugStringA(formattedMessage.str().c_str());
}

int GetXToken(std::string& token)
{
	std::wstring mixerRelyingParty = L"https://mixer.com";
	std::wstring authRequestHeaders = L"";
	auto platformHttp = ref new Platform::String(L"POST");
	auto platformUrl = ref new Platform::String(mixerRelyingParty.c_str());
	auto platformHeaders = ref new Platform::String(authRequestHeaders.c_str());

	std::wstring wstrReturnedToken = L"";

	HRESULT hr = S_OK;

	// Note: This would fail certification. You must pop TCUI and allow a user to be chosen.
	if (0 == Windows::Xbox::System::User::Users->Size)
	{
		DebugPrint("No user signed in. Please sign in a user and try this sample again.");
		return E_ABORT;
	}

	Windows::Xbox::System::User^ currentUser = Windows::Xbox::System::User::Users->GetAt(0);

	try
	{
		// Make platform call to get token.
		auto asyncOp = currentUser->GetTokenAndSignatureAsync(platformHttp, platformUrl, platformHeaders);

		// Construct an object that waits for the AsyncOperation to finish
		task<GetTokenAndSignatureResult^> asyncTask = create_task(asyncOp);

		// Capture the async then so we can ensure the lambda finishes before continuing
		auto asyncFinalise = asyncTask.then(
			[&wstrReturnedToken, &hr](
				task<GetTokenAndSignatureResult^> operation)
		{
			try
			{
				GetTokenAndSignatureResult^ result = operation.get();

				if (result->Token->IsEmpty())
				{
					hr = E_UNEXPECTED;
					return;
				}

				wstrReturnedToken = result->Token->Data();
				hr = S_OK;
			}
			catch (Platform::Exception ^ e)
			{
				hr = e->HResult;
			}
		});

		// Clean up the async task
		asyncTask.wait();
		asyncOp->Close();

		// Wait for the lambda to finish.
		asyncFinalise.wait();

		// Check for any failures
		if (FAILED(hr))
		{
			return hr;
		}

		// Convert data to utf8
		token = converter.to_bytes(wstrReturnedToken);
	}
	catch (Platform::Exception^ e)
	{
		return e->HResult;
	}

	return S_OK;
}

void handle_error(void* context, interactive_session session, int errorCode, const char* errorMessage, size_t errorMessageLength)
{
	(context);
	(session);
	(errorMessageLength);
	DebugPrint("Mixer error " + std::to_string(errorCode) + ": " + errorMessage + "\r\n");
}

// Handle user data.
void handle_user(void* context, interactive_session session, const interactive_user* user)
{
	(session);
	(context);
	std::cout << "Connecting as: " << user->userName << std::endl;
	std::cout << "Avatar: " << user->avatarUrl << std::endl;
	std::cout << "Experience: " << user->experience << std::endl;
	std::cout << "Level: " << user->level << std::endl;
	std::cout << "Sparks: " << user->sparks << std::endl;
	std::cout << "Broadcasting: " << (user->isBroadcasting ? "true" : "false") << std::endl;
}

void handle_input(void* context, interactive_session session, const interactive_input* input)
{
	Game* game = (Game*)context;
	if ((input_type_key == input->type || input_type_click == input->type) && interactive_button_action_down == input->buttonData.action)
	{
		// Capture the transaction on button down to deduct sparks
		int err = interactive_capture_transaction(session, input->transactionId);
		if (!err)
		{
			game->m_controlsById[input->transactionId] = input->control.id;
		}
	}
}

void handle_transaction(void* context, interactive_session session, const char* transactionId, size_t transactionIdLength, unsigned int errorCode, const char* errorMessage, size_t errorMessageLength)
{
	UNREFERENCED_PARAMETER(session);
	UNREFERENCED_PARAMETER(transactionIdLength);
	UNREFERENCED_PARAMETER(errorMessageLength);
	Game* game = (Game*)context;
	if (errorCode)
	{
		DebugPrint(std::string("ERROR: ") + errorMessage + "(" + std::to_string(errorCode) + ")");
	}
	else
	{
		// Transaction was captured, now execute the most super awesome interactive functionality!
		std::string controlId = game->m_controlsById[transactionId];
		if (0 == strcmp("GiveHealth", controlId.c_str()))
		{
			DebugPrint("Giving health to the player!\n");
		}
	}

	game->m_controlsById.erase(transactionId);
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(IUnknown* window)
{
	s_gameStartTime = std::chrono::high_resolution_clock::now();
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
	err = GetXToken(authorization);
	if (err)
	{
		throw err;
		return;
	}

#if MIXER_DEBUG
	interactive_config_debug(interactive_debug_trace, [](const interactive_debug_level dbgMsgType, const char* dbgMsg, size_t dbgMsgSize)
	{
		std::string msgType;
		switch (dbgMsgType)
		{
		case interactive_debug_level::interactive_debug_info:
			msgType = "Info";
			break;
		case interactive_debug_level::interactive_debug_warning:
			msgType = "Warning";
			break;
		case interactive_debug_level::interactive_debug_error:
			msgType = "Error";
			break;
		case interactive_debug_level::interactive_debug_trace:
			msgType = "Trace";
			break;
		}
		std::stringstream formattedMessage;
		formattedMessage << "[" << std::setfill(' ') << std::setw(7) << msgType << "] " 
			<< dbgMsg << std::endl;
		DebugPrint(formattedMessage.str().c_str());
	});
#endif

	// Open an interactive session. This session will remain open for the duration of this sample but it should be 
	// closed using interactive_close_session() to avoid memory leaks.
	err = interactive_open_session(&m_interactiveSession);
	if (err) throw err;

	// Register an error handler for any errors from the interactive service.
	err = interactive_set_error_handler(m_interactiveSession, handle_error);
	if (err) throw err;

	// Set the session context to this object so it can be referenced in callbacks.
	err = interactive_set_session_context(m_interactiveSession, this);
	if (err) throw err;

	// Register a callback for button presses.
	err = interactive_set_input_handler(m_interactiveSession, handle_input);
	if (err) throw err;

	// Register a callback for transactions completing.
	err = interactive_set_transaction_complete_handler(m_interactiveSession, handle_transaction);
	if (err) throw err;

	// Register a state change handler to get user data after connection is successful.
	interactive_set_state_changed_handler(m_interactiveSession, [](void* context, interactive_session session, interactive_state previousState, interactive_state currentState)
	{
		(context);
		if (previousState != interactive_connecting && currentState != interactive_connected)
		{
			return;
		}

		// Get the connected user's data.
		int err = interactive_get_user(session, handle_user);
		if (err) throw err;
	});

	// Connect to the user's interactive channel, using the interactive project specified by the version ID.
	err = interactive_connect(m_interactiveSession, authorization.c_str(), INTERACTIVE_ID, SHARE_CODE, true);
	if (err) throw err;
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
		DebugPrint((std::string("ERROR: Failed to process interactive event: ") + std::to_string(err)));
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
