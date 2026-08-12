#include "PCH.h"
#include "UpdateHook.h"
#include "TetherController.h"

namespace TETHER
{
	// Actor::Update is virtual at vtable index 0x0AD on SE/AE (see CommonLibSSE-NG
	// Actor.h). PlayerCharacter inherits Actor and overrides at the same slot.
	static constexpr std::size_t kUpdateVtableIndex = 0x0AD;

	void PlayerUpdateHook::Install()
	{
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[0] };
		_original = vtbl.write_vfunc(kUpdateVtableIndex, Thunk);
		logger::info("PlayerUpdateHook installed (vtable idx 0x{:X})"sv, kUpdateVtableIndex);
	}

	void PlayerUpdateHook::Thunk(RE::PlayerCharacter* a_this, float a_delta)
	{
		_original(a_this, a_delta);
		TetherController::GetSingleton()->OnUpdate(a_this, a_delta);
	}
}
