#include "ExportPresets.h"

namespace otoha
{
const std::vector<ExportPresetEntry>& allExportPresets()
{
    static const std::vector<ExportPresetEntry> presets =
    {
        { "Lossless WAV",    { ExportFormat::wav,  ExportQuality::standard, 0, 0 } },
        { "Lossless FLAC",   { ExportFormat::flac, ExportQuality::standard, 0, 0 } },
        { "M4A Standard",    { ExportFormat::m4a,  ExportQuality::standard, 0, 0 } },
        { "M4A High",        { ExportFormat::m4a,  ExportQuality::high,     0, 0 } },
        { "Opus Small",      { ExportFormat::opus, ExportQuality::small,    0, 0 } },
        { "Opus Standard",   { ExportFormat::opus, ExportQuality::standard, 0, 0 } },
        { "Opus High",       { ExportFormat::opus, ExportQuality::high,     0, 0 } },
        { "MP3 Standard",    { ExportFormat::mp3,  ExportQuality::standard, 0, 0 } },
        { "MP3 High",        { ExportFormat::mp3,  ExportQuality::high,     0, 0 } },
    };
    return presets;
}

// =============================================================================
// Last-used settings
// =============================================================================
ExportSettingsStore::ExportSettingsStore (const juce::File& otohaBaseDirectory)
{
    juce::PropertiesFile::Options options;
    options.applicationName     = "otoha-export";
    options.filenameSuffix      = ".properties";
    options.folderName          = otohaBaseDirectory.getChildFile ("Database").getFullPathName();
    options.storageFormat       = juce::PropertiesFile::storeAsXML;
    properties.setOptions (options);
    properties.load();
}

ExportFormat ExportSettingsStore::getLastFormat() const
{
    const auto v = properties.getValue ("format", "wav");
    if (v == "flac") return ExportFormat::flac;
    if (v == "m4a")  return ExportFormat::m4a;
    if (v == "opus") return ExportFormat::opus;
    if (v == "mp3")  return ExportFormat::mp3;
    return ExportFormat::wav;
}

ExportQuality ExportSettingsStore::getLastQuality() const
{
    const auto v = properties.getValue ("quality", "standard");
    return v == "small" ? ExportQuality::small
         : v == "high"  ? ExportQuality::high : ExportQuality::standard;
}

juce::File ExportSettingsStore::getLastDirectory() const
{
    const auto path = properties.getValue ("directory", {});
    return path.isEmpty()
        ? juce::File::getSpecialLocation (juce::File::userMusicDirectory)
        : juce::File (path);
}

void ExportSettingsStore::remember (ExportFormat f, ExportQuality q, const juce::File& directory)
{
    properties.setValue ("format", formatToString (f));
    properties.setValue ("quality",
                         q == ExportQuality::small ? "small"
                       : q == ExportQuality::high  ? "high" : "standard");
    if (directory != juce::File{})
        properties.setValue ("directory", directory.getFullPathName());
    properties.saveIfNeeded();
}
} // namespace otoha
