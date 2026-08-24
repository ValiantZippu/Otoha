#include "ExportUi.h"

namespace otoha
{
// =============================================================================
// ExportOptionsComponent
// =============================================================================
ExportOptionsComponent::ExportOptionsComponent (const ExportSettingsStore& store,
                                                int numFiles, bool ffmpegAvailable)
{
    FfmpegLocator locator;
    FfmpegInfo info;
    juce::ignoreUnused (locator, info);   // availability passed in by caller

    addAndMakeVisible (formatLabel);
    formatLabel.setFont (juce::FontOptions (14.0f));
    formatLabel.setText (numFiles > 1 ? "Format (all files)" : "Format",
                         juce::dontSendNotification);
    addAndMakeVisible (formatCombo);

    for (int i = 0; i <= 4; ++i)
    {
        const auto f = (ExportFormat) i;
        const auto caps = capabilitiesFor (f);
        const bool usable = ! caps.requiresFfmpeg || ffmpegAvailable;
        formatCombo.addItem (caps.displayName + (usable ? "" : "  (unavailable)"), i + 1);
        formatCombo.setItemEnabled (i + 1, usable);
    }
    formatCombo.setSelectedItemIndex ((int) store.getLastFormat(), juce::dontSendNotification);

    addAndMakeVisible (qualityLabel);
    qualityLabel.setFont (juce::FontOptions (14.0f));
    addAndMakeVisible (qualityCombo);

    auto refreshQualityItems = [this]
    {
        const auto f = (ExportFormat) formatCombo.getSelectedItemIndex();
        qualityCombo.clear (juce::dontSendNotification);

        if (capabilitiesFor (f).lossless)
        {
            qualityCombo.setTextWhenNothingSelected ("Lossless");
            qualityCombo.setEnabled (false);
        }
        else
        {
            qualityCombo.setEnabled (true);
            for (auto q : { ExportQuality::small, ExportQuality::standard, ExportQuality::high })
                qualityCombo.addItem (qualityLabel (f, q), (int) q + 1);
            qualityCombo.setSelectedItemIndex ((int) store.getLastQuality(), juce::dontSendNotification);
        }
    };
    refreshQualityItems();
    formatCombo.onChange = [refreshQualityItems] { refreshQualityItems(); };

    setSize (360, 110);
}

ExportDialogResult ExportOptionsComponent::result() const
{
    ExportDialogResult r;
    r.confirmed = true;
    r.format = (ExportFormat) juce::jlimit (0, 4, formatCombo.getSelectedItemIndex());
    r.quality = capabilitiesFor (r.format).lossless
        ? ExportQuality::standard
        : (ExportQuality) juce::jlimit (0, 2, qualityCombo.getSelectedItemIndex());
    return r;
}

void ExportOptionsComponent::resized()
{
    auto area = getLocalBounds().reduced (8);

    formatLabel.setBounds (area.removeFromTop (22));
    formatCombo.setBounds (area.removeFromTop (26));
    area.removeFromTop (6);
    qualityLabel.setBounds (area.removeFromTop (22));
    qualityCombo.setBounds (area.removeFromTop (26));
}

ExportDialogResult runExportOptionsDialog (juce::Component* parent,
                                           const ExportSettingsStore& store,
                                           int numFiles, bool ffmpegAvailable)
{
    ExportDialogResult result;

    auto* window = new juce::AlertWindow ("Export",
                                          numFiles > 1
                                              ? "Choose a format and quality for all "
                                                    + juce::String (numFiles) + " recordings:"
                                              : "Choose a format and quality:",
                                          juce::MessageBoxIconType::NoIcon);
    window->addCustomComponent (new ExportOptionsComponent (store, numFiles, ffmpegAvailable));
    window->addButton ("Export", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    window->enterModalState (true,
        juce::ModalCallbackFunction::create ([window, &result] (int code)
        {
            if (code == 1)
                if (auto* options = dynamic_cast<ExportOptionsComponent*> (
                        window->getCustomComponent (0)))
                    result = options->result();
        }),
        false /* stay alive until we read the result below */);

    // Desktop: run modally so callers get a synchronous answer.
    window->runModalLoop();

    if (result.confirmed)
        if (auto* options = dynamic_cast<ExportOptionsComponent*> (
                window->getCustomComponent (0)))
            result = options->result();

    delete window;
    return result;
}

// =============================================================================
// Progress window
// =============================================================================
namespace
{
class ExportProgressTracker : private juce::Timer
{
public:
    ExportProgressTracker (juce::Component* parent, ExportManager& m)
        : manager (m),
          window ("Exporting", "", juce::MessageBoxIconType::NoIcon)
    {
        bar.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        window.addProgressBarComponent (bar);
        currentFile.setFont (juce::FontOptions (13.0f));
        window.setMessage (currentFileName);
        window.addButton ("Cancel", 0);
        window.enterModalState (false, nullptr, false);
        window.setVisible (true);
        startTimerHz (8);
        juce::ignoreUnused (parent);
    }

    ~ExportProgressTracker() override { stopTimer(); }

private:
    void timerCallback() override
    {
        const auto statuses = manager.getStatuses();

        int done = 0, total = (int) statuses.size(), waitingOrActive = 0;
        float sum = 0.0f;

        for (const auto& s : statuses)
        {
            switch (s.state)
            {
                case JobStatus::State::completed:
                case JobStatus::State::failed:
                case JobStatus::State::cancelled:
                case JobStatus::State::skipped:
                    ++done; sum += 1.0f; break;
                default:
                    ++waitingOrActive; sum += s.progress; break;
            }
        }

        bar.setValue (total > 0 ? sum / (double) total : 0.0, juce::dontSendNotification);

        for (const auto& s : statuses)
            if (s.state == JobStatus::State::rendering || s.state == JobStatus::State::encoding)
                currentFileName = "Current: " + s.displayName;

        window.setMessage (currentFileName.isEmpty()
                               ? juce::String ("Preparing...") : currentFileName);

        const bool allDone = done == total && total > 0;
        if (allDone || statuses.empty())
        {
            stopTimer();

            const auto summary = manager.getSummary();
            window.exitModalState (0);
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::InfoIcon, "Export finished",
                juce::String (summary.succeeded) + " file(s) exported successfully."
                    + (summary.failed + summary.cancelled + summary.skipped > 0
                        ? "\n" + juce::String (summary.failed) + " failed · "
                              + juce::String (summary.cancelled) + " cancelled · "
                              + juce::String (summary.skipped) + " skipped"
                        : ""));
            deleteThisWhenSafe = true;
        }

        if (deleteThisWhenSafe)
        {
            stopTimer();
            delete this;
        }
    }

    ExportManager& manager;
    juce::AlertWindow window;
    juce::ProgressBar bar { 0.0 };
    juce::Label currentFile;
    juce::String currentFileName;
    bool deleteThisWhenSafe = false;
};
} // namespace

void showExportProgressWindow (juce::Component* parent, ExportManager& manager)
{
    new ExportProgressTracker (parent, manager);   // self-deleting when done
}
} // namespace otoha
