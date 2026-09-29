#include "PCH.h"
#include "UI.h"
#include "Settings.h"

#include <filesystem>
#include "API/SKSEMenuFramework/SKSEMenuFramework.h"

namespace ImGui = ImGuiMCP;

namespace TETHER::UI
{
	namespace
	{
		using namespace ImGuiMCP;

		// DIK scancodes; 256+ are mouse buttons (same table as PREY / ImmersiveSellChest).
		const char* const kKbNames[] = {
			"[NONE]",
			"Escape", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
			"Minus", "Equals", "Backspace", "Tab",
			"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P",
			"Left Bracket", "Right Bracket", "Enter",
			"Left Control", "A", "S", "D", "F", "G", "H", "J", "K", "L",
			"Semicolon", "Apostrophe", "~ (Console)", "Left Shift", "Back Slash",
			"Z", "X", "C", "V", "B", "N", "M",
			"Comma", "Period", "Forward Slash", "Right Shift",
			"NUM*", "Left Alt", "Spacebar", "Caps Lock",
			"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10",
			"Num Lock", "Scroll Lock",
			"NUM7", "NUM8", "NUM9", "NUM-", "NUM4", "NUM5", "NUM6", "NUM+",
			"NUM1", "NUM2", "NUM3", "NUM0", "NUM.",
			"F11", "F12",
			"NUM Enter", "Right Control",
			"NUM/",
			"SysRq / PrtScr", "Right Alt",
			"Pause",
			"Home", "Up Arrow", "PgUp", "Left Arrow", "Right Arrow", "End", "Down Arrow", "PgDown",
			"Insert", "Delete",
			"Left Mouse Button", "Right Mouse Button", "Middle/Wheel Mouse Button",
			"Mouse Button 3", "Mouse Button 4", "Mouse Button 5",
			"Mouse Button 6", "Mouse Button 7", "Mouse Wheel Up", "Mouse Wheel Down"
		};
		const std::uint32_t kKbValues[] = {
			0,
			1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
			12, 13, 14, 15,
			16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
			26, 27, 28,
			29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
			39, 40, 41, 42, 43,
			44, 45, 46, 47, 48, 49, 50,
			51, 52, 53, 54,
			55, 56, 57, 58,
			59, 60, 61, 62, 63, 64, 65, 66, 67, 68,
			69, 70,
			71, 72, 73, 74, 75, 76, 77, 78,
			79, 80, 81, 82, 83,
			87, 88,
			156, 157,
			181,
			183, 184,
			197,
			199, 200, 201, 203, 205, 207, 208, 209,
			210, 211,
			256, 257, 258,
			259, 260, 261,
			262, 263, 264, 265
		};

		// Raw XInput codes (BSWin32GamepadDevice::Key) — what SkyPrompt expects.
		const char* const kPadNames[] = {
			"[NONE]",
			"DPAD UP", "DPAD DOWN", "DPAD LEFT", "DPAD RIGHT",
			"START", "BACK",
			"LEFT THUMB", "RIGHT THUMB",
			"LEFT SHOULDER", "RIGHT SHOULDER",
			"A", "B", "X", "Y",
			"LT", "RT"
		};
		const std::uint32_t kPadValues[] = {
			0,
			0x0001, 0x0002, 0x0004, 0x0008,
			0x0010, 0x0020,
			0x0040, 0x0080,
			0x0100, 0x0200,
			0x1000, 0x2000, 0x4000, 0x8000,
			0x0009, 0x000A
		};

		constexpr ImVec4 kBlue{ 0.35f, 0.55f, 0.85f, 1.0f };
		constexpr ImVec4 kOrange{ 0.90f, 0.60f, 0.30f, 1.0f };
		constexpr ImVec4 kGray{ 0.55f, 0.55f, 0.55f, 1.0f };

		template <std::size_t N>
		int FindIndex(const std::uint32_t (&values)[N], std::uint32_t current)
		{
			for (int i = 0; i < static_cast<int>(N); ++i) {
				if (values[i] == current) return i;
			}
			return 0;
		}

		void HelpMarker(const char* desc)
		{
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, kGray);
			ImGui::TextUnformatted("(?)");
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
				ImGui::TextUnformatted(desc);
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
		}

		template <std::size_t N>
		void KeyCombo(const char* label, const char* const (&names)[N], const std::uint32_t (&values)[N],
			std::atomic<std::uint32_t>& value, const char* help, bool& save)
		{
			int idx = FindIndex(values, value.load());
			ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.45f);
			if (ImGui::Combo(label, &idx, names, static_cast<int>(N))) {
				value.store(values[idx]);
				save = true;
			}
			HelpMarker(help);
		}

		void Toggle(const char* label, std::atomic<bool>& value, const char* help, bool& save)
		{
			bool v = value.load();
			if (ImGui::Checkbox(label, &v)) {
				value.store(v);
				save = true;
			}
			HelpMarker(help);
		}

		// Applies while dragging; persists to the INI once the drag ends.
		void Slider(const char* label, std::atomic<float>& value, float lo, float hi, const char* fmt,
			const char* help, bool& save)
		{
			float v = value.load();
			ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.45f);
			if (ImGui::SliderFloat(label, &v, lo, hi, fmt)) {
				value.store(v);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				save = true;
			}
			HelpMarker(help);
		}

		bool Section(const char* label, const ImVec4& color, bool defaultOpen)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			const bool open = ImGui::TreeNodeEx(label, defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
			ImGui::PopStyleColor();
			return open;
		}

		void __stdcall RenderMenu()
		{
			auto* settings = Settings::GetSingleton();
			auto& v        = settings->Values();
			bool  save     = false;

			if (Section("General", kBlue, true)) {
				KeyCombo("Keyboard key", kKbNames, kKbValues, v.keyboardKey,
					"Key to hold hands (via the prompt, or directly when the prompt is off), and to let go while holding hands. Default: H", save);
				KeyCombo("Gamepad button", kPadNames, kPadValues, v.gamepadKey,
					"Gamepad button to hold hands (via the prompt, or directly when the prompt is off), and to let go while holding hands. Default: none", save);

				ImGui::Spacing();
				Toggle("Show 'Hold hands' prompt", v.showPrompt,
					"ON: a 'Hold hands' prompt appears over a follower in front of you. OFF: no prompt; press the key while a follower is in front of you to hold hands. Default: ON", save);
				Slider("Hold hands distance", v.engageDistance, 50.0f, 512.0f, "%.0f u",
					"The prompt appears (and hand-holding can start) only when the follower is within this distance in front of you. Skyrim units, ~1.4 cm each. Default: 512", save);

				ImGui::Spacing();
				ImGui::SeparatorText("Auto-release triggers");
				Toggle("On weapon drawn", v.autoReleaseWeapon,
					"Release automatically when the player draws a weapon. Default: ON", save);
				Toggle("On combat start", v.autoReleaseCombat,
					"Release when either actor enters combat. Default: OFF (keep holding hands while fleeing)", save);
				Toggle("On extreme distance", v.autoReleaseExtDist,
					"Release when the follower stays far away for a sustained period. Default: OFF", save);

				ImGui::BeginDisabled(!v.autoReleaseExtDist.load());
				Slider("Distance threshold", v.extDistThreshold, 100.0f, 2000.0f, "%.0f u",
					"Distance (Skyrim units, ~1.4 cm each) for the extreme-distance release. Default: 500", save);
				Slider("Duration", v.extDistDuration, 1.0f, 30.0f, "%.1f s",
					"How many seconds the distance must be exceeded before releasing. Default: 3", save);
				ImGui::EndDisabled();
				ImGui::TreePop();
			}

			ImGui::Spacing();
			if (Section("Position", kBlue, false)) {
				Slider("Offset right", v.offsetRight, -40.0f, 40.0f, "%.0f u",
					"Right/left offset of the follower relative to the player. Negative = left. Default: 0", save);
				Slider("Offset back", v.offsetBack, -100.0f, 0.0f, "%.0f u",
					"Forward/back offset. Negative = behind. Default: -30", save);
				Slider("Follow radius", v.followRadius, 5.0f, 100.0f, "%.0f u",
					"Within this distance from the ideal spot the follower stops moving. Default: 30", save);
				Slider("Catch-up radius", v.catchupRadius, 10.0f, 200.0f, "%.0f u",
					"Beyond this distance the follower switches to a catch-up run. Default: 60", save);
				ImGui::TreePop();
			}

			ImGui::Spacing();
			if (Section("Tension & Grip", kBlue, false)) {
				Slider("Ideal distance (no cap)", v.tetherIdealDist, 10.0f, 200.0f, "%.0f u",
					"Within this player-follower distance, no speed cap. Default: 40", save);
				Slider("Slack distance (max cap)", v.tetherSlackDist, 30.0f, 400.0f, "%.0f u",
					"Beyond this distance, player speed is capped to walk speed. Between ideal and slack it lerps. Default: 120", save);
				Slider("Player speed near", v.playerSpeedMultNear, 0.3f, 1.0f, "%.2fx",
					"Player speed multiplier when the tether is close. Default: 1.00", save);
				Slider("Player speed far", v.playerSpeedMultFar, 0.1f, 1.0f, "%.2fx",
					"Player speed multiplier when the tether is stretched to the slack limit. Default: 0.35 (walk speed)", save);
				Slider("Follower speed boost", v.followerSpeedMult, 1.0f, 2.5f, "%.2fx",
					"Follower speed multiplier while holding hands. Default: 1.30", save);
				Slider("Palm offset (into hand)", v.palmOffset, 0.0f, 15.0f, "%.1f u",
					"How deep into the palm the constraint attaches. 0 = wrist. Default: 5", save);
				ImGui::TreePop();
			}

			ImGui::Spacing();
			if (Section("Advanced (Animation-critical)", kOrange, false)) {
				ImGui::PushStyleColor(ImGuiCol_Text, kGray);
				ImGui::TextWrapped("These values are tuned to match the shipped animation. Changes may break the visuals. Use \"Restore from shipped defaults\" below to revert.");
				ImGui::PopStyleColor();
				ImGui::Spacing();
				Slider("Grip delay", v.gripDelay, 0.0f, 5.0f, "%.2f s",
					"Seconds from the start of the reach-out (Enter) animation until the hands grip. The body pose at that moment is kept while holding hands. Default: 1.00", save);
				ImGui::TreePop();
			}

			ImGui::Spacing();
			if (Section("Presets", kOrange, false)) {
				ImGui::PushStyleColor(ImGuiCol_Text, kGray);
				ImGui::TextWrapped("Export saves your current values to Data/SKSE/Plugins/TETHER_defaults.ini (author-shipped preset). Restore re-applies that preset over your current settings.");
				ImGui::PopStyleColor();
				ImGui::Spacing();
				if (ImGui::Button("Export current settings as defaults")) {
					settings->ExportDefaults();
				}
				HelpMarker("Author use: overwrites TETHER_defaults.ini with the current values. Ship that file with the mod.");
				if (ImGui::Button("Restore from shipped defaults")) {
					settings->RestoreDefaults();
				}
				HelpMarker("Re-applies TETHER_defaults.ini on top of your current settings and saves to TETHER.ini.");
				ImGui::TreePop();
			}

			if (save) {
				settings->Save();
			}
		}
	}

	void Install()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			logger::warn("[ui] SKSE Menu Framework not installed, settings UI unavailable (TETHER.ini is still used)"sv);
			return;
		}
		SKSEMenuFramework::SetSection("TETHER");
		SKSEMenuFramework::AddSectionItem("Settings", RenderMenu);
		logger::info("[ui] registered with SKSE Menu Framework"sv);
	}
}
