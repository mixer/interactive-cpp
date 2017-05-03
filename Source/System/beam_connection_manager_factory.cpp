//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "pch.h"
#include "user_context.h"
#include "xsapi/beam.h"
#include "beam_connection_manager_internal.h"

NAMESPACE_MICROSOFT_XBOX_SERVICES_BEAM_CPP_BEGIN
static std::mutex s_singletonLock;

std::shared_ptr<beam_connection_manager_factory>
beam_connection_manager_factory::get_singleton_instance()
{
	std::lock_guard<std::mutex> guard(s_singletonLock);
	static std::shared_ptr<beam_connection_manager_factory> instance;
	if (instance == nullptr)
	{
		instance = std::make_shared<beam_connection_manager_factory>();
	}

	return instance;
}

beam_connection_manager_factory::beam_connection_manager_factory()
{
}

/*
const std::shared_ptr<beam_connection_manager>&
beam_connection_manager::get_beam_connection_instance(
	_In_ std::shared_ptr<xbox::services::user_context> userContext,
	_In_ std::shared_ptr<xbox::services::xbox_live_context_settings> xboxLiveContextSettings,
	_In_ std::shared_ptr<xbox::services::xbox_live_app_config> appConfig
)
{
	std::lock_guard<std::mutex> guard(s_singletonLock);
	XSAPI_ASSERT(userContext != nullptr);

	auto xboxUserId = userContext->xbox_user_id();
	auto iter = m_xuidToRTAMap.find(xboxUserId);
	if (iter == m_xuidToRTAMap.end())
	{
		auto rtaService = std::shared_ptr<beam_connection_manager>(
			new beam_connection_manager(
				userContext,
				xboxLiveContextSettings,
				appConfig
			));

		beam_connection_manager_factory_counter rtaImplCounter;
		rtaImplCounter.rtaService = rtaService;
		m_xuidToRTAMap[xboxUserId] = rtaImplCounter;
		iter = m_xuidToRTAMap.find(xboxUserId);
	}
	else
	{
		++iter->second.counter;
	}

	return iter->second.rtaService;
}

void
real_time_activity_service_factory::remove_user_from_rta_map(
	_In_ std::shared_ptr<xbox::services::user_context> userContext
)
{
	std::lock_guard<std::mutex> guard(s_singletonLock);
	XSAPI_ASSERT(userContext != nullptr);
	auto& xuid = userContext->xbox_user_id();
	if (!xuid.empty())
	{
		auto iter = m_xuidToRTAMap.find(xuid);
		if (iter == m_xuidToRTAMap.end())
		{
			return;
		}

		--iter->second.counter;
		if (iter->second.counter == 0)
		{
			m_xuidToRTAMap.erase(xuid);
		}
	}
}
*/

NAMESPACE_MICROSOFT_XBOX_SERVICES_BEAM_CPP_END