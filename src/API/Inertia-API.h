#pragma once

/*******************************************************************
* INERTIA - API
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
            if (!InertiaAPI::LoadAPI()) {
				REPORT_AND_FAIL("Failed to bound to the Inertia API");
			}
			InertiaAPI::g_API->GetVersion();
        }
        break;
    }
});
*/

// Define the API type key
#define INER_API_TYPE_KEY static_cast<uint32_t>(std::byteswap('INER'))

// Define the API version in a structured format
#define INER_API_VERSION_MAJOR 1
#define INER_API_VERSION_MINOR 0
#define INER_API_VERSION_PATCH 0

// Combine the version numbers into a single value
#define INER_API_VERSION ((INER_API_VERSION_MAJOR << 16) | (INER_API_VERSION_MINOR << 8) | INER_API_VERSION_PATCH)

namespace InertiaAPI
{
	class InertiaAPI
	{
	public:
		// API functions
		virtual size_t GetAPIVersion() const;
		
		virtual std::vector<uint32_t> GetVersion() const;

		virtual void ApplyInertia(RE::TESObjectREFR* object, const float duration = 0.0f) const;
	};

	// Global API pointer
	inline extern InertiaAPI* g_API = nullptr;

	// Call this function only after the kDataLoaded event
	inline bool LoadAPI()
	{
		if (g_API != nullptr) return true;
		SKSE::GetMessagingInterface()->Dispatch(INER_API_TYPE_KEY, (void*)&g_API, sizeof(void*), NULL);
		if (g_API) { // API successfully received!
			// Check if the API version matches
			return (g_API->GetAPIVersion() == INER_API_VERSION);
		}
		return false;
	}
}
