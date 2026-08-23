#include "ExportManager.h"

#include "Naming.h"

namespace otoha
{
ExportManager::ExportManager (const juce::File& ffmpegExecutableHint)
    : ffmpegHint (ffmpegExecutableHint)
{
    worker = std::thread ([this] { workerLoop(); });
}

ExportManager::~ExportManager()
{
    cancelAll();
    {
        std::unique_lock<std::mutex> lk (lock);
        shutdown = true;
    }
    wakeWorker.notify_all();
    if (worker.joinable())
        worker.join();
}

juce::int64 ExportManager::submit (const ExportRequest& request)
{
    auto job = std::make_shared<Job>();
    job->request = request;
    job->id = nextId++;

    const std::unique_lock<std::mutex> lk (lock);
    allJobs.push_back (job);
    pending.push_back (*job);
    wakeWorker.notify_one();
    return job->id;
}

void ExportManager::cancelAll()
{
    {
        const std::unique_lock<std::mutex> lk (lock);

        // Waiting jobs: cancel immediately and drop from the queue.
        while (! pending.empty())
        {
            const auto jobId = pending.front().id;
            pending.pop_front();

            for (auto& j : allJobs)
                if (j->id == jobId && j->state == JobStatus::State::waiting)
                {
                    j->cancelFlag.store (false);   // not needed; state suffices
                    j->state = JobStatus::State::cancelled;
                }
        }

        // Active job: raise its flag; it stops on the next chunk/encode poll.
        for (auto& j : allJobs)
            if (j->state == JobStatus::State::rendering || j->state == JobStatus::State::encoding)
                j->cancelFlag.store (true);
    }
    wakeWorker.notify_all();
}

bool ExportManager::cancelJob (juce::int64 jobId)
{
    const std::unique_lock<std::mutex> lk (lock);
    bool found = false;

    for (auto it = pending.begin(); it != pending.end(); ++it)
    {
        if (it->id == jobId)
        {
            it->cancelFlag.store (true);
            pending.erase (it);
            found = true;
            break;
        }
    }

    for (auto& j : allJobs)
        if (j->id == jobId)
        {
            if (std::find_if (pending.begin(), pending.end(),
                              [jobId] (const Job& p) { return p.id == jobId; }) == pending.end())
                j->cancelFlag.store (true);   // active job stops on its next poll
            else
                j->state = JobStatus::State::cancelled;
            found = true;
        }

    wakeWorker.notify_all();
    return found;
}

bool ExportManager::retryJob (juce::int64 jobId)
{
    const std::unique_lock<std::mutex> lk (lock);

    for (auto& j : allJobs)
    {
        if (j->id == jobId
            && (j->state == JobStatus::State::failed || j->state == JobStatus::State::cancelled))
        {
            j->state = JobStatus::State::waiting;
            j->progress = 0.0f;
            j->errorText = {};
            j->cancelFlag.store (false);
            pending.push_back (*j);
            wakeWorker.notify_one();
            return true;
        }
    }
    return false;
}

std::vector<JobStatus> ExportManager::getStatuses() const
{
    const std::unique_lock<std::mutex> lk (lock);
    std::vector<JobStatus> result;
    result.reserve (allJobs.size());

    for (const auto& j : allJobs)
    {
        JobStatus s;
        s.id = j->id;
        s.displayName = j->request.baseName;
        s.state = j->state.load();
        s.progress = j->progress.load();
        s.errorText = j->errorText;
        s.outputFile = j->outputFile;
        result.push_back (s);
    }
    return result;
}

ExportManager::Summary ExportManager::getSummary() const
{
    Summary summary;
    for (const auto& s : getStatuses())
    {
        switch (s.state)
        {
            case JobStatus::State::completed:  ++summary.succeeded;  break;
            case JobStatus::State::failed:     ++summary.failed;     break;
            case JobStatus::State::cancelled:  ++summary.cancelled;  break;
            case JobStatus::State::skipped:    ++summary.skipped;    break;
            default: break;
        }
    }
    return summary;
}

// =============================================================================
// Worker
// =============================================================================
void ExportManager::workerLoop()
{
    for (;;)
    {
        Job jobCopy;
        bool have = false;

        {
            std::unique_lock<std::mutex> lk (lock);
            wakeWorker.wait (lk, [this] { return shutdown || ! pending.empty(); });

            if (shutdown && pending.empty())
                return;
            if (pending.empty())
                continue;

            jobCopy = pending.front();
            pending.pop_front();

            // Reflect "active" state on the shared record.
            for (auto& j : allJobs)
                if (j->id == jobCopy.id)
                {
                    j->state = JobStatus::State::rendering;
                    j->progress = 0.0f;
                }
        }

        const bool ok = jobCopy.cancelFlag.load() ? false : runJob (jobCopy);

        {
            const std::unique_lock<std::mutex> lk (lock);
            for (auto& j : allJobs)
            {
                if (j->id != jobCopy.id)
                    continue;

                if (jobCopy.cancelFlag.load())
                    j->state = JobStatus::State::cancelled;
                else if (! ok)
                    j->state = JobStatus::State::failed;
                else
                    j->state = JobStatus::State::completed;

                j->progress = ok ? 1.0f : j->progress.load();
                j->outputFile = ok ? resolveDestination (jobCopy.request.destinationDirectory,
                                                         jobCopy.request.baseName,
                                                         jobCopy.request.format,
                                                         CollisionPolicy::replace)
                                   : juce::File{};
            }
        }
    }
}

void ExportManager::loadPerRecordingState (const ExportRequest& request,
                                           std::shared_ptr<AudioDocument>& doc,
                                           const ProcessingState*& dsp) const
{
    doc = request.openDocument;

    if (doc == nullptr)
    {
        auto fresh = std::make_shared<AudioDocument>();
        juce::String error;
        if (! fresh->loadFromFile (request.sourceFile, error))
        {
            doc = nullptr;
            dsp = nullptr;
            return;
        }

        // Per-recording recovery (#25/#44): each recording restores its OWN
        // timeline + processing state from its sidecar when present.
        fresh->restoreFromSidecar();
        doc = fresh;
    }

    static ProcessingState overrideHolder;
    if (request.useDspOverride)
    {
        overrideHolder = request.dspStateOverride;
        dsp = &overrideHolder;
    }
    else
    {
        // The document's persisted state governs (off unless Enhance was saved).
        thread_local ProcessingState perDoc = doc->processing;
        perDoc = doc->processing;
        dsp = &perDoc;
    }
}

bool ExportManager::runJob (Job& job)
{
    std::shared_ptr<AudioDocument> doc;
    const ProcessingState* dsp = nullptr;
    loadPerRecordingState (job.request, doc, dsp);

    if (doc == nullptr)
    {
        const std::unique_lock<std::mutex> lk (lock);
        for (auto& j : allJobs)
            if (j->id == job.id)
            {
                j->errorText = "Couldn't read this recording.\nThe file may be missing or damaged.";
                j->progress = 0.0f;
            }
        return false;
    }

    const auto destination = resolveDestination (job.request.destinationDirectory,
                                                 job.request.baseName,
                                                 job.request.format,
                                                 job.request.collision);
    if (destination == juce::File{})
    {
        const std::unique_lock<std::mutex> lk (lock);
        for (auto& j : allJobs)
            if (j->id == job.id)
            {
                j->state = JobStatus::State::skipped;
                j->progress = 1.0f;
            }
        return true;
    }

    AudioExportRequest exportRequest;
    exportRequest.document = doc;
    exportRequest.dsp = dsp;
    exportRequest.format = job.request.format;
    exportRequest.quality = job.request.quality;
    exportRequest.sampleRateOverride = job.request.sampleRateOverride;
    exportRequest.channelOverride = job.request.channelOverride;
    exportRequest.titleMetadata = job.request.baseName;

    FfmpegInfo info;
    const auto needsFfmpeg = capabilitiesFor (job.request.format).requiresFfmpeg;
    const auto encoderStatus = needsFfmpeg ? locator.locate (info) : EncoderStatus::available;

    if (needsFfmpeg && encoderStatus != EncoderStatus::available)
    {
        const std::unique_lock<std::mutex> lk (lock);
        for (auto& j : allJobs)
            if (j->id == job.id)
                j->errorText = encoderStatus == EncoderStatus::unsupported
                    ? "This FFmpeg build can't encode this format."
                    : "Compressed export isn't available right now.\nWAV export is still available.";
        return false;
    }

    juce::String error;

    const bool ok = AudioExporter::exportAudio (
        exportRequest,
        destination,
        info.path.isNotEmpty() ? juce::File (info.path) : juce::File{},
        job.cancelFlag,
        [this, jobId = job.id] (float progress)
        {
            const std::unique_lock<std::mutex> lk (lock);
            for (auto& j : allJobs)
                if (j->id == jobId)
                    j->progress = progress;
        },
        error);

    if (! ok && ! job.cancelFlag.load())
    {
        const std::unique_lock<std::mutex> lk (lock);
        for (auto& j : allJobs)
            if (j->id == job.id)
                j->errorText = error;
    }

    return ok;
}
