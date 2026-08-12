#pragma once

#include "API/OpenAnimationReplacerAPI-Conditions.h"

namespace TETHER::OARConditions
{
	using namespace OAR_API::Conditions;

	// Boolean condition: this actor is the tether's PLAYER (the puller).
	class IsTetheredPlayerCondition : public Conditions::CustomCondition
	{
	public:
		constexpr static inline std::string_view CONDITION_NAME = "TETHER_IsTetheredPlayer"sv;
		IsTetheredPlayerCondition() = default;

		RE::BSString GetName() const override { return CONDITION_NAME.data(); }
		RE::BSString GetDescription() const override
		{
			return "True when this actor is the player and the TETHER hand-hold is currently engaged."sv.data();
		}
		constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }
		RE::BSString GetArgument() const override    { return ""sv.data(); }
		RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_cg, void* a_sm) const override;
	};

	// Boolean condition: this actor is the tether's FOLLOWER (the pulled).
	class IsTetheredFollowerCondition : public Conditions::CustomCondition
	{
	public:
		constexpr static inline std::string_view CONDITION_NAME = "TETHER_IsTetheredFollower"sv;
		IsTetheredFollowerCondition() = default;

		RE::BSString GetName() const override { return CONDITION_NAME.data(); }
		RE::BSString GetDescription() const override
		{
			return "True when this actor is the follower currently held by TETHER."sv.data();
		}
		constexpr REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }
		RE::BSString GetArgument() const override    { return ""sv.data(); }
		RE::BSString GetCurrent(RE::TESObjectREFR* a_refr) const override;

	protected:
		bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_cg, void* a_sm) const override;
	};

	// Register both conditions with OAR. Call from SKSE kPostLoad handler.
	void Register();
}
