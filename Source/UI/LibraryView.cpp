#include "LibraryView.h"

#include "../Core/RecordingSupport.h"
#include "../Export/FfmpegSupport.h"
#include "ExportUi.h"
#include "OtohaTheme.h"
#include "Components/OtohaIcons.h"

using namespace otoha::theme;

#include <algorithm>

/* ======================================================================
   RowComponent — card-style row: waveform + name + duration + date.
   ====================================================================== */
class LibraryView::RowComponent : public juce::Component
{
public:
    RowComponent (LibraryView& owner) : view (owner) {}

    void update (int rowNumber, bool isSelected)
    {
        row = rowNumber;
        selected = isSelected;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (4.0f, 3.0f);

        // Card background: selection → accentSoft; hover → surfaceHover; default → surface
        juce::Colour bg = colors::surface();
        if (selected)                bg = colors::accentSoft();
        else if (isMouseOver())      bg = colors::surfaceHover();

        g.setColour (bg);
        g.fillRoundedRectangle (bounds, (float) Radius::medium);

        // Subtle border: selected → accent; default → borderSubtle
        g.setColour (selected ? colors::accent() : colors::borderSubtle());
        g.drawRoundedRectangle (bounds.reduced (0.5f), (float) Radius::medium, selected ? 1.5f : 1.0f);

        // Focus ring
        if (hasKeyboardFocus (true))
            otoha::ds::drawFocusRing (g, bounds, (float) Radius::medium);

        const auto* item = view.itemForRow (row);
        if (item == nullptr) return;

        // Waveform thumbnail (left side)
        auto waveArea = bounds.removeFromLeft (120.0f).reduced (8.0f, 10.0f);

        std::vector<float> peaks;
        if (view.library.getWaveformCache().getPeaks (*item, peaks) && ! peaks.empty())
        {
            g.setColour (colors::waveform());
            const float midY = waveArea.getCentreY();
            const float step = waveArea.getWidth() / (float) peaks.size();
            for (size_t i = 0; i < peaks.size(); ++i)
            {
                const float h = juce::jlimit (1.5f, waveArea.getHeight(), peaks[i] * waveArea.getHeight());
                g.fillRect (waveArea.getX() + (float) i * step, midY - h * 0.5f,
                            juce::jmax (1.0f, step - 1.0f), h);
            }
        }
        else
        {
            // Fallback: muted icon
            g.setColour (colors::waveformMuted());
            auto icon = otoha::icons::musicNote();
            auto iconArea = waveArea.withSizeKeepingCentre (24.0f, 24.0f);
            g.fillPath (icon, icon.getTransformToScaleToFit (iconArea, true));
        }

        // Text block
        auto text = bounds.reduced (10.0f, 6.0f);

        // Name
        g.setColour (colors::textPrimary());
        g.setFont (font (TextSize::bodySmall));
        g.drawText (item->displayName, text.removeFromTop (20), juce::Justification::centredLeft, true);

        // Duration + date
        g.setColour (colors::textMuted());
        g.setFont (font (TextSize::caption));
        auto infoLine = text.removeFromTop (16);
        g.drawText (otoha::formatDuration (item->durationSeconds),
                    infoLine.removeFromLeft (64), juce::Justification::centredLeft);
        g.drawText (otoha::friendlyRelativeDate (item->createdAt),
                    infoLine, juce::Justification::centredLeft);

        // Play icon overlay when playing
        if (view.player.hasFile() && view.player.getFile() == item->file && view.player.isPlaying())
        {
            g.setColour (colors::accent().withAlpha (0.85f));
            auto playIcon = otoha::icons::play();
            auto playArea = juce::Rectangle<float> (16.0f, 16.0f).withCentre (
                { waveArea.getCentreX(), waveArea.getCentreY() });
            g.fillPath (playIcon, playIcon.getTransformToScaleToFit (playArea, true));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            if (! view.listBox.isRowSelected (row))
                view.selectRowWithModifiers (row, e);
            view.showContextMenuFor (row);
            return;
        }
        view.selectRowWithModifiers (row, e);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown())
            view.handleRowActivated (row);
    }

private:
    LibraryView& view;
    int row = -1;
    bool selected = false;
};

/* ======================================================================
   DetailsPanel — compact metadata for a single selection.
   ====================================================================== */
class LibraryView::DetailsPanel : public juce::Component
{
public:
    explicit DetailsPanel (LibraryService& lib) : library (lib)
    {
        title.setFont (font (TextSize::heading));
        title.setColour (juce::Label::textColourId, colors::textPrimary());
        title.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (title);

        body.setFont (font (TextSize::caption));
        body.setColour (juce::Label::textColourId, colors::textMuted());
        body.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (body);
    }

    void showItem (const otoha::MediaItem& item)
    {
        title.setText (item.displayName.isEmpty() ? item.file.getFileNameWithoutExtension()
                                                  : item.displayName,
                       juce::dontSendNotification);

        auto sizeMb = juce::String ((double) item.fileSizeBytes / (1024.0 * 1024.0), 1);
        body.setText (
            juce::String ("Duration     ") + otoha::formatDuration (item.durationSeconds)
            + "\nFormat       " + item.format
            + "\nSample Rate  " + (item.sampleRate > 0 ? juce::String ((double) item.sampleRate / 1000.0, 1) + " kHz" : "—")
            + "\nBit Depth    " + (item.bitDepth > 0 ? juce::String (item.bitDepth) + "-bit" : "—")
            + "\nChannels     " + (item.channels >= 2 ? "Stereo" : item.channels == 1 ? "Mono" : "—")
            + "\nSize         " + sizeMb + " MB"
            + "\nCreated      " + item.createdAt.formatted ("%Y-%m-%d %H:%M"),
            juce::dontSendNotification);
    }

    void clear()
    {
        title.setText ({}, juce::dontSendNotification);
        body.setText ("Select a recording to see its details.", juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (4.0f);
        g.setColour (colors::surface());
        g.fillRoundedRectangle (area, (float) Radius::large);
        g.setColour (colors::borderSubtle());
        g.drawRoundedRectangle (area, (float) Radius::large, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14);
        title.setBounds (area.removeFromTop (26));
        area.removeFromTop (4);
        body.setBounds (area);
    }

private:
    LibraryService& library;
    juce::Label title, body;
};

/* ======================================================================
   LibraryView — M22 polished recording library.
   ====================================================================== */
LibraryView::LibraryView (LibraryService& lib, Player& pl, std::function<void()> goToRec,
                          OpenInEditorFn openInEditorFn, IsFileOpenFn fileOpenFn,
                          otoha::ExportManager& exportManagerRef,
                          otoha::ExportSettingsStore& exportStoreRef)
    : library (lib), player (pl), goToRecording (std::move (goToRec)),
      openInEditor (std::move (openInEditorFn)), isFileOpenInEditor (std::move (fileOpenFn)),
      exportManager (exportManagerRef), exportStore (exportStoreRef)
{
    setOpaque (true);

    // --- Header ---
    headerTitle.setFont (font (TextSize::title));
    headerTitle.setColour (juce::Label::textColourId, colors::textPrimary());
    addAndMakeVisible (headerTitle);

    countLabel.setFont (font (TextSize::caption));
    countLabel.setColour (juce::Label::textColourId, colors::textMuted());
    addAndMakeVisible (countLabel);

    // --- Search ---
    searchInput = std::make_unique<otoha::ds::Input> ("Search recordings", "Search recordings...");
    searchInput->onTextChange = [this] { refreshItems(); };
    addAndMakeVisible (*searchInput);

    // --- Sort ---
    sortCombo = std::make_unique<otoha::ds::ComboBox> ("Sort recordings");
    sortCombo->addItem ("Newest", 1);
    sortCombo->addItem ("Oldest", 2);
    sortCombo->addItem ("Name A–Z", 3);
    sortCombo->addItem ("Name Z–A", 4);
    sortCombo->addItem ("Longest", 5);
    sortCombo->addItem ("Shortest", 6);
    sortCombo->setSelectedItemIndex (0, juce::dontSendNotification);
    sortCombo->onChange = [this] { refreshItems(); };
    addAndMakeVisible (*sortCombo);

    // --- Filter chips ---
    auto setupFilter = [this] (otoha::ds::Button& btn, otoha::LibraryFilter filter)
    {
        addAndMakeVisible (btn);
        btn.onClick = [this, filter, &btn]
        {
            currentFilter = filter;
            refreshItems();
        };
    };
    setupFilter (filterAllBtn,   otoha::LibraryFilter::all);
    setupFilter (filterAudioBtn, otoha::LibraryFilter::audio);
    setupFilter (filterFavBtn,   otoha::LibraryFilter::favorites);
    filterAllBtn.setEnabled (false);  // "All" is the active/default

    // --- ListBox (virtualised card grid) ---
    listBox.setModel (this);
    listBox.setRowHeight (80);
    listBox.setMultipleSelectionEnabled (true);
    listBox.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (listBox);

    // --- Selection / bulk actions ---
    selectionLabel.setFont (font (TextSize::caption));
    selectionLabel.setColour (juce::Label::textColourId, colors::textMuted());
    addAndMakeVisible (selectionLabel);

    bulkExportBtn.onClick = [this] { exportSelected(); };
    bulkDeleteBtn.onClick = [this] { deleteSelected(); };
    addAndMakeVisible (bulkExportBtn);
    addAndMakeVisible (bulkDeleteBtn);
    bulkExportBtn.setVisible (false);
    bulkDeleteBtn.setVisible (false);

    // --- Details panel ---
    details = std::make_unique<DetailsPanel> (library);
    addAndMakeVisible (*details);
    details->clear();

    // --- Empty states ---
    otoha::ds::EmptyState::Setup emptySetup;
    emptySetup.icon        = otoha::icons::microphone();
    emptySetup.title       = "No recordings yet.";
    emptySetup.description = "Record something to start your library.";
    emptySetup.action      = &emptyRecordBtn;
    emptyState = std::make_unique<otoha::ds::EmptyState> (emptySetup);
    addAndMakeVisible (*emptyState);

    otoha::ds::EmptyState::Setup searchSetup;
    searchSetup.icon        = otoha::icons::search();
    searchSetup.title       = "No recordings found.";
    searchSetup.description = "Try another search.";
    searchEmptyState = std::make_unique<otoha::ds::EmptyState> (searchSetup);
    addAndMakeVisible (*searchEmptyState);

    emptyRecordBtn.onClick = [this] { if (goToRecording) goToRecording(); };

    refreshItems();
    startTimerHz (2);
}

LibraryView::~LibraryView() = default;

void LibraryView::paint (juce::Graphics& g)
{
    g.fillAll (colors::background());
}

void LibraryView::resized()
{
    auto bounds = getLocalBounds().reduced (Spacing::xl);
    const int maxW = 900;
    auto content = bounds.withSizeKeepingCentre (juce::jmin (maxW, bounds.getWidth()),
                                                 bounds.getHeight());

    // Header row: title + count
    {
        auto header = content.removeFromTop (32);
        headerTitle.setBounds (header.removeFromLeft (120));
        countLabel.setBounds (header.removeFromLeft (200));
    }
    content.removeFromTop (Spacing::xs);

    // Toolbar: search + sort + filter chips
    {
        auto toolbar = content.removeFromTop (36);
        searchInput->setBounds (toolbar.removeFromLeft (juce::jmin (320, toolbar.getWidth() / 2))
                                       .withHeight (28));
        toolbar.removeFromLeft (Spacing::sm);
        sortCombo->setBounds (toolbar.removeFromRight (130).withHeight (28));
        toolbar.removeFromRight (Spacing::sm);
        // Filter chips (right-aligned in remaining space)
        auto chips = toolbar;
        const int chipW = 64;
        filterFavBtn.setBounds   (chips.removeFromRight (chipW).withHeight (26));
        chips.removeFromRight (4);
        filterAudioBtn.setBounds (chips.removeFromRight (chipW).withHeight (26));
        chips.removeFromRight (4);
        filterAllBtn.setBounds   (chips.removeFromRight (chipW).withHeight (26));
    }
    content.removeFromTop (Spacing::xs);

    // Bulk actions bar (shown when multiple selected)
    {
        auto bulk = content.removeFromTop (28);
        selectionLabel.setBounds (bulk.removeFromLeft (200));
        bulkDeleteBtn.setBounds (bulk.removeFromRight (72).withHeight (24));
        bulk.removeFromRight (4);
        bulkExportBtn.setBounds (bulk.removeFromRight (72).withHeight (24));
    }

    // Main content: list + details
    {
        auto main = content;
        details->setBounds (main.removeFromRight (240));
        main.removeFromRight (Spacing::sm);
        listBox.setBounds (main);
    }

    // Empty states overlay on the list area
    auto listArea = listBox.getBounds().withSizeKeepingCentre (320, 120);
    emptyState->setBounds (listArea);
    searchEmptyState->setBounds (listArea);
}

void LibraryView::grabDefaultFocus()
{
    if (searchInput != nullptr)
        searchInput->grabKeyboardFocus();
}

bool LibraryView::keyPressed (const juce::KeyPress& key)
{
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
        if (dynamic_cast<juce::TextEditor*> (focused) != nullptr
            && ! key.isKeyCode (juce::KeyPress::escapeKey))
            return false;

    if (key.isKeyCode (juce::KeyPress::spaceKey))
    {
        const int last = listBox.getLastRowSelected();
        if (last >= 0) playItem (last);
        else player.togglePlayPause();
        return true;
    }
    if (key.isKeyCode (juce::KeyPress::deleteKey))
    {
        if (listBox.getNumSelectedRows() > 0) deleteSelected();
        return true;
    }
    if (key.isKeyCode ('A') && key.getModifiers().isCommandDown())
    {
        for (int i = 0; i < getNumRows(); ++i)
            listBox.selectRow (i, true);
        updateBulkBar();
        updateDetailsPanel();
        return true;
    }
    if (key.isKeyCode ('F') && key.getModifiers().isCommandDown())
    {
        grabDefaultFocus();
        return true;
    }
    return false;
}

// --- ListBoxModel ---------------------------------------------------------

int LibraryView::getNumRows() { return (int) items.size(); }

juce::Component* LibraryView::refreshComponentForRow (int row, bool selected, juce::Component* existing)
{
    if (row < 0 || row >= (int) items.size())
    {
        delete existing;
        return nullptr;
    }
    auto* rowComp = dynamic_cast<RowComponent*> (existing);
    if (rowComp == nullptr)
    {
        delete existing;
        rowComp = new RowComponent (*this);
    }
    rowComp->update (row, selected);
    return rowComp;
}

const otoha::MediaItem* LibraryView::itemForRow (int row) const
{
    return (row >= 0 && row < (int) items.size()) ? &items[(size_t) row] : nullptr;
}

void LibraryView::selectRowWithModifiers (int row, const juce::MouseEvent& e)
{
    if (e.mods.isShiftDown() && listBox.getLastRowSelected() >= 0)
        listBox.selectRangeOfRows (listBox.getLastRowSelected(), row);
    else if (e.mods.isCommandDown())
        listBox.flipRowSelection (row);
    else
        listBox.selectRow (row);
    updateBulkBar();
    updateDetailsPanel();
}

void LibraryView::handleRowActivated (int row) { playItem (row); }

void LibraryView::playItem (int row)
{
    const auto* item = itemForRow (row);
    if (item == nullptr || ! item->isValid()) return;
    if (player.loadFile (item->file))
        player.play();
}

// --- Actions --------------------------------------------------------------

void LibraryView::toggleFavoriteForRow (int row)
{
    const auto* item = itemForRow (row);
    if (item == nullptr) return;
    library.setFavorite (item->id, ! item->favorite);
    refreshItems();
}

void LibraryView::showContextMenuFor (int row)
{
    const auto* item = itemForRow (row);
    if (item == nullptr) return;

    juce::PopupMenu menu;
    menu.addItem (1, "Play");
    menu.addItem (7, "Open in Editor");
    menu.addSeparator();
    menu.addItem (2, "Rename...");
    menu.addItem (8, "Duplicate");
    menu.addItem (3, item->favorite ? "Unfavorite" : "Favorite");
    menu.addItem (4, "Export...");
    menu.addItem (5, "Show in Folder");
    menu.addSeparator();
    menu.addItem (6, "Delete");

    auto selectOnlyId = [this] (juce::int64 id)
    {
        listBox.deselectAllRows();
        for (int i = 0; i < (int) items.size(); ++i)
            if (items[(size_t) i].id == id)
                listBox.selectRow (i, true);
    };

    menu.showMenuAsync (juce::PopupMenu::Options(),
                        [this, id = item->id, path = item->file, selectOnlyId] (int result)
                        {
                            switch (result)
                            {
                                case 1: if (player.loadFile (path)) player.play(); break;
                                case 2: renameDialogForId (id); break;
                                case 7: openInEditor (library.get (id)); break;
                                case 8: duplicateForRow (id); break;
                                case 3:
                                    library.setFavorite (id, ! library.get (id).favorite);
                                    refreshItems();
                                    break;
                                case 4:
                                    selectOnlyId (id);
                                    updateBulkBar();
                                    exportSelected();
                                    break;
                                case 5: path.revealToUser(); break;
                                case 6:
                                    selectOnlyId (id);
                                    deleteSelected();
                                    break;
                                default: break;
                            }
                        });
}

void LibraryView::duplicateForRow (juce::int64 id)
{
    const auto newId = library.duplicateMedia (id);
    if (newId == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon, "Couldn't duplicate recording",
            "The copy could not be created.\nCheck free disk space and try again.");
        return;
    }
    refreshItems();
}

// --- Drag-and-drop import -------------------------------------------------

bool LibraryView::isInterestedInFileDrag (const juce::StringArray& files)
{
    return ! files.isEmpty();
}

void LibraryView::filesDropped (const juce::StringArray& files, int, int)
{
    int imported = 0;
    juce::StringArray unsupported;
    for (const auto& f : files)
    {
        const auto file = juce::File (f);
        if (! file.existsAsFile()) continue;
        if (library.registerAudioFile (file) != 0) ++imported;
        else unsupported.add (file.getFileName());
    }
    refreshItems();
    if (! unsupported.isEmpty())
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon, "Some files weren't imported",
            "Otoha can't open this audio format:\n" + unsupported.joinIntoString ("\n"));
}

void LibraryView::renameDialogForId (juce::int64 id)
{
    const auto item = library.get (id);
    if (item.id == 0) return;

    auto* window = new juce::AlertWindow ("Rename recording",
                                          "The file itself keeps its name — this changes how "
                                          "it appears in your library.",
                                          juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("name", item.displayName, "New name:");
    window->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    window->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, id, window] (int result)
        {
            if (result == 1)
                library.rename (id, window->getTextEditorContents ("name"));
            refreshItems();
        }),
        true);
}

void LibraryView::deleteSelected()
{
    std::vector<otoha::MediaItem> selected;
    for (int i = 0; i < listBox.getNumSelectedRows(); ++i)
        if (const auto* it = itemForRow (listBox.getSelectedRow (i)))
            selected.push_back (*it);

    if (selected.empty()) return;

    for (const auto& s : selected)
    {
        if (isFileOpenInEditor && isFileOpenInEditor (s.file))
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Recording is open",
                "\"" + s.displayName + "\" is currently open in the editor.\n"
                "Close the editor first, then delete it.");
            return;
        }
    }

    const bool multiple = selected.size() > 1;
    juce::AlertWindow::showOkCancelBox (
        juce::MessageBoxIconType::QuestionIcon,
        multiple ? "Delete recordings" : "Delete recording",
        multiple
            ? "Move " + juce::String ((int) selected.size()) + " recordings to the trash?\nThis cannot be undone."
            : "Move \"" + selected[0].displayName + "\" to the trash?\nThis cannot be undone.",
        "Delete", "Cancel", this,
        juce::ModalCallbackFunction::create ([this, ids = selected] (int result)
        {
            if (result != 1) return;
            player.unload();
            for (const auto& item : ids)
                library.deleteMedia (item.id);
            refreshItems();
        }));
}

void LibraryView::favoriteSelected()
{
    for (int i = 0; i < listBox.getNumSelectedRows(); ++i)
        if (const auto* it = itemForRow (listBox.getSelectedRow (i)))
            library.setFavorite (it->id, true);
    refreshItems();
}

void LibraryView::exportSelected()
{
    std::vector<otoha::MediaItem> selected;
    for (int i = 0; i < listBox.getNumSelectedRows(); ++i)
        if (const auto* it = itemForRow (listBox.getSelectedRow (i)))
            if (it->type == otoha::MediaType::audio)
                selected.push_back (*it);

    if (selected.empty()) return;

    otoha::FfmpegLocator locator;
    otoha::FfmpegInfo info;
    const bool ffmpegAvailable = locator.locate (info) == otoha::EncoderStatus::available;

    const auto choice = runExportOptionsDialog (this, exportStore, (int) selected.size(), ffmpegAvailable);
    if (! choice.confirmed) return;

    const auto startDir = exportStore.getLastDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Choose the output folder", startDir);
    chooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this, targets = selected, choice] (const juce::FileChooser& fc)
        {
            const auto dir = fc.getResult();
            if (dir == juce::File{}) return;
            exportStore.remember (choice.format, choice.quality, dir);
            for (const auto& item : targets)
            {
                otoha::ExportRequest request;
                request.sourceFile = item.file;
                request.baseName = item.displayName.isEmpty()
                    ? item.file.getFileNameWithoutExtension() : item.displayName;
                request.destinationDirectory = dir;
                request.format = choice.format;
                request.quality = choice.quality;
                exportManager.submit (request);
            }
            showExportProgressWindow (this, exportManager);
        });
}

// --- Refresh / derived state ----------------------------------------------

void LibraryView::refreshItems()
{
    using Sort = otoha::LibrarySort;
    static constexpr Sort sortsByItem[] = { Sort::newestFirst, Sort::oldestFirst,
                                            Sort::nameAscending, Sort::nameDescending,
                                            Sort::longestFirst, Sort::shortestFirst };

    items = library.query (searchInput != nullptr ? searchInput->getText() : juce::String(),
                           currentFilter,
                           sortsByItem[juce::jlimit (0, 5, sortCombo != nullptr ? sortCombo->getSelectedItemIndex() : 0)]);

    const bool hasItems = ! items.empty();
    const bool searching = searchInput != nullptr && searchInput->getText().isNotEmpty();
    const bool nothingAtAll = ! hasItems && ! searching;

    emptyState->setVisible (nothingAtAll);
    searchEmptyState->setVisible (hasItems == false && searching);

    // Recording count
    countLabel.setText (hasItems ? juce::String ((int) items.size()) + " recordings" : juce::String(),
                        juce::dontSendNotification);

    // Update filter button enabled states
    filterAllBtn.setEnabled (currentFilter != otoha::LibraryFilter::all);
    filterAudioBtn.setEnabled (currentFilter != otoha::LibraryFilter::audio);
    filterFavBtn.setEnabled (currentFilter != otoha::LibraryFilter::favorites);

    listBox.updateContent();
    updateBulkBar();
    updateDetailsPanel();
    resized();
}

void LibraryView::updateDetailsPanel()
{
    if (listBox.getNumSelectedRows() == 1)
        if (const auto* item = itemForRow (listBox.getSelectedRow (0)))
        {
            details->showItem (*item);
            return;
        }
    details->clear();
}

void LibraryView::updateBulkBar()
{
    const int n = listBox.getNumSelectedRows();
    const bool bulk = n > 1;
    selectionLabel.setText (bulk ? juce::String (n) + " selected" : juce::String(),
                            juce::dontSendNotification);
    bulkExportBtn.setVisible (bulk);
    bulkDeleteBtn.setVisible (bulk);
}

void LibraryView::timerCallback()
{
    if (library.getWaveformCache().pendingCount() > 0)
        listBox.repaint();
}
