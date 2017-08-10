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
#include "interactivity.h"
#include "mixer_web_socket_connection.h"
#include "mixer_web_socket_connection_state.h"
#include "mixer_web_socket_client.h"


namespace Microsoft {
    /// <summary>
    /// Contains classes and enumerations that let you incorporate
    /// Interactivity functionality into your title.
    /// </summary>
    namespace mixer {

    /// <summary>
    /// Enum representing the current state of the websocket connection
    /// </summary>
    enum interactivity_connection_state
    {
        /// <summary>
        /// Currently connected to the Mixer interactivity experience.
        /// </summary>
        connected,
        /// <summary>
        /// Currently connecting to the Mixer interactivity experience.
        /// </summary>
        connecting,
        /// <summary>
        /// Currently disconnected from the Mixer interactivity experience.
        /// </summary>
        disconnected
    };

    class interactive_group_impl
    {
    public:
        string_t group_id() const;
        string_t etag() const;
        string_t scene_id();
        const std::vector<std::shared_ptr<interactive_participant>> participants();
        void set_scene(std::shared_ptr<interactive_scene> currentScene);
        std::shared_ptr<interactive_scene> scene() const;
        bool update(web::json::value json, bool overwrite = false);

        interactive_group_impl(web::json::value json);
        interactive_group_impl(string_t groupId = L"default");
        interactive_group_impl(
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
        std::shared_ptr<MICROSOFT_MIXER_NAMESPACE::interactivity_manager> m_interactivityManager;

        friend interactive_group;
        friend interactivity_manager_impl;
    };

    class interactive_participant_impl
    {
    public:
        uint32_t mixer_id() const;
        const string_t& username() const;
        uint32_t level() const;
        const interactive_participant_state state() const;
        void set_group(std::shared_ptr<interactive_group> group);
        const std::shared_ptr<interactive_group> group();
        const std::chrono::milliseconds& last_input_at() const;
        const std::chrono::milliseconds& connected_at() const;
        bool input_disabled() const;
        std::shared_ptr<interactive_button_control> button(_In_ const string_t& control_id);
        std::vector<std::shared_ptr<interactive_button_control>> buttons();
        std::shared_ptr<interactive_joystick_control> joystick(_In_ const string_t& control_id);
        std::vector<std::shared_ptr<interactive_joystick_control>> joysticks();

        bool update(web::json::value json, bool overwrite);
        bool init_from_json(web::json::value json);

        interactive_participant_impl(
            uint32_t mixerId,
            string_t username,
            string_t sessionId,
            uint32_t level,
            string_t groupId,
            std::chrono::milliseconds lastInputAt,
            std::chrono::milliseconds connectedAt,
            bool disabled
        );

        interactive_participant_impl();

    private:
        uint32_t m_mixerId;
        string_t m_username;
        string_t m_sessionId;
        string_t m_groupId;
        string_t m_etag;
        uint32_t m_level;
        std::chrono::milliseconds m_lastInputAt;
        std::chrono::milliseconds m_connectedAt;
        bool m_disabled;
        interactive_participant_state m_state;

        bool m_groupIdUpdated;
        bool m_disabledUpdated;
        bool m_stateUpdated;

        std::shared_ptr<interactivity_manager> m_interactivityManager;

        friend interactivity_manager_impl;
    };


    class interactive_scene_impl
    {
    public:
        const string_t& scene_id() const;
        const bool& disabled() const;
        void set_disabled(bool disabled);
        const std::vector<string_t> groups();
        const std::vector<std::shared_ptr<interactive_button_control>> buttons();
        const std::vector<std::shared_ptr<interactive_joystick_control>> joysticks();
        const std::shared_ptr<interactive_button_control> button(_In_ const string_t& controlId);
        const std::shared_ptr<interactive_joystick_control> joystick(_In_ const string_t& controlId);
        void init_from_json(web::json::value json);

        interactive_scene_impl();

        interactive_scene_impl(
            _In_ string_t sceneId,
            _In_ string_t etag,
            _In_ bool disabled
            );

    private:
        std::shared_ptr<MICROSOFT_MIXER_NAMESPACE::interactivity_manager> m_interactivityManager;
        std::vector<string_t> m_buttonIds;
        std::vector<string_t> m_joystickIds;
        std::vector<string_t> m_groupIds;

        string_t m_sceneId;
        string_t m_etag;
        bool m_disabled;
        bool m_isCurrent;

        friend interactive_scene;
        friend interactivity_manager_impl;
    };

    class interactive_control_builder
    {
    public:
        static std::shared_ptr<interactive_button_control> build_button_control();
        static std::shared_ptr<interactive_button_control> build_button_control(string_t parentSceneId, web::json::value json);
        static std::shared_ptr<interactive_button_control> build_button_control(
            _In_ string_t parentSceneId,
            _In_ string_t controlId,
            _In_ bool disabled,
            _In_ float progress,
            _In_ std::chrono::milliseconds cooldownDeadline,
            _In_ string_t buttonText,
            _In_ uint32_t sparkCost
        );

        static std::shared_ptr<interactive_joystick_control> build_joystick_control();
        static std::shared_ptr<interactive_joystick_control> build_joystick_control(string_t parentSceneId, web::json::value json);
        static std::shared_ptr<interactive_joystick_control> build_joystick_control(
            _In_ string_t parentSceneId,
            _In_ string_t controlId,
            _In_ bool disabled,
            _In_ double x,
            _In_ double y
        );
    };

    class interactive_rpc_message
    {
    public:
        string_t to_string();
        uint32_t id();
        std::chrono::milliseconds timestamp();

        interactive_rpc_message(uint32_t id, web::json::value jsonMessage, std::chrono::milliseconds timestamp);

    private:
        int m_retries;
        uint32_t m_id;
        web::json::value m_json;
        std::chrono::milliseconds m_timestamp;

        friend interactivity_manager_impl;
    };


    class interactive_button_state
    {
    public:
        bool is_pressed();
        bool is_down();
        bool is_up();

        interactive_button_state();

    private:
        bool m_isPressed;
        bool m_isDown;
        bool m_isUp;

        friend interactivity_manager_impl;
    };


    class interactive_button_count
    {
    public:
        uint32_t count_of_button_downs();
        uint32_t count_of_button_presses();
        uint32_t count_of_button_ups();

        void clear();

        interactive_button_count();

    private:
        uint32_t m_buttonPresses;
        uint32_t m_buttonDowns;
        uint32_t m_buttonUps;

        friend interactivity_manager_impl;
    };

    class interactive_joystick_state
    {
    public:
        double x();
        double y();

        interactive_joystick_state(double x, double y);

    private:
        double m_x;
        double m_y;

        friend interactivity_manager_impl;
    };

    //
    // Implementation of the manager object that handles Mixer Interactivity functionality
    //
    class interactivity_manager_impl : public std::enable_shared_from_this<interactivity_manager_impl>
    {
    public:
        bool initialize(
            _In_ string_t interactiveVersion,
            _In_ bool goInteractive = true,
            _In_ string_t sharecode = L""
        );

#if TV_API | XBOX_UWP
        std::shared_ptr<interactive_event> set_local_user(_In_ xbox_live_user_t user);
#else
        std::shared_ptr<interactive_event> set_xtoken(_In_ string_t token);

		std::shared_ptr<interactive_event> set_oauth_token(_In_ string_t token);
#endif
#if 0
        void request_linking_code(_In_ const string_t& mixer_id) const;
#endif

        const std::chrono::milliseconds get_server_time();

        const string_t& interactive_version() const;

        const interactivity_state interactivity_state();

        bool start_interactive();

        bool stop_interactive();

        const std::shared_ptr<interactive_participant>& participant(_In_ uint32_t mixerId);

        std::vector<std::shared_ptr<interactive_participant>> participants();

        std::vector<std::shared_ptr<interactive_group>> groups();

        std::shared_ptr<interactive_group> group(_In_ const string_t& group_id = L"default");

        const std::shared_ptr<interactive_scene>& scene(_In_ const string_t& sceneId);

        std::vector<std::shared_ptr<interactive_scene>> scenes();

        std::shared_ptr<interactive_button_control> button(_In_ const string_t& control_id);
        
        std::vector<std::shared_ptr<interactive_button_control>> buttons();
        
        std::shared_ptr<interactive_joystick_control> joystick(_In_ const string_t& control_id);
        
        std::vector<std::shared_ptr<interactive_joystick_control>> joysticks();

        void set_disabled(_In_ const string_t& control_id, _In_ bool disabled);

        void set_progress(_In_ const string_t& control_id, _In_ float progress);

        void trigger_cooldown(_In_ const string_t& control_id, _In_ const std::chrono::milliseconds& cooldownDeadline);

        void send_message(_In_ const string_t& message);

        void capture_transaction(_In_ const string_t& transaction_id);

        std::vector<MICROSOFT_MIXER_NAMESPACE::interactive_event> do_work();

        interactivity_manager_impl();
        ~interactivity_manager_impl();

    private:

        //
        // Message handling
        //
        uint32_t get_next_message_id();
        void send_message(std::shared_ptr<interactive_rpc_message> rpcMessage);
        std::shared_ptr<interactive_rpc_message> remove_awaiting_reply(uint32_t messageId);

        void process_messages_worker();

        void process_reply(const web::json::value& jsonReply);
        void process_reply_error(const web::json::value& jsonReply);
        void process_get_groups_reply(const web::json::value& jsonReply);
        void process_create_groups_reply(const web::json::value& jsonReply);
        void process_update_groups_reply(const web::json::value& jsonReply);
        void process_update_controls_reply(const web::json::value& jsonReply);
        void process_update_participants_reply(const web::json::value& jsonReply);
        void process_get_scenes_reply(const web::json::value& jsonReply);
        void process_get_time_reply(std::shared_ptr<interactive_rpc_message> message, const web::json::value& jsonReply);

        void process_method(const web::json::value& methodJson);

        void process_participant_joined(const web::json::value& participantJoinedJson);
        void process_participant_left(const web::json::value& participantLeftJson);
        void process_on_participant_update(const web::json::value& participantChangeJson);

        void process_on_ready_changed(const web::json::value& onReadyMethod);
        void process_on_group_create(const web::json::value& onGroupCreateMethod);
        void process_on_group_update(const web::json::value& onGroupUpdateMethod);
        void process_on_control_update(const web::json::value& onControlUpdateMethod);
        void process_update_controls(web::json::array controlsToUpdate);

        void process_input(const web::json::value& inputMethod);
        void process_button_input(const web::json::value& inputJson);
        void process_joystick_input(const web::json::value& joystickInputJson);

        //
        // Controls, Scenes and Groups
        //
        std::shared_ptr<MICROSOFT_MIXER_NAMESPACE::interactive_event> try_set_current_scene(_In_ const string_t& scene_id, _In_ const string_t& group_id = L"default");
        void add_group_to_map(std::shared_ptr<interactive_group_impl> group);
        void add_control_to_map(std::shared_ptr<interactive_control> control);
        void update_button_state(string_t buttonId, web::json::value buttonInputParamsJson);
        void update_joystick_state(string_t joystickId, uint32_t mixerId, double x, double y);
        void send_update_participants(std::vector<uint32_t> participantIds);
        void send_create_groups(std::vector<std::shared_ptr<interactive_group_impl>> groupsToCreate);
        void send_update_groups(std::vector<std::shared_ptr<interactive_group_impl>> groupsToUpdate);
        void move_participant_group(uint32_t participantId, string_t oldGroupId, string_t newGroupId);
        void participant_leave_group(uint32_t participantId, string_t groupId);

        //
        // Reporting events to client, server
        //
        void queue_interactive_event_for_client(string_t errorMessage, std::error_code errorCode, interactive_event_type type, std::shared_ptr<interactive_event_args> args);
        void queue_message_for_service(std::shared_ptr<interactive_rpc_message> message);

        //
        // Server and connection management
        //
        void init_worker(_In_ string_t interactiveVersion, _In_ bool goInteractive);
        bool get_auth_token(_Out_ std::shared_ptr<MICROSOFT_MIXER_NAMESPACE::interactive_event> &errorEvent);
#if !TV_API && !XBOX_UWP
		std::shared_ptr<interactive_event> set_auth_token(_In_ string_t token);
#endif
        bool get_interactive_host();
        void initialize_websockets_helper();
        bool initialize_server_state_helper();
        void close_websocket();

        void on_socket_message_received(const string_t& message);
        void on_socket_closed(web::websockets::client::websocket_close_status closeStatus, string_t closeReason);
        void on_socket_connection_state_change(mixer_web_socket_connection_state oldState, mixer_web_socket_connection_state newState);

        void set_interactivity_state(MICROSOFT_MIXER_NAMESPACE::interactivity_state newState);

        //
        // Mocks
        //
        string_t find_or_create_participant_session_id(_In_ uint32_t mixerId);
        void mock_button_event(_In_ uint32_t mixerId, _In_ string_t username, _In_ bool is_down);
        void mock_participant_join(_In_ uint32_t mixerId, _In_ string_t username);
        void mock_participant_leave(_In_ uint32_t mixerId, _In_ string_t username);

#if TV_API | XBOX_UWP
        std::vector<xbox_live_user_t> m_localUsers;
#endif
        string_t m_accessToken;
        string_t m_refreshToken;
        string_t m_interactiveVersion;
        string_t m_sharecode;
        string_t m_interactiveHostUrl;
        MICROSOFT_MIXER_NAMESPACE::interactivity_state m_interactivityState;
        std::chrono::milliseconds m_serverTimeOffset;
        std::chrono::milliseconds m_latency;

        std::recursive_mutex m_lock;
        std::recursive_mutex m_messagesLock;

        pplx::task<void> m_initializingTask;
        bool m_initScenesComplete;
        bool m_initGroupsComplete;
        bool m_initServerTimeComplete;
        bool m_localUserInitialized;
        int m_maxInitRetries;
        int m_initRetryAttempt;
        std::chrono::milliseconds m_initRetryInterval;

        bool m_processing;
        std::queue<std::shared_ptr<interactive_rpc_message>> m_unhandledFromService;
        std::queue<std::shared_ptr<interactive_rpc_message>> m_pendingSend;
        std::vector<std::shared_ptr<interactive_rpc_message>> m_awaitingReply;
        std::vector<interactive_event> m_eventsForClient;

        // Simple websocket connection for now, bring in robust connection manager in the future
        std::shared_ptr<MICROSOFT_MIXER_NAMESPACE::web_socket_connection> m_webSocketConnection;
        interactivity_connection_state m_connectionState;

        string_t m_currentDefaultGroupEtag;
        string_t m_currentScene;
        std::map<string_t, std::vector<uint32_t>> m_participantsByGroupId;
        std::map<string_t, std::shared_ptr<interactive_group_impl>> m_groupsInternal;
        std::map<string_t, std::shared_ptr<interactive_scene>> m_scenes;
        std::map<string_t, std::shared_ptr<interactive_control>> m_controls;
        std::map<uint32_t, std::shared_ptr<interactive_participant>> m_participants;
        std::map<string_t, uint32_t> m_participantTomixerId;

        std::map<string_t, std::shared_ptr<interactive_button_control>> m_buttons;
        std::map<string_t, std::shared_ptr<interactive_joystick_control>> m_joysticks;
        std::map<uint32_t, std::map<string_t, std::shared_ptr<interactive_button_control>>> m_buttonsByParticipantId;
        std::map<uint32_t, std::map<string_t, std::shared_ptr<interactive_joystick_control>>> m_joysticksByParticipantId;

        friend interactivity_manager;
        friend interactive_control_builder;
        friend interactivity_mock_util;
        friend interactive_scene_impl;
        friend interactive_group;
        friend interactive_group_impl;
        friend interactive_participant_impl;
    };

}}

