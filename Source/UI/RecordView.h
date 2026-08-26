#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Audio/Player.h"
#include "../Audio/Recorder.h"
#include "../Library/LibraryService.h"
#include "Components/DsButton.h"
#include "Components/DsCore.h"
#include "Components/DsControls.h"
#include "Components/DsSurfaces.h"

/*    RecordView — the polished Otoha recording screen (M21 → M30 Kaiteyo upgrade).

        ┌───────────────────────────────────────────────┐
        │              RECORDING CARD                   │
        │                                               │
        │              waveform / vis                   │
        │                                               │
        │                00:00                          │
        │                                               │
        │              ● RECORD                         │
        │                                               │
        │  Meter                    CLIP     Format     │
        └───────────────────────────────────────────────┘

        Microphone       [ device ▾ ]
        Countdown        [ 3 sec ▾ ]      Monitor [ ]

        Post-recording:  [ Play ] [ Edit ] [ Export ] [ Delete ]

    M30 upgrade: Kaiteyo-aligned layout with:
      - Main recording card (DsCard) as dominant element
      - Centered timer + record button inside card
      - Settings row below the card (Microphone, Countdown, Monitor)
      - Post-recording actions as a clean row
      - Status/error below
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

    // --- Main recording card (M30: dominant element) ---
    otoha::ds::Card recordingCard { "Recording area" };

    // --- Visualization (inside recording card) ---
    class WaveformPanel;
    std::unique_ptr<WaveformPanel> waveform;
    class LevelMeter;
    std::unique_ptr<LevelMeter> levelMeter;
    juce::Label clipLabel;

    // --- Timer (inside recording card) ---
    juce::Label timeLabel;

    // --- Record button (inside recording card, centered) ---
    std::unique_ptr<juce::ShapeButton> recordButton;

    // --- Format label (inside recording card) ---
    juce::Label formatLabel;

    // --- Settings row (below card) ---
    juce::Label inputLabel;
    std::unique_ptr<otoha::ds::ComboBox> inputCombo;
    juce::Label countdownLabel;
    std::unique_ptr<otoha::ds::ComboBox> countdownCombo;
    std::unique_ptr<otoha::ds::Toggle> monitorToggle;

    // --- Post-recording actions (below settings) ---
    otoha::ds::Button playButton { "Play", otoha::ds::ButtonVariant::secondary };
    otoha::ds::Button stopButton { "Stop", otoha::ds::ButtonVariant::secondary };
    std::unique_ptr<otoha::ds::Button> editButton;
    std::unique_ptr<otoha::ds::Button> exportButton;
    std::unique_ptr<otoha::ds::Button> deleteButton;

    // --- Status / error (below actions) ---
    juce::Label statusLabel;
    juce::Label errorLabel;

    // --- State ---
    bool counting = false;
    double countdownDeadlineMs = 0.0;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecordView)
};
