#include "PCH.h"
#include "Settings.h"

#include <SimpleIni.h>
#include <Windows.h>
#include <filesystem>
#include <type_traits>

namespace TETHER
{
	namespace
	{
		std::filesystem::path GetPluginsDir()
		{
			char buf[MAX_PATH]{};
			if (GetModuleFileNameA(nullptr, buf, MAX_PATH) == 0) {
				return "Data/SKSE/Plugins";
			}
			return std::filesystem::path(buf).parent_path() / "Data/SKSE/Plugins";
		}

		std::filesystem::path GetUserIniPath() { return GetPluginsDir() / "TETHER.ini"; }
		std::filesystem::path GetDefaultsIniPath() { return GetPluginsDir() / "TETHER_defaults.ini"; }

		// Single list of (section, key, field) used by read, write and the load log.
		template <class V, class F>
		void ForEachField(V& v, F&& f)
		{
			f("General", "iKeyboardKey", v.keyboardKey);
			f("General", "iGamepadKey", v.gamepadKey);
			f("General", "bShowPrompt", v.showPrompt);
			f("General", "fEngageDistance", v.engageDistance);
			f("General", "bAutoReleaseWeapon", v.autoReleaseWeapon);
			f("General", "bAutoReleaseCombat", v.autoReleaseCombat);
			f("General", "bAutoReleaseExtDist", v.autoReleaseExtDist);
			f("General", "fExtDistThreshold", v.extDistThreshold);
			f("General", "fExtDistDuration", v.extDistDuration);

			f("Position", "fOffsetRight", v.offsetRight);
			f("Position", "fOffsetBack", v.offsetBack);
			f("Position", "fFollowRadius", v.followRadius);
			f("Position", "fCatchupRadius", v.catchupRadius);

			f("Tension", "fTetherIdealDist", v.tetherIdealDist);
			f("Tension", "fTetherSlackDist", v.tetherSlackDist);
			f("Tension", "fPlayerSpeedMultNear", v.playerSpeedMultNear);
			f("Tension", "fPlayerSpeedMultFar", v.playerSpeedMultFar);
			f("Tension", "fFollowerSpeedMult", v.followerSpeedMult);
			f("Tension", "fPalmOffset", v.palmOffset);

			f("Advanced", "fGripDelay", v.gripDelay);
		}

		template <class A>
		using AtomicValue = typename std::remove_cvref_t<A>::value_type;

		bool ReadIniInto(SettingsValues& v, const std::filesystem::path& path, const char* label)
		{
			CSimpleIniA ini;
			ini.SetUnicode();
			if (ini.LoadFile(path.string().c_str()) < 0) {
				logger::info("[settings] {} INI not found at '{}'"sv, label, path.string());
				return false;
			}
			ForEachField(v, [&](const char* sec, const char* key, auto& a) {
				using T = AtomicValue<decltype(a)>;
				if constexpr (std::is_same_v<T, bool>) {
					a.store(ini.GetBoolValue(sec, key, a.load()));
				} else if constexpr (std::is_same_v<T, float>) {
					a.store(static_cast<float>(ini.GetDoubleValue(sec, key, a.load())));
				} else {
					a.store(static_cast<T>(ini.GetLongValue(sec, key, static_cast<long>(a.load()))));
				}
			});
			logger::info("[settings] loaded {} INI from '{}'"sv, label, path.string());
			return true;
		}

		bool WriteIniFrom(const SettingsValues& v, const std::filesystem::path& path, const char* label)
		{
			std::error_code ec;
			std::filesystem::create_directories(path.parent_path(), ec);

			CSimpleIniA ini;
			ini.SetUnicode();
			ini.LoadFile(path.string().c_str());  // keep unknown keys

			ForEachField(v, [&](const char* sec, const char* key, const auto& a) {
				using T = AtomicValue<decltype(a)>;
				if constexpr (std::is_same_v<T, bool>) {
					ini.SetBoolValue(sec, key, a.load());
				} else if constexpr (std::is_same_v<T, float>) {
					ini.SetDoubleValue(sec, key, a.load());
				} else {
					ini.SetLongValue(sec, key, static_cast<long>(a.load()));
				}
			});

			if (ini.SaveFile(path.string().c_str()) < 0) {
				logger::error("[settings] {} SaveFile failed, path='{}'"sv, label, path.string());
				return false;
			}
			logger::info("[settings] wrote {} INI to '{}'"sv, label, path.string());
			return true;
		}
	}

	Settings* Settings::GetSingleton()
	{
		static Settings instance;
		return &instance;
	}

	void Settings::ResetToDefaults()
	{
		v_.keyboardKey.store(35);  // DIK H
		v_.gamepadKey.store(0);    // none
		v_.showPrompt.store(true);
		v_.engageDistance.store(512.0f);

		v_.autoReleaseWeapon.store(true);
		v_.autoReleaseCombat.store(false);
		v_.autoReleaseExtDist.store(false);
		v_.extDistThreshold.store(500.0f);
		v_.extDistDuration.store(3.0f);

		v_.offsetRight.store(0.0f);
		v_.offsetBack.store(-30.0f);
		v_.followRadius.store(30.0f);
		v_.catchupRadius.store(60.0f);

		v_.tetherIdealDist.store(40.0f);
		v_.tetherSlackDist.store(120.0f);
		v_.playerSpeedMultNear.store(1.0f);
		v_.playerSpeedMultFar.store(0.35f);
		v_.followerSpeedMult.store(1.30f);
		v_.palmOffset.store(5.0f);

		v_.gripDelay.store(1.0f);
	}

	void Settings::Load()
	{
		ResetToDefaults();
		const bool hadDefaults = ReadIniInto(v_, GetDefaultsIniPath(), "author-defaults");
		const bool hadUser     = ReadIniInto(v_, GetUserIniPath(), "user");
		if (!hadUser) {
			Save();
		}
		ForEachField(v_, [](const char* sec, const char* key, const auto& a) {
			logger::info("[settings] {}.{} = {}"sv, sec, key, a.load());
		});
		logger::info("[settings] final (authorDefaults={}, userIni={})"sv, hadDefaults, hadUser);
	}

	void Settings::Save() const
	{
		WriteIniFrom(v_, GetUserIniPath(), "user");
	}

	void Settings::ExportDefaults() const
	{
		WriteIniFrom(v_, GetDefaultsIniPath(), "author-defaults");
	}

	bool Settings::RestoreDefaults()
	{
		ResetToDefaults();
		const bool ok = ReadIniInto(v_, GetDefaultsIniPath(), "restore-defaults");
		if (!ok) {
			logger::warn("[settings] RestoreDefaults: TETHER_defaults.ini not found, using hardcoded values only"sv);
		}
		Save();
		return ok;
	}
}
