#pragma once

#include "DataHandler.hpp"

namespace SettingsIni
{
	// General
	inline int iVerboseMode = 1;

	// Main
	inline bool bDropGear = true;
	inline bool bScaleGear = true;
	inline bool bKeepGearSheathed = true;
	inline bool bAlwaysDropShield = true;
	inline bool bRestoreGearOnReanimate = true;
	inline bool bDroppedEquipmentRecoverable = true;
	inline int  iImmersiveEquipmentDisplays = 0;

	// Collision
	inline bool  bCollisionTweakEnable = true;
	inline bool  bCollisionEnableCharacterCollision = false;
	inline float fCollisionTweakRestoreDelay = 2.0f;
	inline float fCollisionTweakNodeDetachRadius = 10.0f;

	// Motion
	inline bool  bMotionEnable = true;
	inline float fMotionLinearMult = 1.0f;
	inline float fMotionLinearClamp = 11.0f;
	inline float fMotionAngularMult = 0.5f;
	inline float fMotionAngularClamp = 6.0f;

	// Misc
	inline float fMiscDespawnTimeout = 14.0f;
	inline float fMiscFixFalseSummonedNPCDelay = 1.0f;
	inline bool bMiscExcludesSpecialNPCs = true;
	inline int iMiscDisableInventoryRestoration = 0;
	inline bool bMiscClearOwner = true;

	class SettingsManager
	{
	public:
		SettingsManager()
		{
			bindings = {
				// General
				{ "General", "iVerboseMode", &iVerboseMode },

				// Main
				{ "Main", "bDropGear", &bDropGear },
				{ "Main", "bScaleGear", &bScaleGear },
				{ "Main", "bKeepGearSheathed", &bKeepGearSheathed },
				{ "Main", "bAlwaysDropShield", &bAlwaysDropShield },
				{ "Main", "bRestoreGearOnReanimate", &bRestoreGearOnReanimate },
				{ "Main", "bDroppedEquipmentRecoverable", &bDroppedEquipmentRecoverable },
				{ "Main", "iImmersiveEquipmentDisplays", &iImmersiveEquipmentDisplays },

				// Collision
				{ "CollisionTweak", "bEnable", &bCollisionTweakEnable },
				{ "CollisionTweak", "bEnableCharacterCollision", &bCollisionEnableCharacterCollision },
				{ "CollisionTweak", "fRestoreDelay", &fCollisionTweakRestoreDelay },
				{ "CollisionTweak", "fNodeDetachRadius", &fCollisionTweakNodeDetachRadius },

				// Motion
				{ "Motion", "bEnable", &bMotionEnable },
				{ "Motion", "fLinearMult", &fMotionLinearMult },
				{ "Motion", "fLinearClamp", &fMotionLinearClamp },
				{ "Motion", "fAngularMult", &fMotionAngularMult },
				{ "Motion", "fAngularClamp", &fMotionAngularClamp },

				// Misc
				{ "Misc", "fDespawnTimeout", &fMiscDespawnTimeout },
				{ "Misc", "fFixFalseSummonedNPCDelay", &fMiscFixFalseSummonedNPCDelay },
				{ "Misc", "bExcludesSpecialNPCs", &bMiscExcludesSpecialNPCs },
				{ "Misc", "iDisableInventoryRestoration", &iMiscDisableInventoryRestoration },
				{ "Misc", "bClearOwner", &bMiscClearOwner }
			};
		}

		bool ReadSettings()
		{
			std::wstring wpath_str(path.begin(), path.end());
			const wchar_t* wpath = wpath_str.c_str();

			bool readStatus = false;

			logger::info("Trying to read INI file at path: {}", path);

			if (std::filesystem::exists(path)) {
				CSimpleIniA ini;
				ini.SetUnicode();

				if (ini.LoadFile(wpath) >= 0) {
					for (const auto& bind : bindings) {
						std::visit([&](auto* ptr) {
							using T = std::decay_t<decltype(*ptr)>;
							if constexpr (std::is_same_v<T, bool>) {
								*ptr = ini.GetBoolValue(bind.section, bind.key, *ptr);
							} else if constexpr (std::is_same_v<T, int>) {
								*ptr = static_cast<int>(ini.GetLongValue(bind.section, bind.key, *ptr));
							} else if constexpr (std::is_same_v<T, float>) {
								*ptr = static_cast<float>(ini.GetDoubleValue(bind.section, bind.key, *ptr));
							} else if constexpr (std::is_same_v<T, std::string>) {
								*ptr = ini.GetValue(bind.section, bind.key, ptr->c_str());
							}
						}, bind.var);
					}
					readStatus = true;
				} else {
					logger::error("Failed to load INI file at {}", path);
				}
			} else {
				logger::warn("INI file does not exist at {}", path);
			}

			// Clamping logic

			iVerboseMode = std::clamp(iVerboseMode, 0, 2);

			iImmersiveEquipmentDisplays = std::clamp(iImmersiveEquipmentDisplays, 0, 2);
			fCollisionTweakRestoreDelay = std::clamp(fCollisionTweakRestoreDelay, 0.1f, 3.0f);
			fCollisionTweakNodeDetachRadius = std::clamp(fCollisionTweakNodeDetachRadius, 1.0f, 25.0f);
			
			fMotionLinearMult = std::clamp(fMotionLinearMult, 0.0f, 5.0f);
			fMotionLinearClamp = std::clamp(fMotionLinearClamp, 0.0f, 30.0f);
			fMotionAngularMult = std::clamp(fMotionAngularMult, 0.0f, 5.0f);
			fMotionAngularClamp = std::clamp(fMotionAngularClamp, 0.0f, 30.0f);
			
			fMiscDespawnTimeout = std::clamp(fMiscDespawnTimeout, 0.0f, 30.0f);
			fMiscFixFalseSummonedNPCDelay = std::clamp(fMiscFixFalseSummonedNPCDelay, 0.0f, 10.0f);
			iMiscDisableInventoryRestoration = std::clamp(iMiscDisableInventoryRestoration, 0, 2);

			// External data
			[&]() {
				using namespace ModData;

				debugVerboseMode = iVerboseMode;
			}();

			return readStatus;
		}

		template <typename T>
		T GetValue(const std::string& key_section, const T& defaultValue = T{})
		{
			auto sep = key_section.find(':');
			if (sep == std::string::npos) {
				logger::error("Invalid key_section format: '{}'", key_section);
				return defaultValue;
			}

			std::string section = key_section.substr(0, sep);
			std::string key = key_section.substr(sep + 1);

			for (const auto& bind : bindings) {
				if (key == bind.key && section == bind.section) {
					if constexpr (std::is_same_v<T, bool>) {
						if (auto v = std::get_if<bool*>(&bind.var)) return **v;
						if (auto v = std::get_if<int*>(&bind.var)) return **v != 0;
						if (auto v = std::get_if<float*>(&bind.var)) return **v != 0.0f;
					}
					else if constexpr (std::is_same_v<T, int>) {
						if (auto v = std::get_if<int*>(&bind.var)) return **v;
						if (auto v = std::get_if<float*>(&bind.var)) return static_cast<int>(**v);
						if (auto v = std::get_if<bool*>(&bind.var)) return **v ? 1 : 0;
					}
					else if constexpr (std::is_same_v<T, float>) {
						if (auto v = std::get_if<float*>(&bind.var)) return **v;
						if (auto v = std::get_if<int*>(&bind.var)) return static_cast<float>(**v);
						if (auto v = std::get_if<bool*>(&bind.var)) return **v ? 1.0f : 0.0f;
					}
					else if constexpr (std::is_same_v<T, std::string>) {
						if (auto v = std::get_if<std::string*>(&bind.var)) return **v;
					}

					logger::error("Type mismatch for key '{}' in section '{}'", key, section);
					return defaultValue;
				}
			}

			logger::error("No binding found for key '{}' in section '{}'", key, section);
			return defaultValue;
		}

		template <typename T>
		bool SetValue(const std::string& key_section, const T& value)
		{
			auto sep = key_section.find(':');
			if (sep == std::string::npos) {
				logger::error("Invalid key_section format: '{}'", key_section);
				return false;
			}

			std::string section = key_section.substr(0, sep);
			std::string key = key_section.substr(sep + 1);

			if (section.empty() || key.empty()) {
				logger::error("Empty section or key in '{}'", key_section);
				return false;
			}

			for (auto& bind : bindings) {
				if (section == bind.section && key == bind.key) {
					bool matched = std::visit([&](auto* ptr) -> bool {
						using PtrType = std::decay_t<decltype(*ptr)>;
						if constexpr (std::is_same_v<PtrType, T>) {
							*ptr = value;
							return true;
						}
						return false;
					}, bind.var);

					if (!matched) {
						logger::error("Type mismatch for {}:{}", section, key);
						return false;
					}

					CSimpleIniA ini;
					ini.SetUnicode();
					if (std::filesystem::exists(path)) ini.LoadFile(path.c_str());

					if constexpr (std::is_same_v<T, bool>) {
						ini.SetBoolValue(section.c_str(), key.c_str(), value);
					} else if constexpr (std::is_same_v<T, int>) {
						ini.SetLongValue(section.c_str(), key.c_str(), value);
					} else if constexpr (std::is_same_v<T, float>) {
						ini.SetDoubleValue(section.c_str(), key.c_str(), value);
					} else if constexpr (std::is_same_v<T, std::string>) {
						ini.SetValue(section.c_str(), key.c_str(), value.c_str());
					} else {
						return false;
					}

					if (ini.SaveFile(path.c_str()) < 0) {
						logger::error("Failed to save INI file at {}", path);
						return false;
					}

					return true;
				}
			}

			logger::error("No binding found for {}:{}", section, key);
			return false;
		}

	private:
		inline static std::string path = "Data/SKSE/Plugins/DeathDropOverhaul.ini";

		using IniValue = std::variant<bool*, int*, float*, std::string*>;

		struct IniBinding
		{
			const char* section;
			const char* key;
			IniValue    var;
		};

		std::vector<IniBinding> bindings;
	};
}
