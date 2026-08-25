#pragma once

#include <juce_core/juce_core.h>

/*
    AppSettings — Otoha's single configuration store (Milestone 10).

    Separation of concerns (#29):
      * Application files   -> install directory (managed by the installer)
      * User settings       -> %APPDATA%/Otoha (survive upgrades & uninstall)

    Versioning (#42): every file carries `configVersion`. Older files run
    through migrateSettings() step by step; nothing is ever silently dropped.
    Corrupt or future-version files fall back to defaults — never a crash.

    Everything here is plain data + JSON: no UI, no audio, fully testable.
*/
namespace otoha
{
inline constexpr int kCurrentConfigVersion = 1;

// --- sound preferences (the subset SoundView persists) -----------------------
struct SoundPrefs
{
    bool         enabled            = false;    // master ON/OFF at last exit
    float        enhanceAmount      = 1.0f;     // 0..1
    juce::String presetName         = "Natural";// conservative default (#5)
    juce::String outputDeviceId;                    // empty == System Default
    bool         autoSwitchProfiles = false;    // conservative default (#36)
};

struct AppSettings
{
    int          configVersion        = kCurrentConfigVersion;
    bool         firstLaunchComplete  = false;   // #3 onboarding gate
    bool         startWithSystem      = false;   // explicit opt-in only (#38)
    juce::String lastRunVersion;                     // informational only

    // M24 appearance
    juce::String appearanceMode  = "system";   // "system" / "light" / "dark"
    juce::String accentName      = "Sakura";   // key into accentPalette()

    // Session-scoped: set from the --safe-mode command line (#45), never
    // persisted, never sticky across restarts.
    bool         safeModeSession      = false;

    SoundPrefs   sound;
};

/** Default per-user settings directory (%APPDATA%/Otoha on Windows). */
inline juce::File defaultSettingsDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
              .getChildFile ("Otoha");
}

inline juce::File settingsFileIn (const juce::File& directory)
{
    return directory.getChildFile ("settings.json");
}

/** DynamicObject::getProperty takes a single argument in current JUCE; this
    restores the convenient "value or default" form via the NamedValueSet. */
inline juce::var propOr (const juce::DynamicObject& o, const juce::Identifier& id,
                         juce::var fallback = {})
{
    return o.getProperties().getWithDefault (id, fallback);
}

// --- JSON round-trip ---------------------------------------------------------

inline juce::var settingsToVar (const AppSettings& s)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("configVersion", s.configVersion);
    root->setProperty ("firstLaunchComplete", s.firstLaunchComplete);
    root->setProperty ("startWithSystem", s.startWithSystem);
    root->setProperty ("lastRunVersion", s.lastRunVersion);
    root->setProperty ("appearanceMode", s.appearanceMode);
    root->setProperty ("accentName", s.accentName);

    auto* snd = new juce::DynamicObject();
    snd->setProperty ("enabled", s.sound.enabled);
    snd->setProperty ("enhanceAmount", (double) s.sound.enhanceAmount);
    snd->setProperty ("presetName", s.sound.presetName);
    snd->setProperty ("outputDeviceId", s.sound.outputDeviceId);
    snd->setProperty ("autoSwitchProfiles", s.sound.autoSwitchProfiles);
    root->setProperty ("sound", juce::var (snd));

    return juce::var (root);
}

/**
    Step-wise migration chain (#42). Each step upgrades exactly one version so
    future releases append `if (from < N) { ... }` blocks without touching old
    ones. Returns true when the result is usable.
*/
inline bool migrateSettingsVar (juce::DynamicObject& root, int fromVersion)
{
    if (fromVersion < 1)
    {
        // v0 -> v1: initial schema. All v1 fields already have defaults via
        // settingsFromVar(), so this step only stamps the version.
        root.setProperty ("configVersion", 1);
    }
    return true;
}

inline AppSettings settingsFromVar (const juce::var& v)
{
    AppSettings s;   // defaults survive any missing/corrupt field

    const auto* obj = v.getDynamicObject();
    if (obj == nullptr)
        return s;

    s.configVersion       = (int) propOr (*obj, "configVersion", kCurrentConfigVersion);
    s.firstLaunchComplete = (bool) (int) propOr (*obj, "firstLaunchComplete", 0);
    s.startWithSystem     = (bool) (int) propOr (*obj, "startWithSystem", 0);
    s.lastRunVersion      = propOr (*obj, "lastRunVersion").toString();
    s.appearanceMode      = propOr (*obj, "appearanceMode", "system").toString();
    s.accentName          = propOr (*obj, "accentName", "Sakura").toString();

    const auto snd = propOr (*obj, "sound");
    s.sound.enabled            = (bool) (int) snd.getProperty ("enabled", 0);
    s.sound.enhanceAmount      = (float) (double) snd.getProperty ("enhanceAmount", 1.0);
    s.sound.presetName         = snd.getProperty ("presetName", "Natural").toString();
    s.sound.outputDeviceId     = snd.getProperty ("outputDeviceId", {}).toString();
    s.sound.autoSwitchProfiles = (bool) (int) snd.getProperty ("autoSwitchProfiles", 0);

    // Future versions we don't understand: keep defaults rather than guess.
    if (s.configVersion > kCurrentConfigVersion)
        s = {};
    else if (auto* mutableRoot = v.getDynamicObject())
        migrateSettingsVar (*mutableRoot, s.configVersion);   // in-place upgrade

    s.configVersion = kCurrentConfigVersion;
    return s;
}

/** Missing or corrupt file => defaults, never a crash (#35-style resilience). */
inline bool loadAppSettings (AppSettings& out, const juce::File& directory)
{
    const auto file = settingsFileIn (directory);
    if (! file.existsAsFile())
        return false;

    out = settingsFromVar (juce::JSON::parse (file));
    return true;
}

/** Atomic write (temp + move) so a crash mid-save cannot corrupt settings. */
inline bool saveAppSettings (const AppSettings& s, const juce::File& directory)
{
    directory.createDirectory();
    const auto file = settingsFileIn (directory);
    const auto temp = file.getSiblingFile (file.getFileName() + ".tmp");

    std::unique_ptr<juce::FileOutputStream> out (temp.createOutputStream());
    if (out == nullptr) return false;
    out->setPosition (0);
    out->truncate();
    out->writeText (juce::JSON::toString (settingsToVar (s)), false, false, "\n");
    out->flush();
    out.reset();

    return temp.moveFileTo (file);
}

/** #46/#47: two distinct resets — audio prefs only vs. everything. */
inline void resetAudioPrefs (AppSettings& s) { s.sound = {}; }
inline void resetAllSettings  (AppSettings& s) { s = {}; }
} // namespace otoha
