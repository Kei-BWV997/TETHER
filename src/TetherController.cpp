#include "PCH.h"
#include "TetherController.h"
#include "Settings.h"
#include "Prompt.h"
#include "NetImmerseUtils.h"

#include <cmath>

namespace TETHER
{
	// Non-MCM tuning (engage cone geometry — not user-facing).
	static constexpr float kFrontConeCosMin = 0.70710678f; // cos(45°) → ±45° half-cone

	// Mode selection (compile-time for Iteration A; MCM-exposed in Iteration B).
	static constexpr bool kUseKinematicArm    = false;
	static constexpr bool kUseHavokConstraint = true;

	// Grip drop (used only when kUseKinematicArm; kept for that fallback).
	static constexpr float kGripDropZ         =  25.0f;
	static constexpr float kArmStretchFactor  = 1.10f;

	// Engaging phase. ForceAddRagdollToWorld freezes the head/chest relation at
	// whatever pose is current, so the ragdoll + constraint are applied only after
	// the Enter clip has brought the body into the hand-hold pose (grip delay: Settings).
	static constexpr float kEnterWaitMax = 3.0f;  // s from engage; grip anyway if no Enter clip was selected

	// Ignore release-key presses this soon after engaging, so the same press that
	// accepted the prompt can never also count as "let go".
	static constexpr float kReleaseDebounce = 0.25f;

	TetherController* TetherController::GetSingleton()
	{
		static TetherController instance;
		return &instance;
	}

	bool TetherController::IsTetheredPlayer(RE::TESObjectREFR* a_actor) const
	{
		if (state_ != State::Held || !a_actor) return false;
		auto* pc = RE::PlayerCharacter::GetSingleton();
		return pc && a_actor == pc;
	}

	bool TetherController::IsTetheredFollower(RE::TESObjectREFR* a_actor) const
	{
		if (state_ != State::Held || !a_actor) return false;
		auto ptr = target_.get();
		if (!ptr) return false;
		return ptr.get() == a_actor;
	}

	bool TetherController::IsEngagingPlayer(RE::TESObjectREFR* a_actor) const
	{
		if (state_ != State::Engaging || !a_actor) return false;
		auto* pc = RE::PlayerCharacter::GetSingleton();
		return pc && a_actor == pc;
	}

	bool TetherController::IsEngagingFollower(RE::TESObjectREFR* a_actor) const
	{
		if (state_ != State::Engaging || !a_actor) return false;
		auto ptr = target_.get();
		if (!ptr) return false;
		return ptr.get() == a_actor;
	}

	void TetherController::MarkEnterSelected(bool a_player) const
	{
		(a_player ? playerEnterSeen_ : followerEnterSeen_).store(true, std::memory_order_relaxed);
	}

	float TetherController::GetSpeedMultiplierFor(RE::Actor* a_actor) const
	{
		if (state_ != State::Held || !a_actor) {
			return 1.0f;
		}
		auto& s = *Settings::GetSingleton();
		auto  ptr = target_.get();
		if (a_actor->IsPlayerRef()) {
			// Elastic tether: cap player speed based on distance to follower.
			const float multNear = s.PlayerSpeedMultNear();
			if (!ptr) return multNear;
			auto* follower = ptr.get();
			if (!follower) return multNear;

			const RE::NiPoint3 delta    = follower->GetPosition() - a_actor->GetPosition();
			const float        distance = delta.Length();
			const float        ideal    = s.TetherIdealDist();
			const float        slack    = s.TetherSlackDist();
			const float        span     = (std::max)(1e-3f, slack - ideal);
			const float        t        = std::clamp((distance - ideal) / span, 0.0f, 1.0f);
			return std::lerp(multNear, s.PlayerSpeedMultFar(), t);
		}
		if (ptr && ptr.get() == a_actor) {
			return s.FollowerSpeedMult();
		}
		return 1.0f;
	}

	bool TetherController::CanEngage(RE::PlayerCharacter* a_player) const
	{
		if (!a_player) {
			return false;
		}

		// Sheathed = hands are visually free (weapon is on hip/back), regardless of
		// what's in the equip slots. That's all the gate needs.
		if (auto* st = a_player->AsActorState(); !st || st->IsWeaponDrawn()) {
			logger::info("engage denied: weapon drawn"sv);
			return false;
		}
		return true;
	}

	RE::Actor* TetherController::FindTarget(RE::PlayerCharacter* a_player) const
	{
		auto* pl = RE::ProcessLists::GetSingleton();
		if (!pl) {
			return nullptr;
		}

		const auto pPos = a_player->GetPosition();
		const float yaw = a_player->data.angle.z;
		// Skyrim heading: yaw 0 → +Y (north); yaw rotates so forward = (sin, cos, 0).
		const RE::NiPoint3 forward{ std::sin(yaw), std::cos(yaw), 0.0f };

		RE::Actor* best     = nullptr;
		const float maxDist  = Settings::GetSingleton()->EngageDistance();
		float       bestDist = maxDist;

		for (auto& handle : pl->highActorHandles) {
			auto ptr = handle.get();
			if (!ptr) {
				continue;
			}
			auto* actor = ptr.get();
			if (!actor || actor == a_player) {
				continue;
			}
			if (actor->IsDead() || !actor->IsPlayerTeammate()) {
				continue;
			}

			auto        delta = actor->GetPosition() - pPos;
			const float dist  = delta.Length();
			if (dist <= 0.001f || dist > maxDist) {
				continue;
			}
			delta /= dist;
			const float dot = delta.x * forward.x + delta.y * forward.y;
			if (dot < kFrontConeCosMin) {
				continue;
			}
			if (dist < bestDist) {
				bestDist = dist;
				best     = actor;
			}
		}
		return best;
	}

	void TetherController::ApplyOffset(RE::Actor* a_follower, RE::Actor* a_player) const
	{
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm) {
			logger::error("ApplyOffset: VM unavailable"sv);
			return;
		}
		auto* policy = vm->GetObjectHandlePolicy();
		if (!policy) {
			return;
		}
		auto handle = policy->GetHandleForObject(a_follower->GetFormType(), a_follower);
		if (handle == policy->EmptyHandle()) {
			logger::error("ApplyOffset: null follower handle"sv);
			return;
		}

		auto& s = *Settings::GetSingleton();
		auto* args = RE::MakeFunctionArguments(
			std::move(a_player),
			float{ s.OffsetRight() },
			float{ s.OffsetBack() },
			float{ 0.0f },
			float{ 0.0f }, float{ 0.0f }, float{ 0.0f },
			float{ s.CatchupRadius() },
			float{ s.FollowRadius() });

		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		const bool ok = vm->DispatchMethodCall(handle, "Actor"sv, "KeepOffsetFromActor"sv, args, callback);
		logger::info("KeepOffsetFromActor dispatch: {}"sv, ok ? "OK" : "FAILED"sv);
	}

	// Papyrus dispatch helper for ObjectReference method calls with no arguments
	// (used for ForceAddRagdollToWorld / ForceRemoveRagdollFromWorld).
	static bool DispatchZeroArgRefMethod(RE::TESObjectREFR* a_ref, std::string_view a_method)
	{
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm || !a_ref) return false;
		auto* policy = vm->GetObjectHandlePolicy();
		if (!policy) return false;
		auto handle = policy->GetHandleForObject(a_ref->GetFormType(), a_ref);
		if (handle == policy->EmptyHandle()) return false;

		auto* args = RE::MakeFunctionArguments();
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		return vm->DispatchMethodCall(handle, "ObjectReference"sv, a_method, args, callback);
	}

	void TetherController::ApplyHavokConstraint(RE::Actor* a_follower, RE::Actor* a_player) const
	{
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm) {
			return;
		}
		// Prerequisite (per asdsad121 2024): both actors must have their ragdoll bodies
		// added to the physics world for the constraint to actually take effect on
		// alive/walking NPCs. This is what the wiki warning missed.
		const bool pRag = DispatchZeroArgRefMethod(a_player,   "ForceAddRagdollToWorld"sv);
		const bool fRag = DispatchZeroArgRefMethod(a_follower, "ForceAddRagdollToWorld"sv);
		logger::info("ForceAddRagdollToWorld: player={} follower={}"sv,
			pRag ? "OK" : "FAILED"sv, fRag ? "OK" : "FAILED"sv);

		// AddHavokBallAndSocketConstraint is a global (static) function on Game.
		const float palmZ = Settings::GetSingleton()->PalmOffset();
		auto* args = RE::MakeFunctionArguments(
			static_cast<RE::TESObjectREFR*>(a_player),
			RE::BSFixedString{ "NPC L Hand [LHnd]"sv },
			static_cast<RE::TESObjectREFR*>(a_follower),
			RE::BSFixedString{ "NPC R Hand [RHnd]"sv },
			float{ 0.0f }, float{ 0.0f }, float{ palmZ },
			float{ 0.0f }, float{ 0.0f }, float{ palmZ });

		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		const bool ok = vm->DispatchStaticCall("Game"sv, "AddHavokBallAndSocketConstraint"sv, args, callback);
		logger::info("AddHavokBallAndSocketConstraint dispatch: {}"sv, ok ? "OK" : "FAILED"sv);
	}

	// Toggle NPC/actor head tracking. Called on the PLAYER only during tether —
	// stops the engine-side "head follows camera pitch" behavior that overrides
	// the authored animation's head pose in third-person. Follower head tracking
	// is left untouched so NPCs continue to look around normally.
	static void SetHeadTracking(RE::Actor* a_actor, bool a_enable)
	{
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm || !a_actor) return;
		auto* policy = vm->GetObjectHandlePolicy();
		if (!policy) return;
		auto handle = policy->GetHandleForObject(a_actor->GetFormType(), a_actor);
		if (handle == policy->EmptyHandle()) return;

		auto* args = RE::MakeFunctionArguments(std::move(a_enable));
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
		vm->DispatchMethodCall(handle, "Actor"sv, "SetHeadTracking"sv, args, cb);
	}

	void TetherController::ReleaseHavokConstraint(RE::Actor* a_follower, RE::Actor* a_player) const
	{
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm) return;

		// Step 1: explicit constraint removal so subsequent AddHavokBallAndSocketConstraint
		// on re-engage isn't fighting a stale attachment (fixes 2nd-engage no-grip bug).
		{
			auto* args = RE::MakeFunctionArguments(
				static_cast<RE::TESObjectREFR*>(a_player),
				RE::BSFixedString{ "NPC L Hand [LHnd]"sv },
				static_cast<RE::TESObjectREFR*>(a_follower),
				RE::BSFixedString{ "NPC R Hand [RHnd]"sv });
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
			const bool ok = vm->DispatchStaticCall("Game"sv, "RemoveHavokConstraints"sv, args, cb);
			logger::info("Game.RemoveHavokConstraints dispatch: {}"sv, ok ? "OK" : "FAILED"sv);
		}

		// Step 2: leave the ragdoll physics world.
		const bool pOk = DispatchZeroArgRefMethod(a_player,   "ForceRemoveRagdollFromWorld"sv);
		const bool fOk = DispatchZeroArgRefMethod(a_follower, "ForceRemoveRagdollFromWorld"sv);
		logger::info("ForceRemoveRagdollFromWorld: player={} follower={}"sv,
			pOk ? "OK" : "FAILED"sv, fOk ? "OK" : "FAILED"sv);
	}

	void TetherController::ClearOffset(RE::Actor* a_follower) const
	{
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm) {
			return;
		}
		auto* policy = vm->GetObjectHandlePolicy();
		if (!policy) {
			return;
		}
		auto handle = policy->GetHandleForObject(a_follower->GetFormType(), a_follower);
		if (handle == policy->EmptyHandle()) {
			return;
		}

		auto* args = RE::MakeFunctionArguments();
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		const bool ok = vm->DispatchMethodCall(handle, "Actor"sv, "ClearKeepOffsetFromActor"sv, args, callback);
		logger::info("ClearKeepOffsetFromActor dispatch: {}"sv, ok ? "OK" : "FAILED"sv);
	}

	// Skyrim standard bone names (XPMSSE / vanilla skeleton). Forearm suffix
	// confirmed by 5c-A diagnostic = [RLar] / [LLar] on this rig.
	static constexpr auto kNodeRUpperArm = "NPC R UpperArm [RUar]"sv;
	static constexpr auto kNodeRForearm  = "NPC R Forearm [RLar]"sv;
	static constexpr auto kNodeRHand     = "NPC R Hand [RHnd]"sv;
	static constexpr auto kNodeLUpperArm = "NPC L UpperArm [LUar]"sv;
	static constexpr auto kNodeLForearm  = "NPC L Forearm [LLar]"sv;
	static constexpr auto kNodeLHand     = "NPC L Hand [LHnd]"sv;

	// Build a rotation matrix whose bone-forward axis (local +Z, empirically confirmed
	// by axis-probe diagnostic on this rig) points along `forward` in world space.
	// `poleWorld` defines the bend / roll direction (mapped to local +Y).
	// Column layout: col0 = X (right), col1 = Y (up/pole), col2 = Z (forward).
	// Right-hand rule: X × Y = Z ⇒ X = Y × Z ⇒ R = P × F.
	static RE::NiMatrix3 MakeAimMatrix(RE::NiPoint3 forward, RE::NiPoint3 poleWorld)
	{
		float fl = forward.Unitize();
		if (fl < 1e-5f) return RE::NiMatrix3{};

		// Perpendicularize pole to forward (Y stays in the plane orthogonal to Z).
		RE::NiPoint3 P = poleWorld - forward * poleWorld.Dot(forward);
		float pl = P.Unitize();
		if (pl < 1e-5f) {
			// Degenerate: pole parallel to forward — pick any perpendicular.
			P = (std::abs(forward.z) < 0.9f) ? RE::NiPoint3{ 0, 0, 1 } : RE::NiPoint3{ 1, 0, 0 };
			P = P - forward * P.Dot(forward);
			P.Unitize();
		}
		RE::NiPoint3 R = P.Cross(forward);  // X = Y × Z
		R.Unitize();

		RE::NiMatrix3 m;
		m.entry[0][0] = R.x; m.entry[0][1] = P.x; m.entry[0][2] = forward.x;
		m.entry[1][0] = R.y; m.entry[1][1] = P.y; m.entry[1][2] = forward.y;
		m.entry[2][0] = R.z; m.entry[2][1] = P.z; m.entry[2][2] = forward.z;
		return m;
	}

	// Two-bone IK analytical solver: return elbow world position.
	static RE::NiPoint3 SolveElbow(const RE::NiPoint3& S, const RE::NiPoint3& T,
		float L1, float L2, RE::NiPoint3 poleWorld)
	{
		RE::NiPoint3 ST = T - S;
		float d = ST.Length();
		if (d < 1e-4f) return S;

		float maxD = L1 + L2 - 0.001f;
		if (d > maxD) d = maxD;  // caller clamps target position separately if needed
		RE::NiPoint3 dirST = ST * (1.0f / d);

		// Cosine rule: angle at shoulder between UpperArm and line S→T.
		float cosA = (L1 * L1 + d * d - L2 * L2) / (2.0f * L1 * d);
		cosA = std::clamp(cosA, -1.0f, 1.0f);
		float sinA = std::sqrt((std::max)(0.0f, 1.0f - cosA * cosA));

		// Pole-perp = the plane direction where elbow sits.
		RE::NiPoint3 P = poleWorld - dirST * poleWorld.Dot(dirST);
		float pl = P.Unitize();
		if (pl < 1e-5f) {
			P = (std::abs(dirST.z) < 0.9f) ? RE::NiPoint3{ 0, 0, 1 } : RE::NiPoint3{ 1, 0, 0 };
			P = P - dirST * P.Dot(dirST);
			P.Unitize();
		}
		return S + dirST * (L1 * cosA) + P * (L1 * sinA);
	}

	// Write a bone's local rotation from a desired world rotation, using its parent's
	// current world rotation.
	static void WriteBoneWorldRotation(RE::NiAVObject* bone, const RE::NiMatrix3& worldRot)
	{
		if (!bone) return;
		if (auto* parent = bone->parent) {
			bone->local.rotate = parent->world.rotate.Transpose() * worldRot;
		} else {
			bone->local.rotate = worldRot;
		}
	}

	// Solve two-bone IK on an arm chain (upper + fore) so the wrist reaches `target`.
	// The upperarm's `S` (shoulder world position) is fixed; only the two rotations
	// are written. Uses `pole` as the elbow-bend plane hint.
	// Stretch policy: if target beyond reach, extend up to armLen * kArmStretchFactor
	// then clamp target to that boundary.
	static void SolveArmIK(RE::NiAVObject* upper, RE::NiAVObject* fore,
		float L1, float L2,
		const RE::NiPoint3& S, RE::NiPoint3 target,
		const RE::NiPoint3& pole)
	{
		if (!upper || !fore) return;

		const float armLen  = L1 + L2;
		const float maxLen  = armLen * kArmStretchFactor;
		RE::NiPoint3 ST     = target - S;
		float        d      = ST.Length();
		if (d < 1e-3f) return;
		if (d > maxLen) {
			target = S + ST * (maxLen / d);
		}

		const RE::NiPoint3 E = SolveElbow(S, target, L1, L2, pole);

		RE::NiPoint3 upDir = E - S;
		upDir.Unitize();
		const RE::NiMatrix3 upperWorldRot = MakeAimMatrix(upDir, pole);
		WriteBoneWorldRotation(upper, upperWorldRot);

		RE::NiPoint3 foreDir = target - E;
		foreDir.Unitize();
		const RE::NiMatrix3 foreWorldRot = MakeAimMatrix(foreDir, pole);
		fore->local.rotate = upperWorldRot.Transpose() * foreWorldRot;
	}

	static void DumpNodesContaining(RE::NiAVObject* a_root, std::string_view a_needle)
	{
		if (!a_root) return;
		std::string name{ a_root->name.c_str() };
		std::string lname = name;
		std::string needle{ a_needle };
		auto lower = [](std::string& s) { for (auto& c : s) c = static_cast<char>(std::tolower(c)); };
		lower(lname); lower(needle);
		if (lname.find(needle) != std::string::npos) {
			logger::info("  candidate: '{}'"sv, name);
		}
		if (auto* node = a_root->AsNode(); node) {
			auto& kids = node->GetChildren();
			for (std::uint32_t i = 0; i < kids.size(); ++i) {
				DumpNodesContaining(kids[i].get(), a_needle);
			}
		}
	}

	static float WorldDistance(const RE::NiAVObject* a, const RE::NiAVObject* b)
	{
		if (!a || !b) return 0.0f;
		return (a->world.translate - b->world.translate).Length();
	}

	bool TetherController::CacheBones(RE::Actor* a_follower, RE::PlayerCharacter* a_player)
	{
		bones_ = BoneCache{};
		if (!a_follower || !a_player) {
			return false;
		}
		auto* f3d = a_follower->Get3D();
		auto* p3d = a_player->Get3D();
		if (!f3d || !p3d) {
			logger::error("CacheBones: 3D not loaded (follower={}, player={})"sv,
				f3d != nullptr, p3d != nullptr);
			return false;
		}

		// Wrap raw lookups into NiPointer so we hold a ref count. If any of these
		// underlying objects gets replaced by the engine (armor swap, 3D reload,
		// etc.), our NiPointer will still point at valid memory — but the object
		// will be an "orphan" no longer attached to the actor. The per-frame
		// skeleton-attached check in OnUpdate detects that case.
		bones_.fUpper.reset(f3d->GetObjectByName(kNodeRUpperArm));
		bones_.fFore .reset(f3d->GetObjectByName(kNodeRForearm));
		bones_.fHand .reset(f3d->GetObjectByName(kNodeRHand));
		bones_.pUpper.reset(p3d->GetObjectByName(kNodeLUpperArm));
		bones_.pFore .reset(p3d->GetObjectByName(kNodeLForearm));
		bones_.pHand .reset(p3d->GetObjectByName(kNodeLHand));

		if (!bones_.fUpper || !bones_.fFore || !bones_.fHand ||
			!bones_.pUpper || !bones_.pFore || !bones_.pHand) {
			logger::error("CacheBones: missing nodes (fUpper={}, fFore={}, fHand={}, pUpper={}, pFore={}, pHand={})"sv,
				bones_.fUpper != nullptr, bones_.fFore != nullptr, bones_.fHand != nullptr,
				bones_.pUpper != nullptr, bones_.pFore != nullptr, bones_.pHand != nullptr);
			return false;
		}

		// Cache the root 3D too so we can verify per-frame that bones are still
		// under the current skeleton.
		bones_.fRoot.reset(f3d);
		bones_.pRoot.reset(p3d);

		bones_.fL1 = WorldDistance(bones_.fUpper.get(), bones_.fFore.get());
		bones_.fL2 = WorldDistance(bones_.fFore .get(), bones_.fHand.get());
		bones_.pL1 = WorldDistance(bones_.pUpper.get(), bones_.pFore.get());
		bones_.pL2 = WorldDistance(bones_.pFore .get(), bones_.pHand.get());
		bones_.valid = true;

		logger::info("Bones cached: follower arm L1={:.1f} L2={:.1f} (reach={:.1f}u), player arm L1={:.1f} L2={:.1f} (reach={:.1f}u)"sv,
			bones_.fL1, bones_.fL2, bones_.fL1 + bones_.fL2,
			bones_.pL1, bones_.pL2, bones_.pL1 + bones_.pL2);

		// --- NiPointer safety diagnostic (per asdt123123's review) ---------------
		// Proof that we're actually holding NiPointer refs on the cached bones.
		// After NiPointer::reset(raw), the target NiRefObject's refCount is
		// incremented, so these values should all be >= 2 (engine holds 1, we hold 1;
		// higher if other systems also ref it — e.g. animation, physics).
		auto rc = [](const RE::NiRefObject* o) -> unsigned {
			return o ? static_cast<unsigned>(o->GetRefCount()) : 0u;
		};
		logger::info("NiPointer safety: refs acquired — "
			"pRoot@{} rc={}, pUpper@{} rc={}, pFore@{} rc={}, pHand@{} rc={}"sv,
			static_cast<const void*>(bones_.pRoot.get()),  rc(bones_.pRoot.get()),
			static_cast<const void*>(bones_.pUpper.get()), rc(bones_.pUpper.get()),
			static_cast<const void*>(bones_.pFore .get()), rc(bones_.pFore.get()),
			static_cast<const void*>(bones_.pHand .get()), rc(bones_.pHand.get()));
		logger::info("NiPointer safety: refs acquired — "
			"fRoot@{} rc={}, fUpper@{} rc={}, fFore@{} rc={}, fHand@{} rc={}"sv,
			static_cast<const void*>(bones_.fRoot.get()),  rc(bones_.fRoot.get()),
			static_cast<const void*>(bones_.fUpper.get()), rc(bones_.fUpper.get()),
			static_cast<const void*>(bones_.fFore .get()), rc(bones_.fFore.get()),
			static_cast<const void*>(bones_.fHand .get()), rc(bones_.fHand.get()));

		// AXIS DIAGNOSTIC: figure out which local axis is the bone-forward direction
		// on this rig. Compute (Elbow - Shoulder) in world, then dot with each column
		// of Shoulder's world.rotate. The column with dot ≈ ±1.0 is bone-forward.
		auto reportAxis = [](const char* label, RE::NiAVObject* parent, RE::NiAVObject* child) {
			if (!parent || !child) return;
			auto& wr = parent->world.rotate;
			const RE::NiPoint3 c0{ wr.entry[0][0], wr.entry[1][0], wr.entry[2][0] };
			const RE::NiPoint3 c1{ wr.entry[0][1], wr.entry[1][1], wr.entry[2][1] };
			const RE::NiPoint3 c2{ wr.entry[0][2], wr.entry[1][2], wr.entry[2][2] };
			RE::NiPoint3 d = child->world.translate - parent->world.translate;
			d.Unitize();
			logger::info("  [{}] boneDir=({:.2f},{:.2f},{:.2f}) dots: X={:+.2f} Y={:+.2f} Z={:+.2f}"sv,
				label, d.x, d.y, d.z, c0.Dot(d), c1.Dot(d), c2.Dot(d));
		};
		logger::info("Axis probe (identify which local axis is bone-forward):"sv);
		reportAxis("pUpper", bones_.pUpper.get(), bones_.pFore.get());
		reportAxis("pFore",  bones_.pFore .get(), bones_.pHand.get());
		reportAxis("fUpper", bones_.fUpper.get(), bones_.fFore.get());
		reportAxis("fFore",  bones_.fFore .get(), bones_.fHand.get());
		return true;
	}

	void TetherController::ClearBones()
	{
		if (bones_.valid) {
			auto rc = [](const RE::NiRefObject* o) -> unsigned {
				return o ? static_cast<unsigned>(o->GetRefCount()) : 0u;
			};
			logger::info("NiPointer safety: releasing refs — "
				"pHand rc(before drop)={}, fHand rc(before drop)={}, "
				"pRoot rc(before drop)={}, fRoot rc(before drop)={}"sv,
				rc(bones_.pHand.get()), rc(bones_.fHand.get()),
				rc(bones_.pRoot.get()), rc(bones_.fRoot.get()));
		}
		bones_ = BoneCache{};  // NiPointer destructors run here → refCount decremented
	}

	const char* TetherController::DetectTransition(RE::PlayerCharacter* a_player, RE::Actor* a_follower)
	{
		// Skyrim exterior worldspaces are divided into 4096u grid cells; walking across
		// an exterior boundary changes `parentCell` without invalidating 3D or ragdoll
		// state. Only real load transitions (interior door, worldspace change) tear the
		// actor's 3D down. Detect those specifically.
		auto* curWorld = a_player->GetWorldspace();
		auto* curCell  = a_player->parentCell;
		if (curWorld != engageWorldspace_) {
			return "worldspace change";
		}
		if (curCell != engageCell_) {
			const bool wasInterior = engageCell_ && engageCell_->IsInteriorCell();
			const bool nowInterior = curCell && curCell->IsInteriorCell();
			if (wasInterior || nowInterior) {
				return "interior boundary crossed";
			}
			engageCell_ = curCell;
		}
		if (!a_player->Get3D() || !a_follower->Get3D()) {
			return "3D unavailable (transition)";
		}
		return nullptr;
	}

	void TetherController::OnUpdate(RE::PlayerCharacter* a_player, float a_delta)
	{
		if (!a_player) {
			return;
		}
		if (state_ == State::Off) {
			UpdatePrompt(a_player);
			return;
		}
		if (state_ == State::Engaging) {
			UpdateEngaging(a_player, a_delta);
			return;
		}
		if (state_ != State::Held) {
			return;
		}
		auto ptr = target_.get();
		if (!ptr) {
			ForceRelease();
			return;
		}
		auto* follower = ptr.get();
		if (!follower) {
			ForceRelease();
			return;
		}

		if (const char* reason = DetectTransition(a_player, follower)) {
			logger::warn("ForceRelease: {}"sv, reason);
			ForceRelease();
			return;
		}
		auto* p3d = a_player->Get3D();
		auto* f3d = follower->Get3D();

		// Per-frame skeleton-health check (per asdt123123's review):
		// (1) Cached NiPointers must still hold valid NiObjects (vtable sanity).
		// (2) Skeleton root must still be attached to the world scene (3 levels up).
		// (3) Cached bones must still be under the CURRENT skeleton root — if the
		//     actor's 3D was replaced (armor swap, etc.), our cached bones become
		//     "orphan zombies": alive via NiPointer ref-count but no longer part of
		//     the visible skeleton. Bail cleanly.
		using namespace NiSafe;
		if (bones_.valid) {
			// Skeletons must be attached to the world scene AND match what we cached.
			if (!IsSkeletonAttachedToScene(p3d) || !IsSkeletonAttachedToScene(f3d) ||
				p3d != bones_.pRoot.get() || f3d != bones_.fRoot.get()) {
				logger::warn("ForceRelease: skeleton replaced or detached from scene"sv);
				ForceRelease();
				return;
			}
			// Every cached bone must pass vtable sanity AND be a descendant of its root.
			const RE::NiAVObject* bones[]  = {
				bones_.pUpper.get(), bones_.pFore.get(), bones_.pHand.get(),
				bones_.fUpper.get(), bones_.fFore.get(), bones_.fHand.get(),
			};
			const RE::NiAVObject* roots[] = { p3d, p3d, p3d, f3d, f3d, f3d };
			for (size_t i = 0; i < 6; ++i) {
				if (!IsValidNiObject(bones[i]) || !IsBoneUnderRoot(bones[i], roots[i])) {
					logger::warn("ForceRelease: cached bone orphaned or invalid (index {})"sv, i);
					ForceRelease();
					return;
				}
			}

			// --- Positive proof the safety checks ran successfully -------------------
			// One-shot: first frame after CacheBones where all checks passed.
			if (!bones_.diagOkLogged) {
				bones_.diagOkLogged = true;
				logger::info("NiPointer safety: first per-frame check PASSED — "
					"IsSkeletonAttachedToScene(p/f)=1/1, root match(p/f)=1/1, "
					"6/6 bones IsValidNiObject & IsBoneUnderRoot"sv);
			}
			// Heartbeat every ~2s so the log shows the checks kept running.
			bones_.diagHeartbeatAcc += a_delta;
			if (bones_.diagHeartbeatAcc >= 2.0f) {
				bones_.diagHeartbeatAcc = 0.0f;
				logger::info("NiPointer safety: heartbeat OK (2s) — "
					"pHand rc={}, fHand rc={}"sv,
					bones_.pHand ? static_cast<unsigned>(bones_.pHand->GetRefCount()) : 0u,
					bones_.fHand ? static_cast<unsigned>(bones_.fHand->GetRefCount()) : 0u);
			}
		}

		// --- Auto-release triggers (all MCM-toggleable) ------------------------------
		auto& s = *Settings::GetSingleton();
		if (s.AutoReleaseWeapon()) {
			if (auto* st = a_player->AsActorState(); st && st->IsWeaponDrawn()) {
				logger::info("Auto-release: player weapon drawn"sv);
				ForceRelease();
				return;
			}
		}
		if (s.AutoReleaseCombat()) {
			if (a_player->IsInCombat() || follower->IsInCombat()) {
				logger::info("Auto-release: combat state entered"sv);
				ForceRelease();
				return;
			}
		}
		if (s.AutoReleaseExtDist()) {
			const float dist   = (follower->GetPosition() - a_player->GetPosition()).Length();
			const float thresh = s.ExtDistThreshold();
			const float dur    = s.ExtDistDuration();
			if (dist > thresh) {
				extremeDistSecs_ += a_delta;
				if (extremeDistSecs_ >= dur) {
					logger::info("Auto-release: extreme distance sustained ({:.0f}u > {:.0f}u for {:.1f}s)"sv,
						dist, thresh, extremeDistSecs_);
					ForceRelease();
					return;
				}
			} else {
				extremeDistSecs_ = 0.0f;
			}
		} else {
			extremeDistSecs_ = 0.0f;
		}
		// -----------------------------------------------------------------------------

		// Facing correction: force follower's yaw to match player's every frame.
		const float yaw = a_player->data.angle.z;
		follower->SetAngle({ 0.0f, 0.0f, yaw });

		if (!kUseKinematicArm) {
			return;
		}
		if (!bones_.valid) {
			return;
		}

		// Initial grip point = midpoint of the two shoulders, lowered to waist height.
		// This is the AESTHETIC target for the player arm — where the player would
		// naturally position their hand for a hand-hold. The follower then locks
		// exactly onto the player's resulting hand position (see below).
		const RE::NiPoint3 pShoulder = bones_.pUpper->world.translate;
		const RE::NiPoint3 fShoulder = bones_.fUpper->world.translate;
		RE::NiPoint3       grip      = (pShoulder + fShoulder) * 0.5f;
		grip.z -= kGripDropZ;

		// Pole vectors: elbow bends toward each actor's outside (away from body).
		const RE::NiPoint3 leftDir  { -std::cos(yaw),  std::sin(yaw), 0.0f };
		const RE::NiPoint3 rightDir {  std::cos(yaw), -std::sin(yaw), 0.0f };

		// Step 1: solve player left arm IK to grip.
		SolveArmIK(bones_.pUpper.get(), bones_.pFore.get(), bones_.pL1, bones_.pL2, pShoulder, grip, leftDir);

		// Step 2: propagate player arm world transforms IMMEDIATELY so the player's
		// hand world position reflects our IK write before we compute follower target.
		RE::NiUpdateData ud{};
		bones_.pUpper->Update(ud);

		// Step 3: read player's actual hand world position (post-IK). This is the
		// "grip authority" — follower will lock exactly onto this point, giving a
		// rigid handshake without drift between the two hands.
		const RE::NiPoint3 pHandWorld = bones_.pHand->world.translate;

		// Step 4: solve follower right arm IK targeting player's actual hand position.
		SolveArmIK(bones_.fUpper.get(), bones_.fFore.get(), bones_.fL1, bones_.fL2, fShoulder, pHandWorld, rightDir);
		bones_.fUpper->Update(ud);
	}

	void TetherController::UpdatePrompt(RE::PlayerCharacter* a_player)
	{
		if (!Prompt::IsAvailable()) {
			return;
		}
		if (!Settings::GetSingleton()->ShowPrompt()) {
			Prompt::Hide();  // no-op unless it was showing when the option was turned off
			return;
		}
		RE::Actor* candidate = nullptr;
		if (auto* st = a_player->AsActorState(); st && !st->IsWeaponDrawn()) {
			candidate = FindTarget(a_player);
		}
		if (candidate) {
			Prompt::Show(candidate);
		} else {
			Prompt::Hide();
		}
	}

	void TetherController::TryEngage()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || state_ != State::Off) {
			return;
		}
		Prompt::Hide();
		if (!CanEngage(player)) {
			return;
		}
		auto* target = FindTarget(player);
		if (!target) {
			logger::info("engage denied: no eligible follower in front cone (<= {:.0f} units)"sv,
				Settings::GetSingleton()->EngageDistance());
			return;
		}
		target_           = target->GetHandle();
		engageCell_       = player->parentCell;
		engageWorldspace_ = player->GetWorldspace();
		extremeDistSecs_  = 0.0f;
		ResetEngagingTimers();
		ApplyOffset(target, player);
		state_ = State::Engaging;
		logger::info("engaging: target='{}' (formID=0x{:08X}) — waiting for Enter clip (grip {:.2f}s after Enter, fallback {:.1f}s after engage)"sv,
			target->GetName(), target->GetFormID(), Settings::GetSingleton()->GripDelay(), kEnterWaitMax);
	}

	void TetherController::ReleaseByUser()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		if (state_ == State::Engaging) {
			if (engagingSecs_ < kReleaseDebounce) {
				logger::info("release key ignored ({:.0f}ms after engage, debounce)"sv, engagingSecs_ * 1000.0f);
				return;
			}
			CancelEngaging("release key pressed"sv);
			return;
		}
		if (state_ == State::Held) {
			const char* name = "<lost>";
			RE::Actor*  actor = nullptr;
			if (auto ptr = target_.get(); ptr) {
				actor = ptr.get();
				if (actor) {
					name = actor->GetName();
				}
			}
			if (actor) {
				ClearOffset(actor);
				if (kUseHavokConstraint) {
					ReleaseHavokConstraint(actor, player);
				}
			}
			if (player) {
				SetHeadTracking(player, true);  // restore player head tracking
			}
			ClearBones();
			logger::info("released: target='{}' (offset cleared)"sv, name);
			target_           = RE::ActorHandle{};
			engageCell_       = nullptr;
			engageWorldspace_ = nullptr;
			state_            = State::Off;
		}
	}

	void TetherController::ResetEngagingTimers()
	{
		engagingSecs_        = 0.0f;
		sinceEnterSecs_      = -1.0f;
		followerEnterLogged_ = false;
		playerEnterSeen_.store(false, std::memory_order_relaxed);
		followerEnterSeen_.store(false, std::memory_order_relaxed);
	}

	void TetherController::UpdateEngaging(RE::PlayerCharacter* a_player, float a_delta)
	{
		auto  ptr      = target_.get();
		auto* follower = ptr ? ptr.get() : nullptr;
		if (!follower) {
			CancelEngaging("target lost"sv);
			return;
		}
		if (const char* reason = DetectTransition(a_player, follower)) {
			CancelEngaging(reason);
			return;
		}
		if (auto* st = a_player->AsActorState(); st && st->IsWeaponDrawn()) {
			CancelEngaging("player weapon drawn"sv);
			return;
		}

		engagingSecs_ += a_delta;

		if (sinceEnterSecs_ < 0.0f) {
			if (playerEnterSeen_.load(std::memory_order_relaxed)) {
				sinceEnterSecs_ = 0.0f;
				logger::info("engaging: OAR selected player Enter clip at +{:.0f}ms after hotkey"sv,
					engagingSecs_ * 1000.0f);
			}
		} else {
			sinceEnterSecs_ += a_delta;
		}
		if (!followerEnterLogged_ && followerEnterSeen_.load(std::memory_order_relaxed)) {
			followerEnterLogged_ = true;
			logger::info("engaging: OAR selected follower Enter clip at +{:.0f}ms after hotkey"sv,
				engagingSecs_ * 1000.0f);
		}

		if (sinceEnterSecs_ >= Settings::GetSingleton()->GripDelay()) {
			Grip(follower, a_player, "Enter clip + delay"sv);
		} else if (sinceEnterSecs_ < 0.0f && engagingSecs_ >= kEnterWaitMax) {
			Grip(follower, a_player, "fallback timeout (player Enter clip never selected)"sv);
		}
	}

	void TetherController::Grip(RE::Actor* a_follower, RE::PlayerCharacter* a_player, std::string_view a_trigger)
	{
		CacheBones(a_follower, a_player);
		if (kUseHavokConstraint) {
			ApplyHavokConstraint(a_follower, a_player);
		}
		// Stops the engine's camera-pitch head follow on the player. Follower untouched.
		SetHeadTracking(a_player, false);
		state_ = State::Held;

		auto& s = *Settings::GetSingleton();
		logger::info("engaged: target='{}' (formID=0x{:08X}) trigger='{}' at +{:.0f}ms after hotkey offset(R={:.0f} B={:.0f} follow={:.0f}/catch={:.0f}) [kinematic={} constraint={}]"sv,
			a_follower->GetName(), a_follower->GetFormID(), a_trigger, engagingSecs_ * 1000.0f,
			s.OffsetRight(), s.OffsetBack(), s.FollowRadius(), s.CatchupRadius(),
			kUseKinematicArm, kUseHavokConstraint);
	}

	void TetherController::CancelEngaging(std::string_view a_reason)
	{
		if (state_ != State::Engaging) return;

		// Nothing Havok-side was applied yet — only the follow offset needs undoing.
		if (auto ptr = target_.get(); ptr && ptr.get()) {
			ClearOffset(ptr.get());
		}
		target_           = RE::ActorHandle{};
		engageCell_       = nullptr;
		engageWorldspace_ = nullptr;
		state_            = State::Off;
		logger::info("engaging cancelled: {} (at +{:.0f}ms after hotkey)"sv, a_reason, engagingSecs_ * 1000.0f);
		ResetEngagingTimers();
	}

	void TetherController::ForceRelease()
	{
		if (state_ == State::Engaging) {
			CancelEngaging("force release"sv);
			return;
		}
		if (state_ != State::Held) return;

		auto*        player = RE::PlayerCharacter::GetSingleton();
		auto         ptr    = target_.get();
		RE::Actor*   actor  = ptr ? ptr.get() : nullptr;

		if (actor) {
			ClearOffset(actor);
			if (kUseHavokConstraint && player) {
				ReleaseHavokConstraint(actor, player);
			}
		}
		if (player) {
			SetHeadTracking(player, true);
		}
		ClearBones();
		target_           = RE::ActorHandle{};
		engageCell_       = nullptr;
		engageWorldspace_ = nullptr;
		state_            = State::Off;
		logger::warn("ForceRelease: state → Off"sv);
	}
}
