#pragma once

namespace TETHER
{
	// Auto-releases the tether if either the player or the currently-held follower
	// changes equipment. Rationale: mid-tether equip/unequip triggers Skyrim's
	// armor-attach path which can partially rebuild the ragdoll and leave the
	// active constraint attached to a stale rigid body — visible as floating feet
	// and other physics glitches. Releasing cleanly is safer than trying to patch
	// the ragdoll state.
	class EquipEventSink final : public RE::BSTEventSink<RE::TESEquipEvent>
	{
	public:
		static EquipEventSink* GetSingleton();
		static void            Register();

		RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event,
			RE::BSTEventSource<RE::TESEquipEvent>*) override;

	private:
		EquipEventSink() = default;
		EquipEventSink(const EquipEventSink&) = delete;
		EquipEventSink(EquipEventSink&&) = delete;
		EquipEventSink& operator=(const EquipEventSink&) = delete;
		EquipEventSink& operator=(EquipEventSink&&) = delete;
	};
}
