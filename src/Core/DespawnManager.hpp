#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Serialization.hpp"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"

namespace ModCore
{
	class DespawnManager
	{
	public:
		static void Enqueue(RE::Actor* dropperRef, RE::TESObjectREFR* originalObject)
		{
			if (!dropperRef || !originalObject) return;

			if (IsItemPersistent(originalObject)) {
				if (SettingsIni::iMiscDisableInventoryRestoration < 1) {
					TRACE("Picked up persistent dropped object reference [ACTOR:{:08X}] [REF:{:08X}]", dropperRef->formID, originalObject->formID);
				
					if (auto* xOwner = originalObject->extraList.GetByType<RE::ExtraOwnership>()) originalObject->extraList.Remove(xOwner);
					dropperRef->PickUpObject(originalObject, originalObject->extraList.GetCount(), true, false);
					Serialization::Functions::RemoveBoundItem(originalObject->formID);
				} else {
					RemoveItem(originalObject);
				}
				return;
			}

			auto* baseObject = originalObject->GetBaseObject();
			if (!baseObject) {
				RemoveItem(originalObject);
				return;
			}

			dropperRef->AddObjectToContainer(baseObject, nullptr, originalObject->extraList.GetCount(), nullptr);

			const RE::FormID actorID = dropperRef->GetFormID();
			bool shouldStart = false;

			{
				std::lock_guard lock(_mutex);
				auto& state = _queues[actorID];
				state.tasks.push_back(originalObject->GetHandle());
				if (!state.busy) {
					state.busy = true;
					shouldStart = true;
				}
			}

			if (shouldStart) {
				ProcessNext(dropperRef, actorID);
			}
		}

		static void RemoveItem(RE::TESObjectREFR* object)
		{
			if (!object || object->As<RE::Actor>()) return;

			const auto objectFormID = object->formID;
			Serialization::Functions::RemoveBoundItem(objectFormID);

			if (!IsItemPersistent(object) && object->IsDynamicForm()) {
				RE::GarbageCollector::GetSingleton()->Add(object, true);
				TRACE("Removed dropped object reference [REF:{:08X}]", objectFormID);
			}
		}

	private:
		struct ActorState
		{
			std::deque<RE::ObjectRefHandle> tasks;
			bool busy = false;
		};

		inline static std::mutex _mutex;
		inline static std::unordered_map<RE::FormID, ActorState> _queues;

		static RE::BGSEquipType* GetEquipTypeInterface(RE::TESBoundObject* baseObject)
		{
			if (!baseObject) return nullptr;
			return baseObject->As<RE::BGSEquipType>();
		}

		static RE::TESBoundObject* GetPreviouslyEquipped(RE::Actor* actor, RE::TESBoundObject* baseObject, RE::BGSEquipSlot* slot)
		{
			if (!actor || !baseObject) return nullptr;

			switch (baseObject->GetFormType()) {
			case RE::FormType::Armor:
			{
				auto* armor = baseObject->As<RE::TESObjectARMO>();
				return armor ? actor->GetWornArmor(armor->GetSlotMask().get()) : nullptr;
			}
			case RE::FormType::Weapon:
			case RE::FormType::Light:
			case RE::FormType::Ammo:
			case RE::FormType::Scroll:
			{
				return actor->GetEquippedObject(false)->As<RE::TESBoundObject>();
			}
			default:
				return nullptr;
			}
		}

		static bool IsItemPersistent(RE::TESObjectREFR* object)
		{
			if (!object) return false;

			const bool isPersistent =
				object->inGameFormFlags.any(RE::TESForm::InGameFormFlag::kForcedPersistent) ||
				object->inGameFormFlags.any(RE::TESForm::InGameFormFlag::kRefOriginalPersistent);

			return isPersistent;
		}

		static void ReplicateExtraList(RE::Actor* dropperRef, RE::TESBoundObject* baseObject, const RE::ExtraDataList* sourceList)
		{
			if (!dropperRef || !baseObject || !sourceList) return;

			auto* invChanges = dropperRef->GetInventoryChanges();
			if (!invChanges || !invChanges->entryList) return;

			RE::InventoryEntryData* targetEntry = nullptr;
			for (auto* entryData : *invChanges->entryList) {
				if (entryData && entryData->object == baseObject) {
					targetEntry = entryData;
					break;
				}
			}
			if (!targetEntry || !targetEntry->extraLists || targetEntry->extraLists->empty()) return;

			RE::ExtraDataList* targetList = targetEntry->extraLists->front();
			if (!targetList) return;

			if (auto* xHealth = sourceList->GetByType<RE::ExtraHealth>()) {
				auto* newHealth = targetList->GetByType<RE::ExtraHealth>();
				if (!newHealth) {
					newHealth = new RE::ExtraHealth();
					targetList->Add(newHealth);
				}
				newHealth->health = xHealth->health;
			}

			if (auto* xCharge = sourceList->GetByType<RE::ExtraCharge>()) {
				auto* newCharge = targetList->GetByType<RE::ExtraCharge>();
				if (!newCharge) {
					newCharge = new RE::ExtraCharge();
					targetList->Add(newCharge);
				}
				newCharge->charge = xCharge->charge;
			}

			if (auto* xEnch = sourceList->GetByType<RE::ExtraEnchantment>()) {
				auto* newEnch = targetList->GetByType<RE::ExtraEnchantment>();
				if (!newEnch) {
					newEnch = new RE::ExtraEnchantment();
					targetList->Add(newEnch);
				}
				newEnch->enchantment = xEnch->enchantment;
				newEnch->charge = xEnch->charge;
				newEnch->removeOnUnequip = xEnch->removeOnUnequip;
			}

			if (auto* xSoul = sourceList->GetByType<RE::ExtraSoul>()) {
				auto* newSoul = targetList->GetByType<RE::ExtraSoul>();
				if (!newSoul) {
					newSoul = new RE::ExtraSoul();
					targetList->Add(newSoul);
				}
				newSoul->soul = xSoul->soul;
			}

			if (auto* xPoison = sourceList->GetByType<RE::ExtraPoison>()) {
				auto* newPoison = targetList->GetByType<RE::ExtraPoison>();
				if (!newPoison) {
					newPoison = new RE::ExtraPoison();
					targetList->Add(newPoison);
				}
				newPoison->poison = xPoison->poison;
				newPoison->count = xPoison->count;
			}

			if (auto* xOwnership = sourceList->GetByType<RE::ExtraOwnership>()) {
				auto* newOwnership = targetList->GetByType<RE::ExtraOwnership>();
				if (!newOwnership) {
					newOwnership = new RE::ExtraOwnership();
					targetList->Add(newOwnership);
				}
				newOwnership->owner = xOwnership->owner;
			}

			if (auto* xText = sourceList->GetByType<RE::ExtraTextDisplayData>()) {
				if (xText->ownerInstance == RE::ExtraTextDisplayData::DisplayDataType::kCustomName) {
					const auto& fullName = xText->displayName;
					const std::string trimmedName(fullName.c_str(), std::min<std::size_t>(xText->customNameLength, fullName.size()));

					if (!trimmedName.empty()) {
						auto* newText = targetList->GetByType<RE::ExtraTextDisplayData>();
						if (!newText) {
							newText = new RE::ExtraTextDisplayData();
							targetList->Add(newText);
						}
						newText->SetName(trimmedName.c_str());
					}
				}
			}

			TRACE("ReplicateExtraList: extraList replicated for [{:08X}]", baseObject->formID);
		}

		static void ProcessClonedItemInternal(RE::Actor* dropperRef, RE::TESObjectREFR* originalObject,
			RE::TESBoundObject* baseObject, std::function<void()> onDone)
		{
			auto* equipType = GetEquipTypeInterface(baseObject);
			RE::BGSEquipSlot* slot = equipType ? equipType->GetEquipSlot() : nullptr;

			if (!equipType || !slot) {
				ReplicateExtraList(dropperRef, baseObject, &originalObject->extraList);
				RemoveItem(originalObject);
				onDone();
				return;
			}

			RE::TESBoundObject* previouslyEquipped = nullptr;
			if (auto* formObject = dropperRef->GetEquippedObjectInSlot(slot)) {
				previouslyEquipped = formObject->As<RE::TESBoundObject>();
			}
			MiscUtils::EquipItem(dropperRef, baseObject, slot, nullptr);

			TimeUtils::WaitAndCall([] { return std::max(FRAME_DELAY(), std::chrono::duration_cast<std::chrono::nanoseconds>(25ms)); },
				[dropperHandle = dropperRef->GetHandle(), originalObjectHandle = originalObject->GetHandle(), baseObject, slot, previouslyEquipped, onDone]
				(TimeUtils::CallResult result, std::chrono::nanoseconds) {
					if (result != TimeUtils::CallResult::kEndDone) return true;

					auto* dropperRef = MiscUtils::ResolveHandle<RE::Actor>(dropperHandle);
					auto* originalObject = MiscUtils::ResolveHandle(originalObjectHandle);
					if (!dropperRef || !originalObject) return true;

					ReplicateExtraList(dropperRef, baseObject, &originalObject->extraList);

					if (auto* equipManager = RE::ActorEquipManager::GetSingleton()) {
						MiscUtils::UnequipItem(dropperRef, baseObject, slot, nullptr);
						if (previouslyEquipped) {
							MiscUtils::EquipItem(dropperRef, previouslyEquipped, slot, nullptr);
						}
					}
					RemoveItem(originalObject);
					TRACE("ProcessClonedItem: clone processed for [{:08X}]", baseObject->formID);

					onDone();
					return true;
				},
			false);
		}

		static void ProcessNext(RE::Actor* dropperRef, RE::FormID actorID)
		{
			RE::ObjectRefHandle originalObjectHandle;
			{
				std::lock_guard lock(_mutex);
				auto it = _queues.find(actorID);
				if (it == _queues.end() || it->second.tasks.empty()) {
					if (it != _queues.end()) _queues.erase(it);
					return;
				}
				originalObjectHandle = it->second.tasks.front();
				it->second.tasks.pop_front();
			}

			auto* originalObject = MiscUtils::ResolveHandle(originalObjectHandle);
			if (!dropperRef || !dropperRef->Get3D() || !originalObject) {
				ProcessNext(dropperRef, actorID);
				return;
			}

			RE::TESBoundObject* baseObject = originalObject->GetBaseObject();
			if (!baseObject) {
				ProcessNext(dropperRef, actorID);
				return;
			}

			ProcessClonedItemInternal(dropperRef, originalObject, baseObject,
				[dropperRef, actorID]() {
					ProcessNext(dropperRef, actorID);
				});
		}
	};
}
