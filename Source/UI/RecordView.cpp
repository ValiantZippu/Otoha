#include "RecordView.h"

#include "OtohaTheme.h"
#include "Components/DsNavigation.h"

#include "../Core/RecordingSupport.h"

#include <cmath>

using namespace otoha::theme;

/* ======================================================================
   WaveformPanel — live thumbnail, playhead, click-to-seek.
   ====================================================================== */
class RecordView::WaveformPanel : public juce::Component
{
public:
    explicit WaveformPanel (Recorder& r, Player& p) : recorder (r), player (p) {}

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (12.0f);

        g.setColour (colors::surfaceElevated());
        g.fillRoundedRectangle (area, (float) Radius::medium);
        g.setColour (colors::borderSubtle());
        g.drawRoundedRectangle (area, (float) Radius::medium, 1.0f);

        if (! recorder.hasInput())
        {
            g.setColour (colors::textMuted());
            g.setFont (font (TextSize::heading));
            g.drawText ("No microphone available.", area.withTrimmedBottom (22),
                        juce::Justification::centred);
            g.setFont (font (TextSize::caption));
            g.drawText ("Connect a microphone and try again.",
                        area.withTrimmedTop (22), juce::Justification::centred);
            return;
        }

        auto& thumb = recorder.getThumbnail();
        const double totalSeconds = std::max (thumb.getTotalLength(),
                                              player.hasFile() ? player.getLengthSeconds() : 0.0);

        if (totalSeconds <= 0.0)
        {
            g.setColour (colors::textMuted());
            g.setFont (font (TextSize::body));
            g.drawText ("Press Record to begin",
                        area, juce::Justification::centred);
            return;
        }

        g.setColour (colors::waveform());
        if (thumb.getTotalLength() > 0.0)
            thumb.drawChannels (g, area.toNearestInt(), 0.0, thumb.getTotalLength(), 1.0f);

        // Playhead
        double fraction = -1.0;
        if (player.hasFile())
            fraction = player.getPositionSeconds() / totalSeconds;
        else if (recorder.getState() != otoha::TransportState::idle
                 && recorder.getSampleRate() > 0.0)
            fraction = (double) recorder.getTotalSamples()
                       / (recorder.getSampleRate()
                          * (double) juce::jmax (1, recorder.getNumInputChannels()));

        if (fraction >= 0.0 && fraction <= 1.0)
        {
            g.setColour (colors::playhead().withAlpha (0.85f));
            g.drawVerticalLine ((int) (area.getX() + fraction * area.getWidth()),
                                area.getY(), area.getBottom());
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! player.hasFile() || recorder.getState() != otoha::TransportState::idle)
            return;
        const auto area = getLocalBounds().toFloat().reduced (12.0f);
        if (! area.contains (e.position)) return;
        const double fraction = juce::jlimit (0.0, 1.0,
            (double) ((e.position.x - area.getX()) / area.getWidth()));
        player.setPositionSeconds (fraction * player.getLengthSeconds());
    }

private:
    Recorder& recorder;
    Player& player;
};

/* ======================================================================
   LevelMeter — real audio-driven bar with theme tokens.
   ====================================================================== */
class RecordView::LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (Recorder& r) : recorder (r) {}

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();

        g.setColour (colors::surfacePressed());
        g.fillRoundedRectangle (area, area.getHeight() * 0.5f);

        const float rms  = recorder.getLevelRms();
        const float peak = recorder.getLevelPeak();

        auto db = [] (float v) { return juce::Decibels::gainToDecibels (v, -60.0f); };
        auto xForDb = [&] (float dbVal)
        {
            return area.getX() + (dbVal + 60.0f) / 60.0f * area.getWidth();
        };

        const bool clipped = recorder.hasClipped();
        const auto rmsW = juce::jlimit (0.0f, area.getWidth(),
                                         xForDb (db (rms)) - area.getX());
        g.setColour (clipped ? colors::meterClip() : colors::meterSafe());
        g.fillRoundedRectangle (area.getX(), area.getY(), rmsW, area.getHeight(),
                                area.getHeight() * 0.5f);

        const auto peakX = juce::jlimit (area.getX(), area.getRight(), xForDb (db (peak)));
        g.setColour (colors::playhead());
        g.fillRect (peakX - 1.5f, area.getY(), 3.0f, area.getHeight());
    }

private:
    Recorder& recorder;
};

/* ======================================================================
   RecordView — M21 polished recording screen.
   ====================================================================== */
RecordView::RecordView (juce::AudioDeviceManager& dm, Recorder& rec, Player& pl,
                        LibraryService& lib, std::function<void()> editorCallback)
    : deviceManager (dm), recorder (rec), player (pl), libraryService (lib),
      goToEditor (std::move (editorCallback))
{
    setOpaque (true);

    // --- Configuration row ---
    inputLabel.setText ("Microphone", juce::dontSendNotification);
    inputLabel.setFont (font (TextSize::caption));
    inputLabel.setColour (juce::Label::textColourId, colors::textSecondary());
    addAndMakeVisible (inputLabel);

    inputCombo = std::make_unique<otoha::ds::ComboBox> ("Microphone", "< no microphone >");
    inputCombo->onChange = [this] { applyDeviceSelection(); };
    addAndMakeVisible (*inputCombo);

    countdownLabel.setText ("Countdown", juce::dontSendNotification);
    countdownLabel.setFont (font (TextSize::caption));
    countdownLabel.setColour (juce::Label::textColourId, colors::textSecondary());
    addAndMakeVisible (countdownLabel);

    countdownCombo = std::make_unique<otoha::ds::ComboBox> ("Countdown");
    countdownCombo->addItem ("Off", 1);
    countdownCombo->addItem ("3 sec", 2);
    countdownCombo->addItem ("5 sec", 3);
    countdownCombo->addItem ("10 sec", 4);
    countdownCombo->setSelectedItemIndex (2, juce::dontSendNotification);
    addAndMakeVisible (*countdownCombo);

    monitorToggle = std::make_unique<otoha::ds::Toggle> ("Monitor");
    monitorToggle->onClick = [this]
    {
        recorder.setMonitoring (monitorToggle->getToggleState());
        grabKeyboardFocus();
    };
    addAndMakeVisible (*monitorToggle);

    // --- Visualization ---
    waveform   = std::make_unique<WaveformPanel> (recorder, player);
    levelMeter = std::make_unique<LevelMeter> (recorder);
    addAndMakeVisible (*waveform);
    addAndMakeVisible (*levelMeter);

    clipLabel.setFont (font (TextSize::caption, true));
    clipLabel.setColour (juce::Label::textColourId, colors::danger());
    clipLabel.setText ("CLIP", juce::dontSendNotification);
    clipLabel.setJustificationType (juce::Justification::centredRight);
    clipLabel.setVisible (false);
    addAndMakeVisible (clipLabel);

    // --- Timer ---
    timeLabel.setFont (juce::FontOptions ((float) Metrics::titleStripHeight * 0.55f,
                                          juce::Font::bold));
    timeLabel.setColour (juce::Label::textColourId, colors::textPrimary());
    timeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (timeLabel);

    // --- Record button (circle, semantic recording tokens) ---
    recordButton = std::make_unique<juce::ShapeButton> ("Record",
        colors::recording(), colors::recording().brighter (0.08f),
        colors::recording().darker (0.12f));
    recordButton->setOnColours (colors::recording(),
                                colors::recording().darker (0.12f),
                                colors::recording().darker (0.25f));
    recordButton->setName ("Record");
    recordButton->setDescription ("Start recording");
    recordButton->setHelpText ("Starts a new recording with the selected microphone");
    recordButton->onClick = [this] { recordButtonClicked(); };
    addAndMakeVisible (*recordButton);

    // --- Transport actions ---
    playButton.onClick = [this] { playPauseClicked(); };
    stopButton.onClick = [this] { stopClicked(); };
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);

    // --- Post-recording actions ---
    editButton   = std::make_unique<otoha::ds::Button> ("Edit", otoha::ds::ButtonVariant::secondary);
    exportButton = std::make_unique<otoha::ds::Button> ("Export", otoha::ds::ButtonVariant::secondary);
    deleteButton = std::make_unique<otoha::ds::Button> ("Delete", otoha::ds::ButtonVariant::danger);
    editButton->onClick = [this]
    {
        if (goToEditor && player.hasFile()) goToEditor();
        grabKeyboardFocus();
    };
    exportButton->onClick = [this] { exportClicked(); };
    deleteButton->onClick = [this] { deleteClicked(); };
    addAndMakeVisible (*editButton);
    addAndMakeVisible (*exportButton);
    addAndMakeVisible (*deleteButton);

    formatLabel.setFont (font (TextSize::caption));
    formatLabel.setColour (juce::Label::textColourId, colors::textMuted());
    addAndMakeVisible (formatLabel);

    statusLabel.setFont (font (TextSize::caption));
    statusLabel.setColour (juce::Label::textColourId, colors::textSecondary());
    statusLabel.setText ("Ready", juce::dontSendNotification);
    addAndMakeVisible (statusLabel);

    errorLabel.setFont (font (TextSize::caption));
    errorLabel.setColour (juce::Label::textColourId, colors::danger().brighter (0.2f));
    addAndMakeVisible (errorLabel);

    populateDeviceCombos();
    deviceManager.addChangeListener (this);
    updateTransportState();
    startTimerHz (30);
}

RecordView::~RecordView()
{
    stopTimer();
    deviceManager.removeChangeListener (this);
}

void RecordView::paint (juce::Graphics& g)
{
    g.fillAll (colors::background());
}

void RecordView::resized()
{
    auto bounds = getLocalBounds().reduced (Spacing::xl);
    const int maxW = 720;
    auto content = bounds.withSizeKeepingCentre (juce::jmin (maxW, bounds.getWidth()),
                                                 bounds.getHeight());

    const int rowH = 32;
    const int gap  = Spacing::sm;

    // Config row: mic selector + countdown + monitor
    {
        auto config = content.removeFromTop (rowH);
        inputLabel.setBounds (config.removeFromLeft (82));
        inputCombo->setBounds (config.removeFromLeft (150).withHeight (rowH));
        config.removeFromLeft (gap);
        countdownLabel.setBounds (config.removeFromLeft (90));
        countdownCombo->setBounds (config.removeFromLeft (100).withHeight (rowH));
        config.removeFromLeft (gap);
        monitorToggle->setBounds (config.removeFromLeft (80).withHeight (rowH));
    }
    content.removeFromTop (gap);

    // Timer (big centered readout)
    timeLabel.setBounds (content.removeFromTop (Metrics::titleStripHeight));
    content.removeFromTop (gap);

    // Waveform / visualizer (fills available space)
    {
        const int vizH = juce::jmax (120, content.getHeight() - 160);
        waveform->setBounds (content.removeFromTop (vizH));
    }
    content.removeFromTop (gap);

    // Meter + clip indicator
    {
        auto meterRow = content.removeFromTop (30);
        clipLabel.setBounds (meterRow.removeFromRight (60));
        levelMeter->setBounds (meterRow);
    }
    content.removeFromTop (Spacing::md);

    // Record/Stop button (centered circle, large touch target)
    {
        const int btnR = Metrics::touchTargetMin;
        recordButton->setBounds (content.removeFromTop (btnR)
                                    .withSizeKeepingCentre (btnR, btnR));
    }
    content.removeFromTop (gap);

    // Actions row
    {
        auto actions = content.removeFromTop (rowH);
        const int btnW = 72;
        actions = actions.withSizeKeepingCentre (btnW * 5 + gap * 4, rowH);
        playButton.setBounds   (actions.removeFromLeft (btnW).withHeight (rowH));
        actions.removeFromLeft (gap);
        editButton->setBounds   (actions.removeFromLeft (btnW).withHeight (rowH));
        actions.removeFromLeft (gap);
        exportButton->setBounds (actions.removeFromLeft (btnW).withHeight (rowH));
        actions.removeFromLeft (gap);
        stopButton.setBounds   (actions.removeFromLeft (btnW).withHeight (rowH));
        actions.removeFromLeft (gap);
        deleteButton->setBounds (actions.removeFromLeft (btnW).withHeight (rowH));
    }
    content.removeFromTop (gap);

    // Format label (full width, subtle)
    formatLabel.setBounds (content.removeFromTop (18));
    content.removeFromTop (2);

    // Status / error
    errorLabel.setBounds (content.removeFromBottom (20));
    statusLabel.setBounds (content.removeFromBottom (20));

    // Record button shape (circle)
    juce::Path circle;
    circle.addEllipse (recordButton->getLocalBounds().toFloat());
    recordButton->setShape (circle, false, false, false);
}

bool RecordView::keyPressed (const juce::KeyPress& key)
{
    if (key.isKeyCode (juce::KeyPress::spaceKey)) { playPauseClicked();      return true; }
    if (key == juce::KeyPress ('r'))              { recordButtonClicked();   return true; }
    if (key.isKeyCode (juce::KeyPress::escapeKey) && counting) { cancelCountdown(); return true; }
    return false;
}

/* ===== Device management ================================================= */

void RecordView::populateDeviceCombos()
{
    auto* type = deviceManager.getCurrentDeviceTypeObject();
    if (type == nullptr) return;

    inputCombo->clear (juce::dontSendNotification);
    inputCombo->addItem ("(default)", 1);
    for (const auto& name : type->getDeviceNames (true))
        inputCombo->addItem (name, inputCombo->getNumItems() + 1);

    const auto setup = deviceManager.getAudioDeviceSetup();
    int id = 1;
    if (setup.inputDeviceName.isNotEmpty())
        for (int i = 1; i <= inputCombo->getNumItems(); ++i)
            if (inputCombo->getItemText (i - 1) == setup.inputDeviceName) { id = i; break; }
    inputCombo->setSelectedItemIndex (id - 1, juce::dontSendNotification);

    refreshFormatLabel();
}

void RecordView::applyDeviceSelection()
{
    if (recorder.getState() != otoha::TransportState::idle || counting)
        return;

    auto setup = deviceManager.getAudioDeviceSetup();
    setup.inputDeviceName = inputCombo->getSelectedItemIndex() <= 0
                                ? juce::String() : inputCombo->getText();
    const juce::String error = deviceManager.setAudioDeviceSetup (setup, true);
    errorLabel.setText (error.isEmpty() ? juce::String()
                                        : "Audio device problem: " + error,
                        juce::dontSendNotification);
    populateDeviceCombos();
    updateTransportState();
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
    const int ch = juce::jmax (0, recorder.getNumInputChannels());
    formatLabel.setText (juce::String (rate / 1000.0, 1) + " kHz · "
                             + (ch >= 2 ? "Stereo" : ch == 1 ? "Mono" : "no input"),
                         juce::dontSendNotification);
}

/* ===== Transport ========================================================= */

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
    if (counting) return;
    switch (recorder.getState())
    {
        case otoha::TransportState::recording: recorder.pauseRecording();  break;
        case otoha::TransportState::paused:    recorder.resumeRecording(); break;
        case otoha::TransportState::idle:      player.togglePlayPause();   break;
    }
    updateTransportState();
    grabKeyboardFocus();
}

void RecordView::stopClicked()
{
    if (counting) { cancelCountdown(); return; }
    if (recorder.getState() != otoha::TransportState::idle)
        finishRecording();
    else
    {
        player.stop();
        player.setPositionSeconds (0.0);
        updateTransportState();
    }
    grabKeyboardFocus();
}

/* ===== Countdown → recording → finish ==================================== */

void RecordView::beginCountdown()
{
    if (! recorder.hasInput())
    {
        errorLabel.setText ("No microphone available.\nConnect a microphone and try again.",
                            juce::dontSendNotification);
        return;
    }

    static constexpr int secondsChoices[] = { 0, 3, 5, 10 };
    const int seconds = secondsChoices[countdownCombo->getSelectedItemIndex()];
    if (seconds == 0) { beginRecording(); return; }

    counting = true;
    countdownDeadlineMs = juce::Time::getMillisecondCounterHiRes() + seconds * 1000.0;
    updateTransportState();
}

void RecordView::cancelCountdown()
{
    counting = false;
    countdownDeadlineMs = 0.0;
    timeLabel.setText (otoha::formatDuration (0.0), juce::dontSendNotification);
    updateTransportState();
}

void RecordView::beginRecording()
{
#if JUCE_ANDROID
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
    const int bitDepth = 24;  // always high quality

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
    if (file.existsAsFile() && file.getSize() > 44)
    {
        if (player.loadFile (file))
            player.setPositionSeconds (0.0);

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

/* ===== Post-recording actions ============================================ */

void RecordView::exportClicked()
{
    if (! player.hasFile() || recorder.getState() != otoha::TransportState::idle)
        return;

    auto defaultFile = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                           .getChildFile (player.getFile().getFileName());
    chooser = std::make_unique<juce::FileChooser> ("Export recording as WAV",
                                                   defaultFile, "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto target = fc.getResult();
                              if (target == juce::File{}) return;
                              if (target.existsAsFile()) target.deleteFile();
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
                                                if (result != 1) return;
                                                player.unload();
                                                if (! fileToDelete.moveToTrash())
                                                    fileToDelete.deleteFile();
                                                statusLabel.setText ("Deleted " + fileToDelete.getFileName(),
                                                                     juce::dontSendNotification);
                                                updateTransportState();
                                            }));
}

/* ===== Failure handling ================================================== */

void RecordView::handleFailure (otoha::FailureReason reason)
{
    switch (reason)
    {
        case otoha::FailureReason::none: return;
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

/* ===== Timer + state update ============================================== */

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
        clipLabel.setVisible (recorder.hasClipped());
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

    inputCombo->setEnabled (! busy && ! counting);
    countdownCombo->setEnabled (! busy && ! counting);
    monitorToggle->setEnabled (true);

    const bool hasRecording = player.hasFile() && ! busy;

    playButton.setEnabled (busy || hasRecording);
    playButton.setButtonText (busy ? (st == otoha::TransportState::paused ? "Resume" : "Pause")
                                   : "Play");
    stopButton.setEnabled (busy);

    editButton->setEnabled (hasRecording);
    exportButton->setEnabled (hasRecording);
    deleteButton->setEnabled (hasRecording);

    if (busy)
        statusLabel.setText (st == otoha::TransportState::paused ? "PAUSED" : "RECORDING",
                             juce::dontSendNotification);

    repaint();
}
