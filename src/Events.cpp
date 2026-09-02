#include "Events.h"

namespace Events
{
	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESLoadGameEvent* event, RE::BSTEventSource<RE::TESLoadGameEvent>*)
	{
		ModData::lastLoadPoint = std::chrono::steady_clock::now();
		ModData::previousCell = 0x0;

		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESDeathEvent* event, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		if (event->dead) return continueEvent;

		RE::Actor* victim = event->actorDying && event->actorDying.get() ? event->actorDying->As<RE::Actor>() : nullptr;
		if (!victim) return continueEvent;

		ModCore::Main::ProceedActorDeath(victim);

		if (SettingsIni::fMiscFixFalseSummonedNPCDelay > 0.0f) {
			auto delayMs = std::chrono::milliseconds(static_cast<int>(std::lround(SettingsIni::fMiscFixFalseSummonedNPCDelay * 1000.0f)));

			TimeUtils::WaitAndCall(delayMs, [victimHandle = victim->GetHandle()]
			(TimeUtils::CallResult result, const std::chrono::nanoseconds) {
				if (result != TimeUtils::CallResult::kEndDone) return true;

				if (auto* victim = MiscUtils::ResolveHandle<RE::Actor>(victimHandle)) {
					ModCore::Maintenance::MaintainAllActorBoundItems(victim);
				}

				return true;
			});
		}

		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESResetEvent* event, RE::BSTEventSource<RE::TESResetEvent>*)
	{
		if (!event->object || !event->object.get()) return continueEvent;
		
		RE::Actor* resetActor = event->object->As<RE::Actor>();
		if (!resetActor) return continueEvent;

		if (ModCore::Maintenance::MaintainAllActorBoundItems(resetActor)) {
			TRACE("TESResetEvent trigerred on actor [REF:{:08X}].", resetActor->formID);
		}
			
		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESFormDeleteEvent* event, RE::BSTEventSource<RE::TESFormDeleteEvent>*)
	{
		if (!event->formID) return continueEvent;

		if (auto* actor = MiscUtils::GetValidReference<RE::Actor>(event->formID)) {
			if (ModCore::Maintenance::MaintainAllActorBoundItems(actor)) {
				TRACE("TESFormDeleteEvent trigerred on actor [REF:{:08X}].", actor->formID);
			}
		}

		return continueEvent;
	}
}
