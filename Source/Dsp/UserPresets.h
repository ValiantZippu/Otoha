#pragma once

#include "../Dsp/ProcessingState.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <vector>

/*
    UserPresets — custom ("Save Preset") DSP configurations (#13).

    Built-in presets live in the DspPreset enum and are immutable by design;
    this store holds only user creations. Full CRUD, name-uniqueness with
    "(2)"-style disambiguation, JSON persistence (temp + atomic move), and a
    versioned file format. Pure data + logic: no UI, no audio.
*/
namespace otoha
{
struct UserPreset
{
    juce::String     id;       // stable, generated
    juce::String     name;     // unique among user presets
    ProcessingState  state;
    juce::String     basedOn;  // built-in preset it started from (informational)
};

inline constexpr int kUserPresetFileFormatVersion = 1;

class UserPresetStore
{
public:
    explicit UserPresetStore (juce::File storageDirectory)
        : directory (std::move (storageDirectory)) {}

    juce::File file() const { return directory.getChildFile ("custom-presets.json"); }

    /** Missing/corrupt file => empty store; never crashes (#35-style). */
    void load()
    {
        presets.clear();
        const auto f = file();
        if (! f.existsAsFile()) return;

        const auto parsed = juce::JSON::parse (f);
        const auto* arr = parsed.getProperty ("presets", {}).getArray();
        if (arr == nullptr) return;

        for (const auto& v : *arr)
        {
            UserPreset p;
            p.id      = v.getProperty ("id", {}).toString();
            p.name    = v.getProperty ("name", {}).toString();
            p.basedOn = v.getProperty ("basedOn", {}).toString();
            p.state   = ProcessingState::fromJSON (v.getProperty ("state", {}));
            if (p.id.isNotEmpty() && p.name.isNotEmpty())
                presets.push_back (p);
        }
    }

    bool save() const
    {
        juce::Array<juce::var> arr;
        for (const auto& p : presets)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("id", p.id);
            o->setProperty ("name", p.name);
            o->setProperty ("basedOn", p.basedOn);
            o->setProperty ("state", p.state.toJSON());
            arr.add (juce::var (o));
        }

        auto* root = new juce::DynamicObject();
        root->setProperty ("formatVersion", kUserPresetFileFormatVersion);
        root->setProperty ("presets", juce::var (arr));

        directory.createDirectory();
        const auto f = file();
        const auto temp = f.getSiblingFile (f.getFileName() + ".tmp");
        std::unique_ptr<juce::FileOutputStream> out (temp.createOutputStream());
        if (out == nullptr) return false;
        out->setPosition (0);
        out->truncate();
        out->writeText (juce::JSON::toString (juce::var (root)), false, false, "\n");
        out->flush();
        out.reset();

        return temp.moveFileTo (f);
    }

    // --- CRUD ----------------------------------------------------------------

    bool nameExists (const juce::String& name) const
    {
        return std::any_of (presets.begin(), presets.end(),
                            [&] (const UserPreset& p) { return p.name.equalsIgnoreCase (name); });
    }

    /** Creates `name`, auto-disambiguating "My Preset" -> "My Preset (2)".
        Returns the new id, or {} if the state was invalid. */
    juce::String create (const juce::String& desiredName, const ProcessingState& state,
                         const juce::String& basedOn = {})
    {
        if (desiredName.trim().isEmpty())
            return {};

        juce::String name = desiredName.trim();
        int suffix = 2;
        while (nameExists (name))
            name = desiredName.trim() + " (" + juce::String (suffix++) + ")";

        UserPreset p;
        p.id      = "user-" + juce::Uuid().toString();
        p.name    = name;
        p.state   = state;
        p.basedOn = basedOn;
        presets.push_back (p);
        return p.id;
    }

    bool rename (const juce::String& id, const juce::String& newName)
    {
        const auto trimmed = newName.trim();
        if (trimmed.isEmpty()) return false;

        for (auto& p : presets)
        {
            if (p.id != id) continue;
            if (p.name.equalsIgnoreCase (trimmed)) return true;   // no-op rename is fine

            juce::String name = trimmed;
            int suffix = 2;
            while (nameExists (name))
                name = trimmed + " (" + juce::String (suffix++) + ")";
            p.name = name;
            return true;
        }
        return false;
    }

    bool duplicate (const juce::String& id)
    {
        for (const auto& p : presets)
            if (p.id == id)
                return create (p.name + " copy", p.state, p.basedOn).isNotEmpty();
        return false;
    }

    bool remove (const juce::String& id)
    {
        const auto it = std::remove_if (presets.begin(), presets.end(),
                                        [&] (const UserPreset& p) { return p.id == id; });
        if (it == presets.end()) return false;
        presets.erase (it, presets.end());
        return true;
    }

    const UserPreset* get (const juce::String& id) const
    {
        for (const auto& p : presets)
            if (p.id == id) return &p;
        return nullptr;
    }

    /** Sorted by name (case-insensitive) so UI lists are stable. */
    std::vector<UserPreset> all() const
    {
        auto sorted = presets;
        std::sort (sorted.begin(), sorted.end(),
                   [] (const UserPreset& a, const UserPreset& b)
                   { return a.name.compareIgnoreCase (b.name) < 0; });
        return sorted;
    }

    int size() const { return (int) presets.size(); }

private:
    juce::File directory;
    std::vector<UserPreset> presets;
};
} // namespace otoha
