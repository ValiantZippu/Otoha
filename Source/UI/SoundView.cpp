#include "SoundView.h"

#include "../Dsp/Presets.h"
#include "../Sound/ProfileStorage.h"

#include <algorithm>

/*
    SoundView implementation. Parameter flow is strictly:

        UI -> SoundEngine (message-thread setters) -> DspChain

    The timer polls meters/diagnostics at 30 Hz; it never touches the audio
    path. Device changes re-initialise the backend on the message thread with
    stop -> init -> restore state -> start (#8/#31).
*/
SoundView::SoundView (juce::File baseDir)
    : profileStoreDirectory (baseDir)
{
    otoha::sound::loadProfiles (profiles, profileStoreDirectory);
    if (profiles.resolveForDevice ("") == nullptr)
        profiles.upsert ({ "default", "Default", "", presetToState (otoha::DspPreset::natural), true });

    buildUi();
    refreshOutputDevices();
    startTimerHz (30);
}

SoundView::~SoundView()
{
    stopTimer();
    backend.stop();
    backend.shutdown();
}

void SoundView::buildUi()
{
    // --- master toggle --------------------------------------------------------
    addAndMakeVisible (powerToggle);
    powerToggle.setClickingTogglesState (true);
    powerToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    powerToggle.onClick = [this]
    {
        engine.setEnabled (powerToggle.getToggleState());
        statusLabel.setText (engine.isEnabled() ? "ON" : "OFF", juce::dontSendNotification);
    };

    // --- enhance amount ---------------------------------------------------------
    enhanceSlider.setRange (0.0, 1.0, 0.01);
    enhanceSlider.setValue (1.0);
    addAndMakeVisible (enhanceSlider);
    enhanceSlider.onValueChange = [this] { engine.setEnhanceAmount ((float) enhanceSlider.getValue()); };
    engine.setEnhanceAmount (1.0f);

    // --- tonal controls (map onto ProcessingState, same as Studio) ---------------
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
        auto& active = *profiles.resolveForDevice ("");
        active.dspState.bassAmount    = (float) bassSlider.getValue();
        active.dspState.clarityAmount = (float) claritySlider.getValue();
        active.dspState.stereoWidth   = (float) spaceSlider.getValue();   // "Space"
        engine.setParameters (active.dspState);
        persistActiveProfile();
    };
    bassSlider.onValueChange    = publishTone;
    claritySlider.onValueChange = publishTone;
    spaceSlider.onValueChange   = publishTone;

    // --- profile combo -----------------------------------------------------------
    addAndMakeVisible (presetLabel);
    addAndMakeVisible (presetCombo);
    const auto presets = otoha::allDspPresets();
    for (int i = 0; i < presets.size(); ++i)
    {
        const auto p = presets.getReference (i);
        if (p != otoha::DspPreset::off)
            presetCombo.addItem (otoha::presetToString (p), i + 1);
    }
    presetCombo.setSelectedItemIndex (1, juce::dontSendNotification);   // Natural
    presetCombo.onChange = [this, presets]
    {
        const auto idx = juce::jlimit (0, presets.size() - 1, presetCombo.getSelectedItemIndex());
        auto* active = profiles.resolveForDevice ("");
        if (active == nullptr) return;
        active->dspState = otoha::presetToState (presets.getReference (idx));
        active->dspState.enabled = true;
        engine.setParameters (active->dspState);
        persistActiveProfile();
    };

    // --- output device combo -------------------------------------------------------
    addAndMakeVisible (outputLabel);
    addAndMakeVisible (outputCombo);
    outputCombo.setTextWhenNoChoicesAvailable ("No output devices");
    outputCombo.onChange = [this]
    {
        const auto id = outputCombo.getItemText (outputCombo.getSelectedItemIndex()).toStdString();
        backend.stop();
        backend.setSourceDevice ({});
        backend.setActiveDevice (id.empty() ? std::string {} : id);
        handleDeviceChange();
    };

    // --- status / meter / advanced ----------------------------------------------------
    statusLabel.setText ("OFF", juce::dontSendNotification);
    statusLabel.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    statusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (statusLabel);

    latencyLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (latencyLabel);
    addAndMakeVisible (meterLabel);
    addAndMakeVisible (advancedButton);
    advancedButton.onClick = [this] { refreshOutputDevices(); };   // placeholder; full advanced pane later
}

void SoundView::refreshOutputDevices()
{
    outputCombo.clear (juce::dontSendNotification);
    outputIds.clear();
    outputCombo.addItem ("System Default", 1);

    int defaultIndex = 0;
    for (const auto& d : backend.getDevices())
    {
        // Keep ids parallel to combo items (item 1 == empty id = system default).
        outputIds.push_back (d.id);
        outputCombo.addItem (juce::String (d.name), (int) outputIds.size() + 1);
        if (d.isDefault)
            defaultIndex = (int) outputIds.size();
    }
    outputCombo.setSelectedItemIndex (defaultIndex, juce::dontSendNotification);
}

void SoundView::handleDeviceChange()
{
    backend.stop();

    // Stable device binding when we have one; "" follows the default (#33-#36).
    const std::string selectedId =
        outputCombo.getSelectedItemIndex() > 0 && outputCombo.getSelectedItemIndex() <= (int) outputIds.size()
            ? outputIds[(size_t) outputCombo.getSelectedItemIndex() - 1]
            : std::string {};
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
    otoha::dsp::ProcessingContext ctx;
    ctx.sampleRate   = cfg.sampleRate   > 0 ? cfg.sampleRate   : 48000.0;
    ctx.numChannels  = cfg.numChannels  > 0 ? cfg.numChannels  : 2;
    ctx.maxBlockSize = cfg.maxBlockSize > 0 ? cfg.maxBlockSize : 512;

    if (! backend.initialize (ctx))
    {
        const auto status = backend.getStatus();
        statusLabel.setText ("Audio unavailable", juce::dontSendNotification);
        latencyLabel.setText (juce::String (status.message), juce::dontSendNotification);
        return;
    }

    engine.prepare (backend.getStreamConfig().numChannels > 0
                        ? otoha::dsp::ProcessingContext { backend.getStreamConfig().sampleRate,
                                                          backend.getStreamConfig().numChannels,
                                                          backend.getStreamConfig().maxBlockSize }
                        : ctx);

    backend.setProcessStage ([this] (otoha::dsp::AudioBlock& b)
                             { engine.process (b.channelData, b.numChannels, b.numFrames); });

    if (! backend.start())
        statusLabel.setText ("Audio unavailable", juce::dontSendNotification);
}

void SoundView::persistActiveProfile()
{
    otoha::sound::saveProfiles (profiles, profileStoreDirectory);
}

// -----------------------------------------------------------------------------
// Painting / layout / polling
// -----------------------------------------------------------------------------
void SoundView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff000000));                       // AMOLED-friendly base

    // Subtle sakura-pink accent band behind the header.
    juce::ColourGradient gradient (juce::Colour (0x30ff9ecf), (float) getWidth() * 0.2f, 0.0f,
                                   juce::Colour (0x10ff9ecf), (float) getWidth() * 0.8f, 120.0f, false);
    g.setGradientFill (gradient);
    g.fillRect (0, 0, getWidth(), 120);

    // Output meter bar (#25): smoothed peak from the shared chain's meter tap.
    if (! meterRect.isEmpty())
    {
        g.setColour (juce::Colour (0xff141414));
        g.fillRoundedRectangle (meterRect.toFloat(), 4.0f);

        const float level = juce::jlimit (0.0f, 1.0f, meterLevel);
        auto fill = meterRect.withWidth (meterRect.getWidth() * level);
        g.setColour (level > 0.95f ? juce::Colour (0xffff5a7e)      // clip-ish
                                   : juce::Colour (0xffff9ecf));    // sakura pink
        g.fillRoundedRectangle (fill.toFloat(), 4.0f);
    }
}

void SoundView::resized()
{
    auto bounds = getLocalBounds().reduced (24).withTrimmedTop (12);
    const int rowH = 34, gap = 10;

    statusLabel.setBounds (bounds.removeFromTop (48));
    bounds.reduce (40, 0);
    powerToggle.setBounds (bounds.removeFromTop (rowH + 6));
    bounds.translate (0, gap);

    enhanceSlider.setBounds (bounds.removeFromTop (rowH));  bounds.translate (0, gap);
    bassSlider.setBounds    (bounds.removeFromTop (rowH));  bounds.translate (0, gap);
    claritySlider.setBounds (bounds.removeFromTop (rowH));  bounds.translate (0, gap);
    spaceSlider.setBounds   (bounds.removeFromTop (rowH));  bounds.translate (0, gap);

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

    // Default-output changed while running (#9): follow it conservatively.
    if (autoSwitchProfiles && backend.defaultDeviceChangedSinceLastCheck())
        refreshOutputDevices();
}
