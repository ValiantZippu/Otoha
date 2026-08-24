#pragma once

#include <juce_core/juce_core.h>

/*
    PlatformCapabilities — what THIS build can honestly do (M12 #50/#51).

    The UI asks the capability table before showing a feature. A capability
    that is false must correspond to hidden or clearly-disabled UI — never a
    button that pretends.

    Selection is compile-time per target via OTOHA_PLATFORM (set by CMake),
    with an explicit override for headless tests.
*/

namespace otoha
{
enum class PlatformKind { windows, macos, linux, android, ios, unknown };

struct PlatformCapabilities
{
    PlatformKind kind = PlatformKind::unknown;

    // --- Studio -------------------------------------------------------------
    bool microphoneRecording = true;
    bool batchExport         = true;

    // --- Sound (system-wide playback enhancement) ----------------------------
    bool systemWideOutputProcessing = false;
    bool systemTray                 = false;
    bool startupWithOS              = false;

    // --- Mobile lifecycle ------------------------------------------------------
    /** Android: true only when a foreground-service recording flow exists.
        iOS: false — OS suspends audio apps in background by policy. */
    bool backgroundRecording        = false;
    bool nativeShareSheet           = false;   // system share after export (#26)
    bool systemDocumentPicker       = false;   // import via OS file picker (#27)

    juce::String displayName() const
    {
        switch (kind)
        {
            case PlatformKind::windows: return "Windows";
            case PlatformKind::macos:   return "macOS";
            case PlatformKind::linux:   return "Linux";
            case PlatformKind::android: return "Android";
            case PlatformKind::ios:     return "iOS";
            case PlatformKind::unknown: break;
        }
        return "Unknown";
    }

    /** The honest, single place where capabilities are declared (#51).
        Tests may pass an explicit kind; production code uses the compiled target. */
    static PlatformCapabilities forPlatform (PlatformKind k)
    {
        PlatformCapabilities c;
        c.kind = k;

        switch (k)
        {
            case PlatformKind::windows:
                c.systemWideOutputProcessing = true;
                c.systemTray                 = true;
                c.startupWithOS              = true;
                break;

            case PlatformKind::android:
                c.microphoneRecording   = true;
                c.nativeShareSheet      = true;
                c.systemDocumentPicker  = true;
                // backgroundRecording stays false until the foreground-service
                // recording flow is implemented and hardware-tested (#21) —
                // we do not claim it speculatively.
                c.systemWideOutputProcessing = false;   // not possible on Android; never faked
                break;

            case PlatformKind::ios:
                c.microphoneRecording   = true;
                c.nativeShareSheet      = true;
                c.systemDocumentPicker  = true;
                c.backgroundRecording   = false;   // OS policy: no invisible recording
                break;

            case PlatformKind::macos:
            case PlatformKind::linux:
                // Shared Studio foundation builds today; Sound backends are
                // documented future work (docs/audio-backends.md), not faked.
                break;

            case PlatformKind::unknown:
                break;
        }
        return c;
    }

    static PlatformCapabilities current()
    {
#if defined(OTOHA_PLATFORM_WINDOWS)
        return forPlatform (PlatformKind::windows);
#elif defined(OTOHA_PLATFORM_MACOS)
        return forPlatform (PlatformKind::macos);
#elif defined(OTOHA_PLATFORM_LINUX)
        return forPlatform (PlatformKind::linux);
#elif defined(OTOHA_PLATFORM_ANDROID)
        return forPlatform (PlatformKind::android);
#elif defined(OTOHA_PLATFORM_IOS)
        return forPlatform (PlatformKind::ios);
#else
        return forPlatform (PlatformKind::unknown);
#endif
    }
};
} // namespace otoha
