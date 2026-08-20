#include "erdwindow.h"
#include "icon.h"
#include "iconmanager.h"
#include "ui_erdwindow.h"
#include "scene/erdscene.h"
#include "services/dbmanager.h"
#include "mdiwindow.h"
#include "style.h"
#include "erdeditorplugin.h"
#include "services/config.h"
#include "scene/erdentity.h"
#include "panel/erdtablewindow.h"
#include "changes/erdchange.h"
#include "changes/erdchangeregistry.h"
#include "changes/erdchangemodifyentity.h"
#include "db/sqlquery.h"
#include "changes/erdchangecomposite.h"
#include "changes/erdchangedeleteconnection.h"
#include "changes/erdchangedeleteentity.h"
#include "changes/erdchangenewentity.h"
#include "changes/erdchangeregistrydialog.h"
#include "scene/erdconnection.h"
#include "panel/erdconnectionpanel.h"
#include "common/colorpickerpopup.h"
#include "common/extlineedit.h"
#include "changes/erdeffectivechangemerger.h"
#include "common/widgetcover.h"
#include "services/notifymanager.h"
#include "dbtree/dbtree.h"
#include "statusfield.h"
#include <QDebug>
#include <QMdiSubWindow>
#include <QActionGroup>
#include <QShortcut>
#include <QGraphicsOpacityEffect>
#include <QToolButton>
#include <QColorDialog>
#include <QMenu>
#include <QWidgetAction>
#include <QMessageBox>

Icon* ErdWindow::cursorAddTableIcon = nullptr;
Icon* ErdWindow::cursorFkIcon = nullptr;
Icon* ErdWindow::windowIcon = nullptr;
Icon* ErdWindow::fdpIcon = nullptr;
Icon* ErdWindow::neatoIcon = nullptr;
Icon* ErdWindow::lineCurvyIcon = nullptr;
Icon* ErdWindow::lineStraightIcon = nullptr;
Icon* ErdWindow::lineSquareIcon = nullptr;
Icon* ErdWindow::colorPickerIcon = nullptr;

ErdWindow::ErdWindow() :
    ui(new Ui::ErdWindow)
{
    init();
}

ErdWindow::ErdWindow(QWidget *parent, Db* db) :
    MdiChild(parent),
    ui(new Ui::ErdWindow), db(db)
{
    init();
}

ErdWindow::ErdWindow(const ErdWindow& other) :
    MdiChild(other.parentWidget()),
    ui(new Ui::ErdWindow), db(other.db)
{
    init();
}

ErdWindow::~ErdWindow()
{
    disconnect(scene, &QGraphicsScene::selectionChanged, this, &ErdWindow::itemSelectionChanged);
    disconnect(scene, &QGraphicsScene::selectionChanged, this, &ErdWindow::updateSelectionBasedActionsState);
    delete ui;
}

void ErdWindow::staticInit()
{
    qRegisterMetaType<ErdWindow>("ErdWindow");

    cursorAddTableIcon = new Icon("ERD_CURSOR_TABLE_ADD", "cursor_table_add");
    cursorFkIcon = new Icon("ERD_CURSOR_FK", "cursor_fk");
    windowIcon = new Icon("ERD_EDITOR", "erdeditor");
    fdpIcon = new Icon("ERDLAYOUT_FDP", "erdlayout_fdp");
    neatoIcon = new Icon("ERDLAYOUT_NEATO", "erdlayout_neato");
    lineStraightIcon = new Icon("ERDEDITOR_LINE_STRAIGHT", "erdeditor_line_straight");
    lineCurvyIcon = new Icon("ERDEDITOR_LINE_CURVY", "erdeditor_line_curvy");
    lineSquareIcon = new Icon("ERDEDITOR_LINE_SQUARE", "erdeditor_line_square");
    colorPickerIcon = new Icon("ERDEDITOR_COLOR_MAP", "config_colors");
}

void ErdWindow::staticCleanup()
{
    delete lineStraightIcon;
    delete lineCurvyIcon;
    delete lineSquareIcon;
    delete windowIcon;
    delete fdpIcon;
    delete neatoIcon;
    delete cursorAddTableIcon;
    delete cursorFkIcon;
    delete colorPickerIcon;
}

void ErdWindow::init()
{
    ui->setupUi(this);
    ui->view->setErdWindow(this);

    for (auto icon : {cursorAddTableIcon, cursorFkIcon, windowIcon, fdpIcon, neatoIcon,
                    lineStraightIcon, lineCurvyIcon, lineSquareIcon, colorPickerIcon})
        icon->load();

    ui->splitter->setSizes({400, 200});
    ui->splitter->setStretchFactor(0, 2);
    ui->splitter->setStretchFactor(1, 1);

    noSideWidgetContents = ui->noContentWidget;
    noSideWidgetEffect = new QGraphicsOpacityEffect(noSideWidgetContents);
    noSideWidgetEffect->setOpacity(0.5);
    noSideWidgetContents->setGraphicsEffect(noSideWidgetEffect);

    ErdArrowItem::Type arrowType = (ErdArrowItem::Type)CFG_ERD.Erd.ArrowType.get();

    scene = new ErdScene(arrowType, this);
    connect(scene, &ErdScene::changeCreated, this, &ErdWindow::handleCreatedChange, Qt::QueuedConnection);
    connect(ui->view, &ErdView::changeCreated, this, &ErdWindow::handleCreatedChange, Qt::QueuedConnection);
    connect(scene, &ErdScene::sidePanelAbortRequested, this, &ErdWindow::abortSidePanel);
    connect(scene, &ErdScene::sidePanelRefreshRequested, this, &ErdWindow::refreshSidePanel);
    connect(scene, &ErdScene::entityNameEditedInline, this, &ErdWindow::handleEntityNameEditedInline);
    connect(scene, &ErdScene::entityFieldEditedInline, this, &ErdWindow::handleEntityFieldEditedInline);
    connect(scene, &ErdScene::entityFieldDeletedInline, this, &ErdWindow::handleEntityFieldDeletedInline);
    ui->view->setScene(scene);

    initFilter();
    initActions();
    initContextMenu();
    initExecutor();
    initWidgetCover();

    connect(STYLE, &Style::paletteChanged, this, &ErdWindow::uiPaletteChanged);
    connect(scene, &QGraphicsScene::selectionChanged, this, &ErdWindow::itemSelectionChanged);
    connect(scene, &QGraphicsScene::selectionChanged, this, &ErdWindow::updateSelectionBasedActionsState);
    connect(ui->view, &ErdView::draftConnectionRemoved, this, &ErdWindow::handleDraftConnectionRemoved, Qt::QueuedConnection);
    connect(ui->view, &ErdView::tableInsertionAborted, this, &ErdWindow::handleTableInsertionAborted, Qt::QueuedConnection);
    connect(ui->view, &ErdView::newEntityPositionPicked, this, &ErdWindow::createNewEntityAt);
    connect(ui->view, &QWidget::customContextMenuRequested, this, &ErdWindow::sceneContextMenuRequested);
    connect(scene, &ErdScene::requestVisibilityOf, ui->view, &ErdView::handleVisibilityRequest);
    connect(scene, &ErdScene::connectionEditAbortRequested, ui->view, &ErdView::abortDraftConnection);
    connect(scene, &ErdScene::requestToEditColumn, this, &ErdWindow::handleColumnEditRequest);

    changeRegistry = new ErdChangeRegistry(this);
    connect(changeRegistry, &ErdChangeRegistry::effectiveChangeCountUpdated, this, &ErdWindow::updateToolbarState);

    parseAndRestore(true, MemDbInit::FULL);
    updateState();
}

void ErdWindow::createActions()
{
    createAction(CANCEL_CURRENT, tr("Cancels ongoing action", "ERD editor"), this, SLOT(cancelCurrentAction()), this);
    createAction(NEW_TABLE_AT_POSITION, ICONS.TABLE_ADD, tr("Create a table", "ERD editor"), this, SLOT(newTableAtPosition()), this);

    createAction(RELOAD, ICONS.RELOAD, tr("Reload schema", "ERD editor"), this, SLOT(reloadSchema()), ui->toolBar);
    createAction(COMMIT, ICONS.COMMIT, tr("Commit all pending changes", "ERD editor"), this, SLOT(commitPendingChanges()), ui->toolBar);
    createAction(ROLLBACK, ICONS.ROLLBACK, tr("Revert diagram to initial state", "ERD editor"), this, SLOT(rollbackPendingChanges()), ui->toolBar);

    createAction(UNDO, ICONS.ACT_UNDO, tr("Undo", "ERD editor"), this, SLOT(undo()), ui->toolBar, ui->view);
    createAction(REDO, ICONS.ACT_REDO, tr("Redo", "ERD editor"), this, SLOT(redo()), ui->toolBar, ui->view);

    ui->toolBar->addSeparator();
    changeCountLabel = new QToolButton(this);
    QFont bold = changeCountLabel->font();
    bold.setBold(true);
    changeCountLabel->setFont(bold);
    changeCountLabel->setText(QString::asprintf(CHANGE_COUNT_DIGITS, 0));
    changeCountLabel->setToolTip(tr("The number of changes pending for commit. Click to see details.", "ERD editor"));
    changeCountLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    connect(changeCountLabel, &QToolButton::clicked, this, &ErdWindow::showChangeRegistry);
    ui->toolBar->addWidget(changeCountLabel);

    ui->toolBar->addAction(actionMap[UNDO]);
    ui->toolBar->addAction(actionMap[REDO]);
    ui->toolBar->addSeparator();
    createAction(NEW_TABLE, ICONS.TABLE_ADD, tr("Create a table"), this, SLOT(newTableToggled(bool)), ui->toolBar, ui->view);
    createAction(ADD_CONNECTION, ICONS.CONSTRAINT_FOREIGN_KEY_ADD, tr("Add a foreign key", "ERD editor"), this, SLOT(addConnectionToggled(bool)), ui->toolBar, ui->view);
    ui->toolBar->addSeparator();
    createAction(DELETE_SELECTED, ICONS.DELETE_SELECTED, tr("Delete selected items", "ERD editor"), ui->view, SLOT(deleteSelectedItem()), ui->toolBar, ui->view);
    ui->toolBar->addSeparator();
    ui->toolBar->addWidget(createLineStyleAction());
    ui->toolBar->addSeparator();
    ui->toolBar->addWidget(createSetTableColorAction());
    ui->toolBar->addSeparator();
    createAction(ARRANGE_FDP, *fdpIcon, tr("Auto-arrange (local forces)", "ERD editor"), scene, SLOT(arrangeEntitiesFdp()), ui->toolBar);
    createAction(ARRANGE_NEATO, *neatoIcon, tr("Auto-arrange (global balance)", "ERD editor"), scene, SLOT(arrangeEntitiesNeato()), ui->toolBar);
    ui->toolBar->addSeparator();
    actionMap[SETTINGS] = ui->toolBar->addWidget(createSettingsDropdown());
    ui->toolBar->addSeparator();
    actionMap[FILTER_VALUE] = ui->toolBar->addWidget(filterEdit);
    createAction(SELECT_ALL, ICONS.ACT_SELECT_ALL, tr("Select all"), scene, SLOT(selectAll()), this);

    connect(actionMap[LINE_STRAIGHT], &QAction::triggered, this, &ErdWindow::useStraightLine);
    connect(actionMap[LINE_CURVY], &QAction::triggered, this, &ErdWindow::useCurvyLine);
    connect(actionMap[LINE_SQUARE], &QAction::triggered, this, &ErdWindow::useSquareLine);

    updateSelectionBasedActionsState();
}

QToolButton* ErdWindow::createSetTableColorAction()
{
    actionMap[COLOR_PICK] = new QAction(*colorPickerIcon, tr("Set table color", "ERD editor"), ui->toolBar);

    colorPickerMenu = new QMenu(this);
    colorPicker = new ColorPickerPopup();

    QWidgetAction* wa = new QWidgetAction(colorPickerMenu);
    wa->setDefaultWidget(colorPicker);
    colorPickerMenu->addAction(wa);

    QToolButton* btn = new QToolButton();
    btn->setDefaultAction(actionMap[COLOR_PICK]);
    btn->setMenu(colorPickerMenu);
    btn->setPopupMode(QToolButton::InstantPopup);
    btn->setStyleSheet("QToolButton {padding-right: 12px;}");

    actionMap[COLOR_PICK]->setMenu(colorPickerMenu);

    connect(colorPicker, &ColorPickerPopup::colorPicked, this, &ErdWindow::colorPicked);
    connect(colorPicker, &ColorPickerPopup::resetRequested, this, &ErdWindow::colorResetPicked);

    return btn;
}

QToolButton* ErdWindow::createLineStyleAction()
{
    QMenu* lineTypeMenu = new QMenu(this);
    QActionGroup* lineGroup = new QActionGroup(ui->toolBar);
    lineGroup->setExclusive(true);
    actionMap[LINE_STRAIGHT] = new QAction(*lineStraightIcon, tr("Use straight line", "ERD editor"), this);
    actionMap[LINE_CURVY] = new QAction(*lineCurvyIcon, tr("Use curvy line", "ERD editor"), this);
    actionMap[LINE_SQUARE] = new QAction(*lineSquareIcon, tr("Use square line", "ERD editor"), this);
    for (QAction* a : {actionMap[LINE_STRAIGHT], actionMap[LINE_CURVY], actionMap[LINE_SQUARE]})
    {
        a->setCheckable(true);
        lineGroup->addAction(a);
        lineTypeMenu->addAction(a);
    }
    lineTypeButton = new QToolButton();
    lineTypeButton->setPopupMode(QToolButton::InstantPopup);
    lineTypeButton->setMenu(lineTypeMenu);
    lineTypeButton->setStyleSheet("QToolButton {padding-right: 12px;}");
    lineTypeButton->setToolTip(tr("Choose line type", "ERD editor"));
    connect(lineGroup, &QActionGroup::triggered, this, [this](QAction* action)
    {
        lineTypeButton->setIcon(action->icon());
    });
    updateArrowTypeButtons();

    return lineTypeButton;
}

QToolButton* ErdWindow::createSettingsDropdown()
{
    QMenu* settingsMenu = new QMenu(this);

    createAction(SETT_SPACE_PAN, tr("Pan view with Space only", "ERD editor"), this, SLOT(togglePanWithSpace()), this);
    actionMap[SETT_SPACE_PAN]->setCheckable(true);
    actionMap[SETT_SPACE_PAN]->setChecked(CFG_ERD.Erd.DragBySpace.get());
    settingsMenu->addAction(actionMap[SETT_SPACE_PAN]);

    createAction(SETT_GLOW, tr("Highlight related entities", "ERD editor"), this, SLOT(toggleRelationsGlow()), this);
    actionMap[SETT_GLOW]->setCheckable(true);
    actionMap[SETT_GLOW]->setChecked(CFG_ERD.Erd.GlowingRelations.get());
    settingsMenu->addAction(actionMap[SETT_GLOW]);

    // Toolbar button
    QToolButton* settingsButton = new QToolButton();
    settingsButton->setPopupMode(QToolButton::InstantPopup);
    settingsButton->setIcon(ICONS.SQL_EDITOR_SETTINGS);
    settingsButton->setMenu(settingsMenu);
    settingsButton->setStyleSheet("QToolButton {padding-right: 12px;}");
    settingsButton->setToolTip(tr("ERD settings", "ERD editor"));
    return settingsButton;
}

void ErdWindow::applySelectedEntityColor(const QColor& color)
{
    QList<ErdEntity*> appliedEntities = scene->applyColorToSelectedEntities(color);
    if (!appliedEntities.isEmpty())
        colorPicker->markColor(color);
}

void ErdWindow::updatePickerColorFromSelected(QList<QGraphicsItem*> selectedItems)
{
    if (selectedItems.size() == 0)
        return;

    QList<ErdEntity*> entitiesWithColors = selectedItems |
            NNMAP_CAST(ErdEntity*) |
            FILTER(item, {return item->usesCustomColor();});

    if (entitiesWithColors.size() == 0)
        return;

    if (entitiesWithColors.size() == 1)
    {
        colorPicker->markColor(entitiesWithColors[0]->getCustomColor().first);
        return;
    }

    QColor theColor = entitiesWithColors[0]->getCustomColor().first;
    QListIterator<ErdEntity*> it(entitiesWithColors);
    it.next();
    while (it.hasNext())
    {
        if (theColor != it.next()->getCustomColor().first)
            return; // at least 2 different colors = no color marking.
    }
    colorPicker->markColor(theColor);
}

void ErdWindow::initFilter()
{
    filterTimer = new QTimer(this);
    filterTimer->setSingleShot(true);
    filterTimer->setInterval(200);

    filterEdit = new ExtLineEdit();
    filterEdit->setExpandingMinWidth(100);
    filterEdit->setExpandingMaxWidth(200);
    filterEdit->setExpanding(true);
    filterEdit->setClearButtonEnabled(true);
    filterEdit->setPlaceholderText(tr("Filter items", "ERD editor"));
    filterEdit->setToolTip(tr("Items that don’t match the filter will be dimmed.", "ERD editor"));
    connect(filterEdit, SIGNAL(textChanged(QString)), filterTimer, SLOT(start()));
    connect(filterTimer, &QTimer::timeout, this, &ErdWindow::applyItemFiltering);
}

void ErdWindow::initContextMenu()
{
    sceneContextMenu = new QMenu(this);
    sceneContextMenu->addAction(actionMap[NEW_TABLE_AT_POSITION]);
    sceneContextMenu->addSeparator();
    sceneContextMenu->addAction(actionMap[DELETE_SELECTED]);
    sceneContextMenu->addSeparator();
    sceneContextMenu->addAction(actionMap[COLOR_PICK]);
    sceneContextMenu->addSeparator();
    sceneContextMenu->addAction(actionMap[UNDO]);
    sceneContextMenu->addAction(actionMap[REDO]);
    sceneContextMenu->addSeparator();
    sceneContextMenu->addAction(actionMap[SELECT_ALL]);
}

void ErdWindow::setupDefShortcuts()
{
    new QShortcut(QKeySequence::Cancel, this, SLOT(cancelCurrentAction()), SLOT(cancelCurrentAction()), Qt::WidgetWithChildrenShortcut);
    new QShortcut(QKeySequence::Find, this, SLOT(focusFilterInput()), SLOT(focusFilterInput()), Qt::WidgetWithChildrenShortcut);

    actionMap[LINE_STRAIGHT]->setShortcut(Qt::CTRL|Qt::Key_1);
    actionMap[LINE_CURVY]->setShortcut(Qt::CTRL|Qt::Key_2);
    actionMap[LINE_SQUARE]->setShortcut(Qt::CTRL|Qt::Key_3);
    actionMap[UNDO]->setShortcut(QKeySequence::Undo);
    actionMap[REDO]->setShortcut(QKeySequence::Redo);
    actionMap[NEW_TABLE]->setShortcut(Qt::Key_T);
    actionMap[ADD_CONNECTION]->setShortcut(Qt::Key_F);
    actionMap[DELETE_SELECTED]->setShortcut(QKeySequence::Delete);
    actionMap[SELECT_ALL]->setShortcut(QKeySequence::SelectAll);

    static_qstring(shortcutTpl, "%1 (%2)");
    for (QAction*& act : actionMap.values())
    {
        if (!act->shortcut().isEmpty())
            act->setToolTip(shortcutTpl.arg(act->text(), act->shortcut().toString()));
    }

    setShortcutContext({UNDO, REDO, DELETE_SELECTED}, Qt::WidgetWithChildrenShortcut);
}

void ErdWindow::checkIfActivated(Qt::WindowStates oldState, Qt::WindowStates newState)
{
    if (!oldState.testFlag(Qt::WindowActive) && newState.testFlag(Qt::WindowActive))
        ui->view->setFocus();
}

void ErdWindow::uiPaletteChanged()
{
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    scene->update();
}

void ErdWindow::useStraightLine()
{
    applyArrowType(ErdArrowItem::STRAIGHT);
}

void ErdWindow::useStraightLineHotKey()
{
    actionMap[LINE_STRAIGHT]->trigger();
}

void ErdWindow::useCurvyLine()
{
    applyArrowType(ErdArrowItem::CURVY);
}

void ErdWindow::useCurvyLineHotKey()
{
    actionMap[LINE_CURVY]->trigger();
}

void ErdWindow::useSquareLine()
{
    applyArrowType(ErdArrowItem::SQUARE);
}

void ErdWindow::useSquareLineHotKey()
{
    actionMap[LINE_SQUARE]->trigger();
}

void ErdWindow::cancelCurrentAction()
{
    if (ui->view->isDraftingConnection() || ui->view->isPlacingNewEntity())
    {
        ui->view->popOperatingMode();
    }
    else if (currentSideWidget)
    {
        abortSidePanel();
        scene->clearSelection();
    }
    else
    {
        scene->clearSelection();
    }
}

void ErdWindow::newTableToggled(bool enable)
{
    if (enable && !storeCurrentSidePanelModifications())
        return;

    if (enable)
        ui->view->insertNewEntity();
    else if (ui->view->isPlacingNewEntity())
        ui->view->popOperatingMode();
}

void ErdWindow::addConnectionToggled(bool enable)
{
    if (enable && !storeCurrentSidePanelModifications())
        return;

    ui->view->setDraftingConnectionMode(enable);
}

void ErdWindow::handleDraftConnectionRemoved()
{
    actionMap[ADD_CONNECTION]->setChecked(false);
}

void ErdWindow::handleTableInsertionAborted()
{
    actionMap[NEW_TABLE]->setChecked(false);
}

void ErdWindow::createNewEntityAt(const QPointF& pos)
{
    SqliteCreateTable* tableModel = new SqliteCreateTable();
    tableModel->table = scene->getNewEntityName(tr("table name", "ERD editor"), 2);

    SqliteCreateTable::Column* column = new SqliteCreateTable::Column(tr("column name", "ERD editor"), nullptr, {});
    column->setParent(tableModel);
    tableModel->columns << column;

    ErdEntity* entity = new ErdEntity(tableModel);
    entity->setExistingTable(false);
    scene->placeNewEntity(entity, pos);

    focusItem(entity);
    entity->editName();
}

void ErdWindow::newTableAtPosition()
{
    createNewEntityAt(ui->view->getLastClickPos());
}

void ErdWindow::handleEntityNameEditedInline(ErdEntity* entity, const QString& newName)
{
    Q_UNUSED(entity);
    if (!currentSideWidget)
        return;

    ErdTableWindow* tableWin = qobject_cast<ErdTableWindow*>(currentSideWidget);
    if (!tableWin)
        return;

    tableWin->nameEditedInline(newName);
}

void ErdWindow::handleEntityFieldEditedInline(ErdEntity* entity, int colIdx, const QString& newName)
{
    Q_UNUSED(entity);
    if (!currentSideWidget)
        return;

    ErdTableWindow* tableWin = qobject_cast<ErdTableWindow*>(currentSideWidget);
    if (!tableWin)
        return;

    tableWin->columnEditedInline(colIdx, newName);
}

void ErdWindow::handleEntityFieldDeletedInline(ErdEntity* entity, int colIdx)
{
    Q_UNUSED(entity);
    if (!currentSideWidget)
        return;

    ErdTableWindow* tableWin = qobject_cast<ErdTableWindow*>(currentSideWidget);
    if (!tableWin)
        return;

    tableWin->columnDeletedInline(colIdx);
}

void ErdWindow::updateSelectionBasedActionsState()
{
    actionMap[NEW_TABLE_AT_POSITION]->setEnabled(scene->selectedItems().isEmpty());
    actionMap[DELETE_SELECTED]->setEnabled(!scene->selectedItems().isEmpty());
    actionMap[COLOR_PICK]->setEnabled(!scene->getSelectedEntities().isEmpty());
}

void ErdWindow::failedChangeReEditRequested(ErdEntity* entity)
{
    ignoreSelectionChangeEvents = true;
    scene->clearSelection();
    entity->setSelected(true);
    ignoreSelectionChangeEvents = false;
}

void ErdWindow::applyItemFiltering()
{
    scene->applyItemFiltering(filterEdit->text());
}

void ErdWindow::focusFilterInput()
{
    filterEdit->setFocus();
}

void ErdWindow::sceneContextMenuRequested(const QPoint &pos)
{
    sceneContextMenu->popup(ui->view->mapToGlobal(pos));
}

void ErdWindow::colorResetPicked()
{
    colorPicked(QColor());
}

void ErdWindow::colorPicked(const QColor& color)
{
    applySelectedEntityColor(color);
    colorPickerMenu->close();
    sceneContextMenu->close();
    scene->invalidate();
}

void ErdWindow::interruptCommitExecution()
{
    ddlExecutor->interrupt();
}

void ErdWindow::commitExecutionSuccessful(SqlQueryPtr lastQueryResult)
{
    Q_UNUSED(lastQueryResult);

    // All good, we can get rid of pending changes now
    changeRegistry->clear();

    // Need to refresh memDb and update DDL history.
    if (!initMemDb(MemDbInit::FULL))
        qCritical() << "Failed to re-initialize memory database after committing ERD changes.";

    QStringList sqls = ddlExecutor->getQueries();
    CFG->addDdlHistory(sqls.join("\n"), db->getName(), db->getPath());

    DBTREE->refreshSchema(db);
    hideWidgetCover();
    notifyInfo(tr("All changes have been successfully applied to the database.", "ERD editor"));

    STATUSFIELD->releaseFadeOutFor(this);
}

void ErdWindow::commitExecutionSuccessfulNoChanges()
{
    changeRegistry->clear();
    notifyInfo(tr("The changes were successfully committed. No modifications to the database schema were required.", "ERD editor"));
}

void ErdWindow::commitExecutionFailure(int errorCode, const QString& errorText)
{
    hideWidgetCover();
    qWarning() << "Failed to apply ERD changes to the database. Error code:" << errorCode
               << ", details:" << errorText;
    notifyError(tr("Failed to apply changes to the database. Details: %1", "ERD editor").arg(errorText));

    STATUSFIELD->releaseFadeOutFor(this);
}

void ErdWindow::updateCommitExecutionStatus(int queryIdx)
{
    widgetCover->displayProgress(commitChangeDescriptions.size(),
                                 QString("(%v / %m) %1").arg(commitChangeDescriptions[queryIdx]));
    widgetCover->setProgress(queryIdx);
}

void ErdWindow::handleColumnEditRequest(ErdEntity* entity, const QString& columnName)
{
    if (!currentSideWidget)
    {
        qWarning() << "ERD Window: received request to edit column"
                   << columnName << "but no side panel is open.";
        return;
    }

    ErdTableWindow* tableWindow = qobject_cast<ErdTableWindow*>(currentSideWidget);
    if (!tableWindow)
    {
        qWarning() << "ERD Window: received request to edit column"
                   << columnName << "but side panel is not a table window.";
        return;
    }

    if (tableWindow->getEntity() != entity)
    {
        qWarning() << "ERD Window: received request to edit column"
                   << columnName << "but side panel is for a different entity.";
        return;
    }

    if (tableWindow->editColumn(columnName))
        tableWindow->commitErdChange();
}

void ErdWindow::togglePanWithSpace()
{
    bool enabled = actionMap[SETT_SPACE_PAN]->isChecked();
    CFG_ERD.Erd.DragBySpace.set(enabled);
}

void ErdWindow::toggleRelationsGlow()
{
    bool enabled = actionMap[SETT_GLOW]->isChecked();
    CFG_ERD.Erd.GlowingRelations.set(enabled);

    for (ErdEntity* e : scene->getSelectedEntities())
        e->updateGlowingState();
}

void ErdWindow::itemSelectionChanged()
{
    if (ignoreSelectionChangeEvents)
        return;

    colorPicker->clearColorMark();

    QList<QGraphicsItem*> selItems = scene->selectedItems();
    if (!clearSidePanel())
        return;

    if (selItems.size() == 1)
        showSidePanelPropertiesFor(selItems[0]);

    updatePickerColorFromSelected(selItems);
}

void ErdWindow::reloadSchema()
{
    if (changeRegistry->getPendingChangesCount() > 0)
    {
        QMessageBox::StandardButton res = QMessageBox::question(
                    qobject_cast<QWidget*>(parent()),
                    tr("Reload schema", "ERD editor"),
                    tr("This action will discard all your pending changes and "
                       "reload the diagram from the current database schema. "
                       "The undo/redo history will be cleared. "
                       "Do you want to proceed?")
                );

        if (res != QMessageBox::Yes)
            return;
    }

    CFG->set(ERD_CFG_GROUP, db->getPath(), saveSession());
    ui->view->setUpdatesEnabled(false);
    changeRegistry->clear();
    scene->clearScene();
    parseAndRestore(true, MemDbInit::FULL);
    ui->view->setUpdatesEnabled(true);
}

void ErdWindow::commitPendingChanges()
{
    commitChangeDescriptions.clear();
    ErdEffectiveChangeMerger merger(cachedDdls, db->getName());
    QList<ErdEffectiveChange> effectiveChanges = merger.merge(changeRegistry->getPendingChanges(false));
    QStringList queries;
    for (ErdEffectiveChange& ec : effectiveChanges)
    {
        auto ddls = merger.getDdlForChange(ec);
        QString desc = ec.getDescription();
        for (auto&& ddl : ddls)
        {
            Q_UNUSED(ddl);
            commitChangeDescriptions << desc;
        }

        queries += ddls;
    }

    if (queries.size() == 0)
    {
        // No effective changes. Clean up the registry and return.
        commitExecutionSuccessfulNoChanges();
        return;
    }

    STATUSFIELD->blockFadeOutFor(this);

    ddlExecutor->setDb(db);
    ddlExecutor->setQueries(queries);
    showWidgetCover(queries.size());
    ddlExecutor->exec();
}

void ErdWindow::rollbackPendingChanges()
{
    changeRegistry->moveToBeginning();
    ui->view->setUpdatesEnabled(false);
    scene->clearScene();
    parseAndRestore(true, MemDbInit::CACHED);
    ui->view->setUpdatesEnabled(true);
}

void ErdWindow::initWidgetCover()
{
    widgetCover = new WidgetCover(ui->content);
    widgetCover->initWithInterruptContainer();
    widgetCover->displayProgress(0, "(%v / %m)");
    connect(widgetCover, SIGNAL(cancelClicked()), this, SLOT(interruptCommitExecution()));
}

void ErdWindow::initExecutor()
{
    ddlExecutor = new ChainExecutor(this);
    ddlExecutor->setTransaction(true);
    ddlExecutor->setAsync(true);
    ddlExecutor->setDisableForeignKeys(true);
    ddlExecutor->setUseLegacyAlterRename(true);
    ddlExecutor->setDisableObjectDropsDetection(false);

    connect(ddlExecutor, SIGNAL(success(SqlQueryPtr)), this, SLOT(commitExecutionSuccessful(SqlQueryPtr)));
    connect(ddlExecutor, SIGNAL(failure(int,QString)), this, SLOT(commitExecutionFailure(int,QString)));
    connect(ddlExecutor, SIGNAL(aboutToExecuteQueryNumber(int)), this, SLOT(updateCommitExecutionStatus(int)));
}

void ErdWindow::showWidgetCover(int total)
{
    widgetCover->displayProgress(total);
    widgetCover->getContainerLayout()->setColumnMinimumWidth(0, 400);
    widgetCover->show();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    ui->sidePanel->setEnabled(false);
    ui->view->setInteractive(false);
    updateState();
}

void ErdWindow::hideWidgetCover()
{
    widgetCover->hide();
    ui->sidePanel->setEnabled(true);
    ui->view->setInteractive(true);
    updateState();
    commitChangeDescriptions.clear();
}

void ErdWindow::handleCreatedChange(ErdChange* change)
{
    changeRegistry->addChange(change);
    scene->handleChange(change);
}

void ErdWindow::updateState()
{
    bool readOnlyView = !ui->view->isInteractive();
    if (!readOnlyView)
    {
        for (QAction* act : actionMap.values())
            act->setEnabled(true);
    }

    updateToolbarState(changeRegistry->getPendingChangesCount(),
                       changeRegistry->isUndoAvailable(),
                       changeRegistry->isRedoAvailable());

    updateSelectionBasedActionsState();
    if (readOnlyView)
    {
        for (QAction* act : actionMap.values())
            act->setEnabled(false);
    }
}

void ErdWindow::updateToolbarState(int effectiveChangeCount, bool undoAvailable, bool redoAvailable)
{
    bool hasPendingChanges = effectiveChangeCount > 0;
    actionMap[COMMIT]->setEnabled(hasPendingChanges);
    actionMap[ROLLBACK]->setEnabled(hasPendingChanges);
    actionMap[UNDO]->setEnabled(undoAvailable);
    actionMap[REDO]->setEnabled(redoAvailable);
    changeCountLabel->setText(QString::asprintf(CHANGE_COUNT_DIGITS, effectiveChangeCount));
}

void ErdWindow::abortSidePanel()
{
    ErdPropertiesPanel* propertiesPanel = dynamic_cast<ErdPropertiesPanel*>(currentSideWidget);
    if (propertiesPanel)
        propertiesPanel->abortErdChange();

    clearSidePanel();
}

void ErdWindow::showChangeRegistry()
{
    ErdChangeRegistryDialog dialog(memDb, changeRegistry, cachedDdls, this);
    dialog.exec();
}

void ErdWindow::undo()
{
    ErdChange* change = changeRegistry->peekUndo();
    if (scene->undoChange(change))
        changeRegistry->undo();
}

void ErdWindow::redo()
{
    ErdChange* change = changeRegistry->peekRedo();
    if (scene->redoChange(change))
        changeRegistry->redo();
}

void ErdWindow::applyArrowType(ErdArrowItem::Type arrowType)
{
    CFG_ERD.Erd.ArrowType.set(arrowType);
    scene->setArrowType(arrowType);
}

bool ErdWindow::isUncommitted() const
{
    return changeRegistry->getPendingChangesCount() > 0;
}

QString ErdWindow::getQuitUncommittedConfirmMessage() const
{
    return tr("ERD window \"%1\" has uncommitted changes.").arg(getMdiWindow()->windowTitle());
}

void ErdWindow::setMdiWindow(MdiWindow* value)
{
    MdiChild::setMdiWindow(value);
    connect(value, &QMdiSubWindow::windowStateChanged, this, &ErdWindow::checkIfActivated);
}

bool ErdWindow::shouldReuseForArgs(int argCount, ...)
{
    if (argCount != 1)
        return false;

    va_list args;
    va_start(args, argCount);
    Db* argDb = va_arg(args, Db*);
    va_end(args);

    return argDb == db;
}

void ErdWindow::updateArrowTypeButtons()
{
    switch (scene->getArrowType())
    {
        case ErdArrowItem::STRAIGHT:
            actionMap[LINE_STRAIGHT]->setChecked(true);
            lineTypeButton->setIcon(actionMap[LINE_STRAIGHT]->icon());
            break;
        case ErdArrowItem::CURVY:
            actionMap[LINE_CURVY]->setChecked(true);
            lineTypeButton->setIcon(actionMap[LINE_CURVY]->icon());
            break;
        case ErdArrowItem::SQUARE:
            actionMap[LINE_SQUARE]->setChecked(true);
            lineTypeButton->setIcon(actionMap[LINE_SQUARE]->icon());
            break;
    }
}

QToolBar* ErdWindow::getToolBar(int toolbar) const
{
    Q_UNUSED(toolbar);
    return ui->toolBar;
}

QVariant ErdWindow::saveSession()
{
    QHash<QString,QVariant> sessionValue;
    if (db)
        sessionValue["db"] = db->getName();

    QHash<QString, QVariant> sceneConfig = scene->getConfig();
    changeRegistry->complementSceneConfig(sceneConfig);

    QHash<QString, QVariant> erdConfig;
    erdConfig.insert(sceneConfig);
    erdConfig.insert(ui->view->getConfig());
    erdConfig[CFG_KEY_SPLITTER] = ui->splitter->saveState();
    erdConfig[CFG_CUSTOM_COLORS] = QVariant::fromValue<QVector<QColor>>(colorPicker->getCustomColors());

    if (db)
    {
        CFG->set(ERD_CFG_GROUP, db->getPath(), erdConfig);
        CFG->set(ERD_CFG_GROUP, db->getName(), erdConfig);
    }

    return sessionValue;
}

bool ErdWindow::storeEntityModifications(QWidget* sidePanelWidget)
{
    ErdPropertiesPanel* propertiesPanel = dynamic_cast<ErdPropertiesPanel*>(sidePanelWidget);
    if (propertiesPanel)
    {
        if (!propertiesPanel->commitErdChange())
            return false;
    }
    return true;
}

QString ErdWindow::getCurrentSidePanelModificationsEntity() const
{
    if (!currentSideWidget)
        return QString();

    ErdTableWindow* tableWin = qobject_cast<ErdTableWindow*>(currentSideWidget);
    if (tableWin)
        return tableWin->getTable();

    ErdConnectionPanel* connectionPanel = qobject_cast<ErdConnectionPanel*>(currentSideWidget);
    if (connectionPanel)
        return connectionPanel->getStartEntityTable();

    return QString();
}

void ErdWindow::parseAndRestore(bool applyUiConfigPart, MemDbInit createMemDb)
{
    if (!db)
        return;

    if (!initMemDb(createMemDb))
        return;

    QSet<QString> tableNames = scene->parseSchema();
    QVariant erdConfig = CFG->get(ERD_CFG_GROUP, db->getPath());
    if (!tryToApplyConfig(erdConfig, tableNames, applyUiConfigPart))
    {
        erdConfig = CFG->get(ERD_CFG_GROUP, db->getName());
        if (!tryToApplyConfig(erdConfig, tableNames, applyUiConfigPart))
            scene->arrangeEntitiesFdp(true);
    }
}

bool ErdWindow::clearSidePanel()
{
    if (!currentSideWidget)
        return true;

    if (!storeEntityModifications(currentSideWidget))
        return false;

    ui->sidePanel->layout()->removeWidget(currentSideWidget);
    currentSideWidget->deleteLater();
    currentSideWidget = nullptr;
    noSideWidgetContents->setVisible(true);
    ui->sidePanel->layout()->addWidget(noSideWidgetContents);
    return true;
}

bool ErdWindow::setSidePanelWidget(QWidget* widget)
{
    if (currentSideWidget && !storeEntityModifications(currentSideWidget))
        return false;

    if (currentSideWidget) // if still in place after storing (or not) entity updates & refreshing schema
    {
        ui->sidePanel->layout()->removeWidget(currentSideWidget);
        currentSideWidget->deleteLater();
    }
    else
    {
        ui->sidePanel->layout()->removeWidget(noSideWidgetContents);
        noSideWidgetContents->setVisible(false);
    }

    currentSideWidget = widget;
    ui->sidePanel->layout()->addWidget(widget);
    return true;
}

bool ErdWindow::showSidePanelPropertiesFor(QGraphicsItem* item)
{
    ErdEntity* entity = dynamic_cast<ErdEntity*>(item);
    ErdArrowItem* arrow = dynamic_cast<ErdArrowItem*>(item);
    if (entity)
    {
        ErdTableWindow* tableMdiChild = new ErdTableWindow(memDb, entity);
        tableMdiChild->init();
        connect(tableMdiChild, &ErdTableWindow::changeCreated, this, &ErdWindow::handleCreatedChange, Qt::QueuedConnection);
        connect(tableMdiChild, &ErdTableWindow::editedEntityShouldBeDeleted, scene, &ErdScene::removeEntityFromScene, Qt::QueuedConnection);
        connect(tableMdiChild, &ErdTableWindow::requestReEditForEntity, this, &ErdWindow::failedChangeReEditRequested, Qt::QueuedConnection);
        return setSidePanelWidget(tableMdiChild);
    }
    else if (arrow)
    {
        ErdConnection* connection = scene->getConnectionForArrow(arrow);
        if (!connection)
        {
            qCritical() << "No ErdConnection for ErdArrowItem!";
            return false;
        }

        if (!connection->isFinalized())
        {
            qDebug() << "Skipping selection action on connection, as it's not finalized connection.";
            return false;
        }

        if (connection->isCompoundConnection())
        {
            ignoreSelectionChangeEvents = true;
            for (ErdConnection*& assocConn : connection->getAssociatedConnections())
                assocConn->select(false);

            ignoreSelectionChangeEvents = false;
        }

        ErdConnectionPanel* panel = new ErdConnectionPanel(memDb, connection);
        connect(panel, &ErdConnectionPanel::changeCreated, this, &ErdWindow::handleCreatedChange, Qt::QueuedConnection);
        return setSidePanelWidget(panel);
    }
    return true;
}

bool ErdWindow::initMemDb(MemDbInit createMemDb)
{
    if (createMemDb == MemDbInit::NONE)
        return true;

    if (createMemDb == MemDbInit::FULL || cachedDdls.isEmpty())
        cachedDdls = ErdEffectiveChangeMerger::readDbSchema(db);

    // Clear side panel before switching memDb, as it may be connected to the old memDb.
    // New mem db happens when schema is reloaded, or changes were committed (thus schema is reloaded too).
    abortSidePanel();
    scene->clearSelection();

    Db* oldMemDb = memDb; // don't delete it yet, as it will be disconnected from in setDb() methods, etc.
    memDb = ErdEffectiveChangeMerger::createMemDbWithSchema(cachedDdls, db->getName());
    if (!memDb)
    {
        memDb = oldMemDb;
        return false;
    }

    scene->setDb(memDb);
    if (oldMemDb)
    {
        oldMemDb->closeQuiet();
        delete oldMemDb;
    }

    return true;
}

void ErdWindow::refreshSidePanel()
{
    if (!currentSideWidget)
        return;

    ErdTableWindow* tableWindow = qobject_cast<ErdTableWindow*>(currentSideWidget);
    if (tableWindow)
    {
        tableWindow->refreshStructure();
        return;
    }

    // Connection Panel will be destroyed upon entity refresh, because its connection item will be recreated.
    // No need to refresh that panel.
}

bool ErdWindow::storeCurrentSidePanelModifications()
{
    return clearSidePanel();
}

bool ErdWindow::restoreSession(const QVariant& sessionValue)
{
    QHash<QString, QVariant> value = sessionValue.toHash();
    if (value.size() == 0)
        return false;

    if (!value.contains("db"))
        return false;

    QString dbName = value["db"].toString();
    Db* sessionDb = DBLIST->getByName(dbName);
    if (!sessionDb || !sessionDb->isOpen())
        return false;

    db = sessionDb;
    parseAndRestore(true, MemDbInit::FULL);

    return true;
}

bool ErdWindow::tryToApplyConfig(const QVariant& value, const QSet<QString>& tableNames, bool applyUiPart)
{
    if (!value.isValid() || value.isNull() || !value.canConvert<QHash<QString, QVariant>>())
        return false;

    QHash<QString, QVariant> erdConfig = value.toHash();

    // Is it applicable config for this db?
    StrHash<QVariant> cfgEntities = erdConfig[ErdScene::CFG_KEY_ENTITIES].toHash();
    int matched = 0;
    for (const QString& configTable : cfgEntities.lowerKeys())
    {
        if (tableNames.contains(configTable))
            matched++;
    }

    if (matched < 2 || ((qreal)matched / erdConfig.size()) < 0.25)
        return false;

    // Config is applicable.
    scene->applyConfig(erdConfig);
    if (applyUiPart)
    {
        // UI elements restored only at initial run, because subsequent runs
        // come from rollback/reload, so UI should remain untouched.
        ui->splitter->restoreState(erdConfig[CFG_KEY_SPLITTER].toByteArray());
        ui->view->applyConfig(erdConfig);
        colorPicker->setCustomColors(erdConfig[CFG_CUSTOM_COLORS].value<QVector<QColor>>());
        updateArrowTypeButtons();
    }

    return true;
}

void ErdWindow::focusItem(QGraphicsItem* item)
{
    scene->clearSelection();
    scene->setFocusItem(item, Qt::MouseFocusReason);
    item->setSelected(true);
}

Icon* ErdWindow::getIconNameForMdiWindow()
{
    return windowIcon;
}

QString ErdWindow::getTitleForMdiWindow()
{
    if (db)
        return tr("ERD editor (%1)").arg(db->getName());

    return tr("ERD editor");
}
