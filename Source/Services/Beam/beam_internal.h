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
#include "beam.h"
#include "beam_web_socket_connection.h"
#include "beam_web_socket_connection_state.h"
#include "beam_web_socket_client.h"


class beam_connection_manager;


namespace xbox { namespace services {
    /// <summary>
    /// Contains classes and enumerations that let you incorporate
    /// Beam Interactivity functionality into your title.
    /// </summary>
    namespace beam {

    /// <summary>
    /// Enum representing the current state of the websocket connection
    /// </summary>
    enum beam_interactivity_connection_state
    {
        /// <summary>
        /// Currently connected to the beam interactivity service.
        /// </summary>
        connected,
        /// <summary>
        /// Currently connecting to the beam interactivity service.
        /// </summary>
        connecting,
        /// <summary>
        /// Currently disconnected from the beam interactivity service.
        /// </summary>
        disconnected
    };


    /// <summary>
    /// Represents a client side manager that handles connections and communications with
    /// the Beam Interactivity services.
    /// TODO: This is internal to the beam_interactivity_service - does it need to be public?
    /// </summary>
    class beam_interactivity_connection_error_event_args
    {
    public:
        const std::error_code& err();
        const std::string& err_message();

        beam_interactivity_connection_error_event_args(
            std::error_code errc,
            std::string errMessage
        );

    private:
        std::error_code m_err;
        std::string m_errorMessage;
    };

    class beam_group_impl
    {
    public:
        string_t group_id() const;
        string_t etag() const;
        string_t scene_id();
        const std::vector<std::shared_ptr<beam_participant>> participants();
        void set_scene(std::shared_ptr<beam_scene> currentScene);
        std::shared_ptr<beam_scene> scene() const;
        bool update(web::json::value json, bool overwrite = false);

        beam_group_impl(web::json::value json);
        beam_group_impl(string_t groupId = L"default");
        beam_group_impl(
            string_t groupId,
            string_t etag,
            string_t sceneId
        );
    
    private:

        string_t m_groupId;
        string_t m_etag;
        string_t m_sceneId;

        std::recursive_mutex m_lock;
        bool m_sceneChanged;
        bool m_participantsChanged;
        std::shared_ptr<xbox::services::beam::beam_manager> m_beamManager;

        friend beam_group;
        friend beam_manager_impl;
    };

    class beam_participant_impl
    {
    public:
        uint32_t beam_id() const;
        const string_t& beam_username() const;
        uint32_t beam_level() const;
        const beam_participant_state state() const;
        void set_group(std::shared_ptr<beam_group> group);
        const std::shared_ptr<beam_group> group();
        const std::chrono::milliseconds& last_input_at() const;
        const std::chrono::milliseconds& connected_at() const;
        bool input_disabled() const;
        std::shared_ptr<beam_button_control> button(_In_ const string_t& control_id);
        std::vector<std::shared_ptr<beam_button_control>> buttons();
        std::shared_ptr<beam_joystick_control> joystick(_In_ const string_t& control_id);
        std::vector<std::shared_ptr<beam_joystick_control>> joysticks();

        bool update(web::json::value json, bool overwrite);
        bool init_from_json(web::json::value json);

        beam_participant_impl(
            uint32_t beamId,
            string_t username,
            string_t sessionId,
            uint32_t beamLevel,
            string_t groupId,
            std::chrono::milliseconds lastInputAt,
            std::chrono::milliseconds connectedAt,
            bool disabled
        );

        beam_participant_impl();

    private:
        uint32_t m_beamId;
        string_t m_username;
        string_t m_sessionId;
        string_t m_groupId;
        string_t m_etag;
        uint32_t m_beamLevel;
        std::chrono::milliseconds m_lastInputAt;
        std::chrono::milliseconds m_connectedAt;
        bool m_disabled;
        beam_participant_state m_state;

        bool m_groupIdUpdated;
        bool m_disabledUpdated;
        bool m_stateUpdated;

        std::shared_ptr<beam_manager> m_beamManager;

        friend beam_manager_impl;
    };


    class beam_scene_impl
    {
    public:
        const string_t& scene_id() const;
        const bool& disabled() const;
        void set_disabled(bool disabled);
        const std::vector<string_t> groups();
        const std::vector<std::shared_ptr<beam_button_control>> buttons();
        const std::vector<std::shared_ptr<beam_joystick_control>> joysticks();
        const std::shared_ptr<beam_button_control> button(_In_ const string_t& controlId);
        const std::shared_ptr<beam_joystick_control> joystick(_In_ const string_t& controlId);
        void init_from_json(web::json::value json);

        beam_scene_impl();

        beam_scene_impl(
            _In_ string_t sceneId,
            _In_ string_t etag,
            _In_ bool disabled,
            _In_ const std::vector<const string_t&>& controls
            );

    private:
        std::shared_ptr<xbox::services::beam::beam_manager> m_beamManager;
        std::vector<string_t> m_buttonIds;
        std::vector<string_t> m_joystickIds;
        std::vector<string_t> m_groupIds;

        string_t m_sceneId;
        string_t m_etag;
        bool m_disabled;
        bool m_isCurrent;

        friend beam_scene;
        friend beam_manager_impl;
    };

    class beam_control_builder
    {
    public:
        static std::shared_ptr<beam_button_control> build_button_control();
        static std::shared_ptr<beam_button_control> build_button_control(string_t parentSceneId, web::json::value json);
        static std::shared_ptr<beam_button_control> build_button_control(
            _In_ string_t parentSceneId,
            _In_ string_t controlId,
            _In_ bool disabled,
            _In_ string_t helpText,
            _In_ float progress,
            _In_ std::chrono::milliseconds cooldownDeadline,
            _In_ string_t buttonText,
            _In_ uint32_t sparkCost
        );

        static std::shared_ptr<beam_joystick_control> build_joystick_control();
        static std::shared_ptr<beam_joystick_control> build_joystick_control(string_t parentSceneId, web::json::value json);
        static std::shared_ptr<beam_joystick_control> build_joystick_control(
            _In_ string_t parentSceneId,
            _In_ string_t controlId,
            _In_ bool disabled,
            _In_ string_t helpText,
            _In_ double x,
            _In_ double y
        );
    };

    class beam_rpc_message
    {
    public:
        string_t to_string();
        uint32_t id();
        std::chrono::milliseconds timestamp();

        beam_rpc_message(uint32_t id, web::json::value jsonMessage, std::chrono::milliseconds timestamp);

    private:
        uint32_t m_id;
        web::json::value m_json;
        std::chrono::milliseconds m_timestamp;

        friend beam_manager_impl;
    };


    class beam_button_state
    {
    public:
        bool is_pressed();
        bool is_down();
        bool is_up();

        beam_button_state();

    private:
        bool m_isPressed;
        bool m_isDown;
        bool m_isUp;

        friend beam_manager_impl;
    };


    class beam_button_count
    {
    public:
        uint32_t count_of_button_downs();
        uint32_t count_of_button_presses();
        uint32_t count_of_button_ups();

        void clear();

        beam_button_count();

    private:
        uint32_t m_buttonPresses;
        uint32_t m_buttonDowns;
        uint32_t m_buttonUps;

        friend beam_manager_impl;
    };

    //
    // Implementation of the manager object that handles Beam functionality
    //
    class beam_manager_impl : public std::enable_shared_from_this<beam_manager_impl>
    {
    public:
        bool initialize(
            _In_ string_t interactiveVersion,
            _In_ bool goInteractive = true
        );

#if TV_API | XBOX_UWP
        std::shared_ptr<beam_event> add_local_user(_In_ xbox_live_user_t user);
#else
        void request_linking_code(_In_ const string_t& beam_id) const;
#endif

        const std::chrono::milliseconds get_server_time();

        const string_t& interactive_version() const;

        const beam_interactivity_state interactivity_state();

        bool start_interactive();

        bool stop_interactive();

        const std::shared_ptr<beam_participant>& participant(_In_ uint32_t beamId);

        std::vector<std::shared_ptr<beam_participant>> participants();

        std::vector<std::shared_ptr<beam_group>> groups();

        std::shared_ptr<beam_group> group(_In_ const string_t& group_id = L"default");

        const std::shared_ptr<beam_scene>& scene(_In_ const string_t& sceneId);

        std::vector<std::shared_ptr<beam_scene>> scenes();

        std::shared_ptr<beam_button_control> button(_In_ const string_t& control_id);
        
        std::vector<std::shared_ptr<beam_button_control>> buttons();
        
        std::shared_ptr<beam_joystick_control> joystick(_In_ const string_t& control_id);
        
        std::vector<std::shared_ptr<beam_joystick_control>> joysticks();

        void trigger_cooldown(_In_ const string_t& control_id, _In_ const std::chrono::milliseconds& cooldownDeadline);

        std::vector<xbox::services::beam::beam_event> do_work();

        beam_manager_impl();
        ~beam_manager_impl();

    private:

        //
        // Message handling
        //
        uint32_t get_next_message_id();
        void send_message(std::shared_ptr<beam_rpc_message> rpcMessage);
        void add_message_sent(std::shared_ptr<beam_rpc_message> message);
        std::shared_ptr<beam_rpc_message> pop_message_sent(uint32_t messageId);

        void process_reply(web::json::value jsonReply);
        void process_reply_error(web::json::value jsonReply);
        void process_get_groups_reply(web::json::value jsonReply);
        void process_create_groups_reply(web::json::value jsonReply);
        void process_update_groups_reply(web::json::value jsonReply);
        void process_update_controls_reply(web::json::value jsonReply);
        void process_update_participants_reply(web::json::value jsonReply);
        void process_get_scenes_reply(web::json::value jsonReply);
        void process_get_time_reply(std::shared_ptr<beam_rpc_message> message, web::json::value jsonReply);

        void process_method(web::json::value jsonMethod);

        void process_participant_joined(web::json::value participantJoinedJson);
        void process_participant_left(web::json::value participantLeftJson);
        void process_on_participant_update(web::json::value participantChangeJson);

        void process_on_ready_changed(web::json::value onReadyMethod);
        void process_on_group_create(web::json::value onGroupCreate);
        void process_on_group_update(web::json::value onGroupUpdateMethod);
        void process_on_control_update(web::json::value onControlUpdateMethod);
        void process_update_controls(web::json::array controlsToUpdate);

        void process_input(web::json::value inputMethod);
        void process_button_input(web::json::value buttonInputJson);
        void process_joystick_input(web::json::value joystickInputJson);

        //
        // Controls, Scenes and Groups
        //
        std::shared_ptr<xbox::services::beam::beam_event> try_set_current_scene(_In_ const string_t& scene_id, _In_ const string_t& group_id = L"default");
        void add_group_to_map(std::shared_ptr<beam_group_impl> group);
        void add_control_to_map(std::shared_ptr<beam_control> control);
        void update_button_state(string_t buttonId, web::json::value buttonInputParamsJson);
        void send_update_participants(std::vector<uint32_t> participantIds);
        void send_create_groups(std::vector<std::shared_ptr<beam_group_impl>> groupsToCreate);
        void send_update_groups(std::vector<std::shared_ptr<beam_group_impl>> groupsToUpdate);
        void move_participant_group(uint32_t participantId, string_t oldGroupId, string_t newGroupId);
        void participant_leave_group(uint32_t participantId, string_t groupId);

        //
        // Server communication
        //
        void init_worker(_In_ string_t interactiveVersion, _In_ bool goInteractive);
        bool get_auth_token(_Out_ std::shared_ptr<xbox::services::beam::beam_event> &errorEvent);
        bool get_interactive_host();
        void initialize_websockets_helper();
        bool initialize_server_state_helper();
        void close_websocket();

        void on_socket_message_received(const string_t& message);
        void on_socket_closed(web::websockets::client::websocket_close_status closeStatus, string_t closeReason);
        void on_socket_connection_state_change(beam_web_socket_connection_state oldState, beam_web_socket_connection_state newState);

        void set_interactivity_state(beam_interactivity_state newState);

        //
        // Mocks
        //
        string_t find_or_create_participant_session_id(_In_ uint32_t beamId);
        void mock_button_event(_In_ uint32_t beamId, _In_ string_t beamUsername, _In_ bool is_down);
        void mock_participant_join(_In_ uint32_t beamId, _In_ string_t beamUsername);
        void mock_participant_leave(_In_ uint32_t beamId, _In_ string_t beamUsername);

        //
        // Reporting events to client, error handling and logging
        //
        void report_beam_event(string_t errorMessage, std::error_code errorCode, beam_event_type type, std::shared_ptr<beam_event_args> args);

#if TV_API | XBOX_UWP
        std::vector<xbox_live_user_t> m_localUsers;
#endif
        string_t m_accessToken;
        string_t m_refreshToken;
        string_t m_interactiveVersion;
        string_t m_interactiveHostUrl;
        beam_interactivity_state m_interactivityState;
        std::chrono::milliseconds m_serverTimeOffset;
        std::chrono::milliseconds m_latency;

        std::recursive_mutex m_lock;
        std::recursive_mutex m_messageLock;

        bool m_initScenesComplete;
        bool m_initGroupsComplete;
        bool m_initServerTimeComplete;
        pplx::task<void> m_initializingTask;
        int m_maxInitRetries;
        int m_initRetryAttempt;
        std::chrono::milliseconds m_initRetryInterval;

        // Simple websocket connection for now, bring in robust connection manager in the future
        std::shared_ptr<XBOX_BEAM_NAMESPACE::web_socket_connection> m_webSocketConnection;
        beam_interactivity_connection_state m_connectionState;

        std::vector<xbox::services::beam::beam_event> m_eventsForClient;
        std::vector<std::shared_ptr<beam_rpc_message>> m_pendingSend;
        std::vector<std::shared_ptr<beam_rpc_message>> m_awaitingReply;

        string_t m_currentDefaultGroupEtag;
        string_t m_currentScene;
        std::map<string_t, std::vector<uint32_t>> m_participantsByGroupId;
        std::map<string_t, std::shared_ptr<beam_group_impl>> m_groupsInternal;
        std::map<string_t, std::shared_ptr<beam_scene>> m_scenes;
        std::map<string_t, std::shared_ptr<beam_control>> m_controls;
        std::map<uint32_t, std::shared_ptr<beam_participant>> m_participants;
        std::map<string_t, uint32_t> m_participantToBeamId;

        std::map<string_t, std::shared_ptr<beam_button_control>> m_buttons;
        std::map<string_t, std::shared_ptr<beam_joystick_control>> m_joysticks;
        std::map<uint32_t, std::map<string_t, std::shared_ptr<beam_button_control>>> m_buttonsByParticipantId;
        std::map<uint32_t, std::map<string_t, std::shared_ptr<beam_joystick_control>>> m_joysticksByParticipantId;

        friend beam_manager;
        friend beam_control_builder;
        friend beam_mock_util;
        friend beam_scene_impl;
        friend beam_group;
        friend beam_group_impl;
        friend beam_participant_impl;
    };

}}}

