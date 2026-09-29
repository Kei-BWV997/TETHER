#include "PCH.h"
#include "InputHandler.h"
#include "TetherController.h"
#include "Settings.h"

namespace TETHER
{
	// While holding hands the key lets go (no prompt is shown then). While not holding
	// hands the key starts hand-holding only when the 'Hold hands' prompt is turned
	// off in the settings; otherwise SkyPrompt handles the start.
	namespace
	{
		// Some builds report LT/RT as synthetic 0x400/0x800; SkyPrompt and the UI use
		// BSWin32GamepadDevice::Key (0x9/0xA).
		std::uint32_t NormalizePad(std::uint32_t a_raw)
		{
			switch (a_raw) {
			case 0x0400: return 0x0009;
			case 0x0800: return 0x000A;
			default:     return a_raw;
			}
		}

		bool MatchesKey(RE::INPUT_DEVICE a_device, std::uint32_t a_raw)
		{
			const auto& s = *Settings::GetSingleton();
			switch (a_device) {
			case RE::INPUT_DEVICE::kKeyboard:
				return s.KeyboardKey() != 0 && a_raw == s.KeyboardKey();
			case RE::INPUT_DEVICE::kMouse:
				return s.KeyboardKey() >= 256 && a_raw + 256 == s.KeyboardKey();
			case RE::INPUT_DEVICE::kGamepad:
				return s.GamepadKey() != 0 && NormalizePad(a_raw) == s.GamepadKey();
			default:
				return false;
			}
		}
	}

	InputHandler* InputHandler::GetSingleton()
	{
		static InputHandler instance;
		return &instance;
	}

	void InputHandler::Register()
	{
		auto* mgr = RE::BSInputDeviceManager::GetSingleton();
		if (!mgr) {
			logger::error("InputHandler: BSInputDeviceManager unavailable"sv);
			return;
		}
		mgr->AddEventSink(GetSingleton());
		logger::info("InputHandler registered (keyboard={} gamepad=0x{:X} showPrompt={})"sv,
			Settings::GetSingleton()->KeyboardKey(), Settings::GetSingleton()->GamepadKey(),
			Settings::GetSingleton()->ShowPrompt());
	}

	RE::BSEventNotifyControl InputHandler::ProcessEvent(RE::InputEvent* const* a_event,
		RE::BSTEventSource<RE::InputEvent*>*)
	{
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		auto*      ctl   = TetherController::GetSingleton();
		const bool isOff = ctl->GetState() == TetherController::State::Off;
		if (isOff && Settings::GetSingleton()->ShowPrompt()) {
			return RE::BSEventNotifyControl::kContinue;
		}
		// Skip while any menu is up (keeps the key from firing in inventory/console/dialogue).
		if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
			return RE::BSEventNotifyControl::kContinue;
		}

		for (auto* event = *a_event; event; event = event->next) {
			auto* button = event->AsButtonEvent();
			if (!button || !button->IsDown()) {
				continue;
			}
			const auto          device = event->GetDevice();
			const std::uint32_t raw    = button->GetIDCode();
			if (!MatchesKey(device, raw)) {
				continue;
			}
			logger::info("{} key pressed (device={}, raw=0x{:X})"sv,
				isOff ? "Hold hands"sv : "Release"sv,
				device == RE::INPUT_DEVICE::kGamepad ? "gamepad"sv :
				device == RE::INPUT_DEVICE::kMouse   ? "mouse"sv : "keyboard"sv,
				raw);
			if (isOff) {
				ctl->TryEngage();
			} else {
				ctl->ReleaseByUser();
			}
			break;
		}

		return RE::BSEventNotifyControl::kContinue;
	}
}
