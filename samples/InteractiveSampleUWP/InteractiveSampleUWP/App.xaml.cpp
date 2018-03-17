//
// App.xaml.cpp
// Implementation of the App class.
//

#include "pch.h"
#include "MainPage.xaml.h"

#include "..\..\..\source\interactivity.h"

// STL includes
#include <chrono>
#include <thread>
#include <iostream>
#include <codecvt>
#include <string>

std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

using namespace mixer;

#define CLIENT_ID		"f0d20e2d263b75894f5cdaabc8a344b99b1ea6f9ecb7fa4f"
#define INTERACTIVE_ID	"135704"
#define SHARE_CODE		"xe7dpqd5"

// Called ~60 times per second
int update(interactive_session session)
{
	// This call processes any waiting messages from the interactive service. If there are no messages this returns immediately.
	// All previously registered session callbacks will be called from this thread.
	return interactive_run(session, 1);
}

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

	std::wstring oauthUrl = converter.from_bytes(std::string("https://www.mixer.com/go?code=") + shortCode);
	Windows::System::Launcher::LaunchUriAsync(ref new Windows::Foundation::Uri(Platform::StringReference(oauthUrl.c_str())));

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

using namespace InteractiveSampleUWP;

using namespace Platform;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

/// <summary>
/// Initializes the singleton application object.  This is the first line of authored code
/// executed, and as such is the logical equivalent of main() or WinMain().
/// </summary>
App::App() : m_appIsRunning(false)
{
    InitializeComponent();
    Suspending += ref new SuspendingEventHandler(this, &App::OnSuspending);
}

/// <summary>
/// Invoked when the application is launched normally by the end user.  Other entry points
/// will be used such as when the application is launched to open a specific file.
/// </summary>
/// <param name="e">Details about the launch request and process.</param>
void App::OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e)
{
    auto rootFrame = dynamic_cast<Frame^>(Window::Current->Content);

    // Do not repeat app initialization when the Window already has content,
    // just ensure that the window is active
    if (rootFrame == nullptr)
    {
        // Create a Frame to act as the navigation context and associate it with
        // a SuspensionManager key
        rootFrame = ref new Frame();

        rootFrame->NavigationFailed += ref new Windows::UI::Xaml::Navigation::NavigationFailedEventHandler(this, &App::OnNavigationFailed);

        if (e->PreviousExecutionState == ApplicationExecutionState::Terminated)
        {
            // TODO: Restore the saved session state only when appropriate, scheduling the
            // final launch steps after the restore is complete

        }

        if (e->PrelaunchActivated == false)
        {
            if (rootFrame->Content == nullptr)
            {
                // When the navigation stack isn't restored navigate to the first page,
                // configuring the new page by passing required information as a navigation
                // parameter
                rootFrame->Navigate(TypeName(MainPage::typeid), e->Arguments);
            }
            // Place the frame in the current Window
            Window::Current->Content = rootFrame;
            // Ensure the current window is active
            Window::Current->Activate();
        }
    }
    else
    {
        if (e->PrelaunchActivated == false)
        {
            if (rootFrame->Content == nullptr)
            {
                // When the navigation stack isn't restored navigate to the first page,
                // configuring the new page by passing required information as a navigation
                // parameter
                rootFrame->Navigate(TypeName(MainPage::typeid), e->Arguments);
            }
            // Ensure the current window is active
            Window::Current->Activate();
        }
    }	

	m_appIsRunning = true;
	// Simulate game update loop. All callbacks will be called from this thread.
	m_interactiveThread = std::make_unique<std::thread>(std::thread([&]
	{
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
		interactive_session session;
		err = interactive_connect(authorization.c_str(), INTERACTIVE_ID, SHARE_CODE, false, &session);
		if (err) throw err;

		// Register a callback for button presses.
		err = interactive_reg_button_input_handler(session, [](void* context, interactive_session session, const interactive_button_input* input)
		{
			if (button_action::down == input->action)
			{
				// Capture the transaction on button down to deduct sparks
				int err = interactive_capture_transaction(session, input->transactionId);
				if (err)
				{
					std::cerr << "Failed to capture transaction. Participant may not have enough sparks." << std::endl;
					return;
				}

				// Transaction was captured, now execute the most super awesome interactive functionality!
				if (0 == strcmp("GiveHealth", input->control.id))
				{
					std::cout << "Giving health to the player!" << std::endl;
				}
			}
		});

		while (m_appIsRunning)
		{
			int err = update(session);
			if (err) break;
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
		}
	}));
}

/// <summary>
/// Invoked when application execution is being suspended.  Application state is saved
/// without knowing whether the application will be terminated or resumed with the contents
/// of memory still intact.
/// </summary>
/// <param name="sender">The source of the suspend request.</param>
/// <param name="e">Details about the suspend request.</param>
void App::OnSuspending(Object^ sender, SuspendingEventArgs^ e)
{
    (void) sender;  // Unused parameter
    (void) e;   // Unused parameter

    //TODO: Save application state and stop any background activity
	m_appIsRunning = false;
	m_interactiveThread->join();
}

/// <summary>
/// Invoked when Navigation to a certain page fails
/// </summary>
/// <param name="sender">The Frame which failed navigation</param>
/// <param name="e">Details about the navigation failure</param>
void App::OnNavigationFailed(Platform::Object ^sender, Windows::UI::Xaml::Navigation::NavigationFailedEventArgs ^e)
{
    throw ref new FailureException("Failed to load Page " + e->SourcePageType.Name);
}

