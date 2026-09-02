#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Serialization.hpp"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"
#include "Utils/NiUtils.hpp"
#include "Utils/TimeUtils.hpp"

#include "API/Inertia-API.h"

namespace ModCore
{
	using namespace ModData;

	class Main
	{
	public:

		static void ProceedActorDeath(RE::Actor* actor)
		{
			if (!actor) return;

			TRACE("Proceed Death on actor <\"{}\" [REF:{:08X}] [BASE:{:08X}]>",
				actor->GetName(), actor->formID, (actor->GetActorBase() ? actor->GetActorBase()->formID : 0x0));

			WaitUntilRagdollReady(actor);
		};

		static RE::TESObjectREFR* DropItemFromActor(RE::Actor* actor, RE::TESBoundObject* object, RE::NiAVObject* node,
			const float linearIntertiaMult = 0.0f, const float angularIntertiaMult = 0.0f,
			const int count = 1, RE::ExtraDataList* extraDataList = nullptr)
		{
			if (!actor || !object) return nullptr;

			RE::NiPoint3 position = node->world.translate;
			RE::NiPoint3 angle;
			node->world.rotate.ToEulerAnglesXYZ(angle);
			RE::hkVector4 linearVelocity{};
			RE::hkVector4 angularVelocity{};

			if (linearIntertiaMult > -1.0f && angularIntertiaMult > -1.0f) {
				if (auto* hkpRigidBody = NiUtils::GetRigidBody(node)) {
					float mass = hkpRigidBody->motion.GetMass();
					if (mass <= 0.01f) mass = 5.0f;

					linearVelocity = hkpRigidBody->motion.linearVelocity * (linearIntertiaMult ? linearIntertiaMult : SettingsIni::fMotionLinearMult);
					linearVelocity = linearVelocity / mass;
					angularVelocity = hkpRigidBody->motion.angularVelocity * (angularIntertiaMult ? angularIntertiaMult : SettingsIni::fMotionAngularMult);
					angularVelocity = angularVelocity / mass;
				}
			}

			return DropItemFromActor(actor, object, position, angle, linearVelocity, angularVelocity, count, extraDataList);
		}

		static RE::TESObjectREFR* DropItemFromActor(RE::Actor* actor, RE::TESBoundObject* object, 
			const RE::NiPoint3& itemPosition, const RE::NiPoint3& itemAngle,
			const RE::hkVector4& linearVelocity = {}, const RE::hkVector4& angularVelocity = {},
			const int count = 1, RE::ExtraDataList* extraDataList = nullptr)
		{
			if (!actor || !object) return nullptr;

			if (!extraDataList) extraDataList = GetEquippedExtraDataList(actor, object);
			if (!extraDataList) extraDataList = &(actor->extraList);

			auto item = actor->RemoveItem(object, count, RE::ITEM_REMOVE_REASON::kDropping, extraDataList, actor, &itemPosition, &itemAngle);
			if (RE::TESObjectREFR* droppedRef = item ? item.get()->As<RE::TESObjectREFR>() : nullptr) {
				RE::FormID droppedBaseFormID = droppedRef->GetBaseObject() ? droppedRef->GetBaseObject()->formID : 0x0;
				TRACE("   -> Added item to map <[REF:{:08X}] [BASE:{:08X}]>", droppedRef->formID, droppedBaseFormID);

				if (SettingsIni::bDroppedEquipmentRecoverable) SetObjectDropperReference(actor, droppedRef);
				if (droppedRef->IsDynamicForm()) Serialization::Functions::AddBoundItem(actor->formID, droppedRef->formID);
				TweakObjectCollision(droppedRef, actor, linearVelocity, angularVelocity);

				if (SettingsIni::bMiscClearOwner) {
					const auto player = RE::PlayerCharacter::GetSingleton();
					if (player) droppedRef->SetOwner(player->GetActorBase());
				}

				if (SettingsIni::bScaleGear) {
					if (std::abs(actor->GetScale() - 1.0f) > 0.05f) {
						droppedRef->Disable();
						droppedRef->SetScale(actor->GetScale());

						TimeUtils::WaitAndCall(FRAME_DELAY(), [droppedHandle = droppedRef->GetHandle()]
						(TimeUtils::CallResult result, const std::chrono::nanoseconds) {
							if (result != TimeUtils::CallResult::kEndDone) return true;

							auto* droppedRef = MiscUtils::ResolveHandle(droppedHandle);
							if (!droppedRef) return true;

							droppedRef->Enable(false);
							droppedRef->AddChange(RE::TESObjectREFR::ChangeFlags::kScale);

							return true;
						});
					}
				}

				if (SettingsIni::fMiscFixFalseSummonedNPCDelay > 0.0f) {
					droppedRef->SetActivationBlocked(true);
					auto delayMs = std::chrono::milliseconds(static_cast<int>(std::lround(SettingsIni::fMiscFixFalseSummonedNPCDelay * 1000.0f)));

					TimeUtils::WaitAndCall(delayMs, [droppedHandle = droppedRef->GetHandle()]
					(TimeUtils::CallResult result, const std::chrono::nanoseconds) {
						if (result != TimeUtils::CallResult::kEndDone) return true;

						auto* droppedRef = MiscUtils::ResolveHandle(droppedHandle);
						if (!droppedRef) return true;

						droppedRef->SetActivationBlocked(false);

						return true;
					});
				}

				if (Inertia_API_Interface) Inertia_API_Interface->ApplyInertia(droppedRef);

				return droppedRef;
			}

			return nullptr;
		}

		static RE::ExtraDataList* GetEquippedExtraDataList(RE::Actor* actor, RE::TESBoundObject* object, const bool isLeft = false)
		{
			if (!actor || !object) return nullptr;
				
			auto* inventory = actor->GetInventoryChanges();
			if (!inventory || !inventory->entryList) return nullptr;

			RE::ExtraDataList* fallback = nullptr;

			for (auto* entry : *inventory->entryList) {
				if (!entry || entry->object != object || !entry->extraLists) continue;
					
				for (auto* xList : *entry->extraLists) {
					if (!xList) continue;

					if (isLeft && xList->HasType(RE::ExtraDataType::kWornLeft)) return xList;
					if (!isLeft && xList->HasType(RE::ExtraDataType::kWorn)) return xList;
						
					// Fallback
					if (isLeft && xList->HasType(RE::ExtraDataType::kWorn)) fallback = xList;
					if (!isLeft && xList->HasType(RE::ExtraDataType::kWornLeft)) fallback = xList;
				}
			}
			return fallback;
		}

	private:

		static void WaitUntilRagdollReady(RE::Actor* actor)
		{
			const bool isWeaponDrawn = actor->AsActorState() && actor->AsActorState()->IsWeaponDrawn();

			TimeUtils::WaitUntilRagdollReady(actor, [isWeaponDrawn](RE::TESObjectREFR* actorRef, const bool result) {
				if (!result || !actorRef) return;
					
				RE::Actor* actor = actorRef->As<RE::Actor>();
				if (!actor) return;

				ExcludedFormMap excludedFormIds;
				if (SettingsIni::bKeepGearSheathed && !isWeaponDrawn) {
					ReattachEquippedObject(actor, actor->GetEquippedObject(true), excludedFormIds);
					ReattachEquippedObject(actor, actor->GetEquippedObject(false), excludedFormIds);
				}

				const bool dropGear = SettingsIni::bDropGear && !actor->IsSummoned();
				if (dropGear || SettingsIni::bMotionEnable) {
					const auto dropFormMap = GetDropFormMap(actor, excludedFormIds);
					if (dropGear) DropInventory(actor, dropFormMap);
				}
			}, 500ms);
		}

		static void ReattachEquippedObject(RE::Actor* actor, RE::TESForm* object, ExcludedFormMap& excludedFormIds)
		{
			if (!actor || !object) return;

			RE::FormType objectType = object->GetFormType();
			if (objectType == RE::FormType::Light) return;

			auto equipType = object->As<RE::BGSEquipType>();
			RE::BGSEquipSlot* slot = equipType ? equipType->GetEquipSlot() : nullptr;
			if (!slot) return;

			if (objectType == RE::FormType::Armor && SettingsIni::bAlwaysDropShield) {
				if (auto objArmo = object->As<RE::TESObjectARMO>(); objArmo && objArmo->IsShield()) return;
			}

			excludedFormIds.insert(object->formID);
			MiscUtils::UnequipItem(actor, object, slot, nullptr);
				
			TimeUtils::WaitAndCall(FRAME_DELAY(), [actorHandle = actor->GetHandle(), objectFormID = object->formID, slot]
			(TimeUtils::CallResult result, const std::chrono::nanoseconds) {
				if (result != TimeUtils::CallResult::kEndDone) return true;

				auto* actor = MiscUtils::ResolveHandle<RE::Actor>(actorHandle);
				RE::TESForm* object = RE::TESForm::LookupByID<RE::TESForm>(objectFormID);
				if (!actor || !object || !slot) return true;
						
				MiscUtils::EquipItem(actor, object, slot, nullptr);

				return true;
			});
		}

		static RE::NiAVObject* FindParentWithDifferentRigidBodyName(RE::NiAVObject* node)
		{
			if (!node) return nullptr;

			auto* baseRigidBody = NiUtils::GetRigidBody(node);

			RE::NiAVObject* current = node->parent;
			while (current) {
				if (auto* hkpRigidBody = NiUtils::GetRigidBody(current)) {
					if (hkpRigidBody != baseRigidBody) return current;
				}
				current = current->parent;
			}

			return nullptr;
		}

		static bool IsActorValid(RE::Actor* actor)
		{
			if (!actor) return false;

			auto* actorBase = actor->GetActorBase();
			if (!actorBase) return false;

			if (!actorBase->Bleeds()) return false;
			if (actorBase->IsGhost()) return false;

			return true;
		}

		static DropFormMap GetDropFormMap(RE::Actor* targetRef, const ExcludedFormMap& excludedFormIds = {})
		{
			auto dropFormMap = DropFormMap{};

			if (!targetRef) return dropFormMap;

			TRACE("Scanning nodes for actor: <\"{}\" [REF:{:08X}] [BASE:{:08X}]>",
				targetRef->GetName(), targetRef->formID, (targetRef->GetActorBase() ? targetRef->GetActorBase()->formID : 0x0));

			RE::NiAVObject* targetAVStart = targetRef->Get3D();
			if (!targetAVStart) return dropFormMap;

			const bool validActor = !SettingsIni::bMiscExcludesSpecialNPCs || IsActorValid(targetRef);

			auto findNextNode = [&](auto&& self, RE::NiAVObject* node) -> void {
				if (!node) return;

				RE::NiNode* niNode = node->AsNode();
				if (!niNode) return;

				DropSource dropSource;
				RE::FormID formId;

				if (formId = [](std::string_view name) -> RE::FormID {
					std::size_t lastOpen = name.rfind('(');
					std::size_t lastClose = name.rfind(')');
					if (lastOpen != std::string_view::npos && lastClose != std::string_view::npos && lastClose > lastOpen + 1) {
						std::string_view hexStr = name.substr(lastOpen + 1, lastClose - lastOpen - 1);
						if (hexStr.size() == 8 && std::all_of(hexStr.begin(), hexStr.end(), [](char c) {
								return std::isxdigit(static_cast<unsigned char>(c));
							})) {
							return static_cast<RE::FormID>(std::stoul(std::string(hexStr), nullptr, 16));
						}
					}
					return 0x0;
				}(node->name.data())) dropSource = DropSource::Vanilla;

				if (!formId && SettingsIni::iImmersiveEquipmentDisplays > 0) {
					if (formId = [](std::string_view name) -> RE::FormID {
						std::size_t openBracket = name.find('[');
						std::size_t closeBracket = name.find(']');
						if (openBracket != std::string_view::npos && closeBracket != std::string_view::npos && closeBracket > openBracket + 1) {
							std::string_view content = name.substr(openBracket + 1, closeBracket - openBracket - 1);

							std::size_t slashPos = content.find('/');
							if (slashPos != std::string_view::npos) {
								content = content.substr(0, slashPos);
							}

							if (content.size() == 8 && std::all_of(content.begin(), content.end(), [](char c) {
									return std::isxdigit(static_cast<unsigned char>(c));
								})) {
								return static_cast<RE::FormID>(std::stoul(std::string(content), nullptr, 16));
							}
						}
						return 0x0;
					}(node->name.data())) dropSource = DropSource::IED;
				}

				if (formId) {
					RE::TESForm* object = RE::TESForm::LookupByID<RE::TESForm>(formId);
					const bool validItem = (object && object->GetPlayable() && !excludedFormIds.contains(formId));

					RE::NiPoint3 position = node->world.translate;
					RE::NiPoint3 angle;
					node->world.rotate.ToEulerAnglesXYZ(angle);
					RE::hkVector4 linearVelocity{};
					RE::hkVector4 angularVelocity{};

					float mass = 10.0f;
					if (auto* hkpRigidBody = NiUtils::GetRigidBody(node)) {
						mass = hkpRigidBody->motion.GetMass();
					}
	
					if (SettingsIni::bMotionEnable) {
						if (RE::NiAVObject* differentRBNodeName = FindParentWithDifferentRigidBodyName(node)) {
							if (auto* hkpRigidBody = NiUtils::GetRigidBody(differentRBNodeName)) {
								linearVelocity = hkpRigidBody->motion.linearVelocity * SettingsIni::fMotionLinearMult;
								linearVelocity = linearVelocity / mass;
								angularVelocity = hkpRigidBody->motion.angularVelocity * SettingsIni::fMotionAngularMult;
								angularVelocity = angularVelocity / mass;
							}
						}
					
						if (!validItem || !validActor || !SettingsIni::bDropGear) {
							if (auto* hkpRigidBody = NiUtils::GetRigidBody(node)) {
								RE::hkVector4 clampdLinearVelocity = linearVelocity * mass;
								ModUtils::ClampVector4(clampdLinearVelocity, SettingsIni::fMotionLinearClamp);
								RE::hkVector4 clampdAngularVelocity = angularVelocity * mass;
								ModUtils::ClampVector4(clampdAngularVelocity, SettingsIni::fMotionAngularClamp);

								hkpRigidBody->SetLinearVelocity(clampdLinearVelocity);
								hkpRigidBody->SetAngularVelocity(clampdAngularVelocity);
							}
						}
					}

					if (!validItem || !validActor) return;

					dropFormMap[formId].emplace_back(dropSource, position, angle, linearVelocity, angularVelocity);

					TRACE("   -> Added Form ID \"{:08X}\" from node: {}", formId, node->name);
				}

				for (auto& child : niNode->GetChildren()) {
					self(self, child.get());
				}
			};

			findNextNode(findNextNode, targetAVStart);

			if (SettingsIni::iVerboseMode > 1) {
				for (const auto& [formID, transforms] : dropFormMap) {
					for (const auto& [dropSource, pos, angle, linearVelocity, angularVelocity] : transforms) {
						TRACE("   DropMap -> {:08X} | Pos: ({:.2f}, {:.2f}, {:.2f}) | Rot: ({:.2f}, {:.2f}, {:.2f})",
							formID, pos.x, pos.y, pos.z, angle.x, angle.y, angle.z);
					}
				}
			}

			return dropFormMap;
		}

		static void DropInventory(RE::Actor* actor, const DropFormMap& dropFormData, bool onlyHands = true)
		{
			if (!actor) return;

			TRACE("Proceed Drop Inventory on actor <\"{}\" [REF:{:08X}] [BASE:{:08X}]>",
				actor->GetName(), actor->formID, actor->GetActorBase() ? actor->GetActorBase()->formID : 0x0);

			RE::TESForm* leftHand = actor->GetEquippedObject(true);
			RE::TESForm* rightHand = actor->GetEquippedObject(false);
			RE::TESForm* bothHands = nullptr;

			if (rightHand) {
				if (auto weap = rightHand->As<RE::TESObjectWEAP>(); weap && weap->equipSlot == bothHandsSlot) {
					bothHands = weap;
					leftHand = nullptr;
					rightHand = nullptr;
				}
			}

			enum class DropMode { Default, LeftHand, RightHand, BothHands, Count };
			std::unordered_set<DropMode> appliedModes;

			auto processEntry = [&](const RE::InventoryEntryData* entry, const int maxCount, const bool isWorn) {
				if (!entry || !entry->object) return;

				auto form = entry->object->As<RE::TESForm>();
				if (!form) return;

				auto it = dropFormData.find(form->formID);
				if (it == dropFormData.end()) return;

				auto& transforms = const_cast<std::vector<DropFormTuple>&>(it->second); 
				if (transforms.empty()) return;

				auto [dropSource, itemPos, itemAngle, linVel, angVel] = transforms.front();
				transforms.erase(transforms.begin());

				int count = 1;
				if (dropSource == DropSource::Vanilla) {
					if (!isWorn) return;
					if (onlyHands && form != leftHand && form != rightHand && form != bothHands) return;
				} else if (dropSource == DropSource::IED && SettingsIni::iImmersiveEquipmentDisplays > 0) {
					RE::FormType formType = form->GetFormType();
					if (formType == RE::FormType::Light) return;
					if (SettingsIni::iImmersiveEquipmentDisplays < 2 &&
						formType != RE::FormType::Weapon &&
						formType != RE::FormType::Armor) return;
					count = maxCount;
				}

				for (int i = 0; i < static_cast<int>(DropMode::Count); ++i) {
					DropMode mode = static_cast<DropMode>(i);
					if (appliedModes.contains(mode)) continue;

					if ((mode == DropMode::LeftHand && form != leftHand) ||
						(mode == DropMode::RightHand && form != rightHand) ||
						(mode == DropMode::BothHands && form != bothHands) ||
						(mode == DropMode::Default && (form == leftHand || form == rightHand || form == bothHands))) {
						continue;
					}

					auto* extraDataList = GetEquippedExtraDataList(actor, entry->object, (mode == DropMode::LeftHand));
					DropItemFromActor(actor, entry->object, itemPos, itemAngle, linVel, angVel, count, extraDataList);
						
					if (mode != DropMode::Default) appliedModes.insert(mode);
					break;
				}
			};

			for (const auto& [item, data] : actor->GetInventory()) {
				const auto& [count, entry] = data;
				if (!entry) continue;

				const bool isWorn = entry->IsWorn();
				for (int i = 0; i < (isWorn ? count : 1); i++) {
					processEntry(entry.get(), count, isWorn);
				}
			}
		}

		static void SetObjectDropperReference(RE::Actor* actor, RE::TESObjectREFR* object)
		{
			if (!actor || !object) return;

			auto objectHandle = object->GetHandle();
			if (!objectHandle) return;
				
			if (actor->extraList.HasType(RE::ExtraDataType::kDroppedItemList)) {
				auto xDrop = actor->extraList.GetByType<RE::ExtraDroppedItemList>();
				if (xDrop) xDrop->droppedItemList.push_front(objectHandle);
			} else {
				auto xDrop = RE::BSExtraData::Create<RE::ExtraDroppedItemList>();
				if (xDrop) {
					xDrop->droppedItemList.push_front(objectHandle);
					actor->extraList.Add(xDrop);
				}
			}
			actor->AddChange(RE::TESObjectREFR::ChangeFlags::kGameOnlyExtra);
			actor->AddChange(RE::TESObjectREFR::ChangeFlags::kInventory);

			auto actorHandle = actor->GetHandle();
			if (!actorHandle) return;

			if (object->extraList.HasType(RE::ExtraDataType::kItemDropper)) {
				RE::ExtraItemDropper* xItemDropper = object->extraList.GetByType<RE::ExtraItemDropper>();
				if (xItemDropper) xItemDropper->dropper = actorHandle;
			} else {
				RE::ExtraItemDropper* xItemDropper = RE::BSExtraData::Create<RE::ExtraItemDropper>();
				if (xItemDropper) {
					xItemDropper->dropper = actorHandle;
					object->extraList.Add(xItemDropper);
				}
			}
			object->AddChange(RE::TESObjectREFR::ChangeFlags::kGameOnlyExtra);
		}

		static void TweakObjectCollision(RE::TESObjectREFR* object, RE::Actor* actor, const RE::hkVector4& linearVelocity, const RE::hkVector4& angularVelocity)
		{
			if (!object) return;

			TimeUtils::WaitUntilRagdollReady(object, [actorHandle = actor->GetHandle(), linearVelocity, angularVelocity](RE::TESObjectREFR* object, const bool result) {
				if (!result || !object) return;

				if (SettingsIni::bCollisionTweakEnable) {
					//object->MoveHavok(true);
					ModUtils::SetObjectCollision(object, RE::COL_LAYER::kCameraPick);

					ValidateWeaponNodeCollision(MiscUtils::ResolveHandle<RE::Actor>(actorHandle), object);

					if (auto* object3D = object->Get3D()) {
						if (auto* hkpRigidBody = NiUtils::GetRigidBody(object3D)) {
							hkpRigidBody->SetLinearVelocity({});
							hkpRigidBody->SetAngularVelocity({});
						}
					}
				}

				if (SettingsIni::bMotionEnable) {
					TimeUtils::WaitUntilRagdollReady(object, [linearVelocity, angularVelocity](RE::TESObjectREFR* object, const bool result) {
						if (!result || !object) return;

						if (auto* object3D = object->Get3D()) {
							if (auto* hkpRigidBody = NiUtils::GetRigidBody(object3D)) {
								const float mass = hkpRigidBody->motion.GetMass();
								if (!std::isfinite(mass) || mass <= 0.0f) return;

								RE::hkVector4 clampedLinearVelocity = linearVelocity * mass;
								ModUtils::ClampVector4(clampedLinearVelocity, SettingsIni::fMotionLinearClamp);
								RE::hkVector4 clampedAngularVelocity = angularVelocity * mass;
								ModUtils::ClampVector4(clampedAngularVelocity, SettingsIni::fMotionAngularClamp);

								hkpRigidBody->SetLinearVelocity(clampedLinearVelocity);
								hkpRigidBody->SetAngularVelocity(clampedAngularVelocity);
							}
						}
					});
				}
			});
		}

		static RE::NiAVObject* GetClosestWeaponNode(RE::Actor* actor, RE::TESObjectREFR* object)
		{
			if (!actor || !object) return nullptr;

			auto* rootNode = actor->Get3D();
			if (!rootNode) return nullptr;

			const auto objectPosition = object->GetPosition();

			RE::NiAVObject* closestNode = nullptr;
			float minDistance = std::numeric_limits<float>::max();

			NiUtils::TraverseObjectsForward(rootNode, [&](RE::NiAVObject* a_node) {
				float distance = a_node->world.translate.GetDistance(objectPosition);
				if (distance < minDistance) {
					minDistance = distance;
					closestNode = a_node;
				}
				return true;
			});

			return closestNode;
		}

		static void ValidateWeaponNodeCollision(RE::Actor* actor, RE::TESObjectREFR* object)
		{
			if (!actor || !object) return;

			auto restoreObjectCollision = [objectHandle = object->GetHandle()]() -> void {
				auto* object = MiscUtils::ResolveHandle(objectHandle);
				if (!object) return;
				const auto collisionLayer = SettingsIni::bCollisionEnableCharacterCollision ? RE::COL_LAYER::kWeapon : RE::COL_LAYER::kDeadBip;
				ModUtils::SetObjectCollision(object, collisionLayer);
			};

			auto* node = GetClosestWeaponNode(actor, object);
			if (!node) return restoreObjectCollision();

			auto* object3D = object->Get3D();
			if (!object3D) return restoreObjectCollision();

			const RE::NiPoint3 boundMin = object->GetBoundMin();
			const RE::NiPoint3 boundMax = object->GetBoundMax();
			const float nodeDetachRadius = SettingsIni::fCollisionTweakNodeDetachRadius;
			const float maxDelay = SettingsIni::fCollisionTweakRestoreDelay;
			const float minDelayProgress = 0.75f / maxDelay;

			TimeUtils::DoWhileInGame([]() { return FRAME_DELAY(); },
				[node, object, object3D, boundMin, boundMax, nodeDetachRadius, restoreObjectCollision, minDelayProgress]
				(TimeUtils::CallResult result, float progress) -> bool {
					if (TimeUtils::IsEnd(result)) {
						if (result != TimeUtils::CallResult::kEndDone) return true;
						restoreObjectCollision();
						return true;
					}

					if (progress < minDelayProgress) return true;

					const RE::NiPoint3& t = object3D->world.translate;
					const RE::NiMatrix3& r = object3D->world.rotate;
					const RE::NiPoint3 nodePos = node->world.translate;
					const RE::NiPoint3 rel = nodePos - t;
					const RE::NiPoint3 local{
						r.entry[0][0] * rel.x + r.entry[1][0] * rel.y + r.entry[2][0] * rel.z,
						r.entry[0][1] * rel.x + r.entry[1][1] * rel.y + r.entry[2][1] * rel.z,
						r.entry[0][2] * rel.x + r.entry[1][2] * rel.y + r.entry[2][2] * rel.z
					};

					const bool inside =
						(local.x + nodeDetachRadius) >= boundMin.x && (local.x - nodeDetachRadius) <= boundMax.x &&
						(local.y + nodeDetachRadius) >= boundMin.y && (local.y - nodeDetachRadius) <= boundMax.y &&
						(local.z + nodeDetachRadius) >= boundMin.z && (local.z - nodeDetachRadius) <= boundMax.z;

					if (!inside) return false;
					return true;
				},
				std::chrono::duration<float>(maxDelay), true, true
			);
		}
	};
};
