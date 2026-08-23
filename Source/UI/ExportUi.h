#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Dsp/ProcessingState.h"
#include "../Export/ExportManager.h"
#include "../Export/ExportPresets.h"

/*
    ExportUi — thin dialogs over the ExportManager service.

    The service knows nothing about these widgets; everything here only builds
    requests and polls statuses.
*/
namespace otoha
{
struct ExportDialogResult
{
    bool confirmed = false;
    ExportFormat format = ExportFormat::wav;
    ExportQuality quality = ExportQuality::standard;
};

/** Format + quality chooser (capabilities-driven). Embedded in an AlertWindow. */
class ExportOptionsComponent : public juce::Component
{
public:
    ExportOptionsComponent (const ExportSettingsStore& store,
                            int numFiles, bool ffmpegAvailable);

    ExportDialogResult result() const;

    void resized() override;

private:
    juce::Label formatLabel { {}, "Format" }, qualityLabel { {}, "Quality" };
    juce::ComboBox formatCombo, qualityCombo;
};

/** Opens a modal options window; returns the choice or an unconfirmed result. */
ExportDialogResult runExportOptionsDialog (juce::Component* parent,
                                           const ExportSettingsStore& store,
                                           int numFiles, bool ffmpegAvailable);

/** Polls the manager until every job is terminal; shows overall progress,
    current file, cancel button, and a final summary. Safe to dismiss early. */
void showExportProgressWindow (juce::Component* parent, ExportManager& manager);
} // namespace otoha
