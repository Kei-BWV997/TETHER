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

	// Evaluated true ⇒ OAR is selecting the Enter clip (the condition is listed last,
	// so the sub-mod's other conditions already passed). Record it for the grip timer.
	bool IsEngagingPlayerCondition::EvaluateImpl(RE::TESObjectREFR* a_refr,
		RE::hkbClipGenerator* /*a_cg*/, void* /*a_sm*/) const
	{
		auto* ctl = TetherController::GetSingleton();
		const bool result = ctl->IsEngagingPlayer(a_refr);
		if (result) {
			ctl->MarkEnterSelected(true);
		}
		return result;
	}

	RE::BSString IsEngagingPlayerCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
	{
		return TetherController::GetSingleton()->IsEngagingPlayer(a_refr) ? "true"sv.data() : "false"sv.data();
	}

	bool IsEngagingFollowerCondition::EvaluateImpl(RE::TESObjectREFR* a_refr,
		RE::hkbClipGenerator* /*a_cg*/, void* /*a_sm*/) const
	{
		auto* ctl = TetherController::GetSingleton();
		const bool result = ctl->IsEngagingFollower(a_refr);
		if (result) {
			ctl->MarkEnterSelected(false);
		}
		return result;
	}

	RE::BSString IsEngagingFollowerCondition::GetCurrent(RE::TESObjectREFR* a_refr) const
	{
		return TetherController::GetSingleton()->IsEngagingFollower(a_refr) ? "true"sv.data() : "false"sv.data();
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

		const auto ep = AddCustomCondition<IsEngagingPlayerCondition>();
		logger::info("OAR condition register (TETHER_IsEngagingPlayer): {}"sv,
			ep == APIResult::OK ? "OK"sv : "FAILED"sv);

		const auto ef = AddCustomCondition<IsEngagingFollowerCondition>();
		logger::info("OAR condition register (TETHER_IsEngagingFollower): {}"sv,
			ef == APIResult::OK ? "OK"sv : "FAILED"sv);
	}
}
