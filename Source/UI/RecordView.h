#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Audio/Player.h"
#include "../Audio/Recorder.h"
#include "../Library/LibraryService.h"

/*
    RecordView — the Milestone 2 recording screen:

        Otoha
        [input] [output] [quality] [countdown] [monitor]
        +--------------------------------------------------+
        |                waveform / playhead               |
        +--------------------------------------------------+
        [ meter ]                                  CLIP
              (o)      Play    Stop       00:00.0
                          Edit Enhance Export Delete

    - The Recorder owns the transport state; this view only observes it and
      requests transitions.
    - The countdown runs on a monotonic clock; no file is created until it ends
      (cancelling leaves nothing behind).
    - The displayed duration comes from the sample counter, not UI frames.
*/
class RecordView : public juce::Component,
                   private juce::Timer,
                   private juce::ChangeListener
{
public:
    RecordView (juce::AudioDeviceManager& deviceManager, Recorder& recorder,
                Player& player, LibraryService& library,
                std::function<void()> goToEditor = {});
    ~RecordView() override;

    /** The file most recently recorded or loaded for playback, if any. */
    juce::File getCurrentRecordingFile() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    void populateDeviceCombos();
    void applyDeviceSelection();
    void updateTransportState();
    void refreshFormatLabel();

    void recordButtonClicked();
    void playPauseClicked();
    void stopClicked();

    void beginCountdown();
    void cancelCountdown();
    void beginRecording();
    void finishRecording();

    void exportClicked();
    void deleteClicked();
    void handleFailure (otoha::FailureReason reason);

    juce::AudioDeviceManager& deviceManager;
    Recorder& recorder;
    Player& player;
    LibraryService& libraryService;
    std::function<void()> goToEditor;

    // Device settings
    juce::ComboBox inputCombo, outputCombo;
    juce::Label    inputLabel { {}, "Microphone" }, outputLabel { {}, "Output" };
    juce::ComboBox bitDepthCombo, countdownCombo;
    juce::Label    qualityLabel { {}, "Quality" }, countdownLabel { {}, "Countdown" };
    juce::ToggleButton monitorToggle { "Monitor" };
    juce::Label formatLabel;   // e.g. "48.0 kHz · 24-bit · Stereo"

    // Visualization
    class WaveformPanel;
    std::unique_ptr<WaveformPanel> waveform;
    class LevelMeter;
    std::unique_ptr<LevelMeter> levelMeter;
    juce::Label clipLabel { {}, "CLIPPING" };

    // Transport + actions
    juce::ShapeButton recordButton { "record", juce::Colours::transparentBlack,
                                     juce::Colours::transparentBlack, juce::Colours::transparentBlack };
    juce::TextButton playButton { "Play" }, stopButton { "Stop" };
    juce::TextButton editButton { "Edit" }, enhanceButton { "Enhance" };
    juce::TextButton exportButton { "Export" }, deleteButton { "Delete" };
    juce::Label timeLabel;

    // Status
    juce::Label statusLabel, errorLabel;

    // Countdown (monotonic clock; the engine stays idle until it finishes)
    bool counting = false;
    double countdownDeadlineMs = 0.0;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecordView)
};
