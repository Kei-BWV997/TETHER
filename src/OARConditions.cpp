#include "PCH.h"
#include "OARConditions.h"
#include "TetherController.h"

namespace TETHER::OARConditions
{
	bool IsTetheredPlayerCondition::EvaluateImpl(RE::TESObjectREFR* a_refr,
		RE::hkbClipGenerator* /*a_cg*/, void* /*a_sm*/) const
	{
		return TetherController::GetSingleton()->IsTetheredPlayer(a_refr);
	}

	RE::BSString IsTetheredPlayerCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
	{
		return TetherController::GetSingleton()->IsTetheredPlayer(a_refr) ? "true"sv.data() : "false"sv.data();
	}

	bool IsTetheredFollowerCondition::EvaluateImpl(RE::TESObjectREFR* a_refr,
		RE::hkbClipGenerator* /*a_cg*/, void* /*a_sm*/) const
	{
		return TetherController::GetSingleton()->IsTetheredFollower(a_refr);
	}

	RE::BSString IsTetheredFollowerCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
	{
		return TetherController::GetSingleton()->IsTetheredFollower(a_refr) ? "true"sv.data() : "false"sv.data();
	}

	void Register()
	{
		using namespace OAR_API::Conditions;

		const auto p = AddCustomCondition<IsTetheredPlayerCondition>();
		logger::info("OAR condition register (TETHER_IsTetheredPlayer): {}"sv,
			p == APIResult::OK ? "OK"sv : "FAILED"sv);

		const auto f = AddCustomCondition<IsTetheredFollowerCondition>();
		logger::info("OAR condition register (TETHER_IsTetheredFollower): {}"sv,
			f == APIResult::OK ? "OK"sv : "FAILED"sv);
	}
}
