#include "../Core/AppSettings.h"
#include "MainWindow.h"

#include <cmath>

/*
    Otoha — a simple place to record, enhance, edit, and keep your audio.

    One AudioDeviceManager is shared by the whole app; the Recorder listens for
    input, the Player writes output. Milestone 1 opens straight into Record.
*/
class OtohaApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Otoha"; }
    const juce::String getApplicationVersion() override { return OTOHA_VERSION; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String& commandLine) override
    {
        // --- configuration: load, migrate, stamp (#34/#42) ----------------------
        // Startup never blocks on network or auth; a missing/corrupt settings
        // file simply means defaults.
        appSettings = std::make_unique<otoha::AppSettings>();
        otoha::loadAppSettings (*appSettings, otoha::defaultSettingsDirectory());
        appSettings->safeModeSession = commandLine.containsIgnoreCase ("--safe-mode");
        appSettings->lastRunVersion = OTOHA_VERSION;
        saveAppSettings (*appSettings, otoha::defaultSettingsDirectory());
        // Open the default configuration first, then negotiate a sample rate:
        // prefer 48 kHz, but never assume the hardware supports it.
        auto setup = deviceManager.getAudioDeviceSetup();
        setup.sampleRate = 48000.0;
        setup.bufferSize = 512;
        deviceManager.setAudioDeviceSetup (setup, true);

        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            double best = 0.0;
            for (double rate : device->getAvailableSampleRates())
                if (best == 0.0 || std::abs (rate - 48000.0) < std::abs (best - 48000.0))
                    best = rate;

            if (best > 0.0 && std::abs (best - 48000.0) > 0.5)
            {
                setup.sampleRate = best;   // closest supported rate to the 48 kHz ideal
                deviceManager.setAudioDeviceSetup (setup, true);
            }
        }

        recorder = std::make_unique<Recorder> (deviceManager);
        player   = std::make_unique<Player> (deviceManager);

        // Library: open/create the database and reconcile with the filesystem.
        library = std::make_unique<LibraryService> (otohaBaseDirectory());
        juce::String libraryError;
        if (! library->initialise (libraryError))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "Otoha Library",
                                                    libraryError);
        else if (library->performStartupScan().recovered > 0)
            ; // recovered recordings are simply visible in the Library — no fanfare needed

        window = std::make_unique<MainWindow> ("Otoha", deviceManager, *recorder, *player, *library,
                                               appSettings.get());
    }

    void shutdown() override
    {
        window = nullptr;
        library = nullptr;
        player = nullptr;
        recorder = nullptr;
        deviceManager.closeAudioDevice();
    }

    void systemRequestedQuit() override { quit(); }

private:
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<otoha::AppSettings> appSettings;
    std::unique_ptr<Recorder> recorder;
    std::unique_ptr<Player> player;
    std::unique_ptr<LibraryService> library;
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (OtohaApplication)
