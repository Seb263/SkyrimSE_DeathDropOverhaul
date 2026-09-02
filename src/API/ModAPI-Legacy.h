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
            if (!DDO_API_Legacy::LoadAPI()) {
				util::report_and_fail("Failed to bound to the Death Drop Overhaul API");
			}
			DDO_API_Legacy::g_API->GetVersion();
        }
        break;
    }
});
*/

// Define the API type key
#define DDO_API_TYPE_KEY static_cast<uint32_t>(std::byteswap('DDO'))

// Define the API version in a structured format
#define DDO_API_VERSION_MAJOR 1
#define DDO_API_VERSION_MINOR 0
#define DDO_API_VERSION_PATCH 0

// Combine the version numbers into a single value
#define DDO_API_VERSION ((DDO_API_VERSION_MAJOR << 16) | (DDO_API_VERSION_MINOR << 8) | DDO_API_VERSION_PATCH)

namespace DDO_API_Legacy
{
	class Interface
	{
	public:
		// API functions
		virtual size_t GetAPIVersion() const;
		
		virtual std::vector<uint32_t> GetVersion() const;

		virtual RE::TESObjectREFR* DropItemFromActor(RE::Actor* actor, RE::TESBoundObject* object,
			const RE::NiPoint3& position, const RE::NiPoint3& angle,
			const RE::hkVector4& linearVelocity, const RE::hkVector4& angularVelocity) const;

		virtual RE::TESObjectREFR* DropItemFromActor(RE::Actor* actor, RE::TESBoundObject* object,
			RE::NiAVObject* node, const float linearIntertiaMult = 0.0f, const float angularIntertiaMult = 0.0f) const;
	};

	// Global API pointer
	inline extern Interface* g_API = nullptr;

	// Call this function only after the kDataLoaded event
	inline bool LoadAPI()
	{
		if (g_API != nullptr) return true;
		SKSE::GetMessagingInterface()->Dispatch(DDO_API_TYPE_KEY, (void*)&g_API, sizeof(void*), NULL);
		if (g_API) { // API successfully received!
			// Check if the API version matches
			return (g_API->GetAPIVersion() == DDO_API_VERSION);
		}
		return false;
	}
}
