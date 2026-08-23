#pragma once

#include "../Platform/DeviceProfiles.h"

#include <juce_core/juce_core.h>

#include <juce_core/juce_core.h>

/*
    ProfileStorage — persists Otoha Sound device profiles as JSON.

    Location: <Otoha base>/Sound/profiles.json

    Plain data only: profile ids/names, device bindings and DSP state.
    Nothing here is a secret (#35) — device identifiers are opaque strings
    provided by the backend, not credentials.
*/
namespace otoha::sound
{
inline juce::File profilesFile (const juce::File& otohaBaseDirectory)
{
    return otohaBaseDirectory.getChildFile ("Sound").getChildFile ("profiles.json");
}

/** Loads profiles into `manager`. Missing/corrupt file => manager stays empty. */
inline void loadProfiles (platform::ProfileManager& manager, const juce::File& otohaBaseDirectory)
{
    const auto file = profilesFile (otohaBaseDirectory);
    if (! file.existsAsFile())
        return;

    const auto parsed = juce::JSON::parse (file);
    const auto* arr = parsed.getArray();
    if (arr == nullptr)
        return;   // corrupt or future format: start fresh, never crash

    for (const auto& v : *arr)
    {
        platform::AudioProfile p;
        p.id             = v.getProperty ("id", {}).toString().toStdString();
        p.name           = v.getProperty ("name", {}).toString().toStdString();
        p.outputDeviceId = v.getProperty ("outputDeviceId", {}).toString().toStdString();
        p.enabled        = (bool) (int) v.getProperty ("enabled", 1);
        p.dspState       = ProcessingState::fromJSON (v.getProperty ("dspState", {}));

        if (! p.id.empty())
            manager.upsert (p);
    }
}

/** Atomically writes all profiles (temp file + move). */
inline bool saveProfiles (const platform::ProfileManager& manager, const juce::File& otohaBaseDirectory)
{
    juce::Array<juce::var> arr;
    for (const auto& p : manager.getProfiles())
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", juce::String (p.id));
        o->setProperty ("name", juce::String (p.name));
        o->setProperty ("outputDeviceId", juce::String (p.outputDeviceId));
        o->setProperty ("enabled", p.enabled);
        o->setProperty ("dspState", p.dspState.toJSON());
        arr.add (juce::var (o));
    }

    const auto file = profilesFile (otohaBaseDirectory);
    file.getParentDirectory().createDirectory();

    const auto temp = file.getSiblingFile (file.getFileName() + ".tmp");
    std::unique_ptr<juce::FileOutputStream> out (temp.createOutputStream());
    if (out == nullptr) return false;

    out->setPosition (0);
    out->truncate();
    out->writeText (juce::JSON::toString (juce::var (arr)), false, false);
    out->flush();
    out.reset();

    return temp.moveFileTo (file);   // atomic-ish replace
}
} // namespace otoha::sound
