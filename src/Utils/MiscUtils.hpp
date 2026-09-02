#pragma once

class MiscUtils
{

public:

	static void EquipItem(RE::Actor* a_actor, RE::TESForm* a_form, RE::BGSEquipSlot* a_slot, RE::ExtraDataList* a_extralist, bool a_sound = false, bool a_queue = true, bool a_force = false)
	{
		if (!a_actor || !a_form) return;

		RE::ActorEquipManager* equipManager = RE::ActorEquipManager::GetSingleton();
		if (!equipManager) return;

		if (a_form->Is(RE::FormType::Spell)) {
			RE::SpellItem* a_spell = a_form->As<RE::SpellItem>();
			if (a_spell) equipManager->EquipSpell(a_actor, a_spell, a_slot);
		} else if (a_form->Is(RE::FormType::Shout)) {
			RE::TESShout* a_shout = a_form->As<RE::TESShout>();
			if (a_shout) equipManager->EquipShout(a_actor, a_shout);
		} else {
			if (RE::TESBoundObject* a_object = a_form->As<RE::TESBoundObject>()) {
				if (a_form->GetFormType() == RE::FormType::Light) {
					equipManager->EquipObject(a_actor, a_object, nullptr, 1U, a_slot, a_queue, a_force, a_sound, false);
				} else if (HasItem(a_actor, a_form, a_extralist)) {
					equipManager->EquipObject(a_actor, a_object, a_extralist, 1U, a_slot, a_queue, a_force, a_sound, false);
				}
			}
		}
	}

	static void UnequipItem(RE::Actor* a_actor, RE::TESForm* a_form, RE::BGSEquipSlot* a_slot, RE::ExtraDataList* a_list, bool a_sound = false, bool a_queue = false, bool a_force = false)
	{
		if (!a_actor || !a_form) return;

		RE::ActorEquipManager* equipManager = RE::ActorEquipManager::GetSingleton();
		if (!equipManager) return;

		RE::TESBoundObject* a_object = a_form->As<RE::TESBoundObject>();
		if (a_object) equipManager->UnequipObject(a_actor, a_object, a_list, 1U, a_slot, a_queue, a_force, a_sound, false, nullptr);
	}

	static bool HasItem(RE::Actor* a_actor, RE::TESForm* a_form, RE::ExtraDataList* a_extralist)
	{
		if (!a_actor || !a_form) return false;

		auto inv = a_actor->GetInventory();
		for (const auto& [item, data] : inv) {
			const auto& [count, entry] = data;
			if (count < 1 || item->As<RE::TESForm>() != a_form || (a_extralist && entry->extraLists->front() != a_extralist)) continue;

			return true;
		}

		return false;
	}

	static bool SetGameSetting(const std::string& settingName, const std::variant<bool, float, int32_t, uint32_t, std::string>& newValue)
	{
		auto* gsc = RE::GameSettingCollection::GetSingleton();
		if (!gsc || settingName.empty()) return false;

		auto* setting = gsc->GetSetting(settingName.c_str());
		if (!setting) {
			logger::warn("SetGameSetting: setting \"{}\" not found", settingName);
			return false;
		}

		using SettingType = RE::Setting::Type;
		auto settingType = setting->GetType();

		switch (settingType) {
			case SettingType::kBool: if (auto value = std::get_if<bool>(&newValue)) { setting->data.b = *value; return true; } break;
			case SettingType::kFloat: if (auto value = std::get_if<float>(&newValue)) { setting->data.f = *value; return true; } break;
			case SettingType::kInteger: if (auto value = std::get_if<int32_t>(&newValue)) { setting->data.i = *value; return true; } break;
			case SettingType::kUnsignedInteger: if (auto value = std::get_if<uint32_t>(&newValue)) { setting->data.u = *value; return true; } break;
			case SettingType::kString:
				if (auto value = std::get_if<std::string>(&newValue)) {
					free(setting->data.s);
					setting->data.s = _strdup(value->c_str());
					return true;
				} break;
			default: return false;
		}

		return false;
	}

	template <typename T = RE::TESObjectREFR, typename HandleT>
	static T* ResolveHandle(const HandleT& handle)
	{
		auto ptr = handle ? handle.get() : nullptr;
		if (!ptr) return nullptr;

		return ptr->As<T>();
	}

	static bool IsFormIDValid(const RE::FormID formID)
	{
		return (formID > 0x0 && formID < 0xFFFFFFFF);
	}

	template <typename T = RE::TESObjectREFR>
	static T* GetValidReference(RE::FormID formID, const bool extraChecks = false)
	{
		if (!MiscUtils::IsFormIDValid(formID)) return nullptr;
		return GetValidReference<T>(RE::TESForm::LookupByID<RE::TESObjectREFR>(formID), extraChecks);
	}

	template <typename T = RE::TESObjectREFR>
	static T* GetValidReference(RE::TESObjectREFR* ref, const bool extraChecks = false)
	{
		using namespace ModData;

		if (!ref || !ref->As<T>() || !MiscUtils::IsFormIDValid(ref->formID) || ref->IsDeleted()) return nullptr;

		if (extraChecks) {
			if (ref->IsDisabled() || ref->IsMarkedForDeletion()) return nullptr;
		}

		if constexpr (std::is_same_v<T, RE::Actor>) {
			auto* refActor = ref->As<RE::Actor>();
			if (!refActor || !ref->Is(RE::FormType::ActorCharacter)) return nullptr;

			if (extraChecks && (refActor->GetActorRuntimeData().criticalStage != RE::ACTOR_CRITICAL_STAGE::kNone)) return nullptr;
		}

		return ref->As<T>();
	}
};
