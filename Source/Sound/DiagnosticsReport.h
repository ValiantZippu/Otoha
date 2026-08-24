#pragma once

#include <juce_core/juce_core.h>

#ifndef OTOHA_VERSION
 #define OTOHA_VERSION "dev"   // test builds without the app target's define
#endif

/*
    DiagnosticsReport — the Advanced → Export diagnostics text (#44).

    Plain-text, technical-minimal, and guaranteed free of audio content:
    only versions, device names, stream numbers and counters ever enter this
    report. The UI saves it wherever the user chooses.
*/
namespace otoha::sound
{
struct DiagnosticsInput
{
    juce::String version     = OTOHA_VERSION;
    juce::String osName      = juce::SystemStats::getOperatingSystemName();
    juce::String backendName;         // e.g. "WASAPI shared loopback" / "unavailable"
    juce::String outputName;          // human-readable device name
    juce::String profileName;         // active device profile
    double  sampleRate         = 0.0;
    int     channels           = 0;
    int     bufferSizeFrames   = 0;
    bool    hasLatency         = false;   // honest optional fields (#27)
    double  latencyMs          = 0.0;
    unsigned long long underruns = 0;
    unsigned long long blocksProcessed = 0, blocksPassed = 0;
    bool    safeMode           = false;
};


inline juce::String buildDiagnosticsReport (const DiagnosticsInput& in)
{
    juce::StringArray lines;
    lines.add ("Otoha Diagnostics");
    lines.add ("-----------------");
    lines.add ("Version:    " + in.version);
    lines.add ("Windows/OS: " + in.osName);
    lines.add ("Backend:    " + (in.backendName.isEmpty() ? "not initialized" : in.backendName));
    lines.add ("Output:     " + (in.outputName.isEmpty() ? "(system default)" : in.outputName));

    juce::String stream;
    if (in.sampleRate > 0)
        stream = juce::String ((int) in.sampleRate) + " Hz, "
               + juce::String (in.channels) + " ch, "
               + juce::String (in.bufferSizeFrames) + "-frame buffer";
    else
        stream = "inactive";
    lines.add ("Stream:     " + stream);

    lines.add ("Latency:    " + (in.hasLatency
                                    ? "~" + juce::String ((int) in.latencyMs) + " ms"
                                    : juce::String ("information unavailable")));
    lines.add ("Underruns:  " + juce::String ((juce::uint64) in.underruns));
    lines.add ("DSP blocks: " + juce::String ((juce::uint64) in.blocksProcessed)
             + " processed, " + juce::String ((juce::uint64) in.blocksPassed) + " bypassed");
    lines.add ("Profile:    " + (in.profileName.isEmpty() ? "(none)" : in.profileName));
    lines.add ("Safe mode:  " + juce::String (in.safeMode ? "yes" : "no"));
    lines.add ("");
    lines.add ("No audio content is included in this report.");
    return lines.joinIntoString ("\n");
}
} // namespace otoha::sound
