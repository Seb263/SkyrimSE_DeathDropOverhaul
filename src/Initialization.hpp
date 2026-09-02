#pragma once

#include "DataHandler.hpp"
#include "Events.h"
#include "Hooks.hpp"
#include "SettingsIni.hpp"

#include "Core/Main.hpp"
#include "Core/Serialization.hpp"

#include "API/ModAPI.h"
#include "API/ModAPI-Legacy.h"
#include "API/Inertia-API.h"

#include "Utils/ModUtils.hpp"

namespace ModData
{
	class DataHandler
	{
	public:
		bool preLoaded = false;
		bool postLoaded = false;
		bool postLoadedAlternate = false;

		static DataHandler* GetSingleton()
		{
			static DataHandler singleton;
			return &singleton;
		}

		void PreLoadData()
		{
			if (preLoaded) return;
			preLoaded = true;

			TESdataHandler = RE::TESDataHandler::GetSingleton();

			LoadPluginsForms();
			ApplyGameSettings();
			
			Events::ModEventSink::LoadEvents();
			Events::Hooks::InstallHooks();
			Serialization::Functions::RegisterSerializationCallbacks();

			// OLD API
			if (!DDO_API_Legacy::g_API) DDO_API_Legacy::g_API = new DDO_API_Legacy::Interface;
			if (!SKSE::GetMessagingInterface()->RegisterListener(NULL, [](SKSE::MessagingInterface::Message* message) {
				switch (message->type) {
				case DDO_API_TYPE_KEY:
					message->dataLen = sizeof(DDO_API_Legacy::Interface*);
					*(DDO_API_Legacy::Interface**)message->data = DDO_API_Legacy::g_API;
					break;
				}
			})) REPORT_AND_FAIL("Unable to register API message listener.");
			else logger::info("Successfully registered API message listener.");
		}

		void PostLoadData()
		{
			if (postLoaded) return;
			postLoaded = true;

			if (InertiaAPI::LoadAPI()) {
				Inertia_API_Interface = InertiaAPI::g_API;
				logger::info("Inertia API registered successfully.");
			}
		}

		void PostLoadDataAlternate()
		{
			if (postLoadedAlternate) return;
			postLoadedAlternate = true;

			TimeUtils::DoWhile(100ms, [](TimeUtils::CallResult result, std::chrono::nanoseconds) {
				if (TimeUtils::IsEnd(result)) return true;

				auto player = RE::PlayerCharacter::GetSingleton();
				if (player && player->Is3DLoaded() && player->GetParentCell() && player->GetParentCell()->IsAttached()) {
					GetSingleton()->PostLoadData();
					return false;
				}

				return true;
			}, true);
		}

	private:
		static inline void LoadPluginsForms()
		{
			logger::info("Loading Plugins Froms Data...");

			for (const auto& formInfo : pluginForms) {
				*formInfo.formPtr = TESdataHandler->LookupForm(formInfo.formID, formInfo.pluginName.data());
				if (!*formInfo.formPtr && !formInfo.optional) {
					REPORT_AND_FAIL("ERROR: Form \"{}\" not found in \"{}\".", formInfo.pluginName, formInfo.name, formInfo.pluginName);
				}
			}

			logger::info("Loading Plugins Froms Data: DONE");
		}

		static inline void ApplyGameSettings()
		{
			logger::info("Applying Game Settings...");

			MiscUtils::SetGameSetting("iDeathDropWeaponChance", 0);

			logger::info("Applying Game Settings: DONE");
		}
	};
}
