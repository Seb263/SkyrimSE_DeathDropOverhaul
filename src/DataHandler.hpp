#pragma once

#include "API/Inertia-API.h"

namespace ModData
{
	static inline const std::string_view MOD_NAME = "Death Drop Overhaul";

	inline auto lastLoadPoint = std::chrono::steady_clock::now();
	inline std::atomic<RE::FormID> previousCell = 0x0;

	struct PluginForm
	{
		std::string_view name;
		void** formPtr;
		uint32_t formID;
		std::string_view pluginName;
		bool optional = false;
	};

	inline RE::BGSEquipSlot* leftHandSlot;
	inline RE::BGSEquipSlot* rightHandSlot;
	inline RE::BGSEquipSlot* bothHandsSlot;

	static inline const std::vector<PluginForm> pluginForms = {
		{ "leftHandSlot", reinterpret_cast<void**>(&leftHandSlot), 0x13F43, "Skyrim.esm" },
		{ "rightHandSlot", reinterpret_cast<void**>(&rightHandSlot), 0x13F42, "Skyrim.esm" },
		{ "bothHandsSlot", reinterpret_cast<void**>(&bothHandsSlot), 0x13F45, "Skyrim.esm" }
	};

	inline RE::TESDataHandler* TESdataHandler;

	enum class DropSource : uint8_t
	{
		Vanilla,
		IED
	};

	using DropFormTuple = std::tuple<DropSource, RE::NiPoint3, RE::NiPoint3, RE::hkVector4, RE::hkVector4>;
	using DropFormMap = std::unordered_map<RE::FormID, std::vector<DropFormTuple>>;

	using ExcludedFormMap = std::unordered_set<RE::FormID>;

	inline InertiaAPI::InertiaAPI* Inertia_API_Interface = nullptr;
}
