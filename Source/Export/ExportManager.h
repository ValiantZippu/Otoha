#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "../Dsp/ProcessingState.h"
#include "../Library/LibraryModel.h"
#include "AudioExporter.h"
#include "ExportTypes.h"
#include "FfmpegSupport.h"

/*
    ExportManager — export as a service, not a UI feature.

        submit(request) -> jobId      cancel(jobId | all)
        retry(jobId)                  getStatuses() / getSummary()

    One job runs at a time on a single worker thread (#46). Each recording is
    loaded and decoded only when its turn comes; each uses ITS OWN edit/DSP
    state from its sidecar unless the request overrides it (#25, #44). The UI
    polls getStatuses() — the manager never touches widgets.
*/
namespace otoha
{
struct ExportRequest
{
    juce::File sourceFile;
    ProcessingState dspStateOverride;
    bool useDspOverride = false;     // false = read the recording's own sidecar state

    ExportFormat format = ExportFormat::wav;
    ExportQuality quality = ExportQuality::standard;
    int sampleRateOverride = 0;
    int channelOverride = 0;

    juce::String baseName;           // without extension (display name)
    juce::File destinationDirectory;
    CollisionPolicy collision = CollisionPolicy::keepBoth;

    /** Editor path: an already-open document (uses its live timeline+DSP).
        Batch path leaves this null so the manager loads fresh per-recording. */
    std::shared_ptr<const AudioDocument> openDocument;
};

struct JobStatus
{
    enum class State { waiting, rendering, encoding, completed, failed, cancelled, skipped };

    juce::int64 id = 0;
    juce::String displayName;
    std::atomic<State> state { State::waiting };
    std::atomic<float> progress { 0.0f };   // 0..1 for the active job
    juce::String errorText;
    juce::File outputFile;
};

class ExportManager
{
public:
    explicit ExportManager (const juce::File& ffmpegExecutableHint);
    ~ExportManager();

    juce::int64 submit (const ExportRequest& request);

    void cancelAll();                       // stops current + clears waiting
    bool cancelJob (juce::int64 jobId);
    bool retryJob (juce::int64 jobId);      // re-queues a failed/cancelled job

    std::vector<JobStatus> getStatuses() const;

    struct Summary { int succeeded = 0; int failed = 0; int cancelled = 0; int skipped = 0; };
    Summary getSummary() const;

private:
    struct Job
    {
        ExportRequest request;
        juce::int64 id = 0;
        std::atomic<bool> cancelFlag { false };
    };

    void workerLoop();
    bool runJob (Job& job);
    void loadPerRecordingState (const ExportRequest& request,
                                std::shared_ptr<AudioDocument>& doc,
                                const ProcessingState*& dsp) const;

    mutable std::mutex lock;
    std::condition_variable wakeWorker;
    std::deque<Job> pending;
    std::vector<std::shared_ptr<Job>> allJobs;   // status history (id order)
    juce::int64 nextId = 1;
    bool shutdown = false;

    std::thread worker;
    FfmpegLocator locator;
    juce::File ffmpegHint;
};

} // namespace otoha
