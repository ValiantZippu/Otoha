#include "RecordView.h"

#include "../Core/RecordingSupport.h"

#include <cmath>

// =============================================================================
// WaveformPanel — live thumbnail, playhead, click-to-seek.
// =============================================================================
class RecordView::WaveformPanel : public juce::Component
{
public:
    explicit WaveformPanel (Recorder& r, Player& p) : recorder (r), player (p) {}

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (12.0f);

        g.setColour (findColour (juce::ResizableWindow::backgroundColourId).contrasting (0.06f));
        g.fillRoundedRectangle (area, 10.0f);
        g.drawRoundedRectangle (area, 10.0f, 1.0f);

        // Friendly empty state when no input exists — never a crash or blank box.
        if (! recorder.hasInput())
        {
            g.setColour (juce::Colours::grey);
            g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
            g.drawText ("No microphone available.", area.withTrimmedBottom (22), juce::Justification::centred);
            g.setFont (juce::FontOptions (14.0f));
            g.drawText ("Connect a microphone and try again.",
                        area.withTrimmedTop (22), juce::Justification::centred);
            return;
        }

        auto& thumb = recorder.getThumbnail();
        const double totalSeconds = std::max (thumb.getTotalLength(),
                                              player.hasFile() ? player.getLengthSeconds() : 0.0);

        if (totalSeconds <= 0.0)
        {
            g.setColour (juce::Colours::grey);
            g.setFont (juce::FontOptions (16.0f));
            g.drawText ("Press the red button (or R) to start recording",
                        area, juce::Justification::centred);
            return;
        }

        // AudioThumbnail is an efficient peak-aggregated representation; it never
        // stores raw samples, so long recordings stay cheap to draw.
        g.setColour (juce::Colour (0xff4fc3a1)); // calm mint waveform
        if (thumb.getTotalLength() > 0.0)
            thumb.drawChannels (g, area.toNearestInt(), 0.0, thumb.getTotalLength(), 1.0f);

        // Playhead: during playback over the file length; while recording it rides at the end.
        double fraction = -1.0;
        if (player.hasFile())
            fraction = player.getPositionSeconds() / totalSeconds;
        else if (recorder.getState() != otoha::TransportState::idle && recorder.getSampleRate() > 0.0)
            fraction = (double) recorder.getTotalSamples()
                       / (recorder.getSampleRate() * (double) juce::jmax (1, recorder.getNumInputChannels()));

        if (fraction >= 0.0 && fraction <= 1.0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawVerticalLine ((int) (area.getX() + fraction * area.getWidth()),
                                area.getY(), area.getBottom());
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Seek during playback only; never while a take is open.
        if (! player.hasFile() || recorder.getState() != otoha::TransportState::idle)
            return;

        const auto area = getLocalBounds().toFloat().reduced (12.0f);
        if (! area.contains (e.position))
            return;

        const double fraction = juce::jlimit (0.0, 1.0,
            (double) ((e.position.x - area.getX()) / area.getWidth()));
        player.setPositionSeconds (fraction * player.getLengthSeconds());
    }

private:
    Recorder& recorder;
    Player& player;
};

// =============================================================================
// LevelMeter — real audio-driven bar: RMS fill + peak marker + clip latch.
// =============================================================================
class RecordView::LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (Recorder& r) : recorder (r) {}

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (2.0f);

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (area, area.getHeight());

        const float rms  = recorder.getLevelRms();
        const float peak = recorder.getLevelPeak();

        auto db = [] (float v) { return juce::Decibels::gainToDecibels (v, -60.0f); };
        const auto xForDb = [&] (float dbValue)
        {
            return area.getX() + (dbValue + 60.0f) / 60.0f * area.getWidth();
        };

        const auto rmsWidth = juce::jlimit (0.0f, area.getWidth(), xForDb (db (rms)) - area.getX());
        g.setColour (recorder.hasClipped() ? juce::Colour (0xffe05252) : juce::Colour (0xff4fc3a1));
        g.fillRoundedRectangle (area.getX(), area.getY(), rmsWidth, area.getHeight(), area.getHeight());

        const auto peakX = juce::jlimit (area.getX(), area.getRight(), xForDb (db (peak)));
        g.setColour (juce::Colours::white);
        g.fillRect (peakX - 1.5f, area.getY(), 3.0f, area.getHeight());

        if (recorder.hasClipped())
        {
            g.setColour (juce::Colours::red);
            g.fillEllipse (area.getRight() - area.getHeight(), area.getY(),
                           area.getHeight(), area.getHeight());
        }
    }

private:
    Recorder& recorder;
};

// =============================================================================
// RecordView
// =============================================================================
RecordView::RecordView (juce::AudioDeviceManager& dm, Recorder& rec, Player& pl, LibraryService& lib,
                        std::function<void()> editorCallback)
    : deviceManager (dm), recorder (rec), player (pl), libraryService (lib),
      goToEditor (std::move (editorCallback))
{
    setOpaque (true);

    addAndMakeVisible (inputLabel);   addAndMakeVisible (inputCombo);
    addAndMakeVisible (outputLabel);  addAndMakeVisible (outputCombo);
    addAndMakeVisible (qualityLabel); addAndMakeVisible (bitDepthCombo);
    addAndMakeVisible (countdownLabel); addAndMakeVisible (countdownCombo);
    addAndMakeVisible (monitorToggle);
    addAndMakeVisible (formatLabel);

    inputCombo.setTextWhenNothingSelected ("< no microphone >");
    for (auto* combo : { &inputCombo, &outputCombo })
        combo->onChange = [this] { applyDeviceSelection(); };

    bitDepthCombo.addItem ("16-bit", 1);
    bitDepthCombo.addItem ("24-bit", 2);
    bitDepthCombo.setSelectedItemIndex (1, juce::dontSendNotification); // 24-bit default

    countdownCombo.addItem ("Off", 1);
    countdownCombo.addItem ("3 seconds", 2);
    countdownCombo.addItem ("5 seconds", 3);
    countdownCombo.addItem ("10 seconds", 4);
    countdownCombo.setSelectedItemIndex (2, juce::dontSendNotification); // 5 s default

    monitorToggle.onClick = [this]
    {
        recorder.setMonitoring (monitorToggle.getToggleState());
        grabKeyboardFocus();
    };

    waveform   = std::make_unique<WaveformPanel> (recorder, player);
    levelMeter = std::make_unique<LevelMeter> (recorder);
    addAndMakeVisible (*waveform);
    addAndMakeVisible (*levelMeter);

    clipLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    clipLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe05252));
    clipLabel.setVisible (false);
    addAndMakeVisible (clipLabel);

    // Big round record button (shape matched to its final size in resized()).
    recordButton.setOnColours (juce::Colour (0xffe05252), juce::Colour (0xffc33b3b), juce::Colour (0xffa92f2f));
    recordButton.setColours (juce::Colour (0xffe05252), juce::Colour (0xffd14646), juce::Colour (0xffc33b3b));
    // M14 #5/#56: the dominant action must be reachable by name, not just sight.
    recordButton.setName ("Record");
    recordButton.setDescription ("Start recording");
    recordButton.setHelpText ("Starts a new recording with the selected microphone");
    recordButton.onClick = [this] { recordButtonClicked(); };
    addAndMakeVisible (recordButton);

    playButton.onClick = [this] { playPauseClicked(); };
    stopButton.onClick = [this] { stopClicked(); };
    for (auto* b : { &playButton, &stopButton })
        addAndMakeVisible (*b);

    editButton.onClick = [this]
    {
        if (goToEditor && player.hasFile())
            goToEditor();
        grabKeyboardFocus();
    };
    // M14: the old "Enhance placeholder" button is gone (#78) — enhancing
    // happens in the editor, one ✨ tap away via Edit.
    exportButton.onClick = [this] { exportClicked(); };
    deleteButton.onClick = [this] { deleteClicked(); };
    for (auto* b : { &editButton, &exportButton, &deleteButton })
        addAndMakeVisible (*b);

    timeLabel.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    timeLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (timeLabel);

    statusLabel.setFont (juce::FontOptions (13.0f));
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    statusLabel.setText ("Ready", juce::dontSendNotification);
    addAndMakeVisible (statusLabel);

    errorLabel.setFont (juce::FontOptions (13.0f));
    errorLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe08a8a));
    addAndMakeVisible (errorLabel);

    populateDeviceCombos();
    deviceManager.addChangeListener (this);
    updateTransportState();
    startTimerHz (30);
    setSize (900, 660);
    grabKeyboardFocus();
}

RecordView::~RecordView()
{
    stopTimer();
    deviceManager.removeChangeListener (this);
}

void RecordView::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("Otoha", 20, 14, 200, 34, juce::Justification::centredLeft);

    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Record", 220, 24, 300, 20, juce::Justification::centredLeft);
}

void RecordView::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop (54); // title strip

    auto settingsRow = bounds.removeFromTop (40).reduced (16, 4);
    auto placeControl = [&settingsRow] (juce::Label& label, juce::Component& control, int labelW, int controlW)
    {
        label.setBounds (settingsRow.removeFromLeft (labelW));
        control.setBounds (settingsRow.removeFromLeft (controlW).withHeight (28));
        settingsRow.removeFromLeft (12);
    };
    placeControl (inputLabel,     inputCombo,     82, 150);
    placeControl (outputLabel,    outputCombo,    50, 130);
    placeControl (qualityLabel,   bitDepthCombo,  54, 72);
    placeControl (countdownLabel, countdownCombo, 76, 88);
    monitorToggle.setBounds (settingsRow.removeFromLeft (96).withHeight (28));

    auto labelRow = bounds.removeFromBottom (44);
    errorLabel.setBounds  (labelRow.removeFromBottom (20).reduced (16, 0));
    statusLabel.setBounds (labelRow.removeFromBottom (20).reduced (16, 0));

    bounds.reduce (16, 0);

    auto actionsRow = bounds.removeFromBottom (34);
    formatLabel.setBounds (actionsRow.removeFromLeft (260));
    auto actionButtons = actionsRow.withSizeKeepingCentre (240, 26).removeFromRight (240);
    deleteButton.setBounds  (actionButtons.removeFromRight (76).reduced (2, 1));
    exportButton.setBounds  (actionButtons.removeFromRight (76).reduced (2, 1));
    editButton.setBounds    (actionButtons.removeFromRight (66).reduced (2, 1));

    auto meterArea = bounds.removeFromBottom (46);
    clipLabel.setBounds (meterArea.removeFromRight (78));
    levelMeter->setBounds (meterArea.reduced (0, 8));

    auto transportRow = bounds.removeFromBottom (76);
    auto centreButtons = transportRow.withSizeKeepingCentre (280, 56);
    recordButton.setBounds (centreButtons.removeFromLeft (56).withSizeKeepingCentre (56, 56));
    centreButtons.removeFromLeft (28);
    playButton.setBounds (centreButtons.removeFromLeft (88).reduced (4, 10));
    stopButton.setBounds (centreButtons.removeFromLeft (88).reduced (4, 10));
    timeLabel.setBounds (transportRow.removeFromRight (140).reduced (6, 10));

    waveform->setBounds (bounds.reduced (0, 8));

    // Keep the round record-button shape matched to its final size.
    juce::Path p;
    p.addEllipse (recordButton.getLocalBounds().toFloat());
    recordButton.setShape (p, false, false, false);
}

bool RecordView::keyPressed (const juce::KeyPress& key)
{
    if (key.isKeyCode (juce::KeyPress::spaceKey))  { playPauseClicked();      return true; }
    if (key == juce::KeyPress ('r'))              { recordButtonClicked();   return true; }
    if (key.isKeyCode (juce::KeyPress::escapeKey) && counting) { cancelCountdown(); return true; }
    return false;
}

// -----------------------------------------------------------------------------
// Device management
// -----------------------------------------------------------------------------
void RecordView::populateDeviceCombos()
{
    auto* type = deviceManager.getCurrentDeviceTypeObject();
    if (type == nullptr)
        return;

    inputCombo.clear (juce::dontSendNotification);
    outputCombo.clear (juce::dontSendNotification);

    inputCombo.addItem ("(default)", 1);
    outputCombo.addItem ("(default)", 1);
    for (const auto& name : type->getDeviceNames (true))   // capture devices only
        inputCombo.addItem (name, inputCombo.getNumItems() + 1);
    for (const auto& name : type->getDeviceNames (false))  // playback devices only
        outputCombo.addItem (name, outputCombo.getNumItems() + 1);

    const auto setup = deviceManager.getAudioDeviceSetup();
    auto select = [] (juce::ComboBox& box, const juce::String& deviceName)
    {
        int id = 1;
        if (deviceName.isNotEmpty())
            for (int i = 1; i <= box.getNumItems(); ++i)
                if (box.getItemText (i - 1) == deviceName) { id = i; break; }
        box.setSelectedItemIndex (id - 1, juce::dontSendNotification);
    };
    select (inputCombo, setup.inputDeviceName);
    select (outputCombo, setup.outputDeviceName);

    refreshFormatLabel();
}

void RecordView::applyDeviceSelection()
{
    // Changing devices mid-take would corrupt it — the selector is disabled while busy anyway.
    if (recorder.getState() != otoha::TransportState::idle || counting)
        return;

    auto setup = deviceManager.getAudioDeviceSetup();
    setup.inputDeviceName  = inputCombo.getSelectedItemIndex() <= 0 ? juce::String() : inputCombo.getText();
    setup.outputDeviceName = outputCombo.getSelectedItemIndex() <= 0 ? juce::String() : outputCombo.getText();

    // setAudioDeviceSetup closes, reconfigures and reopens the stream atomically;
    // Recorder::audioDeviceAboutToStart re-reads rate/channels afterwards.
    const juce::String error = deviceManager.setAudioDeviceSetup (setup, true);
    errorLabel.setText (error.isEmpty()
                            ? juce::String()
                            : "Audio device problem: " + error,
                        juce::dontSendNotification);

    populateDeviceCombos();   // also refreshes the format label from the new device
    updateTransportState();   // may need to show/hide the "no microphone" state
    grabKeyboardFocus();
}

void RecordView::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &deviceManager)
        populateDeviceCombos();
}

void RecordView::refreshFormatLabel()
{
    const double rate = recorder.getSampleRate();
    if (rate <= 0.0)
    {
        formatLabel.setText ("No audio device", juce::dontSendNotification);
        return;
    }

    const int channels = juce::jmax (0, recorder.getNumInputChannels());
    formatLabel.setText (juce::String (rate / 1000.0, 1) + " kHz · "
                             + juce::String (bitDepthCombo.getText()) + " · "
                             + (channels >= 2 ? "Stereo" : channels == 1 ? "Mono" : "no input"),
                         juce::dontSendNotification);
}

// -----------------------------------------------------------------------------
// Transport
// -----------------------------------------------------------------------------
void RecordView::recordButtonClicked()
{
    if (counting) { cancelCountdown(); grabKeyboardFocus(); return; }

    switch (recorder.getState())
    {
        case otoha::TransportState::idle:      beginCountdown(); break;
        case otoha::TransportState::recording:
        case otoha::TransportState::paused:    finishRecording(); break;
    }
    grabKeyboardFocus();
}

void RecordView::playPauseClicked()
{
    if (counting)
        return;

    switch (recorder.getState())
    {
        case otoha::TransportState::recording:
            recorder.pauseRecording();
            break;
        case otoha::TransportState::paused:
            recorder.resumeRecording();
            break;
        case otoha::TransportState::idle:
            player.togglePlayPause();
            break;
    }
    updateTransportState();
    grabKeyboardFocus();
}

void RecordView::stopClicked()
{
    if (counting) { cancelCountdown(); return; }

    if (recorder.getState() != otoha::TransportState::idle)
    {
        finishRecording();
    }
    else
    {
        player.stop();
        player.setPositionSeconds (0.0);
        updateTransportState();
    }
    grabKeyboardFocus();
}

// -----------------------------------------------------------------------------
// Countdown -> recording -> finish
// -----------------------------------------------------------------------------
void RecordView::beginCountdown()
{
    if (! recorder.hasInput())
    {
        errorLabel.setText ("No microphone available.\n"
                            "Connect a microphone and try again, or pick another input.",
                            juce::dontSendNotification);
        return;
    }

    static constexpr int secondsChoices[] = { 0, 3, 5, 10 };
    const int seconds = secondsChoices[countdownCombo.getSelectedItemIndex()];

    if (seconds == 0)
    {
        beginRecording();
        return;
    }

    counting = true;
    countdownDeadlineMs = juce::Time::getMillisecondCounterHiRes() + seconds * 1000.0;
    updateTransportState();
}

void RecordView::cancelCountdown()
{
    counting = false;
    countdownDeadlineMs = 0.0;
    timeLabel.setText (otoha::formatDuration (0.0), juce::dontSendNotification);
    updateTransportState();   // no file was created — nothing to clean up
}

void RecordView::beginRecording()
{
#if JUCE_ANDROID
    // Request mic permission only when the user actually tries to record.
    if (! juce::RuntimePermissions::isPermissionGranted (juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
            [this] (bool granted)
            {
                if (granted) beginRecording();
                else errorLabel.setText ("Otoha needs microphone access to record.\n"
                                         "Enable it in system settings and try again.",
                                         juce::dontSendNotification);
            });
        return;
    }
#endif

    errorLabel.setText ({}, juce::dontSendNotification);
    recorder.clearClipIndicator();

    juce::String error;
    const int bitDepth = bitDepthCombo.getSelectedItemIndex() == 0 ? 16 : 24;

    if (! recorder.startRecording (otoha::uniqueRecordingFile (otoha::recordingsDirectory(),
                                                               juce::Time::getCurrentTime()),
                                   bitDepth, error))
    {
        errorLabel.setText (error, juce::dontSendNotification);
        updateTransportState();
        return;
    }

    updateTransportState();
}

void RecordView::finishRecording()
{
    counting = false;
    recorder.stopRecording();

    const auto file = recorder.getCurrentFile();
    if (file.existsAsFile() && file.getSize() > 44)   // more than an empty RIFF header
    {
        if (player.loadFile (file))
            player.setPositionSeconds (0.0);

        // Register in the library. If this fails the file stays on disk and the
        // next startup scan recovers it — the recording is never lost.
        const juce::int64 libraryId = libraryService.registerAudioFile (file);

        statusLabel.setText ((libraryId != 0 ? "Saved to Library · " : "Saved ")
                                 + file.getFileName()
                                 + "  ·  " + otoha::formatDuration (
                                       otoha::samplesToSeconds (recorder.getTotalSamples(),
                                                                recorder.getSampleRate())),
                             juce::dontSendNotification);
    }
    else
    {
        player.unload();
        errorLabel.setText ("Otoha couldn't finish saving the recording.\n"
                            "There may not be enough storage space.",
                            juce::dontSendNotification);
    }

    updateTransportState();
}

// -----------------------------------------------------------------------------
// Post-recording actions
// -----------------------------------------------------------------------------
void RecordView::exportClicked()
{
    if (! player.hasFile() || recorder.getState() != otoha::TransportState::idle)
        return;

    auto defaultFile = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                           .getChildFile (player.getFile().getFileName());

    chooser = std::make_unique<juce::FileChooser> ("Export recording as WAV",
                                                   defaultFile, "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto target = fc.getResult();
                              if (target == juce::File{})
                                  return;

                              if (target.existsAsFile())
                                  target.deleteFile();

                              if (player.getFile().copyFileTo (target))
                                  statusLabel.setText ("Exported to " + target.getFullPathName(),
                                                       juce::dontSendNotification);
                              else
                                  errorLabel.setText ("Otoha couldn't save the copy there.\n"
                                                      "Check storage space and folder permissions.",
                                                      juce::dontSendNotification);
                          });
}

void RecordView::deleteClicked()
{
    if (! player.hasFile() || recorder.getState() != otoha::TransportState::idle)
        return;

    const auto fileToDelete = player.getFile();

    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon,
                                        "Delete recording",
                                        "Move \"" + fileToDelete.getFileName() + "\" to the trash?\n"
                                        "This cannot be undone.",
                                        "Delete", "Cancel", this,
                                        juce::ModalCallbackFunction::create (
                                            [this, fileToDelete] (int result)
                                            {
                                                if (result != 1)  // cancelled
                                                    return;

                                                player.unload();
                                                if (! fileToDelete.moveToTrash())
                                                    fileToDelete.deleteFile();

                                                statusLabel.setText ("Deleted " + fileToDelete.getFileName(),
                                                                     juce::dontSendNotification);
                                                updateTransportState();
                                            }));
}

// -----------------------------------------------------------------------------
// Failures surfaced by the engine
// -----------------------------------------------------------------------------
void RecordView::handleFailure (otoha::FailureReason reason)
{
    switch (reason)
    {
        case otoha::FailureReason::none:
            return;

        case otoha::FailureReason::deviceLost:
            finishRecording();
            errorLabel.setText ("Recording stopped: the audio device was disconnected.",
                                juce::dontSendNotification);
            populateDeviceCombos();
            break;

        case otoha::FailureReason::diskFull:
            finishRecording();
            errorLabel.setText ("Otoha couldn't finish saving the recording.\n"
                                "There isn't enough storage space.",
                                juce::dontSendNotification);
            break;
    }
}

// -----------------------------------------------------------------------------
// Timer / state
// -----------------------------------------------------------------------------
juce::File RecordView::getCurrentRecordingFile() const
{
    if (recorder.getCurrentFile().existsAsFile())
        return recorder.getCurrentFile();
    return player.getFile();
}

void RecordView::timerCallback()
{
    levelMeter->repaint();

    if (clipLabel.isVisible() != recorder.hasClipped())
    {
        clipLabel.setVisible (recorder.hasClipped());   // latches until the next take starts
        resized();
    }

    handleFailure (recorder.consumeFailure());

    if (counting)
    {
        const double remainingMs = countdownDeadlineMs - juce::Time::getMillisecondCounterHiRes();
        if (remainingMs <= 0.0)
        {
            counting = false;
            beginRecording();
        }
        else
        {
            timeLabel.setText (juce::String ((int) std::ceil (remainingMs / 1000.0)),
                               juce::dontSendNotification);
        }
    }
    else
    {
        switch (recorder.getState())
        {
            case otoha::TransportState::recording:
            case otoha::TransportState::paused:
                // Duration from the sample counter: exact regardless of UI frame rate.
                timeLabel.setText (otoha::formatDuration (
                                       otoha::samplesToSeconds (recorder.getTotalSamples(),
                                                                recorder.getSampleRate())),
                                   juce::dontSendNotification);
                break;

            case otoha::TransportState::idle:
                if (player.hasFile())
                    timeLabel.setText (otoha::formatDuration (player.getPositionSeconds()),
                                       juce::dontSendNotification);
                break;
        }
    }

    waveform->repaint();
}

void RecordView::updateTransportState()
{
    const auto st = recorder.getState();
    const bool busy = st != otoha::TransportState::idle;

    // Never yank the device out from under an active take.
    inputCombo.setEnabled (! busy && ! counting);
    outputCombo.setEnabled (! busy && ! counting);
    bitDepthCombo.setEnabled (! busy && ! counting);
    countdownCombo.setEnabled (! busy && ! counting);
    monitorToggle.setEnabled (true);   // monitoring is safe at any time

    playButton.setButtonText (busy ? (st == otoha::TransportState::paused ? "Resume" : "Pause")
                                   : "Play");
    stopButton.setEnabled (busy || player.hasFile());

    const bool actionsEnabled = player.hasFile() && ! busy;
    exportButton.setEnabled (actionsEnabled);
    deleteButton.setEnabled (actionsEnabled);
    editButton.setEnabled (player.hasFile());   // opens the editor via the shell

    if (busy)
    {
        statusLabel.setText (st == otoha::TransportState::paused ? "PAUSED" : "RECORDING",
                             juce::dontSendNotification);
    }
    else if (! counting)
    {
        // keep the last message ("Ready"/"Saved ...") when idle
    }

    repaint();
}
