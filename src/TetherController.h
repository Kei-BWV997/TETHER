#pragma once

namespace TETHER
{
	class TetherController
	{
	public:
		enum class State : std::uint8_t
		{
			Off,
			Held
		};

		static TetherController* GetSingleton();

		void  Toggle();
		void  OnUpdate(RE::PlayerCharacter* a_player, float a_delta);
		void  ForceRelease();  // Emergency cleanup (cell change, kPreLoadGame, etc.)
		State GetState() const { return state_; }

		// For OAR custom conditions: which role (if any) does `a_actor` play in the
		// current tether? Returns true if the plugin is engaged AND the given actor
		// matches the role.
		bool  IsTetheredPlayer(RE::TESObjectREFR* a_actor) const;
		bool  IsTetheredFollower(RE::TESObjectREFR* a_actor) const;

		// Multiplier applied to a_actor's max movement speed while tethered.
		// Returns 1.0 for actors not involved in the current tether.
		float GetSpeedMultiplierFor(RE::Actor* a_actor) const;

	private:
		TetherController() = default;
		TetherController(const TetherController&) = delete;
		TetherController(TetherController&&) = delete;
		TetherController& operator=(const TetherController&) = delete;
		TetherController& operator=(TetherController&&) = delete;

		bool        CanEngage(RE::PlayerCharacter* a_player) const;
		RE::Actor*  FindTarget(RE::PlayerCharacter* a_player) const;
		void        ApplyOffset(RE::Actor* a_follower, RE::Actor* a_player) const;
		void        ClearOffset(RE::Actor* a_follower) const;
		void        ApplyHavokConstraint(RE::Actor* a_follower, RE::Actor* a_player) const;
		void        ReleaseHavokConstraint(RE::Actor* a_follower, RE::Actor* a_player) const;

		struct BoneCache
		{
			// Follower right arm chain (2-bone IK, elastic reach)
			RE::NiAVObject* fUpper = nullptr;
			RE::NiAVObject* fFore  = nullptr;
			RE::NiAVObject* fHand  = nullptr;
			float           fL1    = 0.0f;
			float           fL2    = 0.0f;

			// Player left arm chain (rigid FK aim)
			RE::NiAVObject* pUpper = nullptr;
			RE::NiAVObject* pFore  = nullptr;
			RE::NiAVObject* pHand  = nullptr;
			float           pL1    = 0.0f;
			float           pL2    = 0.0f;

			bool            valid  = false;
		};

		bool CacheBones(RE::Actor* a_follower, RE::PlayerCharacter* a_player);
		void ClearBones();

		State                 state_             = State::Off;
		RE::ActorHandle       target_;
		BoneCache             bones_;
		RE::TESObjectCELL*    engageCell_        = nullptr;
		RE::TESWorldSpace*    engageWorldspace_  = nullptr;
		float                 extremeDistSecs_   = 0.0f;
	};
}
