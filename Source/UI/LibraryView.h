#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Audio/Player.h"
#include "../Library/LibraryService.h"

/*
    LibraryView — answers "where are all my recordings?".

    - Search (display-name match, debounced-feel via immediate small SQLite query)
    - Filters: All / Audio / Video / Favorites
    - Sorts: Newest (default), Oldest, Name A–Z / Z–A, Longest, Shortest
    - Multi-select rows (Cmd/Ctrl-click toggle, Shift-click range)
    - Bulk: Favorite / Export (WAV copies) / Delete
    - Context menu: Play / Rename / Favorite / Export / Show in Folder / Delete
    - Details panel for a single selection
    - Empty states: no recordings ("Record something") and "No videos yet."

    Keyboard: Ctrl/Cmd+F search · Space play/pause · Delete remove · Ctrl/Cmd+A all.
    Waveforms come from the background WaveformCache; rows repaint as jobs finish.
*/
class LibraryView : public juce::Component,
                    private juce::Timer,
                    private juce::ListBoxModel
{
public:
    using OpenInEditorFn = std::function<bool (const otoha::MediaItem&)>;
    using IsFileOpenFn   = std::function<bool (const juce::File&)>;

    LibraryView (LibraryService& library, Player& player,
                 std::function<void()> goToRecording,
                 OpenInEditorFn openInEditor,
                 IsFileOpenFn isFileOpenInEditor);
    ~LibraryView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    void grabDefaultFocus()  { searchBox.grabKeyboardFocus(); }

    /** Re-queries the database (called when the shell shows this view). */
    void refreshItemsForDisplay()  { refreshItems(); }

private:
    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow (int row, bool selected, juce::Component* existing) override;

    // Helpers shared with RowComponent
    const otoha::MediaItem* itemForRow (int row) const;
    void selectRowWithModifiers (int row, const juce::MouseEvent& e);
    void handleRowActivated (int row);
    void showContextMenuFor (int row);
    void toggleFavoriteForRow (int row);

    void refreshItems();
    void updateDetailsPanel();
    void updateBulkBar();

    void playItem (int row);
    void renameDialogForId (juce::int64 id);
    void deleteSelected();
    void favoriteSelected();
    void exportSelected();

    void timerCallback() override;

    class RowComponent;
    class DetailsPanel;

    LibraryService& library;
    Player& player;
    std::function<void()> goToRecording;   // empty-state [RECORD] button
    OpenInEditorFn openInEditor;
    IsFileOpenFn isFileOpenInEditor;       // delete-safety guard

    std::vector<otoha::MediaItem> items;

    juce::TextEditor searchBox;
    juce::ToggleButton filterAll { "All" }, filterAudio { "Audio" },
                       filterVideo { "Video" }, filterFavorites { "Favorites" };
    juce::ComboBox sortCombo;

    juce::ListBox listBox;
    juce::Label selectionLabel;
    juce::TextButton bulkFavoriteButton { "Favorite" }, bulkExportButton { "Export" },
                     bulkDeleteButton { "Delete" };

    std::unique_ptr<DetailsPanel> details;

    juce::Label emptyTitle, emptySubtitle;
    juce::TextButton emptyRecordButton { "RECORD" };
    juce::Label videoEmptyLabel { {}, "No videos yet." };

    otoha::LibraryFilter currentFilter = otoha::LibraryFilter::all;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryView)
};
