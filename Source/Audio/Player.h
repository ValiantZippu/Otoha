#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <memory>

namespace juce { class PositionableAudioSource; }

/*
    Player — simple single-file playback on top of AudioTransportSource.
    Registered with the same shared AudioDeviceManager as the Recorder.
*/
class Player
{
public:
    explicit Player (juce::AudioDeviceManager& deviceManager);
    ~Player();

    /** Loads a file for playback. Returns false if it cannot be read as audio. */
    bool loadFile (const juce::File& file);

    /** Plays an arbitrary source (e.g. the editor's TimelineSource) through the
        same transport — the one playback engine for the whole app. */
    void loadCustomSource (std::unique_ptr<juce::PositionableAudioSource> source, double sampleRate);

    void play();
    void pause();
    void togglePlayPause();
    void stop();

    /** Releases the loaded file without deleting it. */
    void unload();

    bool isPlaying() const  { return transport.isPlaying(); }
    bool hasFile() const    { return readerSource != nullptr; }
    juce::File getFile() const  { return currentFile; }

    double getPositionSeconds() const;
    double getLengthSeconds() const;
    void setPositionSeconds (double seconds);

private:
    juce::AudioDeviceManager& deviceManager;
    juce::AudioFormatManager formatManager;
    juce::TimeSliceThread bufferingThread { "Otoha Playback" };
    juce::AudioSourcePlayer sourcePlayer;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::PositionableAudioSource> customSource;
    juce::File currentFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Player)
};
