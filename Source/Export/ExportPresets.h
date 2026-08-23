#pragma once

#include <juce_core/juce_core.h>

#include "ExportTypes.h"

/*
    ExportPresets — the single place export preset values live (tune here only).

    Also remembers the user's last format/quality/directory in a properties
    file, per platform conventions.
*/
namespace otoha
{
struct ExportPreset
{
    ExportFormat   format;
    ExportQuality  quality;
    int  sampleRate  = 0;    // 0 = keep the source rate
    int  channels    = 0;    // 0 = keep the source channels
};

struct ExportPresetEntry
{
    const char*  name;
    ExportPreset preset;
};

/** The eight launch presets, in display order. */
const std::vector<ExportPresetEntry>& allExportPresets();

/** Last-used settings persistence (format/quality/directory). */
class ExportSettingsStore
{
public:
    explicit ExportSettingsStore (const juce::File& otohaBaseDirectory);

    ExportFormat getLastFormat() const;
    ExportQuality getLastQuality() const;
    juce::File getLastDirectory() const;

    void remember (ExportFormat f, ExportQuality q, const juce::File& directory);

private:
    juce::PropertiesFile properties;
};
} // namespace otoha
