#include "PCH.h"
#include "SpeedHook.h"
#include "TetherController.h"

namespace TETHER
{
	void SpeedHook::Install()
	{
		auto& trampoline = SKSE::GetTrampoline();
		REL::Relocation<std::uintptr_t> hookSite{ REL::RelocationID(37013, 37943), REL::Relocate(0x1A, 0x51) };
		_original = trampoline.write_call<5>(hookSite.address(), Thunk);
		logger::info("SpeedHook installed (SetMaximumMovementSpeed)"sv);
	}

	float SpeedHook::Thunk(RE::Actor* a_actor)
	{
		const float original = _original(a_actor);
		return original * TetherController::GetSingleton()->GetSpeedMultiplierFor(a_actor);
	}
}
