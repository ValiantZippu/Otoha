#pragma once

#include <juce_core/juce_core.h>

/*
    OtohaError — the shared, platform-independent error model (M12 #64/#65).

    Platform layers translate their native failures into exactly ONE of these
    categories; shared code never sees an HRESULT / errno / Android exception.
    UI layers render them via userMessage() — never raw enum names.

    Rule: every new failure path in shared code must pick a category here,
    never invent a string-only error.
*/

namespace otoha
{
enum class ErrorCategory
{
    none,
    permissionDenied,
    deviceUnavailable,
    unsupportedFormat,
    fileUnavailable,
    storageUnavailable,
    renderFailed,
    exportFailed,
    audioInterrupted
};

/** Which surface the message is shown on — only affects wording, never logic. */
enum class ErrorSurface
{
    desktop,
    mobile
};

inline const char* toString (ErrorCategory c)
{
    switch (c)
    {
        case ErrorCategory::none:               return "none";
        case ErrorCategory::permissionDenied:   return "permissionDenied";
        case ErrorCategory::deviceUnavailable:  return "deviceUnavailable";
        case ErrorCategory::unsupportedFormat:  return "unsupportedFormat";
        case ErrorCategory::fileUnavailable:    return "fileUnavailable";
        case ErrorCategory::storageUnavailable: return "storageUnavailable";
        case ErrorCategory::renderFailed:       return "renderFailed";
        case ErrorCategory::exportFailed:       return "exportFailed";
        case ErrorCategory::audioInterrupted:   return "audioInterrupted";
    }
    return "unknown";
}

/** User-facing text. Deliberately short, actionable, jargon-free (#65).
    Internal detail strings may be appended by callers for diagnostics views
    only — they must never replace this message in the main UI. */
inline juce::String userMessage (ErrorCategory c, ErrorSurface surface)
{
    const bool mobile = surface == ErrorSurface::mobile;
    switch (c)
    {
        case ErrorCategory::permissionDenied:
            return mobile ? "Microphone access is required to record."
                          : "Otoha doesn't have permission to use this device.";
        case ErrorCategory::deviceUnavailable:
            return mobile ? "Microphone unavailable."
                          : "Audio output unavailable.";
        case ErrorCategory::unsupportedFormat:
            return "Otoha can't open this audio format.";
        case ErrorCategory::fileUnavailable:
            return "This recording's source file has moved or was deleted.";
        case ErrorCategory::storageUnavailable:
            return "There isn't enough free storage to continue.";
        case ErrorCategory::renderFailed:
            return "Couldn't process this recording. Try again, or simplify the edit.";
        case ErrorCategory::exportFailed:
            return "Export failed. Check the destination folder and try again.";
        case ErrorCategory::audioInterrupted:
            return "Recording was interrupted (call or another app took the microphone).";
        case ErrorCategory::none:
            break;
    }
    return {};
}

/** A categorized failure as shared code passes it around. `detail` is for
    diagnostics exports only (never shown as the primary message). */
struct OtohaError
{
    ErrorCategory category = ErrorCategory::none;
    juce::String detail;   // e.g. "AAudio ERROR_INVALID_HANDLE" — diagnostics only

    bool ok() const  { return category == ErrorCategory::none; }
};
} // namespace otoha
