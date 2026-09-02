#pragma once

#include "DataHandler.hpp"

#include "Core/DespawnManager.hpp"
#include "Core/Maintenance.hpp"
#include "Core/Serialization.hpp"

#include "Utils/TimeUtils.hpp"

namespace Events
{
	using namespace ModData;
	using namespace ModCore;

	class Hooks
	{
	public:
		// Initialization of hooks and template functions
		static void InstallHooks()
		{
			REL::Relocation<std::uintptr_t> characterVtbl{ RE::VTABLE_Character[0] };
			_ResurrectHandler = characterVtbl.write_vfunc(REL::Module::IsVR() ? 0x0AD : 0x0AB, ResurrectHookTemplate);
			logger::info("ResurrectHandler hooked at address: 0x{:X}", _ResurrectHandler.address());

			REL::Relocation<std::uintptr_t> reanimateVtbl{ RE::VTABLE_ReanimateEffect[0] };
			_ReanimateHandler = reanimateVtbl.write_vfunc(0x14, ReanimateHookTemplate);
			logger::info("ReanimateHandler hooked at address: 0x{:X}", _ReanimateHandler.address());

			REL::Relocation<std::uintptr_t> playerVtbl{ RE::PlayerCharacter::VTABLE[0] };
			_UpdatePlayer = playerVtbl.write_vfunc(0xAD, UpdatePlayerTemplate);
			logger::info("UpdatePlayer hooked at address: 0x{:X}", _UpdatePlayer.address());

			stl::write_vfunc<RE::Character, PickUpObject>();
			stl::write_vfunc<RE::PlayerCharacter, PickUpObject>();
			logger::info("PickUpObject hooked at address: 0x{:X}", PickUpObject::func.address());
		};

	private:

		struct PickUpObject
		{
			static void thunk(RE::Actor* a_actor, RE::TESObjectREFR* a_object, std::int32_t a_count, bool a_arg3, bool a_playSound)
			{
				if (a_object) {
					SKSE::GetTaskInterface()->AddTask([objectFormID = a_object->formID]() {
						Serialization::Functions::RemoveBoundItem(objectFormID);
					});
				}

				return func(a_actor, a_object, a_count, a_arg3, a_playSound);
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr std::size_t idx()
			{
				return REL::Module::IsVR() ? 0x0CE : 0x0CC;
			}
		};

		static void ResurrectHookTemplate(RE::Character* a_this, bool a_resetInventory, bool a_attach3D)
		{
			SKSE::GetTaskInterface()->AddTask([a_this]() {
				if (RE::Actor* actor = a_this->As<RE::Actor>()) {
					Maintenance::RemoveAllActorBoundItems(actor);
				}
			});

			_ResurrectHandler(a_this, a_resetInventory, a_attach3D);
		}
		static inline REL::Relocation<decltype(ResurrectHookTemplate)> _ResurrectHandler;

		static void ReanimateHookTemplate(RE::ReanimateEffect* a_this)
		{
			if (SettingsIni::bRestoreGearOnReanimate && a_this->commandedActor.get()) {
				if (RE::Actor* victim = a_this->commandedActor.get()->As<RE::Actor>()) {
					auto items = Serialization::Functions::GetAllActorBoundItems(victim->formID);
					for (auto* ref : items) {
						if (SettingsIni::iMiscDisableInventoryRestoration < 2) {
							DespawnManager::Enqueue(victim, ref);
						}
					}
				}
			}
		
			_ReanimateHandler(a_this);
		}
		static inline REL::Relocation<decltype(ReanimateHookTemplate)> _ReanimateHandler;

		static void UpdatePlayerTemplate(RE::PlayerCharacter* a_this, float a_delta)
		{
			_UpdatePlayer(a_this, a_delta);
			if (!a_this) return;

			const auto* cell = a_this->GetParentCell();
			const auto currentCell = cell ? cell->formID : 0x0;
			if (currentCell != previousCell && a_this->Is3DLoaded()) {
				const bool reset = previousCell == 0x0;
				previousCell = currentCell;
				TRACE("Player moved to new cell: [{:08X}].", currentCell);

				TimeUtils::WaitAndCall(300ms, [reset](TimeUtils::CallResult result, std::chrono::nanoseconds) {
					if (result == TimeUtils::CallResult::kEndDone) {
						Maintenance::MaintainLoadedCells(reset);
					}
					return true;
				}, false);
			}
		}
		static inline REL::Relocation<decltype(UpdatePlayerTemplate)*> _UpdatePlayer;
	};
};
