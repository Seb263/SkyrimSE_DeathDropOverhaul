#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Utils/MiscUtils.hpp"

namespace Serialization
{
	constexpr std::uint32_t coSaveId = std::byteswap('DROP');
	constexpr std::uint32_t dropMapId = std::byteswap('DMAP');
	constexpr std::uint32_t mappingVersion = 4;

	// -------------------------------------------------------------------------
	// Data structures
	// -------------------------------------------------------------------------

	struct DroppedEntry
	{
		RE::FormID itemFormID = 0;
		float gameTime = 0.0f;
	};

	struct ActorDroppedState
	{
		std::vector<DroppedEntry> items;
	};

	inline std::unordered_map<RE::FormID, ActorDroppedState> droppedMap; // ActorFormID -> ActorDroppedState
	inline std::unordered_map<RE::FormID, RE::FormID> droppedReverseMap; // ItemFormID -> ActorFormID
	inline std::shared_mutex droppedMapMutex;

	// -------------------------------------------------------------------------
	// Serialization
	// -------------------------------------------------------------------------

	class Functions
	{
	public:

		static void RegisterSerializationCallbacks()
		{
			auto serialization = SKSE::GetSerializationInterface();
			serialization->SetUniqueID(coSaveId);
			serialization->SetSaveCallback(OnSKSESave);
			serialization->SetLoadCallback(OnSKSELoad);
			serialization->SetRevertCallback(OnSKSERevert);
		}

		// ---------------------------------------------------------------------
		// Data accessors
		// ---------------------------------------------------------------------

		static void AddBoundItem(RE::FormID actorFormID, RE::FormID itemFormID)
		{
			if (!actorFormID || !itemFormID) return;

			std::unique_lock lock(droppedMapMutex);

			auto& items = droppedMap[actorFormID].items;
			auto it = std::find_if(items.begin(), items.end(),
				[itemFormID](const DroppedEntry& entry) { return entry.itemFormID == itemFormID; });

			bool added = false;
			if (it == items.end()) {
				const float gameTime = RE::Calendar::GetSingleton()->GetCurrentGameTime();

				items.emplace_back(DroppedEntry{ itemFormID, gameTime });
				droppedReverseMap[itemFormID] = actorFormID;
				added = true;
			}

			TRACE("Added bound item reference [ACTOR:{:08X}] [REF:{:08X}] [SUCCESS:{}]", actorFormID, itemFormID, added);
		}

		static std::vector<RE::TESObjectREFR*> GetAllActorBoundItems(RE::FormID actorFormID)
		{
			std::shared_lock lock(droppedMapMutex);

			std::vector<RE::TESObjectREFR*> result;
			auto it = droppedMap.find(actorFormID);

			if (it != droppedMap.end()) {
				result.reserve(it->second.items.size());
				for (const auto& entry : it->second.items) {
					if (auto* itemRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(entry.itemFormID)) {
						result.push_back(itemRef);
					}
				}
			}
			return result;
		}

		static bool RemoveBoundItem(RE::FormID itemFormID)
		{
			if (!itemFormID) return false;

			std::unique_lock lock(droppedMapMutex);

			auto revIt = droppedReverseMap.find(itemFormID);
			if (revIt == droppedReverseMap.end()) return false;

			const RE::FormID actorFormID = revIt->second;
			droppedReverseMap.erase(revIt);

			bool removed = false;
			auto mapIt = droppedMap.find(actorFormID);
			if (mapIt != droppedMap.end()) {
				auto& items = mapIt->second.items;

				auto newEnd = std::remove_if(items.begin(), items.end(),
					[itemFormID](const DroppedEntry& entry) { return entry.itemFormID == itemFormID; });

				removed = newEnd != items.end();
				items.erase(newEnd, items.end());

				if (items.empty()) droppedMap.erase(mapIt);
			}

			TRACE("Unbound item reference [REF:{:08X}] [SUCCESS:{}]", itemFormID, removed);

			return removed;
		}

		static std::vector<DroppedEntry> ClearActor(RE::FormID actorFormID)
		{
			std::unique_lock lock(droppedMapMutex);

			auto it = droppedMap.find(actorFormID);
			if (it == droppedMap.end()) return {};

			std::vector<DroppedEntry> removed = std::move(it->second.items);
			for (const auto& entry : removed) {
				droppedReverseMap.erase(entry.itemFormID);
			}

			droppedMap.erase(it);
			return removed;
		}

		static std::optional<DroppedEntry> GetDroppedEntryByItemFormID(RE::FormID itemFormID)
		{
			std::shared_lock lock(droppedMapMutex);

			auto revIt = droppedReverseMap.find(itemFormID);
			if (revIt == droppedReverseMap.end()) return std::nullopt;

			auto mapIt = droppedMap.find(revIt->second);
			if (mapIt == droppedMap.end()) return std::nullopt;

			const auto& items = mapIt->second.items;
			auto entryIt = std::find_if(items.begin(), items.end(),
				[itemFormID](const DroppedEntry& entry) { return entry.itemFormID == itemFormID; });

			if (entryIt == items.end()) return std::nullopt;

			return *entryIt;
		}

	private:

		// ---------------------------------------------------------------------
		// SKSE callbacks
		// ---------------------------------------------------------------------

		static void OnSKSESave(SKSE::SerializationInterface* intfc)
		{
			TRACE("Starting SKSE Save for droppedMap.");

			const auto* calendar = RE::Calendar::GetSingleton();
			const float currentGameTime = calendar ? calendar->GetCurrentGameTime() : 0.0f;

			std::shared_lock lock(droppedMapMutex);

			if (!intfc->OpenRecord(dropMapId, mappingVersion)) {
				logger::critical("Failed to open DROP record for serialization.");
				return;
			}

			std::unordered_map<RE::FormID, ActorDroppedState> filteredMap;
			filteredMap.reserve(droppedMap.size());

			for (const auto& [actorFormID, state] : droppedMap) {
				if (!actorFormID) continue;

				ActorDroppedState filteredState;
				filteredState.items.reserve(state.items.size());

				for (const auto& entry : state.items) {
					const bool expired = calendar && (currentGameTime - entry.gameTime) > SettingsIni::fMiscDespawnTimeout;

					if (expired) {
						TRACE("    -> Excluding expired dropped item from save [ACTOR:{:08X}] [REF:{:08X}]", actorFormID, entry.itemFormID);
						continue;
					}

					filteredState.items.push_back(entry);
				}

				if (!filteredState.items.empty()) {
					filteredMap.emplace(actorFormID, std::move(filteredState));
				}
			}

			std::size_t mapSize = filteredMap.size();
			intfc->WriteRecordData(&mapSize, sizeof(mapSize));

			TRACE("    -> Saving droppedMap with {} entries.", mapSize);

			for (const auto& [actorFormID, state] : filteredMap) {
				intfc->WriteRecordData(&actorFormID, sizeof(actorFormID));

				std::size_t itemCount = state.items.size();
				intfc->WriteRecordData(&itemCount, sizeof(itemCount));

				for (const auto& entry : state.items) {
					intfc->WriteRecordData(&entry.itemFormID, sizeof(entry.itemFormID));
					intfc->WriteRecordData(&entry.gameTime, sizeof(entry.gameTime));
				}
			}

			TRACE("Finished SKSE Save for droppedMap.");
		}

		static void OnSKSELoad(SKSE::SerializationInterface* intfc)
		{
			TRACE("Starting SKSE Load for droppedMap.");

			std::unique_lock lock(droppedMapMutex);
			droppedMap.clear();
			droppedReverseMap.clear();

			std::uint32_t type, version, length;
			while (intfc->GetNextRecordInfo(type, version, length)) {
				if (type != dropMapId || version != mappingVersion) continue;

				std::size_t mapSize = 0;
				if (!intfc->ReadRecordData(&mapSize, sizeof(mapSize))) {
					logger::critical("Failed to read droppedMap size.");
					continue;
				}

				TRACE("Loading droppedMap with {} entries.", mapSize);

				for (std::size_t i = 0; i < mapSize; ++i) {
					RE::FormID rawActorFormID = 0;
					if (!intfc->ReadRecordData(&rawActorFormID, sizeof(rawActorFormID))) {
						logger::critical("Failed to read actor FormID ({}/{}); aborting DROP record (stream desynced).", i, mapSize);
						break;
					}

					RE::FormID resolvedActorFormID = 0;
					const bool actorValid = intfc->ResolveFormID(rawActorFormID, resolvedActorFormID) && MiscUtils::IsFormIDValid(resolvedActorFormID);

					if (!actorValid) {
						TRACE("    -> Could not resolve actor FormID {:08X}, its dropped items will be discarded.", rawActorFormID);
					}

					std::size_t itemCount = 0;
					if (!intfc->ReadRecordData(&itemCount, sizeof(itemCount))) {
						logger::critical("Failed to read droppedMap item count for actor {:08X}; aborting DROP record.", resolvedActorFormID);
						break;
					}

					for (std::size_t j = 0; j < itemCount; ++j) {
						RE::FormID rawItemFormID = 0;
						float gameTime = 0.0f;

						if (!intfc->ReadRecordData(&rawItemFormID, sizeof(rawItemFormID)) ||
							!intfc->ReadRecordData(&gameTime, sizeof(gameTime))) {
							logger::critical("Failed to read dropped item entry {}/{} for actor {:08X}; aborting DROP record.", j, itemCount, resolvedActorFormID);
							break;
						}

						if (!actorValid) continue;

						RE::FormID resolvedItemFormID = 0;
						if (!intfc->ResolveFormID(rawItemFormID, resolvedItemFormID) || !MiscUtils::IsFormIDValid(resolvedItemFormID)) {
							TRACE("    -> Could not resolve dropped item FormID {:08X}, item lost (dead ref or mod removed).", rawItemFormID);
							continue;
						}

						droppedMap[resolvedActorFormID].items.emplace_back(DroppedEntry{ resolvedItemFormID, gameTime });
						droppedReverseMap[resolvedItemFormID] = resolvedActorFormID;

						TRACE("    -> Restored dropped item [ACTOR:{:08X}] [REF:{:08X}] [TIME:{:.3f}]", resolvedActorFormID, resolvedItemFormID, gameTime);
					}
				}
			}

			TRACE("Finished SKSE Load for droppedMap.");
		}

		static void OnSKSERevert(SKSE::SerializationInterface*)
		{
			std::unique_lock lock(droppedMapMutex);
			droppedMap.clear();
			droppedReverseMap.clear();
		}
	};
}
