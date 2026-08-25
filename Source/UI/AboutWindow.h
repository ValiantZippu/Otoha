#pragma once

#include "../App/UpdateChecker.h"
#include "../Core/BuildInfo.h"
#include "OtohaTheme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#ifndef OTOHA_VERSION
 #define OTOHA_VERSION "dev"
#endif

/*
    AboutWindow — the tiny About screen (#35).

        OTOHA
        Simple audio enhancement.
        Version <OTOHA_VERSION>

        [ third-party license text ]
        [ Check for updates ]

    Third-party notices are read from THIRD-PARTY-NOTICES.txt next to the
    executable when present (the installer ships it); otherwise a built-in
    summary is shown so no distribution of the app ever lacks notices.

    The dialog manages its own lifetime (DialogWindow::LaunchOptions).
*/
namespace otoha::ui
{
inline juce::String thirdPartyNoticesText()
{
    const auto shipped = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                             .getSiblingFile ("THIRD-PARTY-NOTICES.txt");
    if (shipped.existsAsFile())
        return shipped.loadFileAsString();

    // Built-in fallback: accurate summary even for bare dev builds (#36/#37).
    return
        "Otoha uses these third-party components:\n\n"
        "JUCE 8 (juce.com) - dual licensed AGPLv3 / commercial.\n"
        "SQLite (sqlite.org) - public domain.\n"
        "FFmpeg (ffmpeg.org) - used, when installed on the system, as an external\n"
        "  process by Studio's export feature; never linked or bundled by Otoha.\n"
        "\n"
        "Full details: docs/licensing.md in the source repository.";
}

inline void showAboutWindow()
{
    auto* content = new juce::Component();

    auto* title = new juce::Label ("title", "OTOHA");
    title->setFont (otoha::theme::font (otoha::theme::TextSize::title));
    title->setJustificationType (juce::Justification::centred);
    title->setColour (juce::Label::textColourId, otoha::theme::colors::textPrimary());
    content->addAndMakeVisible (title);

    auto* subtitle = new juce::Label ("sub", "Simple audio enhancement.\n"
                                     + otoha::build::summary());   // #4 build metadata
    subtitle->setJustificationType (juce::Justification::centred);
    subtitle->setColour (juce::Label::textColourId, otoha::theme::colors::textSecondary());
    content->addAndMakeVisible (subtitle);

    auto* licenses = new juce::TextEditor ("licenses");
    licenses->setMultiLine (true);
    licenses->setReadOnly (true);
    licenses->setCaretVisible (false);
    licenses->setText (thirdPartyNoticesText());
    content->addAndMakeVisible (licenses);

    auto* updateButton = new juce::TextButton ("Check for updates");
    content->addAndMakeVisible (updateButton);

    static DisabledUpdateSource disabledSource;   // honest default: no server (#39)
    static UpdateChecker checker { &disabledSource };

    updateButton->onClick = []
    {
        const auto info = checker.checkNow();
        if (! info.checked)
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::InfoIcon, "Updates",
                "Update checking is not configured in this build.\n"
                "Current version: " + juce::String (OTOHA_VERSION));
        else if (UpdateChecker::isNewerThanCurrent (info.latestVersion))
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::InfoIcon, "Update available",
                "Otoha " + info.latestVersion + " is available.\n"
                "You can keep using this version until you're ready.");
        else
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::InfoIcon, "Up to date",
                "Otoha " + juce::String (OTOHA_VERSION) + " is up to date.");
    };

    content->setSize (420, 380);
    title->setBounds (0, 12, 420, 36);
    subtitle->setBounds (0, 50, 420, 44);
    licenses->setBounds (16, 100, 388, 216);
    updateButton->setBounds (16, 328, 160, 30);

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle            = "About Otoha";
    options.dialogBackgroundColour = otoha::theme::colors::background();
    options.content.setOwned (content);
    options.useNativeTitleBar      = true;
    options.resizable              = false;

    options.create()->setVisible (true);   // self-managing dialog
}
} // namespace otoha::ui
