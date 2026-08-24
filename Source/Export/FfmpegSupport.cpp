#include "FfmpegSupport.h"

#include <chrono>
#include <thread>

namespace otoha
{
// =============================================================================
// Discovery
// =============================================================================
namespace
{
// Custom-path storage kept deliberately dependency-light: a one-line text file
// under <base>/Database/, so this module needs nothing beyond juce_core.
juce::File customPathFileFor (const juce::File& base)
{
    return base.getChildFile ("Database").getChildFile ("otoha-export.path");
}
} // namespace

void FfmpegLocator::setCustomPath (const juce::File& base, const juce::String& path)
{
    const auto file = customPathFileFor (base);
    file.getParentDirectory().createDirectory();
    file.replaceWithText (path.trim());
}

juce::String FfmpegLocator::getCustomPath (const juce::File& base)
{
    const auto file = customPathFileFor (base);
    return file.existsAsFile() ? file.loadFileAsString().trim() : juce::String();
}

EncoderStatus FfmpegLocator::locate (FfmpegInfo& out)
{
    if (cached)
    {
        out = cachedInfo;
        return cachedStatus;
    }

    const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();

    // 1. user-configured  2. bundled next to the executable  3. PATH
    // PATH lookup is left to CreateProcess/exec semantics by passing the bare
    // name — but we still validate with -version before trusting it (#7).
    const auto candidates = juce::StringArray {
        getCustomPath (juce::File::getSpecialLocation (juce::File::userHomeDirectory)),
        exeDir.getChildFile (
#if JUCE_WINDOWS
            "ffmpeg.exe"
#else
            "ffmpeg"
#endif
        ).getFullPathName(),
        "ffmpeg"
    };

    for (const auto& candidate : candidates)
    {
        if (candidate.trim().isEmpty())
            continue;

        const auto status = probePath (candidate.trim(), cachedInfo);
        if (status == EncoderStatus::available)
        {
            cachedStatus = EncoderStatus::available;
            cached = true;
            out = cachedInfo;
            return status;
        }
        if (status == EncoderStatus::unsupported)
        {
            // Found but wrong build — keep probing other candidates.
            cachedInfo = {};
        }
    }

    cachedStatus = EncoderStatus::unavailable;
    cachedInfo = {};
    cached = true;
    out = cachedInfo;
    return cachedStatus;
}

EncoderStatus FfmpegLocator::probePath (const juce::String& executable, FfmpegInfo& out) const
{
    // Small, fast-exiting command; read to EOF with the default generous
    // timeout rather than a hand-tuned millisecond count.
    juce::ChildProcess process;
    if (! process.start ("\"" + executable + "\" -version",
                         juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        return EncoderStatus::unavailable;

    process.waitForProcessToFinish (-1);
    const auto output = process.readAllProcessOutput();

    if (! output.contains ("ffmpeg version"))
        return EncoderStatus::unsupported;

    // Validate major version within our supported range: 4.x–7.x.
    const auto versionLine = output.upToFirstOccurrenceOf ("\n", false, false);
    auto digits = versionLine.fromFirstOccurrenceOf ("version", false, true).trim().getCharPointer();
    const int major = (int) digits.getDoubleValue();   // e.g. "6.1.1" -> 6

    if (major < 4 || major > 7)
        return EncoderStatus::unsupported;

    out.path = executable;
    out.versionText = versionLine.trim();
    return EncoderStatus::available;
}

// =============================================================================
// Encoding
// =============================================================================
bool FfmpegEncoder::encode (const juce::File& ffmpegExecutable,
                            const juce::File& intermediateAudio,
                            const juce::File& destination,
                            const Request& request,
                            const std::atomic<bool>& cancelFlag,
                            std::function<void (float)> progress,
                            juce::String& errorOut)
{
    juce::String codecArgs;
    juce::String muxer;

    switch (request.format)
    {
        case ExportFormat::m4a:
            codecArgs << "-c:a aac -b:a " << request.bitrateKbps << "k";
            muxer = "-f ipod";                       // M4A container
            break;
        case ExportFormat::opus:
            codecArgs << "-c:a libopus -b:a " << request.bitrateKbps << "k";
            muxer = "-f ogg";
            break;
        case ExportFormat::mp3:
            codecArgs << "-c:a libmp3lame -b:a " << request.bitrateKbps << "k -write_xing 1";
            muxer = "-f mp3";
            break;
        default:
            errorOut = "This format does not use FFmpeg.";
            return false;
    }

    // Raw PCM in via pipe would be ideal long-term; today JUCE's ChildProcess
    // cannot write to stdin, so we feed a verified intermediate file instead —
    // correctness first (#36 allows this fallback explicitly).

    // M15 #72 hardening: we build one quoted command string for ChildProcess.
    // A path containing an embedded double-quote could terminate its quoting
    // section early on Unix shells. Windows filenames cannot contain '"', and
    // Otoha's own temp/intermediate names never do — but a user-chosen
    // destination on macOS/Linux could. Refuse rather than escape-and-pray.
    if (intermediateAudio.getFullPathName().containsChar ('"')
        || destination.getFullPathName().containsChar ('"'))
    {
        errorOut = "Couldn't export to that location.\n"
                   "Please choose a destination whose path doesn't contain quote characters.";
        return false;
    }

    juce::String command;
    command << "\"" << ffmpegExecutable.getFullPathName() << "\""
            << " -hide_banner -loglevel info -y"
            << " -i \"" << intermediateAudio.getFullPathName() << "\""
            << " " << muxer << " " << codecArgs;

    if (request.titleMetadata.isNotEmpty())
        command << " -metadata title=\"" << request.titleMetadata.replace ("\"", "") << "\"";

    command << " \"" << destination.getFullPathName() << "\"";

    juce::ChildProcess process;
    if (! process.start (command,
                         juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
    {
        errorOut = "Couldn't start the encoder.\nCompressed export may be unavailable right now.";
        return false;
    }

    // Poll the process for completion, honouring cancellation. FFmpeg's own
    // stderr progress parsing is not portable across ChildProcess versions,
    // so progress is reported as a smooth crawl toward done and snapped to 1
    // on success — honest, and never stalls the UI thread (we're background).
    const double duration = request.durationSeconds;
    const auto startTime = std::chrono::steady_clock::now();

    while (process.isRunning())
    {
        if (cancelFlag.load())
        {
            process.kill();                       // terminate cleanly; temp cleanup below
            errorOut = "Export cancelled.";
            return false;
        }

        if (progress != nullptr)
        {
            const double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - startTime).count();
            // Crawl toward done based on expected encode time; never claims
            // completion before FFmpeg actually exits.
            const double expected = duration > 0.0 ? duration * 1.5 : 30.0;
            progress ((float) juce::jlimit (0.0, 0.9, elapsed / expected));
        }

        std::this_thread::sleep_for (std::chrono::milliseconds (100));
    }

    const auto exitCode = process.getExitCode();

    if (cancelFlag.load())
    {
        errorOut = "Export cancelled.";
        return false;
    }

    if (exitCode != 0 || ! destination.existsAsFile() || destination.getSize() == 0)
    {
        errorOut = "The encoder reported an error for this file.\n"
                   "Your original recording is still safe.";
        destination.deleteFile();
        return false;
    }

    if (progress != nullptr)
        progress (1.0f);

    return true;
}
} // namespace otoha
