#pragma once

namespace TETHER
{
	// Hooks Actor::SetMaximumMovementSpeed so we can multiply the engine-final max
	// movement speed while the tether is active. Modifying SpeedMult actor-value
	// alone is not enough — the engine ignores it at the point where max speed is
	// finalized. Technique borrowed from WadeInWater (TESFAN-style thunk_call hook).
	struct SpeedHook
	{
		static void Install();

	private:
		static float Thunk(RE::Actor* a_actor);
		static inline REL::Relocation<decltype(&Thunk)> _original;
	};
}
