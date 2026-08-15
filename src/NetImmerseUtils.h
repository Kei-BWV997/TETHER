#pragma once

// Memory-safety helpers for NiObject bone pointers.
// Techniques adapted from asdt123123's hdtSMP64-dev (BSD-2-Clause / Faster HDT-SMP):
//   - isValidNiObject(): vtable-sanity check to reject garbage / dangling pointers
//   - IsSkeletonAttachedToScene(): 3-level parent-chain probe (per Skeleton::isActiveInScene)
// The comment/reasoning below is asdt's — kept verbatim as attribution.

namespace TETHER::NiSafe
{
	// Returns true if obj looks like a valid NiObject safe to call virtuals on.
	// VR's NiStream can leave bones[] slots as null or as raw char* pointers for
	// unresolved bone references. A char* used as a pointer has its first 8 bytes
	// interpreted as a vtable — those bytes are ASCII text, giving a non-canonical
	// address (> 0x7FFFFFFFFFFF) that faults on access. We also guard against null
	// vtable slots (stub objects from unknown NIF block types).
	// NiObject vtable layout: [0]=~NiRefObject, [1]=DeleteThis, [2]=GetRTTI, [3]=AsNode
	static constexpr std::uintptr_t kCanonicalUserSpaceMax = 0x00007FFFFFFFFFFFull;

	inline bool IsValidNiObject(const RE::NiAVObject* obj)
	{
		if (!obj)
			return false;
		if (reinterpret_cast<std::uintptr_t>(obj) > kCanonicalUserSpaceMax)
			return false;
		auto vtbl = *reinterpret_cast<void* const* const*>(obj);
		if (!vtbl || reinterpret_cast<std::uintptr_t>(vtbl) > kCanonicalUserSpaceMax)
			return false;
		return vtbl[3] != nullptr;  // slot 3 = AsNode
	}

	// Returns true if the given skeleton root is still attached to the world scene.
	// When entering/exiting an interior, NPCs are detached from the scene but not
	// unloaded, so a plain non-null check isn't enough — we need to verify the
	// scene-graph chain still goes up two more levels.
	inline bool IsSkeletonAttachedToScene(const RE::NiAVObject* skeletonRoot)
	{
		return skeletonRoot &&
		       skeletonRoot->parent &&
		       skeletonRoot->parent->parent &&
		       skeletonRoot->parent->parent->parent;
	}

	// Walk the bone's parent chain up. Returns true if `expectedRoot` is somewhere
	// in the ancestry. Detects orphaned bones that are still alive (held by our
	// NiPointer) but no longer belong to the current actor skeleton — e.g., after a
	// 3D reload from an armor swap.
	inline bool IsBoneUnderRoot(const RE::NiAVObject* bone, const RE::NiAVObject* expectedRoot)
	{
		if (!bone || !expectedRoot) return false;
		const RE::NiAVObject* cur = bone;
		for (int guard = 0; guard < 64 && cur; ++guard) {  // guard against pathological cycles
			if (cur == expectedRoot) return true;
			cur = cur->parent;
		}
		return false;
	}
}
