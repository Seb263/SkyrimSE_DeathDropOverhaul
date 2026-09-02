#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/DespawnManager.hpp"
#include "Core/Serialization.hpp"

#include "Utils/ModUtils.hpp"

namespace ModCore
{
	inline std::mutex attachedCellMutex;
	inline std::unordered_set<RE::TESObjectCELL*> attachedCells;

	class Maintenance
	{
	public:

		/**
		 * @brief Runs maintenance tasks on all references when a cell change is detected.
		 * 
		 * This function:
		 * - Processes all loaded cells and the interior cell.
		 * - Runs maintenance on each reference, focusing on proximity in exterior cells.
		 * - Skips cells beyond a certain distance from the player (8192 units).
		 */
		static void MaintainLoadedCells(const bool reset = false)
		{
			using namespace ModData;

			TRACE("Process Cells Maintenance Task.");

			{
				std::lock_guard<std::mutex> lock(attachedCellMutex);
				if (reset) attachedCells.clear();

				for (auto it = attachedCells.begin(); it != attachedCells.end();) {
					auto* cell = *it;

					if (!cell || !cell->IsAttached()) {
						TRACE("  -> Unloaded cell: [{:08X}].", cell ? cell->formID : 0);
						it = attachedCells.erase(it);
					} else {
						++it;
					}
				}
			}

			const auto processCell = [](RE::TESObjectCELL* cell) {
				if (!cell || !cell->IsAttached()) return;

				{
					std::lock_guard<std::mutex> lock(attachedCellMutex);
					if (!attachedCells.insert(cell).second) return;
				}

				TRACE("  -> Maintenance on cell [{:08X}]", cell->formID);

				std::vector<RE::TESObjectREFR*> toProcess;
				cell->ForEachReference([&](RE::TESObjectREFR* ref) -> RE::BSContainer::ForEachResult {
					if (ref && ItemHasDropperRef(ref)) toProcess.push_back(ref);
					return RE::BSContainer::ForEachResult::kContinue;
				});

				for (auto* ref : toProcess) {
					Maintenance::ItemMaintenanceProcess(ref);
				}
			};

			ModUtils::ProcessGridCells(processCell);

			TRACE("Ended Cells Maintenance Task.");
		}

		static bool ItemMaintenanceProcess(RE::TESObjectREFR* object)
		{
			if (!object || !ItemHasDropperRef(object)) return false;

			const auto* calendar = RE::Calendar::GetSingleton();
			auto* dropperRef = MiscUtils::GetValidReference<RE::Actor>(GetItemDropperRef(object));

			if (!calendar || !dropperRef || dropperRef->IsDeleted() || dropperRef->IsMarkedForDeletion()) {
				DespawnItem(object);
				return false;
			}

			auto droppedEntry = Serialization::Functions::GetDroppedEntryByItemFormID(object->formID);
			if (!droppedEntry || (calendar->GetCurrentGameTime() - droppedEntry->gameTime) > SettingsIni::fMiscDespawnTimeout) {
				DespawnItem(object);
				return false;
			}

			return true;
		}

		static bool MaintainAllActorBoundItems(RE::Actor* actor)
		{
			if (!actor) return false;

			const auto items = Serialization::Functions::GetAllActorBoundItems(actor->formID);
			if (items.empty()) return false;

			for (auto* item : items) {
				ItemMaintenanceProcess(item);
			}

			return true;
		}

		static void RemoveAllActorBoundItems(RE::Actor* actor)
		{
			if (!actor) return;

			const auto removed = Serialization::Functions::ClearActor(actor->formID);

			for (const auto& entry : removed) {
				DespawnManager::RemoveItem(MiscUtils::GetValidReference(entry.itemFormID));
			}
		}

		static bool ItemHasDropperRef(RE::TESObjectREFR* object)
		{
			if (!object) return false;

			return object->extraList.HasType(RE::ExtraDataType::kItemDropper);
		}

		static RE::TESObjectREFR* GetItemDropperRef(RE::TESObjectREFR* object)
		{
			if (!object || !object->extraList.HasType(RE::ExtraDataType::kItemDropper)) return nullptr;

			RE::ExtraItemDropper* xItemDropper = object->extraList.GetByType<RE::ExtraItemDropper>();
			if (!xItemDropper) return nullptr;

			return MiscUtils::GetValidReference(MiscUtils::ResolveHandle(xItemDropper->dropper));
		}

		static void DespawnItem(RE::TESObjectREFR* object)
		{
			if (!object) return;
			
			if (auto* dropperRef = MiscUtils::GetValidReference<RE::Actor>(GetItemDropperRef(object))) {
				const auto* actorBase = dropperRef->GetActorBase();
				const auto* actorState = dropperRef->AsActorState();
				const bool shouldDespawn = (dropperRef->Is3DLoaded() && dropperRef->IsDead()) ||
					(actorState && actorState->IsReanimated()) ||
					!(actorBase && actorBase->Respawns());

				if (shouldDespawn && SettingsIni::iMiscDisableInventoryRestoration < 2) {
					const auto objectFormID = object->formID;

					DespawnManager::Enqueue(dropperRef, object);

					TRACE("Despawned dropped object reference [REF:{:08X}]", objectFormID);
					
					return;
				}
			}
			
			DespawnManager::RemoveItem(object);
		}
	};
}
