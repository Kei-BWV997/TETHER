#include "PCH.h"
#include "TetherController.h"
#include "Settings.h"

#include <cmath>

namespace TETHER
{
	// Non-MCM tuning (engage cone geometry — not user-facing).
	static constexpr float kMaxDistance     = 512.0f;    // ~7 m engage range
	static constexpr float kFrontConeCosMin = 0.70710678f; // cos(45°) → ±45° half-cone

	// Mode selection (compile-time for Iteration A; MCM-exposed in Iteration B).
	static constexpr bool kUseKinematicArm    = false;
	static constexpr bool kUseHavokConstraint = true;

	// Grip drop (used only when kUseKinematicArm; kept for that fallback).
	static constexpr float kGripDropZ         =  25.0f;
	static constexpr float kArmStretchFactor  = 1.10f;

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
		float      bestDist = kMaxDistance;

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
			if (dist <= 0.001f || dist > kMaxDistance) {
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

		bones_.fUpper = f3d->GetObjectByName(kNodeRUpperArm);
		bones_.fFore  = f3d->GetObjectByName(kNodeRForearm);
		bones_.fHand  = f3d->GetObjectByName(kNodeRHand);
		bones_.pUpper = p3d->GetObjectByName(kNodeLUpperArm);
		bones_.pFore  = p3d->GetObjectByName(kNodeLForearm);
		bones_.pHand  = p3d->GetObjectByName(kNodeLHand);

		if (!bones_.fUpper || !bones_.fFore || !bones_.fHand ||
			!bones_.pUpper || !bones_.pFore || !bones_.pHand) {
			logger::error("CacheBones: missing nodes (fUpper={}, fFore={}, fHand={}, pUpper={}, pFore={}, pHand={})"sv,
				bones_.fUpper != nullptr, bones_.fFore != nullptr, bones_.fHand != nullptr,
				bones_.pUpper != nullptr, bones_.pFore != nullptr, bones_.pHand != nullptr);
			return false;
		}

		bones_.fL1 = WorldDistance(bones_.fUpper, bones_.fFore);
		bones_.fL2 = WorldDistance(bones_.fFore, bones_.fHand);
		bones_.pL1 = WorldDistance(bones_.pUpper, bones_.pFore);
		bones_.pL2 = WorldDistance(bones_.pFore, bones_.pHand);
		bones_.valid = true;

		logger::info("Bones cached: follower arm L1={:.1f} L2={:.1f} (reach={:.1f}u), player arm L1={:.1f} L2={:.1f} (reach={:.1f}u)"sv,
			bones_.fL1, bones_.fL2, bones_.fL1 + bones_.fL2,
			bones_.pL1, bones_.pL2, bones_.pL1 + bones_.pL2);

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
		reportAxis("pUpper", bones_.pUpper, bones_.pFore);
		reportAxis("pFore",  bones_.pFore,  bones_.pHand);
		reportAxis("fUpper", bones_.fUpper, bones_.fFore);
		reportAxis("fFore",  bones_.fFore,  bones_.fHand);
		return true;
	}

	void TetherController::ClearBones()
	{
		bones_ = BoneCache{};
	}

	void TetherController::OnUpdate(RE::PlayerCharacter* a_player, float a_delta)
	{
		if (state_ != State::Held || !a_player) {
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

		// Cell transition safety. Skyrim exterior worldspaces are divided into 4096u
		// grid cells; walking across an exterior boundary changes `parentCell` without
		// invalidating 3D or ragdoll state. Only real load transitions (interior
		// door, worldspace change) tear the actor's 3D down. Detect those specifically.
		auto* curWorld = a_player->GetWorldspace();
		auto* curCell  = a_player->parentCell;
		if (curWorld != engageWorldspace_) {
			logger::warn("ForceRelease: worldspace change"sv);
			ForceRelease();
			return;
		}
		if (curCell != engageCell_) {
			const bool wasInterior = engageCell_ && engageCell_->IsInteriorCell();
			const bool nowInterior = curCell && curCell->IsInteriorCell();
			if (wasInterior || nowInterior) {
				logger::warn("ForceRelease: interior boundary crossed"sv);
				ForceRelease();
				return;
			}
			// Exterior-to-exterior in same worldspace — safe, just update tracker.
			engageCell_ = curCell;
		}
		// 3D validity fallback for anything the above didn't catch.
		if (!a_player->Get3D() || !follower->Get3D()) {
			logger::warn("ForceRelease: 3D unavailable (transition)"sv);
			ForceRelease();
			return;
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
		SolveArmIK(bones_.pUpper, bones_.pFore, bones_.pL1, bones_.pL2, pShoulder, grip, leftDir);

		// Step 2: propagate player arm world transforms IMMEDIATELY so the player's
		// hand world position reflects our IK write before we compute follower target.
		RE::NiUpdateData ud{};
		bones_.pUpper->Update(ud);

		// Step 3: read player's actual hand world position (post-IK). This is the
		// "grip authority" — follower will lock exactly onto this point, giving a
		// rigid handshake without drift between the two hands.
		const RE::NiPoint3 pHandWorld = bones_.pHand->world.translate;

		// Step 4: solve follower right arm IK targeting player's actual hand position.
		SolveArmIK(bones_.fUpper, bones_.fFore, bones_.fL1, bones_.fL2, fShoulder, pHandWorld, rightDir);
		bones_.fUpper->Update(ud);
	}

	void TetherController::Toggle()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		if (state_ == State::Off) {
			if (!CanEngage(player)) {
				return;
			}
			auto* target = FindTarget(player);
			if (!target) {
				logger::info("engage denied: no eligible follower in front cone (<= {:.0f} units)"sv, kMaxDistance);
				return;
			}
			target_           = target->GetHandle();
			state_            = State::Held;
			engageCell_       = player->parentCell;
			engageWorldspace_ = player->GetWorldspace();
			extremeDistSecs_  = 0.0f;
			ApplyOffset(target, player);
			CacheBones(target, player);
			if (kUseHavokConstraint) {
				ApplyHavokConstraint(target, player);
			}
			auto& s = *Settings::GetSingleton();
			logger::info("engaged: target='{}' (formID=0x{:08X}) offset(R={:.0f} B={:.0f} follow={:.0f}/catch={:.0f}) [kinematic={} constraint={}]"sv,
				target->GetName(), target->GetFormID(),
				s.OffsetRight(), s.OffsetBack(), s.FollowRadius(), s.CatchupRadius(),
				kUseKinematicArm, kUseHavokConstraint);
		} else {
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
			ClearBones();
			logger::info("released: target='{}' (offset cleared)"sv, name);
			target_           = RE::ActorHandle{};
			engageCell_       = nullptr;
			engageWorldspace_ = nullptr;
			state_            = State::Off;
		}
	}

	void TetherController::ForceRelease()
	{
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
		ClearBones();
		target_           = RE::ActorHandle{};
		engageCell_       = nullptr;
		engageWorldspace_ = nullptr;
		state_            = State::Off;
		logger::warn("ForceRelease: state → Off"sv);
	}
}
