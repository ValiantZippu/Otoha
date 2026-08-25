#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Audio/Player.h"
#include "../Export/ExportManager.h"
#include "../Export/ExportPresets.h"
#include "../Library/LibraryService.h"
#include "Components/DsButton.h"
#include "Components/DsControls.h"
#include "Components/DsCore.h"
#include "Components/DsSurfaces.h"

/*    LibraryView — Otoha's recording library (M22).

      Responsive card-grid browsing with search, sort, selection, bulk actions,
      playback preview, rename, and delete. All visuals consume OtohaTheme tokens.

      Preserves all existing M7–M14 business logic (ListBox virtualisation,
      drag-drop import, context menu, duplicate, export pipeline integration)
      while restyling through the M18 design-system component kit.
*/
class LibraryView : public juce::Component,
                    private juce::Timer,
                    private juce::ListBoxModel,
                    private juce::FileDragAndDropTarget
{
public:
    using OpenInEditorFn = std::function<bool (const otoha::MediaItem&)>;
    using IsFileOpenFn   = std::function<bool (const juce::File&)>;

    LibraryView (LibraryService& library, Player& player,
                 std::function<void()> goToRecording,
                 OpenInEditorFn openInEditor,
                 IsFileOpenFn isFileOpenInEditor,
                 otoha::ExportManager& exportManager,
                 otoha::ExportSettingsStore& exportStore);
    ~LibraryView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    void grabDefaultFocus();
    void refreshItemsForDisplay() { refreshItems(); }

private:
    int getNumRows() override;
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow (int row, bool selected, juce::Component* existing) override;

    const otoha::MediaItem* itemForRow (int row) const;
    void selectRowWithModifiers (int row, const juce::MouseEvent& e);
    void handleRowActivated (int row);
    void showContextMenuFor (int row);
    void toggleFavoriteForRow (int row);

    void refreshItems();
    void updateDetailsPanel();
    void updateBulkBar();

    void playItem (int row);
    void duplicateForRow (juce::int64 id);
    void renameDialogForId (juce::int64 id);
    void deleteSelected();
    void favoriteSelected();
    void exportSelected();

    void timerCallback() override;

    class RowComponent;
    class DetailsPanel;

    LibraryService& library;
    Player& player;
    std::function<void()> goToRecording;
    OpenInEditorFn openInEditor;
    IsFileOpenFn isFileOpenInEditor;
    otoha::ExportManager& exportManager;
    otoha::ExportSettingsStore& exportStore;

    std::vector<otoha::MediaItem> items;

    // M18 DS components for toolbar
    juce::Label headerTitle { {}, "Library" };
    juce::Label countLabel;
    std::unique_ptr<otoha::ds::Input> searchInput;
    std::unique_ptr<otoha::ds::ComboBox> sortCombo;

    // Filter chips (M18 buttons acting as toggles)
    otoha::ds::Button filterAllBtn     { "All",     otoha::ds::ButtonVariant::secondary, otoha::ds::ButtonSize::small };
    otoha::ds::Button filterAudioBtn   { "Audio",   otoha::ds::ButtonVariant::secondary, otoha::ds::ButtonSize::small };
    otoha::ds::Button filterFavBtn     { "Favorites", otoha::ds::ButtonVariant::secondary, otoha::ds::ButtonSize::small };

    // Card grid (virtualised ListBox)
    juce::ListBox listBox;

    // Bulk actions bar
    juce::Label selectionLabel;
    otoha::ds::Button bulkExportBtn { "Export", otoha::ds::ButtonVariant::secondary, otoha::ds::ButtonSize::small };
    otoha::ds::Button bulkDeleteBtn { "Delete", otoha::ds::ButtonVariant::danger,    otoha::ds::ButtonSize::small };

    std::unique_ptr<DetailsPanel> details;

    // Empty state
    otoha::ds::Button emptyRecordBtn { "Record", otoha::ds::ButtonVariant::primary };
    std::unique_ptr<otoha::ds::EmptyState> emptyState;
    std::unique_ptr<otoha::ds::EmptyState> searchEmptyState;

    otoha::LibraryFilter currentFilter = otoha::LibraryFilter::all;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryView)
};
