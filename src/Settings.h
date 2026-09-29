#pragma once

#include <atomic>

namespace TETHER
{
	// Values are read by the game thread (TetherController, SpeedHook, InputHandler,
	// Prompt) and written by the SKSE Menu Framework UI, so every field is atomic.
	struct SettingsValues
	{
		// Activation. keyboardKey: DIK scancode, or 256+ for mouse buttons.
		// gamepadKey: raw XInput code (BSWin32GamepadDevice::Key), 0 = none.
		std::atomic<std::uint32_t> keyboardKey{};
		std::atomic<std::uint32_t> gamepadKey{};
		std::atomic<bool>          showPrompt{};      // false = no prompt, the key starts hand-holding directly
		std::atomic<float>         engageDistance{};  // front-cone range for the prompt / engage

		// Auto-release triggers
		std::atomic<bool>  autoReleaseWeapon{};
		std::atomic<bool>  autoReleaseCombat{};
		std::atomic<bool>  autoReleaseExtDist{};
		std::atomic<float> extDistThreshold{};
		std::atomic<float> extDistDuration{};

		// Follower positioning (KeepOffsetFromActor)
		std::atomic<float> offsetRight{};
		std::atomic<float> offsetBack{};
		std::atomic<float> followRadius{};
		std::atomic<float> catchupRadius{};

		// Elastic tether + grip point
		std::atomic<float> tetherIdealDist{};
		std::atomic<float> tetherSlackDist{};
		std::atomic<float> playerSpeedMultNear{};
		std::atomic<float> playerSpeedMultFar{};
		std::atomic<float> followerSpeedMult{};
		std::atomic<float> palmOffset{};

		// Animation-critical
		std::atomic<float> gripDelay{};  // s from Enter clip selection to ragdoll + constraint
	};

	// Data/SKSE/Plugins/TETHER.ini (user values, written by the UI) layered over
	// TETHER_defaults.ini (author-shipped values, written by the UI's Export).
	class Settings
	{
	public:
		static Settings* GetSingleton();

		void Load();  // Call once on kDataLoaded.
		void Save() const;
		void ExportDefaults() const;
		bool RestoreDefaults();

		SettingsValues&       Values() { return v_; }
		const SettingsValues& Values() const { return v_; }

		std::uint32_t KeyboardKey() const { return v_.keyboardKey.load(); }
		std::uint32_t GamepadKey() const { return v_.gamepadKey.load(); }
		bool          ShowPrompt() const { return v_.showPrompt.load(); }
		float         EngageDistance() const { return v_.engageDistance.load(); }

		float OffsetRight() const { return v_.offsetRight.load(); }
		float OffsetBack() const { return v_.offsetBack.load(); }
		float FollowRadius() const { return v_.followRadius.load(); }
		float CatchupRadius() const { return v_.catchupRadius.load(); }

		float TetherIdealDist() const { return v_.tetherIdealDist.load(); }
		float TetherSlackDist() const { return v_.tetherSlackDist.load(); }
		float PlayerSpeedMultNear() const { return v_.playerSpeedMultNear.load(); }
		float PlayerSpeedMultFar() const { return v_.playerSpeedMultFar.load(); }
		float FollowerSpeedMult() const { return v_.followerSpeedMult.load(); }
		float PalmOffset() const { return v_.palmOffset.load(); }

		bool  AutoReleaseWeapon() const { return v_.autoReleaseWeapon.load(); }
		bool  AutoReleaseCombat() const { return v_.autoReleaseCombat.load(); }
		bool  AutoReleaseExtDist() const { return v_.autoReleaseExtDist.load(); }
		float ExtDistThreshold() const { return v_.extDistThreshold.load(); }
		float ExtDistDuration() const { return v_.extDistDuration.load(); }

		float GripDelay() const { return v_.gripDelay.load(); }

	private:
		Settings() { ResetToDefaults(); }
		void ResetToDefaults();

		SettingsValues v_;
	};
}
