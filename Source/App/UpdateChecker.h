#pragma once

#include <juce_core/juce_core.h>

#ifndef OTOHA_VERSION
 #define OTOHA_VERSION "0.0.0-dev"   // test builds without the app target's define
#endif

/*
    UpdateChecker — minimal, honest update architecture (#39-#41).

    Design decisions for 1.0:
      * No update server ships in this milestone: the default source is
        DisabledUpdateSource, which reports "unknown" instead of inventing a
        version. A real source (GitHub Releases API, etc.) plugs in via
        UpdateSource without touching any call-site.
      * Checking is always opt-in/user-visible; nothing forces or auto-installs
        (#40), and updates can never touch user data (#41 — the installer and
        AppSettings own that guarantee).
*/
namespace otoha
{
struct UpdateInfo
{
    bool         checked       = false;   // false == we genuinely don't know
    juce::String latestVersion;
    juce::String downloadUrl;
};

class UpdateSource
{
public:
    virtual ~UpdateSource() = default;
    virtual UpdateInfo fetchLatest() = 0;
};

/** Ships by default: performs no network I/O, honestly reports "unknown". */
class DisabledUpdateSource final : public UpdateSource
{
public:
    UpdateInfo fetchLatest() override { return {}; }
};

struct UpdateChecker
{
    /** Semantic-version compare of "MAJOR.MINOR.PATCH" strings.
        Returns -1 / 0 / +1 (a<b / equal / a>b). Non-numeric segments
        degrade gracefully rather than crashing. */
    static int compareVersions (const juce::String& a, const juce::String& b)
    {
        const auto partsOf = [] (const juce::String& v)
        {
            juce::StringArray parts;
            parts.addTokens (v, ".", {});
            return parts;
        };

        const auto pa = partsOf (a), pb = partsOf (b);
        const int n = juce::jmax (pa.size(), pb.size());
        for (int i = 0; i < n; ++i)
        {
            const long long va = i < pa.size() ? pa[i].getLargeIntValue() : 0;
            const long long vb = i < pb.size() ? pb[i].getLargeIntValue() : 0;
            if (va != vb) return va < vb ? -1 : 1;
        }
        return 0;
    }

    static bool isNewerThanCurrent (const juce::String& latest)
    {
        return compareVersions (latest, OTOHA_VERSION) > 0;
    }

    explicit UpdateChecker (UpdateSource* source = nullptr) : source (source) {}

    void setSource (UpdateSource* s) { source = s; }

    /** Synchronous, user-initiated check only (#40). Never called on startup. */
    UpdateInfo checkNow()
    {
        return source != nullptr ? source->fetchLatest() : UpdateInfo{};
    }

private:
    UpdateSource* source;
};
} // namespace otoha
