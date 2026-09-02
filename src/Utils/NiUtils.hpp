#pragma once

class NiUtils
{

public:

	template <class T = RE::COL_LAYER>
	static void SetObjectCollision(RE::NiAVObject* targetNode, T colLayer)
	{
		if (auto* hkpRigidBody = GetRigidBody(targetNode)) {
			auto& collisionFilterInfo = hkpRigidBody->collidable.broadPhaseHandle.collisionFilterInfo;
			collisionFilterInfo.SetCollisionLayer(static_cast<RE::COL_LAYER>(colLayer));
		}
	}

	static bool IsReferenceRagdollReady(RE::TESObjectREFR* ref)
	{
		if (!ref || !ref->Is3DLoaded()) return false;

		RE::NiAVObject* niAVObject = ref->Get3D();
		if (!niAVObject) return false;
		
		auto* hkpRigidBody = GetRigidBody(niAVObject);
		if (hkpRigidBody && hkpRigidBody->world && hkpRigidBody->motion.GetMass() > 0.0f) return true;

		return false;
	}

	static RE::hkpRigidBody* GetRigidBody(RE::NiAVObject* a_object)
	{
		if (!a_object) return nullptr;

		const auto collisionObject = a_object->GetCollisionObject();
		if (!collisionObject) return nullptr;

		const auto bhkRigidBody = RE::NiPointer<RE::bhkRigidBody>(collisionObject->GetRigidBody());
		if (!bhkRigidBody || !bhkRigidBody->referencedObject) return nullptr;

		const auto hkpRigidBody = static_cast<RE::hkpRigidBody*>(bhkRigidBody->referencedObject.get());
		return hkpRigidBody;
	}

	static bool TraverseObjectsForward(RE::NiAVObject* a_object, std::function<bool(RE::NiAVObject*, int)> a_func, int depth = 0)
	{
		if (!a_object) return true;
		if (!a_func(a_object, depth)) return false;

		if (auto node = a_object->AsNode()) {
			for (auto& child : node->GetChildren()) {
				if (!TraverseObjectsForward(child.get(), a_func, depth + 1)) {
					return false;
				}
			}
		}

		return true;
	}

	static bool TraverseObjectsForward(RE::NiAVObject* a_object, std::function<bool(RE::NiAVObject*)> a_func)
	{
		return TraverseObjectsForward(a_object, [&](RE::NiAVObject* obj, int) { return a_func(obj); });
	}
};
