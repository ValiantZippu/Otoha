#pragma once

#include "UnsupportedAudioBackend.h"

/*
    AndroidAudioBackend — Otoha SOUND on Android (M12).

    Honest placeholder: system-wide playback enhancement of OTHER apps is not
    possible on stock Android (no public loopback/virtual-endpoint API), and we
    do not fake it (#51/#52). This backend always reports its unsupported status,
    so any future Sound UI on Android shows exactly that instead of pretending.

    What IS real on Android is Studio: microphone → RecorderPhase → lossless
    source → timeline → DSP chain → render/export. Studio recording uses the
    platform input layer (JUCE's Android audio device support — AAudio on
    current Android, OpenSL ES fallback), NOT this Sound backend and NOT any
    Windows code. See docs/cross-platform.md.
*/
namespace otoha::platform
{
class AndroidAudioBackend : public UnsupportedAudioBackend
{
public:
    AndroidAudioBackend()
        : UnsupportedAudioBackend ("Android") {}

    const std::string& getUnsupportedReason() const
    {
        static const std::string androidReason =
            "Otoha Sound (system-wide enhancement) isn't possible on Android. "
            "Otoha Studio recording, editing, enhancing and exporting work normally.";
        return androidReason;
    }
};
} // namespace otoha::platform
