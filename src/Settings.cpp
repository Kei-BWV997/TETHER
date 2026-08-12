#include "PCH.h"
#include "Settings.h"

namespace TETHER
{
	Settings* Settings::GetSingleton()
	{
		static Settings instance;
		return &instance;
	}

	static RE::TESGlobal* FindGlobal(std::string_view a_editorID)
	{
		auto* form = RE::TESForm::LookupByEditorID(a_editorID);
		if (!form) return nullptr;
		if (form->GetFormType() != RE::FormType::Global) return nullptr;
		return static_cast<RE::TESGlobal*>(form);
	}

	void Settings::Load()
	{
		g_Hotkey              = FindGlobal("TETHER_Hotkey"sv);
		g_OffsetRight         = FindGlobal("TETHER_OffsetRight"sv);
		g_OffsetBack          = FindGlobal("TETHER_OffsetBack"sv);
		g_FollowRadius        = FindGlobal("TETHER_FollowRadius"sv);
		g_CatchupRadius       = FindGlobal("TETHER_CatchupRadius"sv);
		g_TetherIdealDist     = FindGlobal("TETHER_TetherIdealDist"sv);
		g_TetherSlackDist     = FindGlobal("TETHER_TetherSlackDist"sv);
		g_PlayerSpeedMultNear = FindGlobal("TETHER_PlayerSpeedMultNear"sv);
		g_PlayerSpeedMultFar  = FindGlobal("TETHER_PlayerSpeedMultFar"sv);
		g_FollowerSpeedMult   = FindGlobal("TETHER_FollowerSpeedMult"sv);
		g_PalmOffset          = FindGlobal("TETHER_PalmOffset"sv);
		g_AutoRelWeapon       = FindGlobal("TETHER_AutoRelWeapon"sv);
		g_AutoRelCombat       = FindGlobal("TETHER_AutoRelCombat"sv);
		g_AutoRelExtDist      = FindGlobal("TETHER_AutoRelExtDist"sv);
		g_ExtDistThreshold    = FindGlobal("TETHER_ExtDistThreshold"sv);
		g_ExtDistDuration     = FindGlobal("TETHER_ExtDistDuration"sv);

		int found = 0;
		if (g_Hotkey)              ++found;
		if (g_OffsetRight)         ++found;
		if (g_OffsetBack)          ++found;
		if (g_FollowRadius)        ++found;
		if (g_CatchupRadius)       ++found;
		if (g_TetherIdealDist)     ++found;
		if (g_TetherSlackDist)     ++found;
		if (g_PlayerSpeedMultNear) ++found;
		if (g_PlayerSpeedMultFar)  ++found;
		if (g_FollowerSpeedMult)   ++found;
		if (g_PalmOffset)          ++found;
		if (g_AutoRelWeapon)       ++found;
		if (g_AutoRelCombat)       ++found;
		if (g_AutoRelExtDist)      ++found;
		if (g_ExtDistThreshold)    ++found;
		if (g_ExtDistDuration)     ++found;

		logger::info("Settings::Load: {}/16 globals bound from TETHER.esp"sv, found);
		if (found == 0) {
			logger::warn("No TETHER globals found. Is TETHER.esp installed and are editor IDs preserved (powerofthree's Tweaks)? Falling back to hardcoded defaults."sv);
		} else if (found < 16) {
			logger::warn("Some TETHER globals missing — check ESP editor IDs match the spec."sv);
		}
	}

	std::uint32_t Settings::Hotkey() const
	{
		return g_Hotkey ? static_cast<std::uint32_t>(g_Hotkey->value) : 0x23u;
	}

	float Settings::OffsetRight() const         { return g_OffsetRight         ? g_OffsetRight->value         : 0.0f;   }
	float Settings::OffsetBack() const          { return g_OffsetBack          ? g_OffsetBack->value          : -30.0f; }
	float Settings::FollowRadius() const        { return g_FollowRadius        ? g_FollowRadius->value        : 30.0f;  }
	float Settings::CatchupRadius() const       { return g_CatchupRadius       ? g_CatchupRadius->value       : 60.0f;  }
	float Settings::TetherIdealDist() const     { return g_TetherIdealDist     ? g_TetherIdealDist->value     : 40.0f;  }
	float Settings::TetherSlackDist() const     { return g_TetherSlackDist     ? g_TetherSlackDist->value     : 120.0f; }
	float Settings::PlayerSpeedMultNear() const { return g_PlayerSpeedMultNear ? g_PlayerSpeedMultNear->value : 1.0f;   }
	float Settings::PlayerSpeedMultFar() const  { return g_PlayerSpeedMultFar  ? g_PlayerSpeedMultFar->value  : 0.35f;  }
	float Settings::FollowerSpeedMult() const   { return g_FollowerSpeedMult   ? g_FollowerSpeedMult->value   : 1.30f;  }
	float Settings::PalmOffset() const          { return g_PalmOffset          ? g_PalmOffset->value          : 5.0f;   }
	float Settings::ExtDistThreshold() const    { return g_ExtDistThreshold    ? g_ExtDistThreshold->value    : 500.0f; }
	float Settings::ExtDistDuration() const     { return g_ExtDistDuration     ? g_ExtDistDuration->value     : 3.0f;   }

	bool Settings::AutoReleaseWeapon() const    { return g_AutoRelWeapon  ? (g_AutoRelWeapon->value  != 0.0f) : true;  }
	bool Settings::AutoReleaseCombat() const    { return g_AutoRelCombat  ? (g_AutoRelCombat->value  != 0.0f) : false; }
	bool Settings::AutoReleaseExtDist() const   { return g_AutoRelExtDist ? (g_AutoRelExtDist->value != 0.0f) : false; }
}
