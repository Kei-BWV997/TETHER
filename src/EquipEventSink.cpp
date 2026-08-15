#include "PCH.h"
#include "EquipEventSink.h"
#include "TetherController.h"

namespace TETHER
{
	EquipEventSink* EquipEventSink::GetSingleton()
	{
		static EquipEventSink instance;
		return &instance;
	}

	void EquipEventSink::Register()
	{
		auto* src = RE::ScriptEventSourceHolder::GetSingleton();
		if (!src) {
			logger::error("EquipEventSink: ScriptEventSourceHolder unavailable"sv);
			return;
		}
		src->AddEventSink<RE::TESEquipEvent>(GetSingleton());
		logger::info("EquipEventSink registered"sv);
	}

	RE::BSEventNotifyControl EquipEventSink::ProcessEvent(const RE::TESEquipEvent* a_event,
		RE::BSTEventSource<RE::TESEquipEvent>*)
	{
		if (!a_event || !a_event->actor) {
			return RE::BSEventNotifyControl::kContinue;
		}
		auto* ctl = TetherController::GetSingleton();
		if (ctl->GetState() != TetherController::State::Held) {
			return RE::BSEventNotifyControl::kContinue;
		}
		auto* actor = a_event->actor.get();
		if (!actor) {
			return RE::BSEventNotifyControl::kContinue;
		}
		if (ctl->IsTetheredPlayer(actor) || ctl->IsTetheredFollower(actor)) {
			logger::info("Auto-release: equipment change on tethered actor"sv);
			ctl->ForceRelease();
		}
		return RE::BSEventNotifyControl::kContinue;
	}
}
