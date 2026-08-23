#pragma once

#include "../Dsp/ProcessingState.h"

#include <string>
#include <vector>

/*
    DeviceProfiles — Otoha Sound's per-output-device DSP configuration.

    Concept:  Speakers -> Natural, Headphones -> Music, Bluetooth -> Bass.

    Resolution order (docs/profiles.md):
        1. profile bound to the active output device id
        2. the Default profile
        3. a neutral/bypassed state

    NOTE (M7): profiles are architecture only. Automatic device matching is
    NOT activated — ProfileManager::resolveForDevice exists so backends with
    reliable device identification can opt in later. Nothing here touches
    platform APIs.
*/

namespace otoha::platform
{
struct AudioProfile
{
    std::string      id;             // unique profile identifier
    std::string      name;           // human-readable ("Music", "Late Night")
    std::string      outputDeviceId; // empty => applies to any device (Default)
    ProcessingState  dspState;       // full, serializable DSP configuration
    bool             enabled = true;
};

class ProfileManager
{
public:
    /** Full list, including the built-in Default profile. */
    const std::vector<AudioProfile>& getProfiles() const { return profiles; }

    /**
        Adds or updates a profile with the same id. Returns its index.
        A device binding of "" makes it the Default-profile candidate.
    */
    int upsert (const AudioProfile& profile)
    {
        for (size_t i = 0; i < profiles.size(); ++i)
            if (profiles[i].id == profile.id) { profiles[i] = profile; return (int) i; }
        profiles.push_back (profile);
        return (int) profiles.size() - 1;
    }

    bool remove (const std::string& profileId)
    {
        for (size_t i = 0; i < profiles.size(); ++i)
            if (profiles[i].id == profileId) { profiles.erase (profiles.begin() + i); return true; }
        return false;
    }

    /** Explicit lookup by device binding (device profiles + default). */
    const AudioProfile* findForDevice (const std::string& deviceId) const
    {
        const AudioProfile* fallback = nullptr;
        for (const auto& p : profiles)
        {
            if (! p.enabled || p.outputDeviceId.empty())
                continue;
            if (p.outputDeviceId == deviceId)
                return &p;
            if (p.id == "default")
                fallback = &p;
        }
        return fallback;
    }

    /**
        Resolution: device-bound profile -> default profile -> nullptr.
        Callers decide what to do with nullptr (usually: neutral/bypassed).
        Intentionally not wired to automatic device switching yet (M7).
    */
    const AudioProfile* resolveForDevice (const std::string& deviceId) const
    {
        return findForDevice (deviceId);
    }

private:
    std::vector<AudioProfile> profiles;
};
} // namespace otoha::platform
