#include "PCH.h"
#include "Prompt.h"
#include "Settings.h"
#include "TetherController.h"
#include "API/SkyPrompt/API.hpp"

#include <array>

namespace TETHER::Prompt
{
	namespace
	{
		constexpr SkyPromptAPI::EventID  kEventTETHER    = 8702;
		constexpr SkyPromptAPI::ActionID kActionHoldHand = 0;

		struct HoldHandsSink : public SkyPromptAPI::PromptSink
		{
			SkyPromptAPI::Prompt m_prompt;

			HoldHandsSink()
			{
				m_prompt.text       = "Hold hands";
				m_prompt.eventID    = kEventTETHER;
				m_prompt.actionID   = kActionHoldHand;
				m_prompt.type       = SkyPromptAPI::PromptType::kSinglePress;
				m_prompt.refid      = 0;
				m_prompt.text_color = 0xFFFFFFFF;
				m_prompt.progress   = 0.f;
			}

			std::span<const SkyPromptAPI::Prompt> GetPrompts() const override
			{
				return { &m_prompt, 1 };
			}

			void ProcessEvent(SkyPromptAPI::PromptEvent a_event) const override
			{
				if (a_event.type != SkyPromptAPI::PromptEventType::kAccepted) return;
				SKSE::GetTaskInterface()->AddTask([]() {
					logger::info("[prompt] Hold hands accepted"sv);
					TetherController::GetSingleton()->TryEngage();
				});
			}
		};

		// Sink, prompt text and button_key storage must outlive every SendPrompt.
		HoldHandsSink          g_sink;
		SkyPromptAPI::ClientID g_clientID = 0;
		RE::FormID             g_shownRefID = 0;  // 0 = not showing (game thread only)
		std::array<std::pair<RE::INPUT_DEVICE, SkyPromptAPI::ButtonID>, 2> g_keys{};

		std::size_t BuildButtonKeys()
		{
			auto&       s     = *Settings::GetSingleton();
			std::size_t count = 0;
			if (const auto kb = s.KeyboardKey(); kb != 0) {
				g_keys[count++] = kb >= 256 ?
					std::pair{ RE::INPUT_DEVICE::kMouse, static_cast<SkyPromptAPI::ButtonID>(kb - 256) } :
					std::pair{ RE::INPUT_DEVICE::kKeyboard, static_cast<SkyPromptAPI::ButtonID>(kb) };
			}
			if (const auto pad = s.GamepadKey(); pad != 0) {
				g_keys[count++] = { RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(pad) };
			}
			return count;
		}
	}

	void Install()
	{
		g_clientID = SkyPromptAPI::RequestClientID();
		if (g_clientID == 0) {
			logger::warn("[prompt] SkyPrompt not installed or client ID request failed. Hand-holding cannot be started."sv);
			return;
		}
		logger::info("[prompt] SkyPrompt client ID={}"sv, g_clientID);
	}

	bool IsAvailable()
	{
		return g_clientID != 0;
	}

	void Show(RE::Actor* a_target)
	{
		if (g_clientID == 0 || !a_target) return;
		const RE::FormID refID = a_target->GetFormID();
		if (refID == g_shownRefID) return;

		// Keys are re-read on every (re)show so UI changes apply without a restart.
		const std::size_t count = BuildButtonKeys();
		g_sink.m_prompt.refid      = refID;
		g_sink.m_prompt.button_key = { g_keys.data(), count };

		const bool sent = SkyPromptAPI::SendPrompt(&g_sink, g_clientID);
		g_shownRefID = refID;
		logger::info("[prompt] Show 'Hold hands' target='{}' (0x{:08X}) keys={} sent={}"sv,
			a_target->GetName(), refID, count, sent);
	}

	void Hide()
	{
		if (g_clientID == 0 || g_shownRefID == 0) return;
		SkyPromptAPI::RemovePrompt(&g_sink, g_clientID);
		logger::info("[prompt] Hide (was 0x{:08X})"sv, g_shownRefID);
		g_shownRefID = 0;
	}
}
