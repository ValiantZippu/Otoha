#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Audio/Player.h"
#include "../Audio/Recorder.h"
#include "../Library/LibraryService.h"
#include "Components/DsButton.h"
#include "Components/DsCore.h"
#include "Components/DsControls.h"

/*    RecordView — the polished Otoha recording screen (M21).

        Microphone selector
        Countdown selector
        Monitor toggle
            ┌───────────────────────────────┐
            │      live waveform/vis        │
            └───────────────────────────────┘
        Level meter                CLIP
            00:00
              ● Record / ■ Stop
         [ Play ]  [ Edit ]  [ Export ]  [ Delete ]

    The Recorder owns the transport state; this view only observes it and
    requests transitions. Countdown runs on a monotonic clock; no file is
    created until it finishes.

    All visuals consume OtohaTheme tokens (M17/M18).
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

    juce::File getCurrentRecordingFile() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    // Device management
    void populateDeviceCombos();
    void applyDeviceSelection();
    void refreshFormatLabel();

    // Transport
    void recordButtonClicked();
    void playPauseClicked();
    void stopClicked();
    void beginCountdown();
    void cancelCountdown();
    void beginRecording();
    void finishRecording();
    void handleFailure (otoha::FailureReason reason);

    // Post-recording actions
    void exportClicked();
    void deleteClicked();

    void updateTransportState();

    // --- References (owned by AppShell) ---
    juce::AudioDeviceManager& deviceManager;
    Recorder& recorder;
    Player& player;
    LibraryService& libraryService;
    std::function<void()> goToEditor;

    // --- Configuration row ---
    juce::Label inputLabel;
    std::unique_ptr<otoha::ds::ComboBox> inputCombo;
    juce::Label countdownLabel;
    std::unique_ptr<otoha::ds::ComboBox> countdownCombo;
    std::unique_ptr<otoha::ds::Toggle> monitorToggle;

    // --- Visualization ---
    class WaveformPanel;
    std::unique_ptr<WaveformPanel> waveform;
    class LevelMeter;
    std::unique_ptr<LevelMeter> levelMeter;
    juce::Label clipLabel;

    // --- Timer ---
    juce::Label timeLabel;

    // --- Transport ---
    std::unique_ptr<juce::ShapeButton> recordButton;
    otoha::ds::Button playButton { "Play", otoha::ds::ButtonVariant::secondary };
    otoha::ds::Button stopButton { "Stop", otoha::ds::ButtonVariant::secondary };

    // --- Post-recording actions ---
    std::unique_ptr<otoha::ds::Button> editButton;
    std::unique_ptr<otoha::ds::Button> exportButton;
    std::unique_ptr<otoha::ds::Button> deleteButton;
    juce::Label formatLabel;
    juce::Label statusLabel;
    juce::Label errorLabel;

    // --- State ---
    bool counting = false;
    double countdownDeadlineMs = 0.0;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecordView)
};
