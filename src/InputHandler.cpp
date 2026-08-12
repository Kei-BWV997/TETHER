#include "PCH.h"
#include "InputHandler.h"
#include "TetherController.h"
#include "Settings.h"

namespace TETHER
{
	// Translate the raw ButtonEvent IDCode to SkyUI's unified keycode space, which
	// is what `AddKeyMapOption` in MCM stores. Keyboard = 0..255 (DIK, identity),
	// Mouse = 256+button, Gamepad = 266..281 mapped from XInput/Skyrim raw values.
	// Skyrim's gamepad raw codes are XInput bitmasks for buttons plus synthetic
	// 0x400/0x800 for LT/RT triggers.
	static std::uint32_t TranslateToSkyUICode(RE::INPUT_DEVICE device, std::uint32_t raw)
	{
		if (device == RE::INPUT_DEVICE::kKeyboard) {
			return raw;
		}
		if (device == RE::INPUT_DEVICE::kMouse) {
			return raw + 256;
		}
		if (device == RE::INPUT_DEVICE::kGamepad) {
			switch (raw) {
				case 0x0001: return 266;  // DPad Up
				case 0x0002: return 267;  // DPad Down
				case 0x0004: return 268;  // DPad Left
				case 0x0008: return 269;  // DPad Right
				case 0x0010: return 270;  // Start
				case 0x0020: return 271;  // Back
				case 0x0040: return 272;  // Left Thumb click
				case 0x0080: return 273;  // Right Thumb click
				case 0x0100: return 274;  // LB (Left Shoulder)
				case 0x0200: return 275;  // RB (Right Shoulder)
				case 0x1000: return 276;  // A
				case 0x2000: return 277;  // B
				case 0x4000: return 278;  // X
				case 0x8000: return 279;  // Y
				case 0x0009:              // LT alt code (some builds)
				case 0x0400: return 280;  // LT
				case 0x000A:              // RT alt code
				case 0x0800: return 281;  // RT
				default:     return raw;  // DPad diagonals, unrecognized — no MCM binding
			}
		}
		return raw;
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
		logger::info("InputHandler registered (toggle key from MCM, default DIK 0x{:02X})"sv,
			Settings::GetSingleton()->Hotkey());
	}

	RE::BSEventNotifyControl InputHandler::ProcessEvent(RE::InputEvent* const* a_event,
		RE::BSTEventSource<RE::InputEvent*>*)
	{
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}

		// Skip while any menu is up (keeps hotkey from firing in inventory/console/dialogue).
		if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
			return RE::BSEventNotifyControl::kContinue;
		}

		const std::uint32_t hotkey = Settings::GetSingleton()->Hotkey();

		for (auto* event = *a_event; event; event = event->next) {
			auto* button = event->AsButtonEvent();
			if (!button) {
				continue;
			}
			const auto device = event->GetDevice();
			if (device != RE::INPUT_DEVICE::kKeyboard &&
				device != RE::INPUT_DEVICE::kMouse    &&
				device != RE::INPUT_DEVICE::kGamepad) {
				continue;
			}
			const std::uint32_t rawCode  = button->GetIDCode();
			const std::uint32_t skyuiCode = TranslateToSkyUICode(device, rawCode);
			if (skyuiCode != hotkey) {
				continue;
			}
			if (button->IsDown()) {
				logger::info("Hotkey pressed (device={}, raw=0x{:X}, skyui=0x{:X})"sv,
					device == RE::INPUT_DEVICE::kGamepad ? "gamepad"sv :
					device == RE::INPUT_DEVICE::kMouse   ? "mouse"sv : "keyboard"sv,
					rawCode, skyuiCode);
				TetherController::GetSingleton()->Toggle();
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}
}
