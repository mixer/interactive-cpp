// BeamSample.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdlib.h>
#include <time.h>

using namespace xbox::services::beam;

bool g_interactivityInitialized;
bool g_generateMockInput;
bool g_shutDown;


// 
// Mock data
//

struct mock_participant
{
    uint32_t beam_id;
    string_t beam_username;
    bool joined;
};

struct mock_button_control
{
    string_t control_id;
    string_t button_text;
    uint32_t cost;
};


std::vector<mock_participant> g_mockParticipants{
    { 2355,     L"participant2355", false },
    { 78453,    L"participant78453", false },
    { 1997,     L"participant1997", false },
    { 39072,    L"participant39072", false },
    { 13,       L"participant13", false },
    { 9872,     L"participant9872", false },
    { 456,      L"participant456", false },
    { 199845,   L"participant199845", false },
    { 982345,   L"participant982345", false },
    { 3124,     L"participant3124", false }
};


std::vector<mock_button_control> g_mockButtons{
    { L"btnFireLasers",         L"FIRE LASERS", 0 },
    { L"btnDeployMines",        L"DEPLOY MINES", 0 },
    { L"btnGenericButton000",   L"generic button 0", 0 },
    { L"btnGenericButton001",   L"generic button 1", 0 },
    { L"btnGenericButton002",   L"generic button 2", 0 },
    { L"btnGenericButton003",   L"generic button 3", 0 },
    { L"btnGenericButton004",   L"generic button 4", 0 },
    { L"btnGenericButton005",   L"generic button 5", 0 },
    { L"btnGenericButton006",   L"generic button 6", 0 },
    { L"btnGenericButton007",   L"generic button 7", 0 },
    { L"btnGenericButton008",   L"generic button 8", 0 }
};

static uint32_t next_mock_input_id()
{
    static uint32_t mock_input_id = 2000; // high-ish number to avoid immediate conflict with game client generated message ids

    return mock_input_id++;
}

// create set of test spectators
void generate_random_input_thread()
{
    srand(time(NULL));
    std::shared_ptr<beam_mock_util> mockInput = beam_mock_util::get_singleton_instance();

    while (!g_shutDown)
    {
        if (g_generateMockInput)
        {
            int participantIndex = rand() % 10;
            int buttonIndex = rand() % 10;

            if (!g_mockParticipants[participantIndex].joined)
            {
                g_mockParticipants[participantIndex].joined = true;

                mockInput->mock_participant_join(g_mockParticipants[participantIndex].beam_id, g_mockParticipants[participantIndex].beam_username);
                OutputDebugString(L"Mocking participant join for ");
                OutputDebugString(g_mockParticipants[participantIndex].beam_username.c_str());
                OutputDebugString(L"\n");
            }

            mockInput->mock_button_event(g_mockParticipants[participantIndex].beam_id, g_mockButtons[buttonIndex].control_id, 0 == (rand() % 2));

            // sleep a semi-random interval before next participant input
            Sleep(buttonIndex * 150);
        }
    }
}


//
// Example beam event handling
//

void HandleInteractivityStateChange(beam_event event)
{
    std::shared_ptr<beam_interactivity_state_change_event_args> eventArgs = std::dynamic_pointer_cast<beam_interactivity_state_change_event_args>(event.event_args());
    if (!g_interactivityInitialized  && eventArgs->new_state() == beam_interactivity_state::interactivity_disabled)
    {
        OutputDebugString(L"Interactivity state change: initialized\n");
        g_interactivityInitialized = true;
        beam_manager::get_singleton_instance()->start_interactive();
    }
    else if (eventArgs->new_state() == beam_interactivity_state::interactivity_pending)
    {
        OutputDebugString(L"Interactivity state change: interactivity pending\n");
    }
    else if (eventArgs->new_state() == beam_interactivity_state::interactivity_enabled)
    {
        OutputDebugString(L"Interactivity state change: interactivity enabled\n");
        g_generateMockInput = true;
    }
    else if (eventArgs->new_state() == beam_interactivity_state::interactivity_disabled)
    {
        OutputDebugString(L"Interactivity state change: stopped\n");
    }
    return;
}

void HandleParticipantStateChange(beam_event event)
{
    OutputDebugString(L"Participant state change\n");
    return;
}

void HandleInteractiveControlEvent(beam_event event)
{
    if (beam_event_type::button == event.event_type())
    {
        OutputDebugString(L"Received button input\n");
        std::shared_ptr<beam_button_event_args> buttonArgs = std::dynamic_pointer_cast<beam_button_event_args>(event.event_args());

        if (nullptr != buttonArgs)
        {
            string_t buttonId = buttonArgs->control_id();
            if (true == buttonArgs->is_pressed())
            {
                OutputDebugString(L"Button ");
                OutputDebugString(buttonId.c_str());
                OutputDebugString(L" is down\n");
            }
            else
            {
                OutputDebugString(L"Button ");
                OutputDebugString(buttonId.c_str());
                OutputDebugString(L" is up\n");
            }
        }
        else
        {
            OutputDebugString(L"Invalid beam_button_event_args\n");
        }
    }
    else if (beam_event_type::joystick == event.event_type())
    {
        OutputDebugString(L"Received joystick event\n");
    }
    return;
}


void HandleInteractivityError(beam_event event)
{
    OutputDebugString(L"Interactivity error!\n");
    OutputDebugString(event.err_message().c_str());
    OutputDebugString(L"\n");
    return;
}

//
// Example update loop and method handlers
//

void update()
{
    std::shared_ptr<beam_manager> manager = beam_manager::get_singleton_instance();
    std::vector<beam_event> events = manager->do_work();

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
        if (eventType == beam_event_type::button || eventType == beam_event_type::button)
        {
            HandleInteractiveControlEvent(event);
            break;
        }
    }

    if (beam_interactivity_state::interactivity_enabled == manager->interactivity_state())
    {
        // If you want to query the state of a control
        std::shared_ptr<beam_scene> defaultScene = manager->get_singleton_instance()->scene(L"default");

        // Check to make sure the current scene has been populated
        if (nullptr != defaultScene)
        {
            auto btnFireLasers = defaultScene->button(L"btnFireLasers");
            if (nullptr != btnFireLasers && btnFireLasers->is_down())
            {
                // Trigger lasers
            }

            // How many people have spawned a turret?
            auto btnSpawnTurret = defaultScene->button(L"btnSpawnTurret");
            if (nullptr != btnSpawnTurret)
            {
                int countOfTurretsToSpawn = btnSpawnTurret->count_of_button_downs();
                while (countOfTurretsToSpawn > 0)
                {
                    // Spawn a turret
                    countOfTurretsToSpawn--;
                }
            }

            // If you query a scene for a control it doesn't have...
            if (nullptr != btnSpawnTurret)
            {
                if (nullptr != btnSpawnTurret)
                {
                    int countOfTurretsToSpawn = defaultScene->button(L"btnSpawnTurret")->count_of_button_downs();
                    while (countOfTurretsToSpawn > 0)
                    {
                        // Spawn a turret
                        countOfTurretsToSpawn--;
                    }
                }
            }
        }
    }
}

int main()
{
    g_shutDown = false;
    g_interactivityInitialized = false;

    // Creating a thread to generate mock participant input
    std::thread mock_input_thread(generate_random_input_thread);

    std::shared_ptr<beam_manager> manager = beam_manager::get_singleton_instance();

    string_t mockOAuthToken = L"test";
    string_t mockOAuthRefreshToken = L"test";
    string_t mockInteractiveVersion = L"0";
    string_t mockProtocolVersion = L"2.0";

    std::shared_ptr<beam_mock_util> mockInput = beam_mock_util::get_singleton_instance();
    mockInput->set_oauth_token(mockOAuthToken);

    manager->initialize(mockInteractiveVersion, false);

    while (true)
    {
        update();
        Sleep(30);
    }

    mock_input_thread.join();
    return 0;
}
