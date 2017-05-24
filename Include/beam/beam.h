//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#pragma once

namespace xbox {
    namespace services {
        namespace system {
            class xbox_live_user;
        }
    }
}

#if TV_API | XBOX_UWP
typedef  Windows::Xbox::System::User^ xbox_live_user_t;
#else
typedef std::shared_ptr<xbox::services::system::xbox_live_user> xbox_live_user_t;
#endif

namespace xbox { namespace services {
    /// <summary>
    /// Contains classes and enumerations that let you incorporate
    /// Beam Interactivity functionality into your title.
    /// </summary>
    namespace beam {

class beam_connection_manager;
class beam_manager_impl;
class beam_manager;
class beam_participant_impl;
class beam_scene;
class beam_scene_impl;
class beam_group;
class beam_group_impl;
class beam_control_builder;
class beam_control;
class beam_button_control;
class beam_joystick_control;
class beam_mock_util;
class beam_button_state;
class beam_button_count;

/// <summary>
/// Enum that describes the types of control objects.
/// </summary>
enum beam_control_type
{
    /// <summary>
    /// The button control.
    /// </summary>
    button,

    /// <summary>
    /// The joystick control.
    /// </summary>
    joystick
};

/// <summary>
/// Enum that describes the current state of the interactivity service.
/// </summary>
enum beam_interactivity_state
{
    /// <summary>
    /// The Beam manager is not initialized.
    /// </summary>
    not_initialized,

    /// <summary>
    /// The Beam manager is initializing.
    /// </summary>
    initializing,

    /// <summary>
    /// The Beam manager is initialized.
    /// </summary>
    initialized,

    /// <summary>
    /// The Beam manager is initialized, but interactivity is not enabled.
    /// </summary>
    interactivity_disabled,

    /// <summary>
    /// The Beam manager is currently connecting to the Beam interactive service.
    /// </summary>
    interactivity_pending,

    /// <summary>
    /// Interactivity is enabled.
    /// </summary>
    interactivity_enabled
};


/// <summary>
/// Enum representing the current state of the participant
/// </summary>
enum beam_participant_state
{
    /// <summary>
    /// The participant joined the channel.
    /// </summary>
    joined,

    /// <summary>
    /// The participant's input is disabled.
    /// </summary>
    input_disabled,

    /// <summary>
    /// The participant left the channel.
    /// </summary>
    left
};


/// <summary>
/// This class represents a user who is currently viewing a Beam interactive stream. This
/// user (also known as a beam_participant) has both a Beam account and a Microsoft Security
/// Account (MSA).
/// </summary>
class beam_participant
{
public:

    /// <summary>
    /// The Beam ID of the user.
    /// </summary>
    _BEAMIMP uint32_t beam_id() const;

    /// <summary>
    /// The Beam username of the user.
    /// </summary>
    _BEAMIMP const string_t& beam_username() const;

    /// <summary>
    /// The Beam level of the user.
    /// </summary>
    _BEAMIMP uint32_t beam_level() const;

    /// <summary>
    /// The current state of the participant.
    /// </summary>
    _BEAMIMP const beam_participant_state state() const;

    /// <summary>
    /// Assigns the user to a specified group. This method 
    /// also updates the list of participants that are in this group.
    /// </summary>
    _BEAMIMP void set_group(std::shared_ptr<beam_group> group);

    /// <summary>
    /// Returns a pointer to the group that the user is assigned to.
    /// By default, participants are placed in a group named "default".
    /// </summary>
    _BEAMIMP const std::shared_ptr<beam_group> group();

    /// <summary>
    /// The time (in UTC) at which the user last used the interactive control input.
    /// </summary>
    _BEAMIMP const std::chrono::milliseconds& last_input_at() const;

    /// <summary>
    /// The time (in UTC) at which the user connected to the Beam Interactive stream.
    /// </summary>
    _BEAMIMP const std::chrono::milliseconds& connected_at() const;

    /// <summary>
    /// A Boolean value that indicates whether or not the user input is disabled.
    /// If TRUE, user input has been disabled.
    /// </summary>
    _BEAMIMP bool input_disabled() const;

#if 0
    /// <summary>
    /// Returns a particular button. If the button does not exist,
    /// NULL is returned.
    /// </summary>
    _BEAMIMP const std::shared_ptr<beam_button_control> button(_In_ const string_t& controlId);

    /// <summary>
    /// Returns buttons that the participant has interacted with.
    /// </summary>
    _BEAMIMP const std::vector<std::shared_ptr<beam_button_control>>& buttons();

    /// <summary>
    /// Returns a particular joystick. If the joystick does not exist, 
    /// NULL is returned.
    /// </summary>
    _BEAMIMP const std::shared_ptr<beam_joystick_control> joystick(_In_ const string_t& controlId);

    /// <summary>
    /// Returns joysticks that the participant has interacted with.
    /// </summary>
    _BEAMIMP const std::vector<std::shared_ptr<beam_joystick_control>>& joysticks();
#endif

private:

    /// <summary>
    /// Internal function to construct a beam_participant.
    /// </summary>
    beam_participant(
        _In_ uint32_t beamId,
        _In_ string_t username,
        _In_ uint32_t beamLevel,
        _In_ string_t groupId,
        _In_ std::chrono::milliseconds lastInputAt,
        _In_ std::chrono::milliseconds connectedAt,
        _In_ bool disabled
    );


    /// <summary>
    /// Internal function to construct a beam_participant.
    /// </summary>
    beam_participant();

    std::shared_ptr<xbox::services::beam::beam_manager> m_beamManager;
    std::shared_ptr<xbox::services::beam::beam_participant_impl> m_impl;

    friend beam_participant_impl;
    friend beam_manager_impl;
};


/// <summary>
/// Describes the types of Beam message objects.
/// </summary>
enum class beam_event_type
{
    /// <summary>
    /// An error message object. This object type is returned when the service
	/// or manager encounters an error. The err and err_message members will
	/// contain pertinent info.
    /// </summary>
    error,

    /// <summary>
    /// An interactivity state changed message object.
    /// </summary>
    interactivity_state_changed,

    /// <summary>
    /// A participant state changed message object.
    /// </summary>
    participant_state_changed,

    /// <summary>
    /// A button message object.
    /// </summary>
    button,

    /// <summary>
    /// A joystick message object.
    /// </summary>
    joystick
};


/// <summary>
/// Base class for all Beam event args. Contains information for Beam Interactive events.
/// </summary>
class beam_event_args
{
public:

    /// <summary>
    /// Constructor for the Beam event args object.
    /// </summary>
    beam_event_args(){}

    /// <summary>
    /// Virtual destructor for the Beam event args object.
    /// </summary>
    virtual ~beam_event_args(){}
};

/// <summary>
/// Base class for all Beam Interactive events. Beam Interactivity
/// is an event-driven service.
/// </summary>
class beam_event
{
public:

    /// <summary>
    /// Constructor for the Beam event object.
    /// </summary>
    beam_event(
        _In_ std::chrono::milliseconds time,
        _In_ std::error_code errorCode,
        _In_ string_t errorMessage,
        _In_ beam_event_type eventType,
        _In_ std::shared_ptr<beam_event_args> eventArgs
        );

    /// <summary>
    /// The time (in UTC) when this event is raised.
    /// </summary>
    _BEAMIMP const std::chrono::milliseconds& time() const;

    /// <summary>
    /// The error code indicating the result of the operation.
    /// </summary>
    _BEAMIMP const std::error_code& err() const;

    /// <summary>
    /// Returns call specific error message with debug information.
    /// Message is not localized as it is meant to be used for debugging only.
    /// </summary>
    _BEAMIMP const string_t& err_message() const;

    /// <summary>
    /// Type of the event raised.
    /// </summary>
    _BEAMIMP beam_event_type event_type() const;

    /// <summary>
    /// Returns a pointer to an event argument. Cast the event arg to a specific
    /// event arg class type before retrieving the data.
    /// </summary>
    _BEAMIMP const std::shared_ptr<beam_event_args>& event_args() const;

private:

    std::chrono::milliseconds m_time;
    std::error_code m_errorCode;
    string_t m_errorMessage;
    beam_event_type m_eventType;
    std::shared_ptr<beam_event_args> m_eventArgs;
};


/// <summary>
/// Contains information when the state of interactivity changes.
/// </summary>
class beam_interactivity_state_change_event_args : public beam_event_args
{
public:

    /// <summary>
    /// The current interactivity state.
    /// </summary>
    _BEAMIMP const beam_interactivity_state new_state() const;

    /// <summary>
    /// Constructor for the Beam interactivity state change event args object.
    /// </summary>
    beam_interactivity_state_change_event_args(
        _In_ beam_interactivity_state newState
    );

private:

    beam_interactivity_state    m_newState;
};


/// <summary>
/// Contains information for a participant state change event. 
/// The state changes when a participant joins or leaves the channel.
/// </summary>
class beam_participant_state_change_event_args : public beam_event_args
{
public:

    /// <summary>
    /// The participant whose state has changed. For example, a 
    /// participant who has just joined the Beam channel.
    /// </summary>
    _BEAMIMP const std::shared_ptr<beam_participant>& participant() const;

    /// <summary>
    /// The current state of the participant.
    /// </summary>
    _BEAMIMP const beam_participant_state& state() const;

    /// <summary>
    /// Constructor for the beam_participant_state_change_event_args object.
    /// </summary>
    beam_participant_state_change_event_args(
        _In_ std::shared_ptr<beam_participant> participant,
        _In_ beam_participant_state state
    );

private:

    std::shared_ptr<beam_participant> m_participant;
    beam_participant_state m_state;
};


/// <summary>
/// Contains information for a button event.
/// </summary>
class beam_button_event_args : public beam_event_args
{
public:

    /// <summary>
    /// Unique string identifier for this control
    /// </summary>
    _BEAMIMP const string_t& control_id() const;

    /// <summary>
    /// The user who raised this event.
    /// </summary>
    _BEAMIMP const std::shared_ptr<beam_participant>& participant() const;

    /// <summary>
    /// Boolean to indicate if the button is up or down.
    /// Returns TRUE if button is down.
    /// </summary>
    _BEAMIMP bool is_pressed() const;

    /// <summary>
    /// Constructor for the Beam button event args object.
    /// </summary>
    beam_button_event_args::beam_button_event_args(
        _In_ string_t controlId,
        _In_ std::shared_ptr<beam_participant> participant,
        _In_ bool isPressed
    );

private:

    string_t m_controlId;
    std::shared_ptr<beam_participant> m_participant;
    bool m_isPressed;
};

/// <summary>
/// Contains information for a joystick event. These arguments are sent 
/// at an interval frequency configured via the Beam Lab.
/// </summary>
class beam_joystick_event_args : public beam_event_args
{
public:

    /// <summary>
    /// Unique string identifier for this control.
    /// </summary>
    _BEAMIMP const string_t& control_id() const;

    /// <summary>
    /// The X coordinate of the joystick, in the range of [-1, 1].
    /// </summary>
    _BEAMIMP double x() const;
    /// <summary>
    /// The Y coordinate of the joystick, in the range of [-1, 1].
    /// </summary>
    _BEAMIMP double y() const;

    /// <summary>
    /// Participant whose action this event represents.
    /// </summary>
    _BEAMIMP const std::shared_ptr<beam_participant>& participant() const;

    /// <summary>
    /// Constructor for the beam_joystick_event_args object.
    /// </summary>
    beam_joystick_event_args(
        _In_ std::shared_ptr<beam_participant> participant,
        _In_ double x,
        _In_ double y,
        _In_ string_t control_id
    );

private:

    string_t m_controlId;
    double m_x;
    double m_y;
    std::shared_ptr<beam_participant> m_participant;
};

/// <summary>
/// Base class for all Beam interactivity controls.
/// All controls are created and configured using the Beam Lab.
/// </summary>
class beam_control
{
public:

    /// <summary>
    /// The type of control.
    /// </summary>
    _BEAMIMP const beam_control_type& control_type() const;

    /// <summary>
    /// Unique string identifier for the control.
    /// </summary>
    _BEAMIMP const string_t& control_id() const;

protected:

    /// <summary>
    /// Internal constructor for the beam_control object.
    /// </summary>
    beam_control();

    /// <summary>
    /// Internal virtual destructor for the beam_control object.
    /// </summary>
    virtual ~beam_control()
    {
    }

    /// <summary>
    /// Internal constructor for the beam_control object.
    /// </summary>
    beam_control(
        _In_ string_t parentScene,
        _In_ string_t controlId,
        _In_ bool disabled
        );

    /// <summary>
    /// Internal function to clear the state of the beam_control object.
    /// </summary>
    virtual void clear_state() = 0;

    /// <summary>
    /// Internal function to update the state of the beam_control object.
    /// </summary>
    virtual bool update(web::json::value json, bool overwrite) = 0;

    /// <summary>
    /// Internal function to initialize beam_control object.
    /// </summary>
    virtual bool init_from_json(_In_ web::json::value json) = 0;

    std::shared_ptr<beam_manager> m_beamManager;
    string_t m_parentScene;
    beam_control_type m_type;
    string_t m_controlId;
    bool m_disabled;
    string_t m_etag;

    friend beam_control_builder;
    friend beam_manager_impl;
};


/// <summary>
/// Represents a Beam interactivity button control. This class is 
/// derived from beam_control. All controls are created and 
/// configured using the Beam Lab.
/// </summary>
class beam_button_control : public beam_control
{
public:

    /// <summary>
    /// Text displayed on the button control.
    /// </summary>
    _BEAMIMP const string_t& button_text() const;

    /// <summary>
    /// Spark cost assigned to the button control.
    /// </summary>
    _BEAMIMP uint32_t cost() const;

    /// <summary>
    /// Indicates whether the button is enabled or disabled. If TRUE, 
    /// button is disabled.
    /// </summary>
    _BEAMIMP bool disabled() const;

#if 0
    /// <summary>
    /// Function to enable or disable the button.
    /// </summary>
    /// <param name="disabled">Value to enable or disable the button. 
    /// Set this value to TRUE to disable the button.</param>
    _BEAMIMP void set_disabled(_In_ bool disabled);
#endif

    /// <summary>
    /// Sets the cooldown duration (in milliseconds) required between triggers. 
    /// Disables the button for a period of time.
    /// </summary>
    /// <param name="cooldown">Duration (in milliseconds) required between triggers.</param>
    _BEAMIMP void trigger_cooldown(std::chrono::milliseconds cooldown) const;

    /// <summary>
    /// Time remaining (in milliseconds) before the button can be triggered again.
    /// </summary>
    _BEAMIMP std::chrono::milliseconds remaining_cooldown() const;

#if 0
    /// <summary>
    /// Current progress of the button control.
    /// </summary>
    _BEAMIMP float progress() const;

    /// <summary>
    /// Sets the progress value for the button control.
    /// </summary>
    /// <param name="progress">The progress value, in the range of 0.0 to 1.0.</param>
    _BEAMIMP void set_progress(_In_ float progress);
#endif

    /// <summary>
    /// Returns the total count of button downs since the last call to do_work().
    /// </summary>
    _BEAMIMP uint32_t count_of_button_downs();

#if 0
    /// <summary>
    /// Returns the total count of button downs by the specified participant
    /// since the last call to do_work().
    /// </summary>
    _BEAMIMP uint32_t count_of_button_downs(_In_ uint32_t beamId);
#endif

    /// <summary>
    /// Returns the total count of button presses since the last call to do_work().
    /// </summary>
    _BEAMIMP uint32_t count_of_button_presses();

#if 0
    /// <summary>
    /// Returns the total count of button presses by the specified participant
    /// since the last call to do_work().
    /// </summary>
    _BEAMIMP uint32_t count_of_button_presses(_In_ uint32_t beamId);
#endif

    /// <summary>
    /// Returns the total count of button ups since the last call to do_work().
    /// </summary>
    _BEAMIMP uint32_t count_of_button_ups();

#if 0
    /// <summary>
    /// Returns the total count of button ups by the specified participant
    /// since the last call to do_work().
    /// </summary>
    _BEAMIMP uint32_t count_of_button_ups(_In_ uint32_t beamId);
#endif

    /// <summary>
    /// Returns TRUE if button is currently pressed.
    /// </summary>
    _BEAMIMP bool is_pressed();

#if 0
    /// <summary>
    /// Returns TRUE if the button is currently pressed by the specified participant.
    /// </summary>
    _BEAMIMP bool is_pressed(_In_ uint32_t beamId);
#endif

    /// <summary>
    /// Returns TRUE if button is currently down.
    /// </summary>
    _BEAMIMP bool is_down();

#if 0
    /// <summary>
    /// Returns TRUE if the button is clicked down by the specified participant.
    /// </summary>
    _BEAMIMP bool is_down(_In_ uint32_t beamId);
#endif

    /// <summary>
    /// Returns TRUE if button is currently up.
    /// </summary>
    _BEAMIMP bool is_up();

#if 0
    /// <summary>
    /// Returns TRUE if the button is currently up for the specified participant.
    /// </summary>
    _BEAMIMP bool is_up(_In_ uint32_t beamId);
#endif

private:

    /// <summary>
    /// Constructor for beam_button_control object.
    /// </summary>
    beam_button_control();

    /// <summary>
    /// Constructor for beam_button_control object.
    /// </summary>
    beam_button_control(
        _In_ string_t parentSceneId,
        _In_ string_t controlId,
        _In_ bool enabled,
        _In_ float progress,
        _In_ std::chrono::milliseconds m_cooldownDeadline,
        _In_ string_t buttonText,
        _In_ uint32_t sparkCost
    );

    /// <summary>
    /// Internal function to initialize a beam_button_control object.
    /// </summary>
    bool init_from_json(web::json::value json);

    /// <summary>
    /// Internal function to clear the state of the beam_button_control object.
    /// </summary>
    void clear_state();

    /// <summary>
    /// Internal function to update the state of the beam_control object.
    /// </summary>
    bool update(web::json::value json, bool overwrite);

    float                     m_progress;
    std::chrono::milliseconds m_cooldownDeadline;
    string_t                  m_buttonText;
    uint32_t                  m_sparkCost;
    std::map<uint32_t, std::shared_ptr<beam_button_state>> m_buttonStateByBeamId;
    std::shared_ptr<beam_button_count> m_buttonCount;

    friend beam_control_builder;
    friend beam_manager_impl;
};


/// <summary>
/// Represents a Beam interactivity joystick control. This class is derived from beam_control. 
/// All controls are created and configured using the Beam Lab.
/// </summary>
class beam_joystick_control : public beam_control
{
public:

    /// <summary>
    /// The current X coordinate of the joystick, in the range of [-1, 1].
    /// </summary>
    _BEAMIMP double x() const;

    /// <summary>
    /// The current Y coordinate of the joystick, in the range of [-1, 1].
    /// </summary>
    _BEAMIMP double y() const;

private:

    /// <summary>
    /// Constructor for the beam_joystick_control object.
    /// </summary>
    beam_joystick_control();

    /// <summary>
    /// Constructor for the beam_joystick_control object.
    /// </summary>
    beam_joystick_control(
        _In_ string_t parentSceneId,
        _In_ string_t controlId,
        _In_ bool enabled,
        _In_ double x,
        _In_ double y
        );

    /// <summary>
    /// Internal function to initialize a beam_joystick_control object.
    /// </summary>
    bool init_from_json(web::json::value json);

    /// <summary>
    /// Internal function to clear the state of the beam_joystick_control object.
    /// </summary>
    void clear_state();

    /// <summary>
    /// Internal function to update the state of the beam_joystick_control object.
    /// </summary>
    bool update(web::json::value json, bool overwrite);

    double m_x;
    double m_y;

    friend beam_control_builder;
    friend beam_manager_impl;
};


/// <summary>
/// Represents a Beam interactivity group. This group functionality is used to 
/// segment your audience watching a stream. beam_group is useful when you want 
/// portions of your audience to be shown a different scene while watching a stream. 
/// Participants can only be assigned to a single group.
/// </summary>
class beam_group
{
public:

    /// <summary>
    /// Constructor for the beam_group object. If no scene is specified, 
    /// the group is shown the "default" scene.
    /// </summary>
    /// <param name="groupId">The unique string identifier for the group.</param>
    beam_group(
        _In_ string_t groupId
        );

    /// <summary>
    /// Constructor for the beam_group object.
    /// </summary>
    /// <param name="groupId">The unique string identifier for the group.</param>
    /// <param name="scene">The scene shown to the group.</param>
    beam_group(
        _In_ string_t groupId,
        _In_ std::shared_ptr<beam_scene> scene
        );

    /// <summary>
    /// Unique string identifier for the group.
    /// </summary>
    _BEAMIMP const string_t& group_id() const;

    /// <summary>
    /// Returns a pointer to the scene assigned to the group.
    /// </summary>
    _BEAMIMP std::shared_ptr<beam_scene> scene();

    /// <summary>
    /// Assigns a scene to the group.
    /// </summary>
    _BEAMIMP void set_scene(std::shared_ptr<beam_scene> currentScene);

    /// <summary>
    /// Gets all the participants assigned to the group. The list may be empty.
    /// </summary>
    _BEAMIMP const std::vector<std::shared_ptr<beam_participant>> participants();

private:

    /// <summary>
    /// Internal function to construct a beam_group object.
    /// </summary>
    beam_group();

    std::shared_ptr<xbox::services::beam::beam_manager> m_beamManager;
    std::shared_ptr<xbox::services::beam::beam_group_impl> m_impl;

    friend beam_group_impl;
    friend beam_manager_impl;
    friend beam_manager;
};


/// <summary>
/// Represents a Beam interactivity scene. These scenes are configured 
/// using the Beam Lab.
/// </summary>
class beam_scene
{
public:

    /// <summary>
    /// Unique string identifier for the scene.
    /// </summary>
    _BEAMIMP const string_t& scene_id() const;

    /// <summary>
    /// Returns all the groups that the scene is assigned to. This list may be empty.
    /// </summary>
    _BEAMIMP const std::vector<string_t> groups();

    /// <summary>
    /// Returns a list of all the buttons in the scene. This list may be empty.
    /// </summary>
    _BEAMIMP const std::vector<std::shared_ptr<beam_button_control>> buttons();

    /// <summary>
    /// Returns the pointer to the specified button, if it exist.
    /// </summary>
    /// <param name="controlId">The unique string identifier of the button.</param>
    _BEAMIMP const std::shared_ptr<beam_button_control> button(_In_ const string_t& controlId);

    /// <summary>
    /// Returns a list of all the joysticks in the scene. This list may be empty.
    /// </summary>
    _BEAMIMP const std::vector<std::shared_ptr<beam_joystick_control>> joysticks();

    /// <summary>
    /// Returns the pointer to the specified joystick, if it exist.
    /// </summary>
    /// <param name="controlId">The unique string identifier of the joystick.</param>
    _BEAMIMP const std::shared_ptr<beam_joystick_control> joystick(_In_ const string_t& controlId);

private:

    /// <summary>
    /// Constructor for the beam_scene object.
    /// </summary>
    beam_scene();

    /// <summary>
    /// Constructor for the beam_scene object.
    /// </summary>
    beam_scene(
        _In_ string_t sceneId,
        _In_ bool enabled,
        _In_ const std::vector<const string_t&>& controls
        );

    std::shared_ptr<xbox::services::beam::beam_manager> m_beamManager;
    std::shared_ptr<xbox::services::beam::beam_scene_impl> m_impl;

    friend beam_scene_impl;
    friend beam_manager_impl;
    friend beam_manager;
};


/// <summary>
/// Manager service class that handles the Beam interactivity event 
/// experience between the Beam service and the title.
/// </summary>
class beam_manager : public std::enable_shared_from_this<beam_manager>
{
public:

    /// <summary>
    /// Gets the singleton instance of beam_manager.
    /// </summary>
    _BEAMIMP static std::shared_ptr<beam_manager> get_singleton_instance();

    /// <summary>
    /// Sets up the connection for the Beam interactivity event experience by
    /// initializing a background task.
    /// </summary>
    /// <returns>Value that indicates whether the initialization request is accepted or not. 
    /// If TRUE, the initialization request is accepted.</returns>
    /// <param name="interactiveVersion"> The version of the Beam interactivity experience created for the title.</param>
    /// <param name="goInteractive">Value that indicates whether or not to start interactivity immediately. 
    /// If FALSE, you need to actively start_interactive() to intiate interactivity after initialization.</param>
    /// <remarks></remarks>
    _BEAMIMP bool initialize(
        _In_ string_t interactiveVersion,
        _In_ bool goInteractive = true
    );

#if TV_API | XBOX_UWP
    /// <summary>
    /// Authenticates a signed in local user for the Beam interactivity experience.
    /// </summary>
    /// <param name="user">The user's Xbox Live identifier.</param>
    /// <returns>Returns a Beam event to report any potential error. A nullptr is returned if there's no error.</returns>
    _BEAMIMP std::shared_ptr<beam_event> add_local_user(_In_ xbox_live_user_t user);
#else
    /// <summary>
    /// Set an xtoken retrieved from a signed in user. This is used to authenticate into the Beam interactivity experience.
    /// </summary>
    /// <param name="user">The user's xtoken.</param>
    /// <returns>Returns a Beam event to report any potential error. A nullptr is returned if there's no error.</returns>
    _BEAMIMP std::shared_ptr<beam_event> set_xtoken(_In_ string_t token);
#endif

#if 0
    /// <summary>
    /// Requests an OAuth account authorization code from the Beam services. The title needs to display this 
    /// code and prompt the user to enter it at beam.pro/go. This process allows the user's Beam account to 
    /// be linked to an interactivity stream.
    /// </summary>
    /// <param name="beam_id">The Beam ID of the user.</param>
    _BEAMIMP void request_linking_code(_In_ uint32_t beam_id) const;
#endif

    /// <summary>
    /// The time of the Beam interactivity service, in UTC. Used to maintain the 
    /// title's synchronization with the Beam interactivity service.
    /// </summary>
    _BEAMIMP const std::chrono::milliseconds get_server_time();

    /// <summary>
    /// Returns the version of the Beam interactivity experience created.
    /// </summary>
    _BEAMIMP const string_t& interactive_version() const;

    /// <summary>
    /// The enum value that indicates the interactivity state of the Beam manager.
    /// </summary>
    _BEAMIMP const beam_interactivity_state interactivity_state();

    /// <summary>
    /// Used by the title to inform the Beam service that it is ready to receive interactivity input.
    /// </summary>
    /// <remarks></remarks>
    _BEAMIMP bool start_interactive();

    /// <summary>
    /// Used by the title to inform the Beam service that it is no longer receiving interactivity input.
    /// </summary>
    /// <returns></returns>
    /// <remarks></remarks>
    _BEAMIMP bool stop_interactive();

    /// <summary>
    /// Returns currently active participants of this interactivity experience.
    /// </summary>
    _BEAMIMP std::vector<std::shared_ptr<beam_participant>> participants();

    /// <summary>
    /// Gets all the groups associated with the current interactivity instance.
    /// Empty list is returned if initialization is not yet completed.
    /// </summary>
    _BEAMIMP std::vector<std::shared_ptr<beam_group>> groups();

    /// <summary>
    /// Gets the pointer to a specific group. Returns a NULL pointer if initialization
    /// is not yet completed or if the group does not exist.
    /// </summary>
    _BEAMIMP std::shared_ptr<beam_group> group(_In_ const string_t& group_id = L"default");

    /// <summary>
    /// Gets all the scenes associated with the current interactivity instance.
    /// Returns a NULL pointer if initialization is not yet completed.
    /// </summary>
    _BEAMIMP std::vector<std::shared_ptr<beam_scene>> scenes();

    /// <summary>
    /// Gets the pointer to a specific scene. Returns a NULL pointer if initialization 
    /// is not yet completed or if the scene does not exist.
    /// </summary>
    _BEAMIMP std::shared_ptr<beam_scene> scene(_In_ const string_t&  scene_id);

    /// <summary>
    /// Disables a specific control for a period of time, specified in milliseconds.
    /// </summary>
    /// <param name="control_id">The unique string identifier of the control.</param>
    /// <param name="cooldown">Cooldown duration (in milliseconds).</param>
    _BEAMIMP void trigger_cooldown(_In_ const string_t& control_id, _In_ const std::chrono::milliseconds& cooldown) const;

    /// <summary>
    /// Manages and maintains proper state updates between the title and the Beam Service.
    /// To ensure best performance, do_work() must be called frequently, such as once per frame.
    /// Title needs to be thread safe when calling do_work() since this is when states are changed.
    /// This also clears the state of the input controls.
    /// </summary>
    /// <returns>A list of all the events the title has to handle. Empty if no events have been triggered
    /// during this update.</returns>
    _BEAMIMP std::vector<xbox::services::beam::beam_event> do_work();

private:

    /// <summary>
    /// Internal function
    /// </summary>
    beam_manager();

    std::shared_ptr<beam_manager_impl> m_impl;

    friend beam_control_builder;
    friend beam_mock_util;
    friend beam_scene_impl;
    friend beam_group;
    friend beam_group_impl;
    friend beam_participant_impl;
};

#if 0
/// <summary>
/// Represents mock Beam events. This class mocks events between the Beam interactivity
/// service and participants.
/// </summary>
class beam_mock_util : public std::enable_shared_from_this<beam_mock_util>
{
public:

    /// <summary>
    /// Returns the singleton instance of the mock class.
    /// </summary>
    _BEAMIMP static std::shared_ptr<beam_mock_util> get_singleton_instance();

    /// <summary>
    /// Sets the OAuth token; skips the linking code API.
    /// </summary>
    /// <param name="token">The token.</param>
    _BEAMIMP void set_oauth_token(_In_ string_t token);

    /// <summary>
    /// Creates a mock button event. Note: A mocked participant must first join before a mock input 
    /// such as a mock button event can be sent. Else, it will be ignored by the Beam manager.
    /// </summary>
    /// <param name="beamId">The Beam ID of the user.</param>
    /// <param name="buttonId">The unique string identifier of the button control.</param>
    /// <param name="is_down">TValue that indicates the button is down or not. If TRUE, the button is down.</param>
    _BEAMIMP void mock_button_event(_In_ uint32_t beamId, _In_ string_t buttonId, _In_ bool is_down);

    /// <summary>
    /// Simulates a specific mock participant joining the interactivity.
    /// </summary>
    /// <param name="beamId">The Beam ID of the user.</param>
    /// <param name="beamUsername">The Beam username of the user.</param>
    _BEAMIMP void mock_participant_join(_In_ uint32_t beamId, _In_ string_t beamUsername);

    /// <summary>
    /// Simulates a specific mock participant leaving interactivity.
    /// </summary>
    /// <param name="beamId">The Beam ID of the user.</param>
    /// <param name="beamUsername">The Beam username of the user.</param>
    _BEAMIMP void mock_participant_leave(_In_ uint32_t beamId, _In_ string_t beamUsername);

private:

    /// <summary>
    /// Internal function
    /// </summary>
    beam_mock_util();

    std::shared_ptr<xbox::services::beam::beam_manager_impl> m_beamManagerImpl;

    friend beam_control_builder;
    friend beam_manager_impl;
};
#endif

}}}
