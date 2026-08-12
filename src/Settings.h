#pragma once

namespace TETHER
{
	// Reads TETHER.esp's GlobalVariables at runtime. Getters fall back to hardcoded
	// defaults when the ESP is missing or a specific global is not found, so the
	// plugin remains functional without the ESP installed (compile-time defaults).
	class Settings
	{
	public:
		static Settings* GetSingleton();
		void Load();  // Call once on kDataLoaded.

		// Activation
		std::uint32_t Hotkey() const;

		// Follower positioning (KeepOffsetFromActor)
		float OffsetRight() const;
		float OffsetBack() const;
		float FollowRadius() const;
		float CatchupRadius() const;

		// Elastic tether
		float TetherIdealDist() const;
		float TetherSlackDist() const;
		float PlayerSpeedMultNear() const;
		float PlayerSpeedMultFar() const;
		float FollowerSpeedMult() const;

		// Grip
		float PalmOffset() const;

		// Auto-release triggers
		bool  AutoReleaseWeapon() const;
		bool  AutoReleaseCombat() const;
		bool  AutoReleaseExtDist() const;
		float ExtDistThreshold() const;
		float ExtDistDuration() const;

	private:
		Settings() = default;

		RE::TESGlobal* g_Hotkey              = nullptr;
		RE::TESGlobal* g_OffsetRight         = nullptr;
		RE::TESGlobal* g_OffsetBack          = nullptr;
		RE::TESGlobal* g_FollowRadius        = nullptr;
		RE::TESGlobal* g_CatchupRadius       = nullptr;
		RE::TESGlobal* g_TetherIdealDist     = nullptr;
		RE::TESGlobal* g_TetherSlackDist     = nullptr;
		RE::TESGlobal* g_PlayerSpeedMultNear = nullptr;
		RE::TESGlobal* g_PlayerSpeedMultFar  = nullptr;
		RE::TESGlobal* g_FollowerSpeedMult   = nullptr;
		RE::TESGlobal* g_PalmOffset          = nullptr;
		RE::TESGlobal* g_AutoRelWeapon       = nullptr;
		RE::TESGlobal* g_AutoRelCombat       = nullptr;
		RE::TESGlobal* g_AutoRelExtDist      = nullptr;
		RE::TESGlobal* g_ExtDistThreshold    = nullptr;
		RE::TESGlobal* g_ExtDistDuration     = nullptr;
	};
}
