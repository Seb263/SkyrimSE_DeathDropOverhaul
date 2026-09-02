#include "API/ModAPI.h"

#include "Core/Main.hpp"

namespace DDO_API
{
    class Impl_V1 : public Interface_V1
    {
    public:
        static Impl_V1* GetSingleton() noexcept
        {
            static Impl_V1 instance;
            return &instance;
        }

        REL::Version GetVersion() noexcept override
        {
			const auto plugin{ SKSE::PluginDeclaration::GetSingleton() };
			const auto name{ plugin->GetName() };
			const auto version{ plugin->GetVersion() };

			return version;
        }

		RE::TESObjectREFR* DropItemFromActor(RE::Actor* actor, RE::TESBoundObject* object,
			const RE::NiPoint3& position, const RE::NiPoint3& angle,
			const RE::hkVector4& linearVelocity, const RE::hkVector4& angularVelocity,
			const int count, RE::ExtraDataList* extraDataList) noexcept override
		{
			return ModCore::Main::DropItemFromActor(actor, object, position, angle, linearVelocity, angularVelocity, count, extraDataList);
		}

		RE::TESObjectREFR* DropItemFromActor(RE::Actor* actor, RE::TESBoundObject* object, RE::NiAVObject* node,
			const float linearIntertiaMult, const float angularIntertiaMult,
			const int count, RE::ExtraDataList* extraDataList) noexcept override
		{
			return ModCore::Main::DropItemFromActor(actor, object, node, linearIntertiaMult, angularIntertiaMult, count, extraDataList);
		}

		RE::ExtraDataList* GetEquippedExtraDataList(RE::Actor* actor, RE::TESBoundObject* object, const bool isLeft) noexcept override
		{
			return ModCore::Main::GetEquippedExtraDataList(actor, object, isLeft);
		}
    };
}

extern "C" DLLEXPORT void* SKSEAPI RequestPluginAPI(DDO_API::InterfaceVersion version, const char* pluginName, REL::Version pluginVersion)
{
    if (!pluginName) {
        logger::error("DDO_API::RequestPluginAPI called with a nullptr plugin name");
        return nullptr;
    }

    void* api = nullptr;

    switch (version)
    {
        case DDO_API::InterfaceVersion::V1:
            api = DDO_API::Impl_V1::GetSingleton();
            break;
        default:
            logger::warn("RequestPluginAPI called with invalid InterfaceVersion {}", static_cast<uint8_t>(version));
            return nullptr;
    }

    logger::info("RequestPluginAPI called: [InterfaceVersion:{}], [PluginName:{}], [PluginVersion:{}]",
		static_cast<uint8_t>(version) + 1, pluginName, pluginVersion.string("."));

    return api;
}
