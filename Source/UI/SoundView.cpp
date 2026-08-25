#include "SoundView.h"

#include "AboutWindow.h"
#include "OtohaTheme.h"
#include "SoundAdvancedPanel.h"
#include "Components/DsButton.h"
#include "Components/DsControls.h"
#include "Components/DsSurfaces.h"

#include "../Dsp/Presets.h"
#include "../Sound/DiagnosticsReport.h"
#include "../Sound/ProfileStorage.h"

#include <algorithm>
#include <cmath>

/*
    SoundView implementation. Parameter flow is strictly:

        UI -> SoundEngine (message-thread setters) -> DspChain

    The timer polls meters/diagnostics at 30 Hz; it never touches the audio
    path. Device changes re-initialise the backend on the message thread with
    stop -> init -> restore state -> start (#8), surfacing as Recovering in the
    lifecycle while that happens (#18/#21).
*/
SoundView::SoundView (juce::File baseDir, otoha::AppSettings* appSettings, bool startInSafeMode)
    : baseDirectory (baseDir),
      settings (appSettings),
      lifecycle (otoha::AppState::starting),
      safeMode (startInSafeMode),
      userPresets (baseDir)
{
    otoha::sound::loadProfiles (profiles, baseDirectory);
    if (profiles.resolveForDevice ("") == nullptr)
        profiles.upsert ({ "default", "Default", "", presetToState (otoha::DspPreset::natural), true });

    userPresets.load();

    if (settings != nullptr && settings->sound.autoSwitchProfiles && ! safeMode)
        autoSwitchProfiles = true;

    buildUi();
    refreshOutputDevices();
    lifecycle.request (otoha::AppState::ready);
    startTimerHz (30);
}

SoundView::~SoundView()
{
    stopTimer();
    backend.stop();
    backend.shutdown();
    lifecycle.request (otoha::AppState::stopping);
}

void SoundView::buildUi()
{
    // --- master toggle --------------------------------------------------------
    addAndMakeVisible (powerToggle);
    powerToggle.setClickingTogglesState (true);
    powerToggle.setColour (juce::ToggleButton::textColourId, otoha::theme::colors::textPrimary());
    powerToggle.setName ("Sound power");
    powerToggle.setDescription ("Toggle Otoha Sound processing on or off");
    powerToggle.onClick = [this]
    {
        engine.setEnabled (powerToggle.getToggleState());
        lifecycle.setPower (powerToggle.getToggleState());
        updateStatusText();
        if (settings != nullptr)
        {
            settings->sound.enabled = powerToggle.getToggleState();
            saveAppSettings (*settings, otoha::defaultSettingsDirectory());
        }
    };

    // --- enhance amount ---------------------------------------------------------
    enhanceSlider.setRange (0.0, 1.0, 0.01);
    enhanceSlider.setValue (1.0);
    addAndMakeVisible (enhanceLabel);
    enhanceLabel.setFont (otoha::theme::font (otoha::theme::TextSize::caption));
    enhanceLabel.setColour (juce::Label::textColourId, otoha::theme::colors::textSecondary());
    addAndMakeVisible (enhanceSlider);
    enhanceSlider.onValueChange = [this]
    {
        engine.setEnhanceAmount ((float) enhanceSlider.getValue());
        if (settings != nullptr)
            settings->sound.enhanceAmount = (float) enhanceSlider.getValue();
    };
    engine.setEnhanceAmount (1.0f);

    // --- tonal controls (map onto ProcessingState, same as Studio) ---------------
    for (auto* lbl : { &bassLabel, &clarityLabel, &spaceLabel })
    {
        lbl->setFont (otoha::theme::font (otoha::theme::TextSize::caption));
        lbl->setColour (juce::Label::textColourId, otoha::theme::colors::textSecondary());
        addAndMakeVisible (*lbl);
    }
    for (auto* s : { &bassSlider, &claritySlider, &spaceSlider })
    {
        s->setRange (0.0, 1.0, 0.01);
        addAndMakeVisible (*s);
    }
    bassSlider.setValue (0.7);       // matches the Bass preset's character
    claritySlider.setValue (0.0);
    spaceSlider.setValue (0.5);

    auto publishTone = [this]
    {
        auto& active = *profiles.resolveForDeviceMutable ("");
        active.dspState.bassAmount    = (float) bassSlider.getValue();
        active.dspState.clarityAmount = (float) claritySlider.getValue();
        active.dspState.stereoWidth   = (float) spaceSlider.getValue();   // "Space"
        engine.setParameters (active.dspState);
        persistActiveProfile();
    };
    bassSlider.onValueChange    = publishTone;
    claritySlider.onValueChange = publishTone;
    spaceSlider.onValueChange   = publishTone;

    // --- preset combo: built-ins + custom + Save Preset… (#13) -------------------
    addAndMakeVisible (presetLabel);
    addAndMakeVisible (presetCombo);
    rebuildPresetCombo (settings != nullptr ? settings->sound.presetName : juce::String ("Natural"));
    presetCombo.onChange = [this]
    {
        if (presetCombo.getSelectedId() == kSavePresetItemId)
        {
            showSavePresetDialog();
            return;
        }
        applyPresetSelection();
    };

    // --- output device combo -------------------------------------------------------
    addAndMakeVisible (outputLabel);
    addAndMakeVisible (outputCombo);
    outputCombo.setTextWhenNoChoicesAvailable ("No audio output available");
    outputCombo.onChange = [this]
    {
        const int idx = outputCombo.getSelectedItemIndex();
        const auto id = (idx > 0 && idx <= (int) outputIds.size())
                            ? outputIds[(size_t) idx - 1] : std::string {};
        if (settings != nullptr)
        {
            settings->sound.outputDeviceId = juce::String (id);
            saveAppSettings (*settings, otoha::defaultSettingsDirectory());
        }
        handleDeviceChange();
    };

    // --- status / meter / advanced ----------------------------------------------------
    statusLabel.setFont (otoha::theme::font (otoha::theme::TextSize::heading));
    statusLabel.setColour (juce::Label::textColourId, otoha::theme::colors::textSecondary());
    statusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (statusLabel);

    latencyLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (latencyLabel);
    addAndMakeVisible (meterLabel);
    addAndMakeVisible (advancedButton);
    otoha::theme::styleCardButton (advancedButton);
    otoha::theme::label (advancedButton, "Advanced", "EQ, compressor, limiter, diagnostics");
    advancedButton.onClick = [this] { showAdvancedMenu(); };

    // --- restore persisted preferences -------------------------------------------
    if (settings != nullptr)
    {
        powerToggle.setToggleState (settings->sound.enabled, juce::dontSendNotification);
        engine.setEnabled (settings->sound.enabled);
        enhanceSlider.setValue (settings->sound.enhanceAmount, juce::dontSendNotification);
        engine.setEnhanceAmount ((float) settings->sound.enhanceAmount);
    }

    updateStatusText();
}

// -----------------------------------------------------------------------------
// Preset combo management (built-ins + custom + Save Preset…)
// -----------------------------------------------------------------------------

void SoundView::rebuildPresetCombo (const juce::String& selectName)
{
    presetCombo.clear (juce::dontSendNotification);

    const auto presets = otoha::allDspPresets();
    for (int i = 0; i < presets.size(); ++i)
    {
        const auto p = presets.getReference (i);
        if (p != otoha::DspPreset::off)
            presetCombo.addItem (otoha::presetToString (p), i + 1);
    }

    if (! safeMode)
    {
        presetCombo.addSeparator();
        const auto customs = userPresets.all();
        for (size_t i = 0; i < customs.size(); ++i)
            presetCombo.addItem (customs[i].name, kFirstCustomItemId + (int) i);
        presetCombo.addSeparator();
        presetCombo.addItem ("Save Preset…", kSavePresetItemId);
    }

    // Restore selection by name (built-in or custom).
    for (int i = 0; i < presetCombo.getNumItems(); ++i)
    {
        if (presetCombo.getItemText (i).equalsIgnoreCase (selectName))
        {
            presetCombo.setSelectedItemIndex (i, juce::dontSendNotification);
            break;
        }
    }
}

void SoundView::showSavePresetDialog()
{
    auto* window = new juce::AlertWindow ("Save Preset",
                                          "Name your sound. Built-in preset names stay "
                                          "untouched; yours are kept separately.",
                                          juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("name", "My Sound", "Name:");
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    window->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, window] (int result)
        {
            if (result != 1) return;

            const auto name = window->getTextEditorContents ("name");
            const auto state = profiles.resolveForDevice ("") != nullptr
                                   ? profiles.resolveForDevice ("")->dspState : otoha::ProcessingState {};

            const auto basedOn = presetCombo.getText();
            const auto id = userPresets.create (name, state, basedOn);
            if (id.isEmpty()) return;

            userPresets.save();
            rebuildPresetCombo (userPresets.get (id)->name);
        }),
        true /* deleteWhenDismissed */);
}

// -----------------------------------------------------------------------------
// Devices & stream lifecycle
// -----------------------------------------------------------------------------

void SoundView::refreshOutputDevices()
{
    outputCombo.clear (juce::dontSendNotification);
    outputIds.clear();
    outputNames.clear();
    outputCombo.addItem ("System Default", 1);

    int defaultIndex = 0;
    for (const auto& d : backend.getDevices())
    {
        // Keep ids parallel to combo items (item 1 == empty id = system default).
        outputIds.push_back (d.id);
        outputNames.push_back (juce::String (d.name));
        outputCombo.addItem (juce::String (d.name), (int) outputIds.size() + 1);
        if (d.isDefault)
            defaultIndex = (int) outputIds.size();
    }

    if (outputIds.empty())
    {
        outputCombo.setSelectedItemIndex (0, juce::dontSendNotification);
        lifecycle.request (otoha::AppState::unavailable);
        updateStatusText();     // "No audio output available" (#19)
        return;
    }

    outputCombo.setSelectedItemIndex (defaultIndex, juce::dontSendNotification);
}

void SoundView::handleDeviceChange()
{
    lifecycle.request (otoha::AppState::recovering);
    updateStatusText();
    backend.stop();

    // Stable device binding when we have one; "" follows the default (#33-#36).
    const int idx = outputCombo.getSelectedItemIndex();
    const std::string selectedId =
        idx > 0 && idx <= (int) outputIds.size() ? outputIds[(size_t) idx - 1] : std::string {};
    backend.setSourceDevice ({});          // capture follows system playback
    backend.setActiveDevice (selectedId);

    // Profile lookup by stable device id (#33): device-bound -> default -> natural.
    otoha::ProcessingState state;
    if (auto* profile = profiles.resolveForDevice (selectedId))
        state = profile->dspState;
    else if (auto* def = profiles.resolveForDevice (""))
        state = def->dspState;
    else
        state = otoha::presetToState (otoha::DspPreset::natural);

    engine.setParameters (state);
    bassSlider.setValue (state.bassAmount, juce::dontSendNotification);
    claritySlider.setValue (state.clarityAmount, juce::dontSendNotification);
    spaceSlider.setValue (state.stereoWidth, juce::dontSendNotification);

    // Rebuild the stream: stop -> init -> prepare DSP -> start (#8).
    auto cfg = backend.getStreamConfig();
    if (cfg.sampleRate   <= 0) cfg.sampleRate   = 48000.0;
    if (cfg.numChannels  <= 0) cfg.numChannels  = 2;
    if (cfg.maxBlockSize <= 0) cfg.maxBlockSize = 512;

    if (! backend.initialize (cfg))
    {
        const auto status = backend.getStatus();
        latencyLabel.setText (juce::String (status.message), juce::dontSendNotification);

        lifecycle.request (otoha::AppState::unavailable);
        updateStatusText();

        // Actionable guidance once — no stack traces, no HRESULTs (#20).
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon, "Couldn't start audio processing",
            "Otoha couldn't access this audio device.\n\n"
            "Try:\n"
            "- selecting another output\n"
            "- restarting Otoha\n"
            "- checking Windows sound settings");
        return;
    }

    engine.prepare (otoha::dsp::ProcessingContext { cfg.sampleRate, cfg.numChannels, cfg.maxBlockSize });

    backend.setProcessStage ([this] (otoha::dsp::AudioBlock& b)
                             { engine.process (b.channelData, b.numChannels, b.numFrames); });

    if (! backend.start())
    {
        lifecycle.request (otoha::AppState::unavailable);
        updateStatusText();
        return;
    }

    lifecycle.setPower (powerToggle.getToggleState());
    updateStatusText();
}

/** Publishes the combo's current selection into the profile + engine (#6). */
void SoundView::applyPresetSelection()
{
    const int id = presetCombo.getSelectedId();
    auto* active = profiles.resolveForDeviceMutable ("");
    if (active == nullptr || id <= 0) return;

    if (id >= kFirstCustomItemId && id < kSavePresetItemId)
    {
        const auto customs = userPresets.all();
        const size_t idx = (size_t) (id - kFirstCustomItemId);
        if (idx >= customs.size()) return;
        active->dspState = customs[idx].state;
        active->dspState.enabled = true;
    }
    else
    {
        const auto presets = otoha::allDspPresets();
        const int pIdx = juce::jlimit (0, presets.size() - 1, id - 1);
        active->dspState = otoha::presetToState (presets.getReference (pIdx));
        active->dspState.enabled = true;
    }

    engine.setParameters (active->dspState);
    bassSlider.setValue    (active->dspState.bassAmount,    juce::dontSendNotification);
    claritySlider.setValue (active->dspState.clarityAmount, juce::dontSendNotification);
    spaceSlider.setValue   (active->dspState.stereoWidth,   juce::dontSendNotification);
    persistActiveProfile();

    if (settings != nullptr)
    {
        settings->sound.presetName = presetCombo.getText();
        saveAppSettings (*settings, otoha::defaultSettingsDirectory());
    }
}

void SoundView::applyFirstLaunchChoices (bool enhanceOn, const juce::String& presetName)
{
    powerToggle.setToggleState (enhanceOn, juce::sendNotificationSync);
    rebuildPresetCombo (presetName);
    applyPresetSelection();

    if (settings != nullptr)
    {
        settings->sound.enabled     = enhanceOn;
        settings->sound.presetName  = presetName;
        settings->sound.enhanceAmount = (float) enhanceSlider.getValue();
        saveAppSettings (*settings, otoha::defaultSettingsDirectory());
    }
}

void SoundView::persistActiveProfile()
{
    otoha::sound::saveProfiles (profiles, baseDirectory);
}

void SoundView::updateStatusText()
{
    // One source of truth: the lifecycle (#18). The UI cannot claim ON unless
    // the state machine is actually in `processing`.
    statusLabel.setText (otoha::appStateToString (lifecycle.current()),
                         juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId,
                           lifecycle.current() == otoha::AppState::processing ? otoha::theme::colors::textPrimary()
                           : lifecycle.current() == otoha::AppState::unavailable ? otoha::theme::colors::meterClip()
                                                                                 : otoha::theme::colors::textSecondary());
}

// -----------------------------------------------------------------------------
// Advanced menu (#7/#13/#35/#44/#47)
// -----------------------------------------------------------------------------

void SoundView::showAdvancedMenu()
{
    juce::PopupMenu menu;

    menu.addItem (1, "Equalizer / Compressor / Limiter…");
    menu.addSeparator();
    menu.addItem (2, "Export diagnostics report…");
    menu.addItem (3, "About Otoha");
    menu.addSeparator();
    menu.addItem (4, "Reset audio settings");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (advancedButton),
                        [this] (int result)
                        {
                            switch (result)
                            {
                                case 1:
                                {
                                    const auto current = profiles.resolveForDevice ("") != nullptr
                                        ? profiles.resolveForDevice ("")->dspState : otoha::ProcessingState{};
                                    otoha::ui::showAdvancedPanel (current, [this] (const otoha::ProcessingState& s)
                                    {
                                        if (auto* active = profiles.resolveForDeviceMutable (""))
                                        {
                                            active->dspState = s;
                                            engine.setParameters (s);
                                            persistActiveProfile();
                                        }
                                    });
                                    break;
                                }
                                case 2: exportDiagnosticsReport(); break;
                                case 3: otoha::ui::showAboutWindow(); break;
                                case 4:
                                    juce::AlertWindow::showOkCancelBox (
                                        juce::MessageBoxIconType::QuestionIcon,
                                        "Reset audio settings",
                                        "This restores Otoha's audio settings to their defaults.\n"
                                        "Your recordings, library and device profiles are kept.",
                                        "Reset", "Cancel", this,
                                        juce::ModalCallbackFunction::create ([this] (int ok)
                                        {
                                            if (ok != 1 || settings == nullptr) return;
                                            otoha::resetAudioPrefs (*settings);
                                            saveAppSettings (*settings, otoha::defaultSettingsDirectory());

                                            powerToggle.setToggleState (false, juce::dontSendNotification);
                                            engine.setEnabled (false);
                                            lifecycle.setPower (false);
                                            enhanceSlider.setValue (1.0, juce::dontSendNotification);
                                            engine.setEnhanceAmount (1.0f);
                                            updateStatusText();
                                        }));
                                    break;
                                default: break;
                            }
                        });
}

void SoundView::exportDiagnosticsReport()
{
    const auto streamCfg = backend.getStreamConfig();
    const auto stats = engine.getStats();
    const double latency = backend.getLatencyMs();

    otoha::sound::DiagnosticsInput in;
    in.backendName =
        #if JUCE_WINDOWS
        "WASAPI shared loopback";
        #else
        juce::String (backend.getUnsupportedReason());
        #endif
    in.outputName       = outputCombo.getText();
    in.profileName      = profiles.resolveForDevice ("") != nullptr ? juce::String ("Default") : juce::String();
    in.sampleRate       = streamCfg.sampleRate;
    in.channels         = streamCfg.numChannels;
    in.bufferSizeFrames = streamCfg.maxBlockSize;
    in.hasLatency       = latency > 0.0;
    in.latencyMs        = latency;
    in.underruns        = backend.getUnderruns();
    in.blocksProcessed  = stats.blocksProcessed;
    in.blocksPassed     = stats.blocksPassed;
    in.safeMode         = safeMode;

    chooser = std::make_unique<juce::FileChooser> ("Save diagnostics report",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                       .getChildFile ("Otoha-Diagnostics.txt"),
                                                   "*.txt");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles,
                          [report = otoha::sound::buildDiagnosticsReport (in)] (const juce::FileChooser& fc)
                          {
                              if (const auto file = fc.getResult(); file != juce::File{})
                                  file.replaceWithText (report);
                          });
}

// -----------------------------------------------------------------------------
// Painting / layout / polling
// -----------------------------------------------------------------------------

void SoundView::paint (juce::Graphics& g)
{
    g.fillAll (otoha::theme::colors::background());               // AMOLED-friendly base

    // Subtle accent band behind the header.
    juce::ColourGradient gradient (otoha::theme::colors::accent().withAlpha (0.19f), (float) getWidth() * 0.2f, 0.0f,
                                   otoha::theme::colors::accent().withAlpha (0.06f), (float) getWidth() * 0.8f, 120.0f, false);
    g.setGradientFill (gradient);
    g.fillRect (0, 0, getWidth(), 120);

    // Output meter bar (#25): smoothed peak from the shared chain's meter tap.
    if (! meterRect.isEmpty())
    {
        g.setColour (otoha::theme::colors::surfaceElevated());
        g.fillRoundedRectangle (meterRect.toFloat(), 4.0f);

        const float level = juce::jlimit (0.0f, 1.0f, meterLevel);
        auto fill = meterRect.withWidth (meterRect.getWidth() * level);
        g.setColour (level > 0.95f ? otoha::theme::colors::meterClip()      // clip-ish
                                   : otoha::theme::colors::meterSafe());   // safe level
        g.fillRoundedRectangle (fill.toFloat(), 4.0f);
    }
}

void SoundView::resized()
{
    auto bounds = getLocalBounds().reduced (24).withTrimmedTop (12);
    const int rowH = 34, gap = 10;

    statusLabel.setBounds (bounds.removeFromTop (32));
    bounds.reduce (40, 0);
    powerToggle.setBounds (bounds.removeFromTop (rowH + 6));
    bounds.translate (0, gap);

    {
        auto row = bounds.removeFromTop (rowH);
        enhanceLabel.setBounds (row.removeFromLeft (80));
        enhanceSlider.setBounds (row);
    }
    bounds.translate (0, gap);
    {
        auto row = bounds.removeFromTop (rowH);
        bassLabel.setBounds (row.removeFromLeft (80));
        bassSlider.setBounds (row);
    }
    bounds.translate (0, gap);
    {
        auto row = bounds.removeFromTop (rowH);
        clarityLabel.setBounds (row.removeFromLeft (80));
        claritySlider.setBounds (row);
    }
    bounds.translate (0, gap);
    {
        auto row = bounds.removeFromTop (rowH);
        spaceLabel.setBounds (row.removeFromLeft (80));
        spaceSlider.setBounds (row);
    }
    bounds.translate (0, gap);

    auto profileRow = bounds.removeFromTop (rowH);
    presetLabel.setBounds  (profileRow.removeFromLeft (80));
    presetCombo.setBounds  (profileRow);
    bounds.translate (0, gap);

    auto outputRow = bounds.removeFromTop (rowH);
    outputLabel.setBounds (outputRow.removeFromLeft (80));
    outputCombo.setBounds (outputRow);
    bounds.translate (0, gap);

    meterLabel.setBounds (bounds.removeFromTop (20));
    meterRect = bounds.removeFromTop (14).withTrimmedRight (160);
    bounds.translate (0, gap);

    latencyLabel.setBounds (bounds.removeFromTop (22));
    advancedButton.setBounds (bounds.removeFromBottom (28));
}

void SoundView::timerCallback()
{
    // Meters: smoothed peak from the shared chain's meter tap.
    const auto m = engine.getMeters();
    meterLevel = std::max (m.peak, meterLevel * 0.85f);
    repaint();

    // Diagnostics text — honest numbers only (#27/#30).
    const double latency = backend.getLatencyMs();
    juce::String info;
    if (latency > 0.0)
        info << "~" << (int) std::round (latency) << " ms latency";
    else
        info << "Latency information unavailable";

    const auto underruns = backend.getUnderruns();
    if (underruns > 0)
        info << "  ·  " << underruns << " underruns";
    latencyLabel.setText (info, juce::dontSendNotification);

    // Default-output changed while running (#9/#19): follow it conservatively.
    if (autoSwitchProfiles && backend.defaultDeviceChangedSinceLastCheck())
    {
        refreshOutputDevices();
        handleDeviceChange();
    }
}
