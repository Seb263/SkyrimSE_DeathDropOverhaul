#pragma once

/*******************************************************************
* DEATH DROP OVERHAUL - API
* Do not forget to include this source file to your project!
*******************************************************************/

/* How to create a hook to the API and use it:
SKSE::GetMessagingInterface()->RegisterListener([](MessagingInterface::Message* message) 
{
    switch (message->type) 
    {
        case MessagingInterface::kPostLoadGame:
        case MessagingInterface::kNewGame:
        {
            if (auto* apiInterface = static_cast<DDO_API::Interface*>(DDO_API::GetAPI())) {
				auto apiVersion = apiInterface->GetVersion().string(".");
				logger::info("Death Drop Overhaul API v{} registered successfully.", apiVersion);
			} else {
				logger::warn("Death Drop Overhaul API not found.");
			}
        }
        break;
    }
});
*/

namespace DDO_API
{
	inline void* g_Interface = nullptr;

	enum class InterfaceVersion : uint8_t
	{
		V1,
		Latest = V1
	};

    class Interface_V1
    {
    public:
		virtual ~Interface_V1() = default;

		virtual REL::Version GetVersion() noexcept = 0;

		virtual RE::TESObjectREFR* DropItemFromActor(RE::Actor* actor, RE::TESBoundObject* object,
			const RE::NiPoint3& position, const RE::NiPoint3& angle,
			const RE::hkVector4& linearVelocity, const RE::hkVector4& angularVelocity,
			const int count = 1, RE::ExtraDataList* extraDataList = nullptr) noexcept = 0;

		virtual RE::TESObjectREFR* DropItemFromActor(RE::Actor* actor, RE::TESBoundObject* object, RE::NiAVObject* node,
			const float linearIntertiaMult = 0.0f, const float angularIntertiaMult = 0.0f,
			const int count = 1, RE::ExtraDataList* extraDataList = nullptr) noexcept = 0;

		virtual RE::ExtraDataList* GetEquippedExtraDataList(RE::Actor* actor, RE::TESBoundObject* object, const bool isLeft = false) noexcept = 0;
	};

	using Interface = Interface_V1;

	using _RequestPluginAPI = void* (*)(InterfaceVersion version, const char* pluginName, REL::Version pluginVersion);

    inline void* GetAPI(InterfaceVersion version = InterfaceVersion::Latest)
    {
        if (g_Interface) return g_Interface;

        const auto handle = GetModuleHandleA("DeathDropOverhaul.dll");
        if (!handle) return nullptr;

        const auto request = reinterpret_cast<_RequestPluginAPI>(GetProcAddress(handle, "RequestPluginAPI"));
        if (!request) return nullptr;

        const auto plugin = SKSE::PluginDeclaration::GetSingleton();
        g_Interface = request(version, std::string(plugin->GetName()).c_str(), plugin->GetVersion());

        return g_Interface;
    }
}
