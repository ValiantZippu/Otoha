#include "FfmpegSupport.h"

#include <chrono>
#include <thread>

namespace otoha
{
// =============================================================================
// Discovery
// =============================================================================
void FfmpegLocator::setCustomPath (const juce::File& base, const juce::String& path)
{
    juce::PropertiesFile::Options options;
    options.applicationName = "otoha-export";
    options.filenameSuffix  = ".properties";
    options.folderName      = base.getChildFile ("Database").getFullPathName();
    options.storageFormat   = juce::PropertiesFile::storeAsXML;
    juce::PropertiesFile props (options);
    props.load();
    props.setValue ("ffmpeg.path", path);
    props.saveIfNeeded();
}

juce::String FfmpegLocator::getCustomPath (const juce::File& base)
{
    juce::PropertiesFile::Options options;
    options.applicationName = "otoha-export";
    options.filenameSuffix  = ".properties";
    options.folderName      = base.getChildFile ("Database").getFullPathName();
    options.storageFormat   = juce::PropertiesFile::storeAsXML;
    juce::PropertiesFile props (options);
    props.load();
    return props.getValue ("ffmpeg.path", {});
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
        exeDir.getChildFile (JUCE_WINDOWS ? "ffmpeg.exe" : "ffmpeg").getFullPathName(),
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
    juce::ChildProcess process;
    if (! process.start ("\"" + executable + "\" -version",
                         juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        return EncoderStatus::unavailable;

    const auto output = process.readAllProcessOutput (8000);
    process.waitForProcessToExit (2000);

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
    juce::String command;
    command << "\"" << ffmpegExecutable << "\""
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

    // Poll: parse stderr progress ("time=00:00:12.34") and honour cancellation.
    const double duration = request.durationSeconds.count();
    juce::String accumulated;
    auto lastProgressPush = std::chrono::steady_clock::now();

    while (process.isRunning())
    {
        if (cancelFlag.load())
        {
            process.kill();                       // terminate cleanly; temp cleanup below
            errorOut = "Export cancelled.";
            return false;
        }

        accumulated += process.readAllProcessOutput (0);

        const auto now = std::chrono::steady_clock::now();
        if (progress != nullptr && duration > 0.0
            && now - lastProgressPush > std::chrono::milliseconds (250))
        {
            const int timeIndex = accumulated.lastIndexOf ("time=");
            if (timeIndex >= 0)
            {
                const auto stamp = accumulated.substring (timeIndex + 5, timeIndex + 16);
                const auto parts = juce::StringArray::fromTokens (stamp, ":", "");
                if (parts.size() == 3)
                {
                    const double seconds = parts[0].getDoubleValue() * 3600.0
                                         + parts[1].getDoubleValue() * 60.0
                                         + parts[2].getDoubleValue();
                    progress ((float) juce::jlimit (0.0, 1.0, seconds / duration));
                }
            }
            lastProgressPush = now;
        }

        std::this_thread::sleep_for (std::chrono::milliseconds (60));
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
