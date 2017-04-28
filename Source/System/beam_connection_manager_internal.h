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
        namespace beam {

            struct beam_connection_manager_factory_counter
            {
                beam_connection_manager_factory_counter() : counter(1) {}

                uint32_t counter;
                std::shared_ptr<xbox::services::beam::beam_connection_manager> beamConnectionManager;
            };

            class beam_connection_manager_factory
            {
                public:
                beam_connection_manager_factory();

                static std::shared_ptr<beam_connection_manager_factory> get_singleton_instance();

                /*
                void remove_user_from_rta_map(
                    _In_ std::shared_ptr<xbox::services::user_context> userContext
                );
                */
            private:
                //std::unordered_map <string_t, beam_connection_manager_factory_counter> m_xuidToRTAMap;
            };

        }
    }
}