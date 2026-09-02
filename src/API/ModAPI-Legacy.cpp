#include "ModAPI-Legacy.h"

#include "Core/Main.hpp"

size_t DDO_API_Legacy::Interface::GetAPIVersion() const
{
	return DDO_API_VERSION;
}

std::vector<uint32_t> DDO_API_Legacy::Interface::GetVersion() const
{
	using namespace SKSE;
	const auto* plugin = PluginDeclaration::GetSingleton();
	auto        version = plugin->GetVersion();

	uint32_t versionMajor = plugin->GetVersion().major();
	uint32_t versionMinor = plugin->GetVersion().minor();
	uint32_t versionPatch = plugin->GetVersion().patch();

	std::vector<uint32_t> versionVector;
	versionVector.push_back(versionMajor);
	versionVector.push_back(versionMinor);
	versionVector.push_back(versionPatch);

	return versionVector;
}

RE::TESObjectREFR* DDO_API_Legacy::Interface::DropItemFromActor(RE::Actor* actor, RE::TESBoundObject* object,
	const RE::NiPoint3& position, const RE::NiPoint3& angle,
	const RE::hkVector4& linearVelocity, const RE::hkVector4& angularVelocity) const
{
	return ModCore::Main::DropItemFromActor(actor, object, position, angle, linearVelocity, angularVelocity);
}

RE::TESObjectREFR* DDO_API_Legacy::Interface::DropItemFromActor(RE::Actor* actor, RE::TESBoundObject* object,
	RE::NiAVObject* node, const float linearIntertiaMult, const float angularIntertiaMult) const
{
	return ModCore::Main::DropItemFromActor(actor, object, node, linearIntertiaMult, angularIntertiaMult);
}
