#pragma once

namespace TETHER
{
	// Vtable hook on PlayerCharacter::Update(float) — fires once per frame per player.
	// From there we drive TetherController::OnUpdate for any per-frame tether behavior
	// (facing correction, and later: arm IK, spring/damper, L4).
	struct PlayerUpdateHook
	{
		static void Install();

	private:
		static void Thunk(RE::PlayerCharacter* a_this, float a_delta);
		static inline REL::Relocation<decltype(&Thunk)> _original;
	};
}
