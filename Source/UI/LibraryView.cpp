#include "LibraryView.h"

#include "../Core/RecordingSupport.h"
#include "../Export/FfmpegSupport.h"
#include "ExportUi.h"
#include "OtohaTheme.h"
#include "Components/OtohaIcons.h"
#include "Components/DsResponsive.h"

using namespace otoha::theme;

#include <algorithm>

/* ======================================================================
   RowComponent — Kaiteyo-aligned card: waveform + name + meta + actions.
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
        auto bounds = getLocalBounds().toFloat().reduced (3.0f, 2.0f);

        // Card background
        juce::Colour bg = colors::surface();
        if (selected && view.selectMode)
            bg = colors::accentSoft();
        else if (isMouseOver())
            bg = colors::surfaceHover();

        g.setColour (bg);
        g.fillRoundedRectangle (bounds, (float) Radius::medium);

        // Border
        const bool showBorder = selected && view.selectMode;
        g.setColour (showBorder ? colors::accent() : colors::borderSubtle());
        g.drawRoundedRectangle (bounds.reduced (0.5f), (float) Radius::medium,
                                showBorder ? 1.5f : 1.0f);

        // Accent top-line on hover (Kaiteyo pattern)
        if (isMouseOver() && ! isMouseButtonDown())
        {
            g.setColour (colors::accent().withAlpha (0.6f));
            g.fillRect (bounds.getX(), bounds.getY(), bounds.getWidth(), 2.0f);
        }

        // Focus ring
        if (hasKeyboardFocus (true))
            otoha::ds::drawFocusRing (g, bounds, (float) Radius::medium);

        const auto* item = view.itemForRow (row);
        if (item == nullptr) return;

        // --- Selection checkbox (when in select mode) ---
        if (view.selectMode)
        {
            const float cbSize = 18.0f;
            auto cbBounds = juce::Rectangle<float> (cbSize, cbSize)
                .withCentre ({ bounds.getX() + 20.0f, bounds.getCentreY() });

            g.setColour (selected ? colors::accent() : colors::border());
            g.drawRoundedRectangle (cbBounds, 4.0f, 1.5f);

            if (selected)
            {
                g.setColour (colors::accent());
                auto check = otoha::icons::play(); // reuse as checkmark placeholder
                g.fillRoundedRectangle (cbBounds.reduced (4.0f), 2.0f);
            }
        }

        const float contentLeft = view.selectMode ? bounds.getX() + 38.0f : bounds.getX();

        // --- Waveform thumbnail (left side) ---
        const float waveW = juce::jmin (130.0f, bounds.getWidth() * 0.22f);
        auto waveArea = juce::Rectangle<float> (waveW, bounds.getHeight() - 20.0f)
            .withCentre ({ contentLeft + waveW * 0.5f + Spacing::md, bounds.getCentreY() });

        std::vector<float> peaks;
        if (view.library.getWaveformCache().getPeaks (*item, peaks) && ! peaks.empty())
        {
            g.setColour (colors::waveform());
            const float midY = waveArea.getCentreY();
            const float step = waveArea.getWidth() / (float) peaks.size();
            for (size_t i = 0; i < peaks.size(); ++i)
            {
                const float h = juce::jlimit (1.0f, waveArea.getHeight(), peaks[i] * waveArea.getHeight());
                g.fillRect (waveArea.getX() + (float) i * step, midY - h * 0.5f,
                            juce::jmax (1.0f, step - 0.8f), h);
            }
        }
        else
        {
            g.setColour (colors::waveformMuted());
            auto icon = otoha::icons::musicNote();
            auto iconArea = waveArea.withSizeKeepingCentre (24.0f, 24.0f);
            g.fillPath (icon, icon.getTransformToScaleToFit (iconArea, true));
        }

        // --- Play overlay when playing ---
        if (view.player.hasFile() && view.player.getFile() == item->file && view.player.isPlaying())
        {
            g.setColour (colors::accent().withAlpha (0.85f));
            auto playIcon = otoha::icons::play();
            auto playArea = juce::Rectangle<float> (18.0f, 18.0f).withCentre (waveArea.getCentre());
            g.fillPath (playIcon, playIcon.getTransformToScaleToFit (playArea, true));
        }

        // --- Text block (right of waveform) ---
        auto text = juce::Rectangle<float> (contentLeft + waveW + Spacing::lg * 2,
                                            bounds.getY() + 12.0f,
                                            bounds.getRight() - Spacing::xl * 2 - contentLeft - waveW - Spacing::lg * 2,
                                            bounds.getHeight() - 24.0f);

        // Name
        g.setColour (colors::textPrimary());
        g.setFont (font (TextSize::bodySmall));
        auto nameArea = text.removeFromTop (20.0f);
        g.drawText (item->displayName, nameArea, juce::Justification::centredLeft, true);

        // Duration + format badge
        auto metaLine = text.removeFromTop (18.0f);
        g.setColour (colors::textMuted());
        g.setFont (font (TextSize::caption));
        g.drawText (otoha::formatDuration (item->durationSeconds),
                    metaLine.removeFromLeft (80), juce::Justification::centredLeft);

        // Format badge
        if (item->format.isNotEmpty())
        {
            auto badgeText = item->format.toUpperCase();
            g.setFont (font (TextSize::caption));
            // Approximate width for short format labels (WAV, FLAC, M4A, etc.)
            const float badgeW = (float) badgeText.length() * 7.0f + 12.0f;
            auto badgeArea = juce::Rectangle<float> (badgeW, 14.0f)
                .withCentre ({ metaLine.getX() + badgeW * 0.5f + 4.0f, metaLine.getCentreY() });
            g.setColour (colors::accent().withAlpha (0.12f));
            g.fillRoundedRectangle (badgeArea, 7.0f);
            g.setColour (colors::accent());
            g.drawText (badgeText, badgeArea, juce::Justification::centred);
        }

        // Date (right side)
        g.setColour (colors::textMuted());
        g.setFont (font (TextSize::caption));
        auto dateArea = juce::Rectangle<float> (bounds.getRight() - Spacing::xl * 2 - 100.0f,
                                                metaLine.getY(), 100.0f, metaLine.getHeight());
        g.drawText (otoha::friendlyRelativeDate (item->createdAt),
                    dateArea, juce::Justification::centredRight, true);

        // Favorite star
        if (item->favorite)
        {
            g.setColour (colors::accent());
            auto starIcon = otoha::icons::musicNote(); // placeholder for star
            auto starArea = juce::Rectangle<float> (12.0f, 12.0f)
                .withCentre ({ bounds.getRight() - 16.0f, bounds.getY() + 16.0f });
            g.fillEllipse (starArea);
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
        if (e.mods.isLeftButtonDown() && ! view.selectMode)
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
            + "\nSample Rate  " + (item.sampleRate > 0 ? juce::String ((double) item.sampleRate / 1000.0, 1) + " kHz" : "\u2014")
            + "\nBit Depth    " + (item.bitDepth > 0 ? juce::String (item.bitDepth) + "-bit" : "\u2014")
            + "\nChannels     " + (item.channels >= 2 ? "Stereo" : item.channels == 1 ? "Mono" : "\u2014")
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
   LibraryView — M31 Kaiteyo-aligned recording library.
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

    // --- Header: title + subtitle ---
    headerTitle.setFont (font (TextSize::title));
    headerTitle.setColour (juce::Label::textColourId, colors::textPrimary());
    addAndMakeVisible (headerTitle);

    headerSubtitle.setFont (font (TextSize::caption));
    headerSubtitle.setColour (juce::Label::textColourId, colors::textMuted());
    addAndMakeVisible (headerSubtitle);

    // --- Search field (Kaiteyo DsSearchField) ---
    searchField = std::make_unique<otoha::ds::SearchField> ("Search recordings...");
    searchField->onTextChange = [this] { refreshItems(); };
    addAndMakeVisible (*searchField);

    // --- Sort ---
    sortCombo = std::make_unique<otoha::ds::ComboBox> ("Sort recordings");
    sortCombo->addItem ("Newest", 1);
    sortCombo->addItem ("Oldest", 2);
    sortCombo->addItem ("Name A\u2013Z", 3);
    sortCombo->addItem ("Name Z\u2013A", 4);
    sortCombo->addItem ("Longest", 5);
    sortCombo->addItem ("Shortest", 6);
    sortCombo->setSelectedItemIndex (0, juce::dontSendNotification);
    sortCombo->onChange = [this] { refreshItems(); };
    addAndMakeVisible (*sortCombo);

    // --- Filter chips ---
    auto setupFilter = [this] (otoha::ds::Button& btn, otoha::LibraryFilter filter)
    {
        addAndMakeVisible (btn);
        btn.onClick = [this, filter]
        {
            currentFilter = filter;
            refreshItems();
        };
    };
    setupFilter (filterAllBtn,   otoha::LibraryFilter::all);
    setupFilter (filterAudioBtn, otoha::LibraryFilter::audio);
    setupFilter (filterFavBtn,   otoha::LibraryFilter::favorites);
    filterAllBtn.setEnabled (false);

    // --- Select mode toggle ---
    addAndMakeVisible (selectToggleBtn);
    selectToggleBtn.onClick = [this]
    {
        selectMode = ! selectMode;
        selectToggleBtn.setButtonText (selectMode ? "Cancel" : "Select");
        if (! selectMode)
            listBox.deselectAllRows();
        updateBulkBar();
    };

    // --- ListBox (virtualised card grid) ---
    listBox.setModel (this);
    listBox.setRowHeight (88);   // taller cards for Kaiteyo-aligned layout
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
    searchSetup.description = "Try a different search or clear your filters.";
    searchEmptyState = std::make_unique<otoha::ds::EmptyState> (searchSetup);
    addAndMakeVisible (*searchEmptyState);

    emptyRecordBtn.onClick = [this] { if (goToRecording) goToRecording(); };

    // M34: toast overlay (must be added last so it covers everything)
    addAndMakeVisible (toastHost);

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
    const int w = bounds.getWidth();
    const bool compact = otoha::ds::responsive::isCompact (w);
    const int maxW = 960;
    auto content = bounds.withSizeKeepingCentre (juce::jmin (maxW, bounds.getWidth()),
                                                 bounds.getHeight());

    // Header: title + subtitle
    {
        auto header = content.removeFromTop (36);
        headerTitle.setBounds (header.removeFromLeft (compact ? 120 : 180));
        headerSubtitle.setBounds (header);  // takes remaining width
    }
    content.removeFromTop (Spacing::xs);

    // M35: Toolbar row adapts to compact — search full-width, filters below
    if (compact)
    {
        // Compact: search full width
        auto searchRow = content.removeFromTop (36);
        searchField->setBounds (searchRow.withHeight (30));
        content.removeFromTop (Spacing::xs);
        // Second row: filters + sort + select
        auto filterRow = content.removeFromTop (30);
        const int chipW = 56;
        filterAllBtn.setBounds   (filterRow.removeFromLeft (chipW).withHeight (26));
        filterRow.removeFromLeft (4);
        filterAudioBtn.setBounds (filterRow.removeFromLeft (chipW).withHeight (26));
        filterRow.removeFromLeft (4);
        filterFavBtn.setBounds   (filterRow.removeFromLeft (chipW).withHeight (26));
        filterRow.removeFromLeft (Spacing::sm);
        selectToggleBtn.setBounds (filterRow.removeFromLeft (68).withHeight (26));
        filterRow.removeFromLeft (Spacing::sm);
        sortCombo->setBounds (filterRow.withHeight (30));
        content.removeFromTop (Spacing::xs);
    }
    else
    {
        // Expanded: single toolbar row
        auto toolbar = content.removeFromTop (36);
        const int searchW = juce::jmin (300, toolbar.getWidth() / 3);
        searchField->setBounds (toolbar.removeFromLeft (searchW).withHeight (30));
        toolbar.removeFromLeft (Spacing::sm);
        sortCombo->setBounds (toolbar.removeFromRight (120).withHeight (30));
        toolbar.removeFromRight (Spacing::sm);
        selectToggleBtn.setBounds (toolbar.removeFromRight (68).withHeight (26));
        toolbar.removeFromRight (Spacing::sm);
        auto chips = toolbar;
        const int chipW = 64;
        filterFavBtn.setBounds   (chips.removeFromRight (chipW).withHeight (26));
        chips.removeFromRight (4);
        filterAudioBtn.setBounds (chips.removeFromRight (chipW).withHeight (26));
        chips.removeFromRight (4);
        filterAllBtn.setBounds   (chips.removeFromRight (chipW).withHeight (26));
    }
    content.removeFromTop (Spacing::xs);

    // Bulk actions bar (shown when items selected in select mode)
    {
        auto bulk = content.removeFromTop (selectMode ? 32 : 0);
        selectionLabel.setBounds (bulk.removeFromLeft (200));
        bulkDeleteBtn.setBounds (bulk.removeFromRight (72).withHeight (24));
        bulk.removeFromRight (4);
        bulkExportBtn.setBounds (bulk.removeFromRight (72).withHeight (24));
    }

    // Main content: list + details (details hidden on compact for more list space)
    {
        auto main = content;
        if (! compact)
        {
            details->setBounds (main.removeFromRight (240));
            main.removeFromRight (Spacing::sm);
        }
        else
        {
            details->setVisible (false);
        }
        listBox.setBounds (main);
    }

    // Empty states overlay on the list area
    auto listArea = listBox.getBounds().withSizeKeepingCentre (320, 140);
    emptyState->setBounds (listArea);
    searchEmptyState->setBounds (listArea);

    toastHost.setBounds (getLocalBounds());
}

void LibraryView::grabDefaultFocus()
{
    if (searchField != nullptr)
        searchField->focusEditor();
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
    if (key.isKeyCode (juce::KeyPress::escapeKey) && selectMode)
    {
        selectMode = false;
        selectToggleBtn.setButtonText ("Select");
        listBox.deselectAllRows();
        updateBulkBar();
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

    juce::Array<otoha::ds::MenuItem> menuItems;
    menuItems.add ({ "Play",        {}, {}, false, false, false, true, [this, path = item->file] { if (player.loadFile (path)) player.play(); } });
    menuItems.add ({ "Open in Editor", {}, {}, false, false, false, true, [this, id = item->id] { openInEditor (library.get (id)); } });
    menuItems.add ({ {}, {}, {}, false, false, false, true, {} });  // separator
    menuItems.add ({ "Rename...",   {}, {}, false, false, false, true, [this, id = item->id] { renameDialogForId (id); } });
    menuItems.add ({ "Duplicate",   {}, {}, false, false, false, true, [this, id = item->id] { duplicateForRow (id); } });
    menuItems.add ({ item->favorite ? "Unfavorite" : "Favorite", {}, {}, false, false, false, true,
                     [this, id = item->id] { library.setFavorite (id, ! library.get (id).favorite); refreshItems(); } });
    menuItems.add ({ "Export...",   {}, {}, false, false, false, true, [this, id = item->id] {
        listBox.deselectAllRows();
        for (int i = 0; i < (int) items.size(); ++i)
            if (items[(size_t) i].id == id) listBox.selectRow (i, true);
        updateBulkBar(); exportSelected(); } });
    menuItems.add ({ "Show in Folder", {}, {}, false, false, false, true, [path = item->file] { path.revealToUser(); } });
    menuItems.add ({ {}, {}, {}, false, false, false, true, {} });  // separator
    menuItems.add ({ "Delete",      {}, {}, false, false, true,  true, [this, id = item->id] {
        listBox.deselectAllRows();
        for (int i = 0; i < (int) items.size(); ++i)
            if (items[(size_t) i].id == id) listBox.selectRow (i, true);
        deleteSelected(); } });

    // Position at current mouse cursor
    const auto mouseScreen = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition().toInt();
    const auto localPos = getTopLevelComponent()->getScreenPosition();
    otoha::ds::showMenuPopup (this, menuItems, mouseScreen - localPos);
}

void LibraryView::duplicateForRow (juce::int64 id)
{
    const auto newId = library.duplicateMedia (id);
    if (newId == 0)
    {
        toastHost.show (otoha::ds::ToastHost::Kind::error,
                         "Couldn't duplicate recording. Check free disk space and try again.");
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
        toastHost.show (otoha::ds::ToastHost::Kind::info,
                         "Some files weren't imported — unsupported audio format.");
}

void LibraryView::renameDialogForId (juce::int64 id)
{
    const auto item = library.get (id);
    if (item.id == 0) return;

    auto* dlg = new otoha::ds::PromptDialog ("Rename recording",
                                               "New name:",
                                               item.displayName);
    dlg->onSave = [this, id] (const juce::String& newName)
    {
        library.rename (id, newName);
        refreshItems();
    };
    dlg->onDismiss = [dlg] { dlg->removeFromDesktop(); };
    addAndMakeVisible (dlg);
    dlg->setBounds (getLocalBounds());
    dlg->toFront (true);
    dlg->grabKeyboardFocus();
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
            toastHost.show (otoha::ds::ToastHost::Kind::warning,
                             "\"" + s.displayName + "\" is open in the editor. Close it first.");
            return;
        }
    }

    const bool multiple = selected.size() > 1;
    const auto title = multiple ? "Delete recordings" : "Delete recording";
    const auto message = multiple
        ? "Move " + juce::String ((int) selected.size()) + " recordings to the trash?\nThis cannot be undone."
        : "Move \"" + selected[0].displayName + "\" to the trash?\nThis cannot be undone.";
    otoha::ds::showConfirmDialog (this, title, message, "Delete",
        [this, ids = selected]
        {
            player.unload();
            for (const auto& item : ids)
                library.deleteMedia (item.id);
            refreshItems();
            toastHost.show (otoha::ds::ToastHost::Kind::success, "Recording deleted.");
        }, true);
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

    items = library.query (searchField != nullptr ? searchField->getText() : juce::String(),
                           currentFilter,
                           sortsByItem[juce::jlimit (0, 5, sortCombo != nullptr ? sortCombo->getSelectedItemIndex() : 0)]);

    const bool hasItems = ! items.empty();
    const bool searching = searchField != nullptr && searchField->getText().isNotEmpty();
    const bool nothingAtAll = ! hasItems && ! searching;

    emptyState->setVisible (nothingAtAll);
    searchEmptyState->setVisible (hasItems == false && searching);

    // Update subtitle with recording count
    headerSubtitle.setText (hasItems ? juce::String ((int) items.size()) + " recordings"
                                     : "Your recordings",
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
    const bool bulk = n > 0 && selectMode;
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
