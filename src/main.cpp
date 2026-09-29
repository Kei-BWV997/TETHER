#include "PCH.h"
#include "InputHandler.h"
#include "UpdateHook.h"
#include "SpeedHook.h"
#include "TetherController.h"
#include "Settings.h"
#include "OARConditions.h"
#include "EquipEventSink.h"
#include "Prompt.h"
#include "UI.h"

namespace
{
	void InitializeLog()
	{
#ifndef NDEBUG
		auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
		auto path = logger::log_directory();
		if (!path) {
			util::report_and_fail("Failed to find standard logging directory"sv);
		}
		*path /= fmt::format("{}.log"sv, Plugin::NAME);
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

#ifndef NDEBUG
		const auto level = spdlog::level::trace;
#else
		const auto level = spdlog::level::info;
#endif

		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(level);
		log->flush_on(level);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);
	}

	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kPostLoad:
			// OAR expects custom-condition registration during kPostLoad (before
			// OAR itself scans OpenAnimationReplacer folders on kDataLoaded).
			TETHER::OARConditions::Register();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			TETHER::Settings::GetSingleton()->Load();
			TETHER::Prompt::Install();
			TETHER::UI::Install();
			TETHER::InputHandler::Register();
			TETHER::PlayerUpdateHook::Install();
			TETHER::SpeedHook::Install();
			TETHER::EquipEventSink::Register();
			logger::info("kDataLoaded: TETHER ready"sv);
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			// Tear down cleanly before save loads — otherwise cached bone pointers
			// and havok state become invalid across the load and we CTD.
			TETHER::TetherController::GetSingleton()->ForceRelease();
			TETHER::Prompt::Hide();
			break;
		default:
			break;
		}
	}
}

// NG-style plugin declaration (Skyrim 1.6.11xx / AE, Address Library).
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Plugin::VERSION);
	v.PluginName(Plugin::NAME);
	v.AuthorName("legitassmotion"sv);
	v.UsesAddressLibrary();
	v.CompatibleVersions({ SKSE::RUNTIME_SSE_LATEST });
	v.UsesNoStructs();
	return v;
}();

// Legacy query (kept for SE/old-loader compatibility).
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Plugin::NAME.data();
	a_info->version = Plugin::VERSION.pack();

	if (a_skse->IsEditor()) {
		return false;
	}
	return true;
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	REL::Module::reset();  // CommonLibSSE-NG workaround

	InitializeLog();
	logger::info("{} v{} loading"sv, Plugin::NAME, Plugin::VERSION.string());

	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(1 << 10);

	auto messaging = SKSE::GetMessagingInterface();
	if (!messaging->RegisterListener("SKSE", MessageHandler)) {
		return false;
	}

	logger::info("{} loaded"sv, Plugin::NAME);
	return true;
}
