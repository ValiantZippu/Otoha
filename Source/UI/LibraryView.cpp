#include "LibraryView.h"

#include "../Core/RecordingSupport.h"
#include "../Export/FfmpegSupport.h"
#include "ExportUi.h"

#include <algorithm>

// =============================================================================
// RowComponent — waveform + name + duration + date + favorite star.
// =============================================================================
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
        auto bounds = getLocalBounds().toFloat().reduced (8.0f, 4.0f);

        g.setColour (selected ? juce::Colour (0xff2b3a36)
                              : findColour (juce::ResizableWindow::backgroundColourId).contrasting (0.04f));
        g.fillRoundedRectangle (bounds, 8.0f);

        const auto* item = view.itemForRow (row);
        if (item == nullptr)
            return;

        // Waveform area on the left.
        auto waveArea = bounds.removeFromLeft (150.0f).reduced (6.0f);

        std::vector<float> peaks;
        if (view.library.getWaveformCache().getPeaks (*item, peaks) && ! peaks.empty())
        {
            g.setColour (juce::Colour (0xff4fc3a1));
            const float midY = waveArea.getCentreY();
            const float step = waveArea.getWidth() / (float) peaks.size();

            for (size_t i = 0; i < peaks.size(); ++i)
            {
                const float h = juce::jlimit (1.5f, waveArea.getHeight(), peaks[i] * waveArea.getHeight());
                g.fillRect (waveArea.getX() + (float) i * step, midY - h * 0.5f,
                            juce::jmax (1.0f, step - 1.0f), h);
            }
        }
        else if (item->type == otoha::MediaType::video)
        {
            g.setColour (juce::Colours::darkgrey);
            g.fillRoundedRectangle (waveArea, 6.0f);   // thumbnail placeholder until video lands
        }
        else
        {
            g.setColour (juce::Colours::grey.withAlpha (0.35f));
            g.setFont (juce::FontOptions (11.0f));
            g.drawText ("…", waveArea, juce::Justification::centred);  // generating
        }

        // Text block.
        auto text = bounds.reduced (10.0f, 0.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawText (item->displayName, text.removeFromTop (24), juce::Justification::centredLeft, true);

        g.setColour (juce::Colours::grey);
        g.setFont (juce::FontOptions (13.0f));

        auto infoLine = text.removeFromTop (18);
        g.drawText (otoha::formatDuration (item->durationSeconds), infoLine.removeFromLeft (70),
                    juce::Justification::centredLeft);
        g.drawText (otoha::friendlyRelativeDate (item->createdAt), infoLine.removeFromRight (110),
                    juce::Justification::centredLeft);

        // Favorite star, far right — also a click target (see mouseDown).
        auto star = bounds.removeFromRight (34.0f);
        g.setColour (item->favorite ? juce::Colour (0xffe8c35a) : juce::Colours::grey);
        g.setFont (juce::FontOptions (18.0f));
        g.drawText (item->favorite ? "★" : "☆", star, juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto* item = view.itemForRow (row);
        if (item == nullptr)
            return;

        // Star hit-test: toggle favorite without changing selection.
        if (e.position.x > (float) getWidth() - 40.0f)
        {
            view.toggleFavoriteForRow (row);
            return;
        }

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
        if (e.mods.isLeftButtonDown() && e.position.x <= (float) getWidth() - 40.0f)
            view.handleRowActivated (row);
    }

private:
    LibraryView& view;
    int row = -1;
    bool selected = false;
};

// =============================================================================
// DetailsPanel — simple metadata for a single selection.
// =============================================================================
class LibraryView::DetailsPanel : public juce::Component
{
public:
    explicit DetailsPanel (LibraryService& lib) : library (lib)
    {
        title.setFont (juce::FontOptions (17.0f, juce::Font::bold));
        title.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (title);

        body.setFont (juce::FontOptions (13.0f));
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
        auto area = getLocalBounds().toFloat().reduced (8.0f);
        g.setColour (findColour (juce::ResizableWindow::backgroundColourId).contrasting (0.04f));
        g.fillRoundedRectangle (area, 10.0f);
        g.drawRoundedRectangle (area, 10.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14);
        title.setBounds (area.removeFromTop (26));
        body.setBounds (area.removeFromTop (160));
    }

private:
    LibraryService& library;
    juce::Label title, body;
};

// =============================================================================
// LibraryView
// =============================================================================
LibraryView::LibraryView (LibraryService& lib, Player& pl, std::function<void()> goToRec,
                          OpenInEditorFn openInEditorFn, IsFileOpenFn fileOpenFn,
                          otoha::ExportManager& exportManagerRef,
                          otoha::ExportSettingsStore& exportStoreRef)
    : library (lib),
      player (pl),
      goToRecording (std::move (goToRec)),
      openInEditor (std::move (openInEditorFn)),
      isFileOpenInEditor (std::move (fileOpenFn)),
      exportManager (exportManagerRef),
      exportStore (exportStoreRef)
{
    addAndMakeVisible (searchBox);
    searchBox.setTextToShowWhenEmpty ("Search recordings...", juce::Colours::grey);
    searchBox.onTextChange = [this] { refreshItems(); };

    for (auto* b : { &filterAll, &filterAudio, &filterVideo, &filterFavorites })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1);
        addAndMakeVisible (*b);
        b->onClick = [this]
        {
            if      (filterAll.getToggleState())       currentFilter = otoha::LibraryFilter::all;
            else if (filterAudio.getToggleState())     currentFilter = otoha::LibraryFilter::audio;
            else if (filterVideo.getToggleState())     currentFilter = otoha::LibraryFilter::video;
            else                                       currentFilter = otoha::LibraryFilter::favorites;
            refreshItems();
        };
    }
    filterAll.setToggleState (true, juce::dontSendNotification);

    sortCombo.addItem ("Newest", 1);
    sortCombo.addItem ("Oldest", 2);
    sortCombo.addItem ("Name A–Z", 3);
    sortCombo.addItem ("Name Z–A", 4);
    sortCombo.addItem ("Longest", 5);
    sortCombo.addItem ("Shortest", 6);
    sortCombo.setSelectedItemIndex (0, juce::dontSendNotification);   // Newest first
    sortCombo.onChange = [this] { refreshItems(); };
    addAndMakeVisible (sortCombo);

    listBox.setModel (this);
    listBox.setRowHeight (76);
    listBox.setMultipleSelectionEnabled (true);
    listBox.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (listBox);

    selectionLabel.setFont (juce::FontOptions (13.0f));
    selectionLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (selectionLabel);

    for (auto* b : { &bulkFavoriteButton, &bulkExportButton, &bulkDeleteButton })
    {
        addAndMakeVisible (*b);
        b->setVisible (false);
    }
    bulkFavoriteButton.onClick = [this] { favoriteSelected(); };
    bulkExportButton.onClick   = [this] { exportSelected(); };
    bulkDeleteButton.onClick   = [this] { deleteSelected(); };

    details = std::make_unique<DetailsPanel> (library);
    addAndMakeVisible (*details);
    details->clear();

    emptyTitle.setText ("No recordings yet.", juce::dontSendNotification);
    emptyTitle.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    emptyTitle.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (emptyTitle);

    emptySubtitle.setText ("Record something with Otoha.", juce::dontSendNotification);
    emptySubtitle.setFont (juce::FontOptions (15.0f));
    emptySubtitle.setJustificationType (juce::Justification::centred);
    emptySubtitle.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (emptySubtitle);

    emptyRecordButton.onClick = [this] { if (goToRecording) goToRecording(); };
    addAndMakeVisible (emptyRecordButton);

    videoEmptyLabel.setText ("No videos yet.", juce::dontSendNotification);
    videoEmptyLabel.setFont (juce::FontOptions (18.0f));
    videoEmptyLabel.setJustificationType (juce::Justification::centred);
    videoEmptyLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addChildComponent (videoEmptyLabel);

    refreshItems();
    startTimerHz (2);   // repaints while background waveforms finish generating
}

LibraryView::~LibraryView() = default;

void LibraryView::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("Otoha", 20, 14, 200, 34, juce::Justification::centredLeft);

    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Library", 220, 24, 300, 20, juce::Justification::centredLeft);
}

void LibraryView::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (54); // title strip

    auto searchRow = bounds.removeFromTop (38).reduced (16, 2);
    searchBox.setBounds (searchRow.removeFromLeft (juce::jmin (420, searchRow.getWidth())));
    searchRow.removeFromLeft (12);
    sortCombo.setBounds (searchRow.removeFromRight (130));

    auto filterRow = bounds.removeFromTop (32).reduced (16, 0);
    auto placeFilter = [&filterRow] (juce::ToggleButton& b, int w)
    {
        b.setBounds (filterRow.removeFromLeft (w).withHeight (26));
        filterRow.removeFromLeft (10);
    };
    placeFilter (filterAll, 60);
    placeFilter (filterAudio, 72);
    placeFilter (filterVideo, 72);
    placeFilter (filterFavorites, 92);

    auto bulkRow = bounds.removeFromTop (30).reduced (16, 0);
    selectionLabel.setBounds (bulkRow.removeFromLeft (200));
    bulkDeleteButton.setBounds (bulkRow.removeFromRight (80).reduced (2, 2));
    bulkExportButton.setBounds (bulkRow.removeFromRight (80).reduced (2, 2));
    bulkFavoriteButton.setBounds (bulkRow.removeFromRight (88).reduced (2, 2));

    auto content = bounds.reduced (16, 8);
    details->setBounds (content.removeFromRight (260));

    listBox.setBounds (content);

    auto centre = listBox.getBounds().withSizeKeepingCentre (320, 120);
    emptyTitle.setBounds (centre.removeFromTop (34));
    emptySubtitle.setBounds (centre.removeFromTop (24));
    emptyRecordButton.setBounds (centre.removeFromTop (40).withSizeKeepingCentre (140, 34));
    videoEmptyLabel.setBounds (listBox.getBounds().withSizeKeepingCentre (300, 30));
}

bool LibraryView::keyPressed (const juce::KeyPress& key)
{
    // Never steal shortcuts from the search field or dialogs.
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
        if (dynamic_cast<juce::TextEditor*> (focused) != nullptr && ! key.isKeyCode (juce::KeyPress::escapeKey))
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

// -----------------------------------------------------------------------------
// ListBoxModel
// -----------------------------------------------------------------------------
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
    if (item == nullptr || ! item->isValid())
        return;

    if (player.loadFile (item->file))
        player.play();
}

// -----------------------------------------------------------------------------
// Actions
// -----------------------------------------------------------------------------
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
                                case 1:
                                {
                                    if (player.loadFile (path)) player.play();
                                    break;
                                }
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

// -----------------------------------------------------------------------------
// #20 Duplicate — an independent copy; the original is never touched.
// -----------------------------------------------------------------------------
void LibraryView::duplicateForRow (juce::int64 id)
{
    const auto newId = library.duplicateMedia (id);
    if (newId == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon, "Couldn't duplicate recording",
            "The copy could not be created.\nCheck free disk space and try again.");   // #71
        return;
    }
    refreshItems();
}

// -----------------------------------------------------------------------------
// #63-#65 Drag-and-drop import — reference in place, friendly on failure.
// -----------------------------------------------------------------------------
bool LibraryView::isInterestedInFileDrag (const juce::StringArray& files)
{
    return ! files.isEmpty();
}

void LibraryView::filesDropped (const juce::StringArray& files, int /*x*/, int /*y*/)
{
    int imported = 0;
    juce::StringArray unsupported;

    for (const auto& f : files)
    {
        const auto file = juce::File (f);
        if (! file.existsAsFile()) continue;

        // #64: register the external file where it lives — no conversion,
        // no move, original stays untouched. The startup scan also tolerates
        // these references going missing later (#66).
        if (library.registerAudioFile (file) != 0) ++imported;
        else                                       unsupported.add (file.getFileName());
    }

    refreshItems();

    if (! unsupported.isEmpty())
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon, "Some files weren't imported",
            "Otoha can't open this audio format:\n"
            + unsupported.joinIntoString ("\n"));   // #65
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
        true /* deleteWhenDismissed */);
}

void LibraryView::deleteSelected()
{
    std::vector<otoha::MediaItem> selected;
    for (int i = 0; i < listBox.getNumSelectedRows(); ++i)
        if (const auto* it = itemForRow (listBox.getSelectedRow (i)))
            selected.push_back (*it);

    if (selected.empty()) return;

    // Delete safety: never trash a recording whose editor document is still open.
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
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon,
                                        multiple ? "Delete recordings" : "Delete recording",
                                        multiple
                                            ? "Move " + juce::String ((int) selected.size())
                                                  + " recordings to the trash?\nThis cannot be undone."
                                            : "Move \"" + selected[0].displayName
                                                  + "\" to the trash?\nThis cannot be undone.",
                                        "Delete", "Cancel", this,
                                        juce::ModalCallbackFunction::create (
                                            [this, ids = selected] (int result)
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

    // Batch export: one format/quality choice for the whole selection (#23).
    otoha::FfmpegLocator locator;
    otoha::FfmpegInfo info;
    const bool ffmpegAvailable = locator.locate (info) == otoha::EncoderStatus::available;

    const auto choice = runExportOptionsDialog (this, exportStore, (int) selected.size(), ffmpegAvailable);
    if (! choice.confirmed) return;

    const auto startDir = exportStore.getLastDirectory();
    chooser = std::make_unique<juce::FileChooser> (
        "Choose the output folder", startDir);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this, targets = selected, choice] (const juce::FileChooser& fc)
                          {
                              const auto dir = fc.getResult();
                              if (dir == juce::File{}) return;

                              exportStore.remember (choice.format, choice.quality, dir);

                              // Each recording uses its OWN timeline/DSP state from
                              // its sidecar — the manager loads per-recording state.
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

// -----------------------------------------------------------------------------
// Refreshing / derived state
// -----------------------------------------------------------------------------
void LibraryView::refreshItems()
{
    using Sort = otoha::LibrarySort;
    static constexpr Sort sortsByItem[] = { Sort::newestFirst, Sort::oldestFirst,
                                            Sort::nameAscending, Sort::nameDescending,
                                            Sort::longestFirst, Sort::shortestFirst };

    items = library.query (searchBox.getText(),
                           currentFilter,
                           sortsByItem[juce::jlimit (0, 5, sortCombo.getSelectedItemIndex())]);

    // Empty states feel intentional, never like an error.
    const bool nothingAtAll = items.empty() && currentFilter != otoha::LibraryFilter::video
                                          && searchBox.getText().isEmpty();
    emptyTitle.setVisible (nothingAtAll);
    emptySubtitle.setVisible (nothingAtAll);
    emptyRecordButton.setVisible (nothingAtAll);
    videoEmptyLabel.setVisible (currentFilter == otoha::LibraryFilter::video && items.empty());

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
    bulkFavoriteButton.setVisible (bulk);
    bulkExportButton.setVisible (bulk);
    bulkDeleteButton.setVisible (bulk);
}

void LibraryView::timerCallback()
{
    // Rows show placeholders while their waveforms generate in the background.
    if (library.getWaveformCache().pendingCount() > 0)
        listBox.repaint();
}
