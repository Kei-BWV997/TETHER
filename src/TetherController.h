#pragma once

#include <atomic>

namespace TETHER
{
	class TetherController
	{
	public:
		// Off → Engaging (hotkey; Enter clip plays while still fully animated, no
		// ragdoll yet) → Held (ragdoll + Havok constraint applied).
		enum class State : std::uint8_t
		{
			Off,
			Engaging,
			Held
		};

		static TetherController* GetSingleton();

		void  TryEngage();      // SkyPrompt 'Hold hands' accepted (game thread)
		void  ReleaseByUser();  // release key pressed while Engaging/Held
		void  OnUpdate(RE::PlayerCharacter* a_player, float a_delta);
		void  ForceRelease();  // Emergency cleanup (cell change, kPreLoadGame, etc.)
		State GetState() const { return state_; }

		// For OAR custom conditions: which role (if any) does `a_actor` play in the
		// current tether? Returns true if the plugin is engaged AND the given actor
		// matches the role.
		bool  IsTetheredPlayer(RE::TESObjectREFR* a_actor) const;
		bool  IsTetheredFollower(RE::TESObjectREFR* a_actor) const;
		bool  IsEngagingPlayer(RE::TESObjectREFR* a_actor) const;
		bool  IsEngagingFollower(RE::TESObjectREFR* a_actor) const;

		// Called from OAR condition evaluation (may run off the main thread) when the
		// Enter sub-mod's condition evaluates true, i.e. OAR is selecting the Enter clip.
		void  MarkEnterSelected(bool a_player) const;

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
			// NiPointer (ref-counted) instead of raw pointers to prevent dangling
			// access if the actor's 3D is torn down while tethered. Per asdt123123's
			// review — see NetImmerseUtils.h for full rationale.
			// Even with NiPointer, the object can become an "orphan zombie"
			// (alive but detached from the current skeleton), so per-frame skeleton
			// health checks are still required (see OnUpdate).

			// Follower right arm chain
			RE::NiPointer<RE::NiAVObject> fUpper;
			RE::NiPointer<RE::NiAVObject> fFore;
			RE::NiPointer<RE::NiAVObject> fHand;
			float                         fL1 = 0.0f;
			float                         fL2 = 0.0f;

			// Player left arm chain
			RE::NiPointer<RE::NiAVObject> pUpper;
			RE::NiPointer<RE::NiAVObject> pFore;
			RE::NiPointer<RE::NiAVObject> pHand;
			float                         pL1 = 0.0f;
			float                         pL2 = 0.0f;

			// Root 3D of each actor at engage time — used per-frame to verify our
			// cached bones are still attached to the same skeleton.
			RE::NiPointer<RE::NiAVObject> fRoot;
			RE::NiPointer<RE::NiAVObject> pRoot;

			bool                          valid = false;

			// Diagnostic state for verifying NiPointer + per-frame safety checks
			// are actually running. Reset with the rest of BoneCache each engage.
			bool                          diagOkLogged      = false;  // one-shot "all checks passed"
			float                         diagHeartbeatAcc  = 0.0f;   // seconds since last heartbeat
		};

		bool CacheBones(RE::Actor* a_follower, RE::PlayerCharacter* a_player);
		void ClearBones();

		// Returns a reason string if a load transition / 3D loss was detected, else nullptr.
		// Updates engageCell_ on harmless exterior-grid crossings.
		const char* DetectTransition(RE::PlayerCharacter* a_player, RE::Actor* a_follower);

		void UpdatePrompt(RE::PlayerCharacter* a_player);
		void UpdateEngaging(RE::PlayerCharacter* a_player, float a_delta);
		void Grip(RE::Actor* a_follower, RE::PlayerCharacter* a_player, std::string_view a_trigger);
		void CancelEngaging(std::string_view a_reason);
		void ResetEngagingTimers();

		State                 state_             = State::Off;
		RE::ActorHandle       target_;
		BoneCache             bones_;
		RE::TESObjectCELL*    engageCell_        = nullptr;
		RE::TESWorldSpace*    engageWorldspace_  = nullptr;
		float                 extremeDistSecs_   = 0.0f;

		// Engaging-phase timers (main thread only).
		float                 engagingSecs_      = 0.0f;   // since hotkey
		float                 sinceEnterSecs_    = -1.0f;  // since player's Enter clip was selected; <0 = not yet
		bool                  followerEnterLogged_ = false;

		// Set from OAR evaluation threads, consumed on the main thread.
		mutable std::atomic<bool> playerEnterSeen_{ false };
		mutable std::atomic<bool> followerEnterSeen_{ false };
	};
}
