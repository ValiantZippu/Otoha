#include "Player.h"

Player::Player (juce::AudioDeviceManager& dm)
    : deviceManager (dm)
{
    formatManager.registerBasicFormats();
    bufferingThread.startThread (juce::Thread::Priority::low);
    sourcePlayer.setSource (&transport);
    deviceManager.addAudioCallback (&sourcePlayer);
}

Player::~Player()
{
    transport.setSource (nullptr);
    deviceManager.removeAudioCallback (&sourcePlayer);
    sourcePlayer.setSource (nullptr);
    bufferingThread.stopThread (5000);
}
bool Player::loadFile (const juce::File& file)
{
    stop();

    auto* reader = formatManager.createReaderFor (file);
    if (reader == nullptr)
        return false;

    customSource = nullptr;   // only one source drives the transport at a time
    currentFile = file;
    readerSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);

    const auto setup = deviceManager.getAudioDeviceSetup();
    transport.setSource (readerSource.get(),
                         32768,                       // read-ahead buffer
                         &bufferingThread,
                         reader->sampleRate,
                         2);
    return true;
}

void Player::loadCustomSource (std::unique_ptr<juce::PositionableAudioSource> source, double sampleRate)
{
    stop();

    readerSource = nullptr;
    currentFile = {};
    customSource = std::move (source);

    transport.setSource (customSource.get(), 32768, &bufferingThread, sampleRate, 2);
}

void Player::play()
{
    if (readerSource != nullptr || customSource != nullptr)
        transport.start();
}
void Player::pause()  { transport.stop(); }

void Player::togglePlayPause()
{
    if (readerSource == nullptr && customSource == nullptr)
        return;

    if (transport.isPlaying())
        transport.stop();
    else if (transport.hasStreamFinished())
        transport.setPosition (0.0);
    transport.start();
}

void Player::stop()
{
    transport.stop();
    transport.setPosition (0.0);
}

void Player::unload()
{
    stop();
    transport.setSource (nullptr);
    readerSource = nullptr;
    customSource = nullptr;
    currentFile = {};
}

double Player::getPositionSeconds() const   { return transport.getCurrentPosition(); }
double Player::getLengthSeconds() const     { return transport.getLengthInSeconds(); }

void Player::setPositionSeconds (double seconds) { transport.setPosition (seconds); }
