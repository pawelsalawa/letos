#include "tablewindow.h"
#include "common/iconpositionitemdelegate.h"
#include "ui_tablewindow.h"
#include "services/dbmanager.h"
#include "services/notifymanager.h"
#include "schemaresolver.h"
#include "iconmanager.h"
#include "datagrid/sqltablemodel.h"
#include "mainwindow.h"
#include "tablestructuremodel.h"
#include "tableconstraintsmodel.h"
#include "dialogs/columndialog.h"
#include "dialogs/constraintdialog.h"
#include "mdiarea.h"
#include "dialogs/newconstraintdialog.h"
#include "db/chainexecutor.h"
#include "common/widgetcover.h"
#include "mdiwindow.h"
#include "dbtree/dbtree.h"
#include "constrainttabmodel.h"
#include "parser/ast/sqlitecreateindex.h"
#include "parser/ast/sqlitecreatetrigger.h"
#include "dialogs/messagelistdialog.h"
#include "services/codeformatter.h"
#include "uiconfig.h"
#include "dialogs/ddlpreviewdialog.h"
#include "services/config.h"
#include "services/importmanager.h"
#include "dbobjectdialogs.h"
#include "dialogs/exportdialog.h"
#include "common/centerediconitemdelegate.h"
#include "themetuner.h"
#include "dialogs/importdialog.h"
#include "dialogs/populatedialog.h"
#include "common/dbcombobox.h"
#include "tablemodifier.h"
#include "datagrid/sqlqueryview.h"
#include "statusfield.h"
#include <QMenu>
#include <QToolButton>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QDebug>
#include <QStyleFactory>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QtSystemDetection>
#else
#include <qsystemdetection.h>
#endif

// TODO extend QTableView for columns and constraints, so they show full-row-width drop indicator,
// instead of single column drop indicator.

CFG_KEYS_DEFINE(TableWindow)

TableWindow::TableWindow(QWidget* parent) :
    MdiChild(parent),
    ui(new Ui::TableWindow)
{
    init();
}

TableWindow::TableWindow(Db* db, QWidget* parent) :
    MdiChild(parent),
    db(db),
    ui(new Ui::TableWindow)
{
    newTable();
    init();
    initDbAndTable();
}

TableWindow::TableWindow(const TableWindow& win) :
    MdiChild(win.parentWidget()),
    db(win.db),
    database(win.database),
    table(win.table),
    ui(new Ui::TableWindow)
{
    init();
    initDbAndTable();
}

TableWindow::TableWindow(QWidget *parent, Db* db, const QString& database, const QString& table) :
    MdiChild(parent),
    db(db),
    database(database),
    table(table),
    ui(new Ui::TableWindow)
{
    init();
    initDbAndTable();
}

TableWindow::TableWindow(QWidget* parent, Db* db, const QString& database, const QString& table, bool existingTable) :
    MdiChild(parent),
    db(db),
    database(database),
    table(table),
    ui(new Ui::TableWindow),
    existingTable(existingTable)
{
    init();
}

TableWindow::~TableWindow()
{
    delete ui;

    if (tableModifier)
    {
        delete tableModifier;
        tableModifier = nullptr;
    }
}

void TableWindow::staticInit()
{
    qRegisterMetaType<TableWindow>("TableWindow");
}

void TableWindow::insertAction(ExtActionPrototype* action, TableWindow::ToolBar toolbar)
{
    return ExtActionContainer::insertAction<TableWindow>(action, toolbar);
}

void TableWindow::insertActionBefore(ExtActionPrototype* action, TableWindow::Action beforeAction, TableWindow::ToolBar toolbar)
{
    return ExtActionContainer::insertActionBefore<TableWindow>(action, beforeAction, toolbar);
}

void TableWindow::insertActionAfter(ExtActionPrototype* action, TableWindow::Action afterAction, TableWindow::ToolBar toolbar)
{
    return ExtActionContainer::insertActionAfter<TableWindow>(action, afterAction, toolbar);
}

void TableWindow::removeAction(ExtActionPrototype* action, TableWindow::ToolBar toolbar)
{
    ExtActionContainer::removeAction<TableWindow>(action, toolbar);
}

void TableWindow::newTable()
{
    existingTable = false;
    table = "";
}

void TableWindow::init()
{
    ui->setupUi(this);
    ui->structureSplitter->setStretchFactor(0, 2);
    ui->structureView->horizontalHeader()->setSectionsClickable(false);
    ui->structureView->verticalHeader()->setSectionsClickable(false);
    constraintColumnsDelegate = new CenteredIconItemDelegate(this);
    iconPositionDelegate = new IconPositionItemDelegate(this);

#ifdef Q_OS_MACX
    QStyle *fusion = QStyleFactory::create("Fusion");
    ui->structureToolBar->setStyle(fusion);
    ui->structureTab->layout()->setSpacing(0);
    ui->tableConstraintsToolbar->setStyle(fusion);
    ui->constraintsWidget->layout()->setSpacing(0);
    ui->indexToolBar->setStyle(fusion);
    ui->indexesTab->layout()->setSpacing(0);
    ui->triggerToolBar->setStyle(fusion);
    ui->triggersTab->layout()->setSpacing(0);
#endif

    dataModel = new SqlTableModel(this);
    ui->dataView->init(dataModel);

    initActions();
    updateTabsOrder();
    createDbCombo();

    MAINWINDOW->installToolbarSizeWheelHandler(ui->structureToolBar);
    MAINWINDOW->installToolbarSizeWheelHandler(ui->tableConstraintsToolbar);
    MAINWINDOW->installToolbarSizeWheelHandler(ui->dataView->getToolBar(DataView::TOOLBAR_GRID));
    MAINWINDOW->installToolbarSizeWheelHandler(ui->dataView->getToolBar(DataView::TOOLBAR_FORM));
    MAINWINDOW->installToolbarSizeWheelHandler(ui->indexToolBar);
    MAINWINDOW->installToolbarSizeWheelHandler(ui->triggerToolBar);

    connect(dataModel, SIGNAL(executionSuccessful()), this, SLOT(executionSuccessful()));
    connect(dataModel, SIGNAL(executionFailed(QString)), this, SLOT(executionFailed(QString)));
    connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));
    connect(this, SIGNAL(modifyStatusChanged()), this, SLOT(updateStructureCommitState()));
    connect(ui->tableNameEdit, SIGNAL(textChanged(QString)), this, SIGNAL(modifyStatusChanged()));
    connect(ui->tableNameEdit, SIGNAL(textChanged(QString)), this, SLOT(nameChanged()));
    connect(ui->indexList, SIGNAL(itemSelectionChanged()), this, SLOT(updateIndexesState()));
    connect(ui->triggerList, SIGNAL(itemSelectionChanged()), this, SLOT(updateTriggersState()));
    connect(CFG_UI.General.DataTabAsFirstInTables, SIGNAL(changed(const QVariant&)), this, SLOT(updateTabsOrder()));
    connect(ui->structureView, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(structureViewDoubleClicked(const QModelIndex&)));
    connect(ui->tableConstraintsView, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(constraintsViewDoubleClicked(const QModelIndex&)));
    connect(CFG_UI.Fonts.DataView, SIGNAL(changed(QVariant)), this, SLOT(updateFont()));

    structureExecutor = new ChainExecutor(this);
    connect(structureExecutor, SIGNAL(success(SqlQueryPtr)), this, SLOT(changesSuccessfullyCommitted()));
    connect(structureExecutor, SIGNAL(failure(int,QString)), this, SLOT(changesFailedToCommit(int,QString)));

    connect(NOTIFY_MANAGER, SIGNAL(objectRenamed(Db*,QString,QString,QString)), this, SLOT(handlePossibleIdxOrTrgRename(Db*,QString,QString,QString)));
    connect(NOTIFY_MANAGER, SIGNAL(columnRenamed(Db*,QString,QString,QString,QString)), this, SLOT(handlePossibleColumnRename(Db*,QString,QString,QString,QString)));

    THEME_TUNER->manageCompactLayout({
                                         ui->structureTab,
                                         ui->constraintsWidget,
                                         ui->dataTab,
                                         ui->constraintsTab,
                                         ui->indexesTab,
                                         ui->triggersTab,
                                         ui->ddlTab
                                     });

    updateFont();
    setupCoverWidget();
    updateAfterInit();
}

void TableWindow::createActions()
{
    createStructureActions();
    createDataGridActions();
    createDataFormActions();
    createIndexActions();
    createTriggerActions();

    createAction(NEXT_TAB, "next tab", this, SLOT(nextTab()), this);
    createAction(PREV_TAB, "prev tab", this, SLOT(prevTab()), this);
}

void TableWindow::createStructureActions()
{
    createAction(REFRESH_STRUCTURE, ICONS.RELOAD, tr("Refresh structure", "table window"), this, SLOT(refreshStructure()), ui->structureToolBar, ui->structureView);
    separatorAfterAction[REFRESH_STRUCTURE] = ui->structureToolBar->addSeparator();
    createAction(COMMIT_STRUCTURE, ICONS.COMMIT, tr("Commit structure changes", "table window"), this, SLOT(commitStructure()), ui->structureToolBar, ui->structureView);
    createAction(ROLLBACK_STRUCTURE, ICONS.ROLLBACK, tr("Rollback structure changes", "table window"), this, SLOT(rollbackStructure()), ui->structureToolBar, ui->structureView);
    createAction(ADD_COLUMN, ICONS.TABLE_COLUMN_ADD, tr("Add column", "table window"), this, SLOT(addColumn()), ui->structureToolBar, ui->structureView);
    createAction(EDIT_COLUMN, ICONS.TABLE_COLUMN_EDIT, tr("Edit column", "table window"), this, SLOT(editColumn()), ui->structureToolBar, ui->structureView);
    createAction(DEL_COLUMN, ICONS.TABLE_COLUMN_DELETE, tr("Delete column", "table window"), this, SLOT(delColumn()), ui->structureToolBar, ui->structureView);
    createAction(MOVE_COLUMN_UP, ICONS.MOVE_UP, tr("Move column up", "table window"), this, SLOT(moveColumnUp()), ui->structureToolBar, ui->structureView);
    createAction(MOVE_COLUMN_DOWN, ICONS.MOVE_DOWN, tr("Move column down", "table window"), this, SLOT(moveColumnDown()), ui->structureToolBar, ui->structureView);
    separatorAfterAction[MOVE_COLUMN_DOWN] = ui->structureToolBar->addSeparator();
    createAction(IMPORT, ICONS.TABLE_IMPORT, tr("Import data to the table", "table window"), this, SLOT(importTable()), ui->structureToolBar, ui->structureView);
    createAction(EXPORT, ICONS.TABLE_EXPORT, tr("Export table", "table window"), this, SLOT(exportTable()), ui->structureToolBar, ui->structureView);
    createAction(POPULATE, ICONS.TABLE_POPULATE, tr("Populate table", "table window"), this, SLOT(populateTable()), ui->structureToolBar, ui->structureView);
    separatorAfterAction[POPULATE] = ui->structureToolBar->addSeparator();
    createAction(CREATE_SIMILAR, ICONS.TABLE_CREATE_SIMILAR, tr("Create similar table", "table window"), this, SLOT(createSimilarTable()), ui->structureToolBar);
    createAction(RESET_AUTOINCREMENT, ICONS.RESET_AUTOINCREMENT, tr("Reset autoincrement value", "table window"), this, SLOT(resetAutoincrement()), ui->structureToolBar);

    // Table constraints
    createAction(ADD_TABLE_CONSTRAINT, ICONS.TABLE_CONSTRAINT_ADD, tr("Add table constraint", "table window"), this, SLOT(addConstraint()), ui->tableConstraintsToolbar, ui->tableConstraintsView);
    createAction(EDIT_TABLE_CONSTRAINT, ICONS.TABLE_CONSTRAINT_EDIT, tr("Edit table constraint", "table window"), this, SLOT(editConstraint()), ui->tableConstraintsToolbar, ui->tableConstraintsView);
    createAction(DEL_TABLE_CONSTRAINT, ICONS.TABLE_CONSTRAINT_DELETE, tr("Delete table constraint", "table window"), this, SLOT(delConstraint()), ui->tableConstraintsToolbar, ui->tableConstraintsView);
    createAction(MOVE_CONSTRAINT_UP, ICONS.MOVE_UP, tr("Move table constraint up", "table window"), this, SLOT(moveConstraintUp()), ui->tableConstraintsToolbar, ui->tableConstraintsView);
    createAction(MOVE_CONSTRAINT_DOWN, ICONS.MOVE_DOWN, tr("Move table constraint down", "table window"), this, SLOT(moveConstraintDown()), ui->tableConstraintsToolbar, ui->tableConstraintsView);
    separatorAfterAction[MOVE_CONSTRAINT_DOWN] = ui->tableConstraintsToolbar->addSeparator();
    createAction(ADD_TABLE_PK, ICONS.CONSTRAINT_PRIMARY_KEY_ADD, tr("Add table primary key", "table window"), this, SLOT(addPk()), ui->tableConstraintsToolbar, ui->tableConstraintsView);
    createAction(ADD_TABLE_FK, ICONS.CONSTRAINT_FOREIGN_KEY_ADD, tr("Add table foreign key", "table window"), this, SLOT(addFk()), ui->tableConstraintsToolbar, ui->tableConstraintsView);
    createAction(ADD_TABLE_UNIQUE, ICONS.CONSTRAINT_UNIQUE_ADD, tr("Add table unique constraint", "table window"), this, SLOT(addUnique()), ui->tableConstraintsToolbar, ui->tableConstraintsView);
    createAction(ADD_TABLE_CHECK, ICONS.CONSTRAINT_CHECK_ADD, tr("Add table check constraint", "table window"), this, SLOT(addCheck()), ui->tableConstraintsToolbar, ui->tableConstraintsView);
}

void TableWindow::createDataGridActions()
{
    QAction* before = ui->dataView->getAction(DataView::FILTER_VALUE);
    ui->dataView->getToolBar(DataView::TOOLBAR_GRID)->insertAction(before, actionMap[IMPORT]);
    ui->dataView->getToolBar(DataView::TOOLBAR_GRID)->insertAction(before, actionMap[EXPORT]);
    ui->dataView->getToolBar(DataView::TOOLBAR_GRID)->insertAction(before, actionMap[POPULATE]);
    ui->dataView->getToolBar(DataView::TOOLBAR_GRID)->insertSeparator(before);
}

void TableWindow::createDataFormActions()
{
}

void TableWindow::createIndexActions()
{
    createAction(REFRESH_INDEXES, ICONS.RELOAD, tr("Refresh index list", "table window"), this, SLOT(updateIndexes()), ui->indexToolBar, ui->indexList);
    separatorAfterAction[REFRESH_INDEXES] = ui->indexToolBar->addSeparator();
    createAction(ADD_INDEX, ICONS.INDEX_ADD, tr("Create index", "table window"), this, SLOT(addIndex()), ui->indexToolBar, ui->indexList);
    createAction(EDIT_INDEX, ICONS.INDEX_EDIT, tr("Edit index", "table window"), this, SLOT(editCurrentIndex()), ui->indexToolBar, ui->indexList);
    createAction(DEL_INDEX, ICONS.INDEX_DEL, tr("Delete index", "table window"), this, SLOT(delIndex()), ui->indexToolBar, ui->indexList);
    connect(ui->indexList, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(indexViewDoubleClicked(QModelIndex)));
}

void TableWindow::createTriggerActions()
{
    createAction(REFRESH_TRIGGERS, ICONS.RELOAD, tr("Refresh trigger list", "table window"), this, SLOT(updateTriggers()), ui->triggerToolBar, ui->triggerList);
    separatorAfterAction[REFRESH_TRIGGERS] = ui->triggerToolBar->addSeparator();
    createAction(ADD_TRIGGER, ICONS.TRIGGER_ADD, tr("Create trigger", "table window"), this, SLOT(addTrigger()), ui->triggerToolBar, ui->triggerList);
    createAction(EDIT_TRIGGER, ICONS.TRIGGER_EDIT, tr("Edit trigger", "table window"), this, SLOT(editTrigger()), ui->triggerToolBar, ui->triggerList);
    createAction(DEL_TRIGGER, ICONS.TRIGGER_DEL, tr("Delete trigger", "table window"), this, SLOT(delTrigger()), ui->triggerToolBar, ui->triggerList);
    connect(ui->triggerList, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(triggerViewDoubleClicked(QModelIndex)));
}

bool TableWindow::editColumn(const QModelIndex& idx)
{
    if (!idx.isValid())
    {
        addColumn();
        return true;
    }

    SqliteCreateTable::Column* column = structureModel->getColumn(idx.row());
    ColumnDialog columnDialog(db, this);
    columnDialog.setColumn(column);
    if (hasAnyPkDefined() && !column->hasConstraint(SqliteCreateTable::Column::Constraint::PRIMARY_KEY))
        columnDialog.disableConstraint(ConstraintDialog::Constraint::PK);

    if (columnDialog.exec() != QDialog::Accepted)
        return false;

    SqliteCreateTable::Column* modifiedColumn = columnDialog.getModifiedColumn();
    structureModel->replaceColumn(idx.row(), modifiedColumn);
    resizeStructureViewColumns();
    updateTableConstraintsToolbarState();
    return true;
}

void TableWindow::delColumn(const QModelIndex& idx)
{
    if (!idx.isValid())
    {
        qWarning() << "Called TableWindow::delColumn() with invalid index.";
        return;
    }

    SqliteCreateTable::Column* column = structureModel->getColumn(idx.row());

    QString msg = tr("Are you sure you want to delete column '%1'?", "table window").arg(column->name);
    int btn = QMessageBox::question(this, tr("Delete column", "table window"), msg);
    if (btn != QMessageBox::Yes)
        return;

    structureModel->delColumn(idx.row());
    resizeStructureViewColumns();
    updateTableConstraintsToolbarState();
}

void TableWindow::executeStructureChanges()
{
    QStringList sqls = generateStructureChangeStatements();
    if (sqls.isEmpty())
        return;

    if (!CFG_UI.General.DontShowDdlPreview.get())
    {
        DdlPreviewDialog dialog(db, this);
        dialog.setDdl(sqls);
        if (dialog.exec() != QDialog::Accepted)
            return;
    }

    STATUSFIELD->blockFadeOutFor(this);
    modifyingThisTable = true;
    structureExecutor->setDb(db);
    structureExecutor->setQueries(sqls);
    structureExecutor->setDisableForeignKeys(true);
    structureExecutor->setDisableObjectDropsDetection(true);
    widgetCover->show();
    structureExecutor->exec();
}

QStringList TableWindow::generateStructureChangeStatements()
{
    QStringList sqls;
    createTable->rebuildTokens();
    if (!existingTable)
    {
        sqls << createTable->detokenize();
    }
    else
    {
        delete tableModifier;

        tableModifier = new TableModifier(db, database, table);
        tableModifier->alterTable(createTable);

        if (tableModifier->hasMessages())
        {
            MessageListDialog dialog(tr("Following problems will take place while modifying the table.\n"
                                        "Would you like to proceed?", "table window"));
            dialog.setWindowTitle(tr("Table modification", "table window"));
            for (QString& error : tableModifier->getErrors())
                dialog.addError(error);

            for (QString& warn : tableModifier->getWarnings())
                dialog.addWarning(warn);

            if (dialog.exec() != QDialog::Accepted)
                return QStringList();
        }

        sqls = tableModifier->getGeneratedSqls();
    }
    return sqls;
}

void TableWindow::updateAfterInit()
{
    updateStructureCommitState();
    updateStructureToolbarState();
    updateTableConstraintsToolbarState();
    updateNewTableState();
    updateIndexesState();
    updateTriggersState();
}

QModelIndex TableWindow::structureCurrentIndex() const
{
    return ui->structureView->selectionModel()->currentIndex();
}

void TableWindow::updateStructureToolbarState()
{
    QItemSelectionModel *selModel = ui->structureView->selectionModel();
    bool validIdx = false;
    bool isFirst = false;
    bool isLast = false;
    if (selModel)
    {
        QModelIndex currIdx = selModel->currentIndex();
        if (currIdx.isValid())
        {
            validIdx = true;
            if (currIdx.row() == 0)
                isFirst = true;

            if (currIdx.row() == (structureModel->rowCount() - 1))
                isLast = true;
        }
    }

    actionMap[EDIT_COLUMN]->setEnabled(validIdx);
    actionMap[DEL_COLUMN]->setEnabled(validIdx);
    actionMap[MOVE_COLUMN_UP]->setEnabled(validIdx && !isFirst);
    actionMap[MOVE_COLUMN_DOWN]->setEnabled(validIdx && !isLast);
}

void TableWindow::updateStructureCommitState()
{
    bool modified = isModified();
    actionMap[COMMIT_STRUCTURE]->setEnabled(modified);
    actionMap[ROLLBACK_STRUCTURE]->setEnabled(modified && existingTable);
}

void TableWindow::updateTableConstraintsToolbarState()
{
    QItemSelectionModel *selModel = ui->tableConstraintsView->selectionModel();
    bool anyColumn = structureModel && structureModel->rowCount() > 0;
    bool validIdx = false;
    bool isFirst = false;
    bool isLast = false;
    if (selModel)
    {
        QModelIndex currIdx = selModel->currentIndex();
        if (currIdx.isValid())
        {
            validIdx = true;
            if (currIdx.row() == 0)
                isFirst = true;

            if (currIdx.row() == (structureConstraintsModel->rowCount() - 1))
                isLast = true;
        }
    }

    actionMap[EDIT_TABLE_CONSTRAINT]->setEnabled(anyColumn && validIdx);
    actionMap[DEL_TABLE_CONSTRAINT]->setEnabled(anyColumn && validIdx);
    actionMap[MOVE_CONSTRAINT_UP]->setEnabled(anyColumn && validIdx && !isFirst);
    actionMap[MOVE_CONSTRAINT_DOWN]->setEnabled(anyColumn && validIdx && !isLast);
    actionMap[ADD_TABLE_PK]->setEnabled(!hasAnyPkDefined());
}

void TableWindow::setupDefShortcuts()
{
    // Widget context
    setShortcutContext({
                           COMMIT_STRUCTURE,
                           ROLLBACK_STRUCTURE,
                           REFRESH_STRUCTURE,
                           REFRESH_TRIGGERS,
                           ADD_COLUMN,
                           EDIT_COLUMN,
                           DEL_COLUMN,
                           ADD_TABLE_CONSTRAINT,
                           EDIT_TABLE_CONSTRAINT,
                           DEL_TABLE_CONSTRAINT,
                           ADD_INDEX,
                           EDIT_INDEX,
                           DEL_INDEX,
                           ADD_TRIGGER,
                           EDIT_TRIGGER,
                           DEL_TRIGGER,
                       },
                       Qt::WidgetWithChildrenShortcut);

    BIND_SHORTCUTS(TableWindow, Action);
}

void TableWindow::executionSuccessful()
{
    dataLoaded = true;
}

void TableWindow::executionFailed(const QString& errorText)
{
    notifyError(tr("Could not load data for table %1. Error details: %2").arg(table, errorText));
}

void TableWindow::defineCurrentContextDb()
{
    ui->dbCombo->setCurrentDb(db);
}

void TableWindow::initDbAndTable()
{
    ui->structureView->setItemDelegateForColumn(0, iconPositionDelegate);
    for (int colIdx = 2; colIdx < 9; colIdx++)
        ui->structureView->setItemDelegateForColumn(colIdx, constraintColumnsDelegate);

    defineCurrentContextDb();

    if (db)
    {
        // Should always be true, but for sake of future possibility of re-allowing null db
        dataModel->setDb(db);
        ui->dbCombo->setDisabled(true);
    }

    if (existingTable)
        dataModel->setDatabaseAndTable(database, table);

    ui->tableNameEdit->setText(table); // TODO no attached/temp db name support here

    if (structureModel)
    {
        delete structureModel;
        structureModel = nullptr;
    }

    if (structureConstraintsModel)
    {
        delete structureConstraintsModel;
        structureConstraintsModel = nullptr;
    }

    if (constraintTabModel)
    {
        delete constraintTabModel;
        constraintTabModel = nullptr;
    }

    structureModel = new TableStructureModel(this);
    structureConstraintsModel = new TableConstraintsModel(this);
    constraintTabModel = new ConstraintTabModel(this);

    // Columns model signals
    connect(structureModel, SIGNAL(columnModified(QString,SqliteCreateTable::Column*)),
            structureConstraintsModel, SLOT(columnModified(QString,SqliteCreateTable::Column*)));
    connect(structureModel, SIGNAL(columnDeleted(QString)),
            structureConstraintsModel, SLOT(columnDeleted(QString)));
    connect(structureModel, SIGNAL(columnsOrderChanged()), this, SLOT(updateStructureToolbarState()));

    connect(structureModel, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(updateDdlTab()));
    connect(structureModel, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)), this, SLOT(updateDdlTab()));
    connect(structureModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(updateDdlTab()));
    connect(structureModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(updateDdlTab()));
    connect(structureModel, SIGNAL(modifiyStateChanged()), this, SIGNAL(modifyStatusChanged()));

    ui->structureView->setModel(structureModel);
    ui->structureView->verticalHeader()->setDefaultSectionSize(ui->structureView->fontMetrics().height() + 8);

    // Constraints model signals
    connect(structureConstraintsModel, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(updateDdlTab()));
    connect(structureConstraintsModel, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)), this, SLOT(updateDdlTab()));
    connect(structureConstraintsModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(updateDdlTab()));
    connect(structureConstraintsModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(updateDdlTab()));

    connect(structureConstraintsModel, SIGNAL(modifiyStateChanged()), this, SIGNAL(modifyStatusChanged()));
    connect(structureConstraintsModel, SIGNAL(constraintOrderChanged()), this, SLOT(updateTableConstraintsToolbarState()));

    ui->tableConstraintsView->setModel(structureConstraintsModel);
    ui->tableConstraintsView->verticalHeader()->setDefaultSectionSize(ui->tableConstraintsView->fontMetrics().height() + 8);

    // Constraint tab model signals
    connect(structureModel, SIGNAL(rowsInserted(QModelIndex,int,int)), constraintTabModel, SLOT(updateModel()));
    connect(structureModel, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)), constraintTabModel, SLOT(updateModel()));
    connect(structureModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), constraintTabModel, SLOT(updateModel()));
    connect(structureModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), constraintTabModel, SLOT(updateModel()));
    connect(structureConstraintsModel, SIGNAL(rowsInserted(QModelIndex,int,int)), constraintTabModel, SLOT(updateModel()));
    connect(structureConstraintsModel, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)), constraintTabModel, SLOT(updateModel()));
    connect(structureConstraintsModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), constraintTabModel, SLOT(updateModel()));
    connect(structureConstraintsModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), constraintTabModel, SLOT(updateModel()));

    ui->constraintsView->setModel(constraintTabModel);

    connect(ui->withoutRowIdCheck, SIGNAL(clicked()), this, SLOT(withOutRowIdChanged()));
    connect(ui->strictTableCheck, SIGNAL(clicked()), this, SLOT(strictChanged()));

    parseDdl();

    updateIndexes();
    ui->indexList->sortByColumn(0, Qt::AscendingOrder);

    updateTriggers();
    ui->triggerList->sortByColumn(0, Qt::AscendingOrder);

    // (Re)connect to DB signals
    connect(db, SIGNAL(dbObjectDeleted(QString,QString,DbObjectType)), this, SLOT(checkIfTableDeleted(QString,QString,DbObjectType)));

    // Selection model is recreated when setModel() is called on the view
    connect(ui->structureView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            this, SLOT(updateStructureToolbarState()));
    connect(ui->tableConstraintsView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            this, SLOT(updateTableConstraintsToolbarState()));
}

void TableWindow::setupCoverWidget()
{
    widgetCover = new WidgetCover(this);
    widgetCover->initWithInterruptContainer();
    widgetCover->hide();
    connect(widgetCover, SIGNAL(cancelClicked()), structureExecutor, SLOT(interrupt()));
}

void TableWindow::parseDdl()
{
    if (!resolveCreateTableStatement())
        return;

    if (!resolveOriginalCreateTableStatement())
        return;

    structureModel->setCreateTable(createTable.data());
    structureConstraintsModel->setCreateTable(createTable.data());
    constraintTabModel->setCreateTable(createTable.data());
    ui->withoutRowIdCheck->setChecked(createTable->withOutRowId);
    ui->strictTableCheck->setChecked(createTable->strict);
    ui->tableConstraintsView->resizeColumnsToContents();
    ui->structureView->resizeColumnsToContents();
    ui->constraintsView->resizeColumnsToContents();

    updateStructureToolbarState();
    updateTableConstraintsToolbarState();
    updateDdlTab();
}

bool TableWindow::resolveCreateTableStatement()
{
    if (existingTable)
    {
        SchemaResolver resolver(db);
        SqliteQueryPtr parsedObject = resolver.getParsedObject(database, table, SchemaResolver::TABLE);
        if (!parsedObject.dynamicCast<SqliteCreateTable>())
        {
            notifyError(tr("Could not process the %1 table correctly. Unable to open a table window.").arg(table));
            invalid = true;
            return false;
        }

        createTable = parsedObject.dynamicCast<SqliteCreateTable>();
    }
    else
    {
        createTable = SqliteCreateTablePtr::create();
        createTable->table = table;
    }
    return true;
}

bool TableWindow::resolveOriginalCreateTableStatement()
{
    originalCreateTable = SqliteCreateTablePtr::create(*createTable);
    return true;
}

void TableWindow::createDbCombo()
{
    ui->dbCombo->setFixedWidth(100);
    ui->dbCombo->setToolTip(tr("Database"));
    connect(ui->dbCombo, SIGNAL(verifiedDbChanged()), this, SLOT(dbChanged()));
}

void TableWindow::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    switch (e->type()) {
        case QEvent::LanguageChange:
            ui->retranslateUi(this);
            break;
        default:
            break;
    }
}

void TableWindow::showEvent(QShowEvent* e)
{
    if (!shownAtLEastOnce)
    {
        applyInitialTab();
        shownAtLEastOnce = true;
    }
    QWidget::showEvent(e);
}

QVariant TableWindow::saveSession()
{
    if (!db || DBLIST->isTemporary(db) || !existingTable)
        return QVariant();

    QHash<QString,QVariant> sessionValue;
    sessionValue["table"] = table;
    sessionValue["db"] = db->getName();
    sessionValue["dataView"] = ui->dataView->getSessionValue();
    return sessionValue;
}

bool TableWindow::restoreSession(const QVariant& sessionValue)
{
    QHash<QString, QVariant> value = sessionValue.toHash();
    if (value.size() == 0)
    {
        notifyWarn(tr("Could not restore window %1, because no database or table was stored in session for this window.").arg(value["title"].toString()));
        return false;
    }

    if (!value.contains("db") || !value.contains("table"))
    {
        notifyWarn(tr("Could not restore window '%1', because no database or table was stored in session for this window.").arg(value["title"].toString()));
        return false;
    }

    db = DBLIST->getByName(value["db"].toString());
    if (!db || !db->isValid() || (!db->isOpen() && !db->open()))
    {
        notifyWarn(tr("Could not restore window '%1', because database %2 could not be resolved.").arg(value["title"].toString(), value["db"].toString()));
        return false;
    }

    table = value["table"].toString();
    database = value["database"].toString();
    SchemaResolver resolver(db);
    if (!resolver.getTables(database).contains(table, Qt::CaseInsensitive))
    {
        notifyWarn(tr("Could not restore window '%1', because the table %2 doesn't exist in the database %3.").arg(value["title"].toString(), table, db->getName()));
        return false;
    }

    if (value.contains("dataView"))
    {
        QVariant dataViewSession = value["dataView"];
        ui->dataView->restoreFromSession(dataViewSession);
    }

    initDbAndTable();
    applyInitialTab();
    return true;
}

Icon* TableWindow::getIconNameForMdiWindow()
{
    return ICONS.TABLE;
}

QString TableWindow::getTitleForMdiWindow()
{
    QString dbSuffix = (!db ? "" : (" (" + db->getName() + ")"));
    if (existingTable)
        return table + dbSuffix;

    QStringList existingNames = MAINWINDOW->getMdiArea()->getWindowTitles();
    if (existingNames.contains(windowTitle()))
        return windowTitle();

    // Generate new name
    QString title = tr("New table %1").arg(newTableWindowNum++);
    while (existingNames.contains(title))
        title = tr("New table %1").arg(newTableWindowNum++);

    title += dbSuffix;
    return title;
}

Db* TableWindow::getDb() const
{
    return db;
}

SqliteCreateTablePtr TableWindow::getTableStatement() const
{
    return createTable;
}

QString TableWindow::getTable() const
{
    return table;
}

void TableWindow::dbClosedFinalCleanup()
{
    db = nullptr;
    dataModel->setDb(nullptr);
    structureExecutor->setDb(nullptr);
}

void TableWindow::checkIfTableDeleted(const QString& database, const QString& object, DbObjectType type)
{
    Q_UNUSED(database);

    // TODO uncomment below when dbnames are supported
//    if (this->database != database)
//        return;

    switch (type)
    {
        case DbObjectType::TABLE:
            break;
        case DbObjectType::INDEX:
            checkIfIndexDeleted(object);
            return;
        case DbObjectType::TRIGGER:
            checkIfTriggerDeleted(object);
            return;
        case DbObjectType::VIEW:
            return;
    }

    if (modifyingThisTable)
        return;

    if (object.compare(table, Qt::CaseInsensitive) == 0)
    {
        dbClosedFinalCleanup();
        MDIAREA->enforceCurrentTaskSelectionAfterWindowClose();
        getMdiWindow()->close();
    }
}

void TableWindow::checkIfIndexDeleted(const QString& object)
{
    for (int i = 0, total = ui->indexList->rowCount(); i < total; ++i)
    {
        if (ui->indexList->item(i, 0)->text().compare(object, Qt::CaseInsensitive) == 0)
        {
            ui->indexList->removeRow(i);
            return;
        }
    }
}

void TableWindow::checkIfTriggerDeleted(const QString& object)
{
    for (int i = 0, total = ui->triggerList->rowCount(); i < total; ++i)
    {
        if (ui->triggerList->item(i, 0)->text().compare(object, Qt::CaseInsensitive) == 0)
        {
            ui->triggerList->removeRow(i);
            return;
        }
    }
}

void TableWindow::refreshStructure()
{
    parseDdl();
    updateIndexes();
    updateTriggers();
}

bool TableWindow::commitStructure(bool skipWarning)
{
    if (!isModified())
    {
        qWarning() << "Called TableWindow::commitStructure(), but isModified() returned false.";
        updateStructureCommitState();
        return false;
    }

    if (!validate(skipWarning))
        return false;

    executeStructureChanges();
    return true;
}

QString TableWindow::updateWindowAfterStructureChanged()
{
    originalCreateTable = createTable;
    structureModel->setCreateTable(createTable.data());
    structureConstraintsModel->setCreateTable(createTable.data());
    dataLoaded = false;

    QString oldTable = table;
    database = createTable->database;
    table = createTable->table;
    existingTable = true;
    initDbAndTable();
    updateStructureCommitState();
    updateNewTableState();
    updateWindowTitle();
    return oldTable;
}

void TableWindow::changesSuccessfullyCommitted()
{
    modifyingThisTable = false;

    QStringList sqls = structureExecutor->getQueries();
    CFG->addDdlHistory(sqls.join("\n"), db->getName(), db->getPath());

    widgetCover->hide();

    QString oldTable = updateWindowAfterStructureChanged();
    emit sessionValueChanged();

    NotifyManager* notifyManager = NotifyManager::getInstance();
    if (oldTable.compare(table, Qt::CaseInsensitive) == 0 || oldTable.isEmpty())
    {
        notifyInfo(tr("Committed changes for table '%1' successfully.").arg(table));
    }
    else
    {
        notifyInfo(tr("Committed changes for table '%1' (named before '%2') successfully.").arg(table, oldTable));
        notifyManager->renamed(db, database, oldTable, table);
    }
    notifyManager->modified(db, database, table);

    DBTREE->refreshSchema(db);

    if (tableModifier)
    {
        QList<QStringList> modifiedObjects = {
            tableModifier->getModifiedTables(),
            tableModifier->getModifiedIndexes(),
            tableModifier->getModifiedTriggers(),
            tableModifier->getModifiedViews()
        };
        for (const QStringList& objList : modifiedObjects)
        {
            for (const QString& obj : objList)
            {
                if (obj.compare(oldTable, Qt::CaseInsensitive) == 0)
                    continue;

                notifyManager->modified(db, database, obj);
            }
        }
    }

    ui->dataView->resetSorting();
    if (ui->tabWidget->currentIndex() == getDataTabIdx())
        ui->dataView->refreshData();

    STATUSFIELD->releaseFadeOutFor(this);
}

void TableWindow::changesFailedToCommit(int errorCode, const QString& errorText)
{
    qDebug() << "TableWindow::changesFailedToCommit:" << errorCode << errorText;

    modifyingThisTable = false;
    widgetCover->hide();
    notifyError(tr("Could not commit table structure. Error message: %1", "table window").arg(errorText));

    STATUSFIELD->releaseFadeOutFor(this);
}

void TableWindow::rollbackStructure()
{
    createTable = SqliteCreateTablePtr::create(*originalCreateTable.data());
    structureModel->setCreateTable(createTable.data());
    structureConstraintsModel->setCreateTable(createTable.data());
    constraintTabModel->setCreateTable(createTable.data());
    ui->tableNameEdit->setText(createTable->table);
    ui->withoutRowIdCheck->setChecked(createTable->withOutRowId);
    ui->strictTableCheck->setChecked(createTable->strict);

    updateStructureCommitState();
    updateStructureToolbarState();
    updateTableConstraintsToolbarState();
    updateDdlTab();
}

void TableWindow::resetAutoincrement()
{
    if (!existingTable)
        return;

    QMessageBox::StandardButton btn = QMessageBox::question(this, tr("Reset autoincrement"), tr("Are you sure you want to reset autoincrement value for table '%1'?")
                                                            .arg(table));
    if (btn != QMessageBox::Yes)
        return;

    SqlQueryPtr res = db->exec("DELETE FROM sqlite_sequence WHERE name = ?;", {table});
    if (res->isError())
        notifyError(tr("An error occurred while trying to reset autoincrement value for table '%1': %2").arg(table, res->getErrorText()));
    else
        notifyInfo(tr("Autoincrement value for table '%1' has been reset successfully.").arg(table));
}

void TableWindow::addColumn()
{
    SqliteCreateTable::Column column;
    column.setParent(createTable.data());

    ColumnDialog columnDialog(db, this);
    columnDialog.setColumn(&column);
    if (hasAnyPkDefined())
        columnDialog.disableConstraint(ConstraintDialog::Constraint::PK);

    if (columnDialog.exec() != QDialog::Accepted)
        return;

    SqliteCreateTable::Column* modifiedColumn = columnDialog.getModifiedColumn();
    structureModel->appendColumn(modifiedColumn);
    ui->structureView->resizeColumnToContents(0);

    ui->structureView->setCurrentIndex(structureModel->index(structureModel->rowCount()-1, 0));
    resizeStructureViewColumns();
    updateTableConstraintsToolbarState();
}

void TableWindow::editColumn()
{
    editColumn(structureCurrentIndex());
}

void TableWindow::delColumn()
{
    QModelIndex idx = structureCurrentIndex();
    delColumn(idx);
}

void TableWindow::moveColumnUp()
{
    QModelIndex idx = structureCurrentIndex();
    if (!idx.isValid())
    {
        qWarning() << "Called TableWindow::moveColumnUp() with invalid index.";
        return;
    }

    structureModel->moveColumnUp(idx.row());
}

void TableWindow::moveColumnDown()
{
    QModelIndex idx = structureCurrentIndex();
    if (!idx.isValid())
    {
        qWarning() << "Called TableWindow::moveColumnDown() with invalid index.";
        return;
    }

    structureModel->moveColumnDown(idx.row());
}


void TableWindow::addConstraint(ConstraintDialog::Constraint mode)
{
    NewConstraintDialog dialog(mode, createTable.data(), db, this);
    if (hasAnyPkDefined())
        dialog.disableMode(ConstraintDialog::PK);

    if (dialog.exec() != QDialog::Accepted)
        return;

    SqliteStatement* constrStmt = dialog.getConstraint();
    SqliteCreateTable::Constraint* tableConstr = dynamic_cast<SqliteCreateTable::Constraint*>(constrStmt);
    if (!tableConstr)
    {
        qCritical() << "Constraint returned from ConstraintDialog was not of table type, while we're trying to add table constraint.";
        return;
    }

    structureConstraintsModel->appendConstraint(tableConstr);
    ui->tableConstraintsView->resizeColumnToContents(0);
    ui->tableConstraintsView->resizeColumnToContents(1);
    updateTableConstraintsToolbarState();
}

bool TableWindow::validate(bool skipWarning)
{
    if (!existingTable && !skipWarning && ui->tableNameEdit->text().isEmpty())
    {
        int res = QMessageBox::warning(this, tr("Empty name"), tr("A blank name for the table is allowed in SQLite, but it is not recommended.\n"
            "Are you sure you want to create a table with blank name?"), QMessageBox::Yes, QMessageBox::No);

        if (res != QMessageBox::Yes)
            return false;
    }

    if (structureModel->rowCount() == 0)
    {
        notifyError(tr("Cannot create a table without at least one column."));
        return false;
    }

    if (ui->withoutRowIdCheck->isChecked())
    {
        bool hasPk = false;
        bool isPkAutoIncr = false;

        if (createTable->getConstraints(SqliteCreateTable::Constraint::PRIMARY_KEY).size() > 0)
            hasPk = true;

        for (SqliteCreateTable::Column*& column : createTable->columns)
        {
            SqliteCreateTable::Column::Constraint* colConstraint = column->getConstraint(SqliteCreateTable::Column::Constraint::PRIMARY_KEY);
            if (colConstraint)
            {
                hasPk = true;
                if (colConstraint->autoincrKw)
                    isPkAutoIncr = true;
            }
        }

        if (!hasPk)
        {
            notifyError(tr("Cannot create table %1, if it has no primary key defined."
                           " Either uncheck the %2, or define a primary key.").arg("WITHOUT ROWID", "WITHOUT ROWID"));
            return false;
        }

        if (isPkAutoIncr)
        {
            notifyError(tr("Cannot use autoincrement for primary key when %1 clause is used."
                           " Either uncheck the %2, or the autoincrement in a primary key.").arg("WITHOUT ROWID", "WITHOUT ROWID"));
            return false;
        }
    }

    if (ui->strictTableCheck->isChecked())
    {
        QStringList nonStrictColumns;
        for (SqliteCreateTable::Column*& column : createTable->columns)
        {
            if (DataType::isStrict(column->type ? column->type->name : QString()))
                continue;

            nonStrictColumns << column->name;
        }

        if (!nonStrictColumns.isEmpty())
        {
            notifyError(tr("Following columns have non-strict data type: %1."
                           " Either disable strict mode of the table, or fix column data types. Valid strict data types are: %2")
                            .arg(nonStrictColumns.join(", "), DataType::getStrictValueNames().join(", ")));
            return false;
        }
    }

    return true;
}

bool TableWindow::isModified() const
{
    return (structureModel && structureModel->isModified()) ||
            (structureConstraintsModel && structureConstraintsModel->isModified()) ||
            (originalCreateTable &&
                (originalCreateTable->table != ui->tableNameEdit->text() ||
                 originalCreateTable->withOutRowId != createTable->withOutRowId ||
                 originalCreateTable->strict != createTable->strict)
            ) ||
            (!existingTable && !ui->tableNameEdit->text().isEmpty());
}

TokenList TableWindow::indexColumnTokens(SqliteCreateIndexPtr index)
{
    if (index->indexedColumns.size() == 0)
        return TokenList();

    SqliteOrderBy* firstCol = index->indexedColumns.first();
    SqliteOrderBy* lastCol = index->indexedColumns.last();
    if (firstCol->tokens.size() == 0)
        return TokenList();

    if (lastCol->tokens.size() == 0)
        return TokenList();

    int firstIdx = index->tokens.indexOf(firstCol->tokens.first());
    int lastIdx = index->tokens.indexOf(lastCol->tokens.last());

    return index->tokens.mid(firstIdx, lastIdx-firstIdx+1);
}

QString TableWindow::getCurrentIndex() const
{
    int row = ui->indexList->currentRow();
    QTableWidgetItem* item = ui->indexList->item(row, 0);
    if (!item)
        return QString();

    return item->text();
}

QString TableWindow::getCurrentTrigger() const
{
    int row = ui->triggerList->currentRow();
    QTableWidgetItem* item = ui->triggerList->item(row, 0);
    if (!item)
        return QString();

    return item->text();
}

void TableWindow::applyInitialTab()
{
    if (existingTable && !table.isNull() && CFG_UI.General.OpenTablesOnData.get())
        ui->tabWidget->setCurrentIndex(getDataTabIdx());
    else
        ui->tabWidget->setCurrentIndex(getStructureTabIdx());
}

void TableWindow::resizeStructureViewColumns()
{
    // Resize all except last one, to avoid shrinking the "extend to end" column.
    for (int c = 0, total = (ui->structureView->horizontalHeader()->count() - 1); c < total; ++c)
        ui->structureView->resizeColumnToContents(c);
}

int TableWindow::getDataTabIdx() const
{
    return ui->tabWidget->indexOf(ui->dataTab);
}

int TableWindow::getStructureTabIdx() const
{
    return ui->tabWidget->indexOf(ui->structureTab);
}

bool TableWindow::hasAnyPkDefined() const
{
    if (structureConstraintsModel)
    {
        for (int i = 0, total = structureConstraintsModel->rowCount(); i < total; ++i)
        {
            SqliteCreateTable::Constraint* constraint = structureConstraintsModel->getConstraint(i);
            if (constraint->type == SqliteCreateTable::Constraint::PRIMARY_KEY)
                return true;
        }
    }

    if (structureModel)
    {
        for (int i = 0, total = structureModel->rowCount(); i < total; ++i)
        {
            SqliteCreateTable::Column* column = structureModel->getColumn(i);
            if (column->hasConstraint(SqliteCreateTable::Column::Constraint::PRIMARY_KEY))
                return true;
        }
    }

    return false;
}

void TableWindow::updateDdlTab()
{
    createTable->rebuildTokens();
    QString ddl = createTable->detokenize();
    if (createTable->columns.size() > 0)
        ddl = LETOS->getCodeFormatter()->format("sql", ddl, db);

    ui->ddlEdit->setPlainText(ddl);
}

void TableWindow::updateNewTableState()
{
    for (QWidget* tab : {ui->dataTab, ui->constraintsTab, ui->indexesTab, ui->triggersTab})
        ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(tab), existingTable);

    actionMap[EXPORT]->setEnabled(existingTable);
    actionMap[IMPORT]->setEnabled(existingTable);
    actionMap[POPULATE]->setEnabled(existingTable);
    actionMap[CREATE_SIMILAR]->setEnabled(existingTable);
    actionMap[RESET_AUTOINCREMENT]->setEnabled(existingTable);
    actionMap[REFRESH_STRUCTURE]->setEnabled(existingTable);
}

void TableWindow::addConstraint()
{
    addConstraint(ConstraintDialog::UNKNOWN);
}

void TableWindow::editConstraint()
{
    QModelIndex idx = ui->tableConstraintsView->currentIndex();
    editConstraint(idx);
}

void TableWindow::delConstraint()
{
    QModelIndex idx = ui->tableConstraintsView->currentIndex();
    delConstraint(idx);
}

void TableWindow::editConstraint(const QModelIndex& idx)
{
    if (!idx.isValid())
    {
        addConstraint();
        return;
    }

    SqliteCreateTable::Constraint* constr = structureConstraintsModel->getConstraint(idx.row());
    QString beforeDdl = constr->detokenize().trimmed();
    ConstraintDialog dialog(ConstraintDialog::EDIT, constr, createTable.data(), db, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString afterDdl = constr->detokenize().trimmed();
    if (beforeDdl != afterDdl)
    {
        structureConstraintsModel->constraintModified(idx.row());
        ui->tableConstraintsView->resizeColumnToContents(0);
        ui->tableConstraintsView->resizeColumnToContents(1);
    }
}

void TableWindow::delConstraint(const QModelIndex& idx)
{
    if (!idx.isValid())
        return;

    SqliteCreateTable::Constraint* constr = structureConstraintsModel->getConstraint(idx.row());

    QString arg = constr->name.isNull() ? constr->typeString() : constr->name;
    QString msg = tr("Are you sure you want to delete table constraint '%1'?", "table window").arg(arg);
    int btn = QMessageBox::question(this, tr("Delete constraint", "table window"), msg);
    if (btn != QMessageBox::Yes)
        return;

    structureConstraintsModel->delConstraint(idx.row());
    ui->structureView->resizeColumnToContents(0);
    updateTableConstraintsToolbarState();
}

void TableWindow::moveConstraintUp()
{
    QModelIndex idx = ui->tableConstraintsView->currentIndex();
    if (!idx.isValid())
        return;

    structureConstraintsModel->moveConstraintUp(idx.row());
    updateTableConstraintsToolbarState();
    updateStructureCommitState();
}

void TableWindow::moveConstraintDown()
{
    QModelIndex idx = ui->tableConstraintsView->currentIndex();
    if (!idx.isValid())
        return;

    structureConstraintsModel->moveConstraintDown(idx.row());
    updateTableConstraintsToolbarState();
    updateStructureCommitState();
}

void TableWindow::addPk()
{
    addConstraint(ConstraintDialog::PK);
}

void TableWindow::addFk()
{
    addConstraint(ConstraintDialog::FK);
}

void TableWindow::addUnique()
{
    addConstraint(ConstraintDialog::UNIQUE);
}

void TableWindow::addCheck()
{
    addConstraint(ConstraintDialog::CHECK);
}

void TableWindow::exportTable()
{
    if (!ExportManager::isAnyPluginAvailable())
    {
        notifyError(tr("Cannot export, because no export plugin is loaded."));
        return;
    }

    ExportDialog dialog(this);
    dialog.setTableMode(db, table);
    dialog.exec();
}

void TableWindow::importTable()
{
    if (!ImportManager::isAnyPluginAvailable())
    {
        notifyError(tr("Cannot import, because no import plugin is loaded."));
        return;
    }

    ImportDialog dialog(this);
    dialog.setDbAndTable(db, table);
    dialog.setPreferTableOverFileName(true);
    if (dialog.exec() == QDialog::Accepted && dataLoaded)
        ui->dataView->refreshData(false);
}

void TableWindow::populateTable()
{
    PopulateDialog dialog(this);
    dialog.setDbAndTable(db, table);
    if (dialog.exec() == QDialog::Accepted && dataLoaded)
        ui->dataView->refreshData(false);
}

void TableWindow::createSimilarTable()
{
    DbObjectDialogs dialog(db);
    dialog.addTableSimilarTo(QString(), table);
}

void TableWindow::tabChanged(int newTab)
{
    if (disableCommitOnTabChange || tabsMoving)
        return;

    if (newTab == getDataTabIdx())
    {
        if (isModified())
        {
            QMessageBox box(QMessageBox::Question, tr("Uncommitted changes"),
                            tr("There are uncommitted structure modifications."),
                            QMessageBox::NoButton, this);
            box.setInformativeText(tr("You cannot browse or edit data until you have "
                                      "table structure settled.\n"
                                      "Do you want to commit the structure, or do you want to go back to the structure tab?"));
            box.addButton(tr("Go back to structure tab"), QMessageBox::RejectRole);
            QAbstractButton* commitButton = box.addButton(tr("Commit modifications and browse data"),
                                                          QMessageBox::ApplyRole);
            box.exec();

            if (box.clickedButton() == commitButton)
                commitStructure(true);
            else
                focusStructureTab();

            return;
        }

        if (!dataLoaded)
            ui->dataView->refreshData(false);
    }
}

void TableWindow::structureViewDoubleClicked(const QModelIndex &index)
{
    editColumn(index);
}

void TableWindow::constraintsViewDoubleClicked(const QModelIndex &index)
{
    editConstraint(index);
}

void TableWindow::nameChanged()
{
    if (!createTable)
        return;

    createTable->table = ui->tableNameEdit->text();
    updateDdlTab();
}

void TableWindow::withOutRowIdChanged()
{
    if (!createTable)
        return;

    createTable->withOutRowId = ui->withoutRowIdCheck->isChecked();
    updateDdlTab();
    emit modifyStatusChanged();
}

void TableWindow::strictChanged()
{
    if (!createTable)
        return;

    createTable->strict = ui->strictTableCheck->isChecked();
    if (createTable->strict)
    {
        for (SqliteCreateTable::Column*& column : createTable->columns)
        {
            if (column->type)
            {
                column->type->precision = QVariant();
                column->type->scale = QVariant();
            }
        }
    }

    updateDdlTab();
    emit modifyStatusChanged();
}

void TableWindow::addIndex()
{
    DbObjectDialogs dialogs(db, this);
    dialogs.addIndex(table);
    updateIndexes();
}

void TableWindow::editCurrentIndex()
{
    QString index = getCurrentIndex();
    if (index.isNull())
        return;

    DbObjectDialogs dialogs(db, this);
    dialogs.editIndex(index);
    updateIndexes();
}

void TableWindow::indexViewDoubleClicked(const QModelIndex& idx)
{
    if (!idx.isValid())
    {
        addIndex();
        return;
    }

    QString index = ui->indexList->item(idx.row(), 0)->text();

    DbObjectDialogs dialogs(db, this);
    dialogs.editIndex(index);
    updateIndexes();
}


void TableWindow::delIndex()
{
    QString index = getCurrentIndex();
    if (index.isNull())
        return;

    DbObjectDialogs dialogs(db, this);
    dialogs.dropObject(DbObjectDialogs::Type::INDEX, index);
    updateIndexes();
}

void TableWindow::addTrigger()
{
    DbObjectDialogs dialogs(db, this);
    dialogs.addTriggerOnTable(table);
    updateTriggers();
}

void TableWindow::editTrigger()
{
    QString trigger = getCurrentTrigger();
    if (trigger.isNull())
    {
        addTrigger();
        return;
    }

    DbObjectDialogs dialogs(db, this);
    dialogs.editTrigger(trigger);
    updateTriggers();
}

void TableWindow::triggerViewDoubleClicked(const QModelIndex& idx)
{
    if (!idx.isValid())
    {
        addTrigger();
        return;
    }

    QString trigger = ui->triggerList->item(idx.row(), 0)->text();

    DbObjectDialogs dialogs(db, this);
    dialogs.editTrigger(trigger);
    updateTriggers();
}

void TableWindow::delTrigger()
{
    QString trigger = getCurrentTrigger();
    if (trigger.isNull())
        return;

    DbObjectDialogs dialogs(db, this);
    dialogs.dropObject(DbObjectDialogs::Type::TRIGGER, trigger);
    updateTriggers();
}

void TableWindow::updateIndexesState()
{
    bool editDel = ui->indexList->currentItem() != nullptr;
    actionMap[REFRESH_INDEXES]->setEnabled(existingTable);
    actionMap[ADD_INDEX]->setEnabled(existingTable);
    actionMap[EDIT_INDEX]->setEnabled(editDel);
    actionMap[DEL_INDEX]->setEnabled(editDel);
}

void TableWindow::updateTriggersState()
{
    bool editDel = ui->triggerList->currentItem() != nullptr;
    actionMap[REFRESH_TRIGGERS]->setEnabled(existingTable);
    actionMap[ADD_TRIGGER]->setEnabled(existingTable);
    actionMap[EDIT_TRIGGER]->setEnabled(editDel);
    actionMap[DEL_TRIGGER]->setEnabled(editDel);
}

void TableWindow::nextTab()
{
    int idx = ui->tabWidget->currentIndex();
    idx++;
    ui->tabWidget->setCurrentIndex(idx);
}

void TableWindow::prevTab()
{
    int idx = ui->tabWidget->currentIndex();
    idx--;
    ui->tabWidget->setCurrentIndex(idx);
}

void TableWindow::updateIndexes()
{
    ui->indexList->clear();

    if (!db || !db->isValid())
        return;

    SchemaResolver resolver(db);
    resolver.setIgnoreSystemObjects(false);
    QList<SqliteCreateIndexPtr> indexes = resolver.getParsedIndexesForTable(database, table);

    ui->indexList->setSortingEnabled(false);
    ui->indexList->setColumnCount(4);
    ui->indexList->setRowCount(indexes.size());
    ui->indexList->setHorizontalHeaderLabels({
                                                 tr("Name", "table window indexes"),
                                                 tr("Unique", "table window indexes"),
                                                 tr("Columns", "table window indexes"),
                                                 tr("Partial index condition", "table window indexes"),
                                             });

    ui->indexList->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    QTableWidgetItem* item = nullptr;
    int row = 0;
    for (SqliteCreateIndexPtr& index : indexes)
    {
        item = new QTableWidgetItem(index->index);
        item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
        ui->indexList->setItem(row, 0, item);

        QCheckBox *check = new QCheckBox(ui->indexList);
        check->setChecked(index->uniqueKw);
        check->setAttribute(Qt::WA_TransparentForMouseEvents);
        check->setFocusPolicy(Qt::NoFocus);
        QWidget *container = new QWidget(ui->indexList);
        QHBoxLayout *layout = new QHBoxLayout(container);
        layout->addWidget(check);
        layout->setAlignment(Qt::AlignCenter);
        layout->setContentsMargins(0, 0, 0, 0);
        ui->indexList->setCellWidget(row, 1, container);

        item = new QTableWidgetItem(indexColumnTokens(index).detokenize());
        item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
        ui->indexList->setItem(row, 2, item);

        item = new QTableWidgetItem(index->where ? index->where->detokenize() : "");
        item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
        ui->indexList->setItem(row, 3, item);

        row++;
    }

    ui->indexList->resizeColumnsToContents();
    ui->indexList->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    ui->indexList->selectionModel()->clearSelection();
    ui->indexList->selectionModel()->clearCurrentIndex();
    ui->indexList->setSortingEnabled(true);
    updateIndexesState();
}

void TableWindow::updateTriggers()
{
    if (!db || !db->isValid())
        return;

    SchemaResolver resolver(db);
    QList<SqliteCreateTriggerPtr> triggers = resolver.getParsedTriggersForTable(database, table);

    ui->triggerList->setSortingEnabled(false);
    ui->triggerList->setColumnCount(4);
    ui->triggerList->setRowCount(triggers.size());
    ui->triggerList->horizontalHeader()->setMaximumSectionSize(200);
    ui->triggerList->setHorizontalHeaderLabels({
                                                 tr("Name", "table window triggers"),
                                                 tr("Event", "table window triggers"),
                                                 tr("Condition", "table window triggers"),
                                                 tr("Details", "table window triggers")
                                             });

    QTableWidgetItem* item = nullptr;
    QString timeAndEvent;
    int row = 0;
    for (SqliteCreateTriggerPtr& trig : triggers)
    {
        item = new QTableWidgetItem(trig->trigger);
        item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
        ui->triggerList->setItem(row, 0, item);

        timeAndEvent = trig->tokensMap["trigger_time"].detokenize() + trig->tokensMap["trigger_event"].detokenize();
        item = new QTableWidgetItem(timeAndEvent);
        item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
        ui->triggerList->setItem(row, 1, item);

        item = new QTableWidgetItem(trig->precondition ? trig->precondition->detokenize().trimmed() : "");
        item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
        ui->triggerList->setItem(row, 2, item);

        item = new QTableWidgetItem(trig->tokensMap["trigger_cmd_list"].detokenize().trimmed());
        item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
        ui->triggerList->setItem(row, 3, item);

        row++;
    }

    ui->triggerList->resizeColumnsToContents();
    ui->triggerList->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    ui->triggerList->selectionModel()->clearSelection();
    ui->triggerList->selectionModel()->clearCurrentIndex();
    ui->triggerList->setSortingEnabled(true);
    updateTriggersState();
}

void TableWindow::editColumn(const QString& columnName)
{
    QModelIndex colIdx = structureModel->findColumn(columnName);
    if (!colIdx.isValid())
        return;

    editColumn(colIdx);
}

void TableWindow::delColumn(const QString& columnName)
{
    QModelIndex colIdx = structureModel->findColumn(columnName);
    if (!colIdx.isValid())
        return;

    delColumn(colIdx);
}

void TableWindow::focusStructureTab()
{
    ui->tabWidget->setCurrentIndex(getStructureTabIdx());
}

void TableWindow::focusDataTab()
{
    ui->tabWidget->setCurrentIndex(getDataTabIdx());
}

void TableWindow::updateTabsOrder()
{
    tabsMoving = true;
    ui->tabWidget->removeTab(getDataTabIdx());
    int idx = 1;
    if (CFG_UI.General.DataTabAsFirstInTables.get())
        idx = 0;

    ui->tabWidget->insertTab(idx, ui->dataTab, tr("Data"));
    tabsMoving = false;
}

bool TableWindow::restoreSessionNextTime()
{
    return existingTable && db && !DBLIST->isTemporary(db);
}

QToolBar* TableWindow::getToolBar(int toolbar) const
{
    switch (static_cast<ToolBar>(toolbar))
    {
        case TOOLBAR_STRUCTURE:
            return ui->structureToolBar;
        case TOOLBAR_INDEXES:
            return ui->indexToolBar;
        case TOOLBAR_TRIGGERS:
            return ui->triggerToolBar;
    }
    return nullptr;
}

bool TableWindow::handleInitialFocus()
{
    if (!existingTable)
    {
        ui->tableNameEdit->setFocus();
        return true;
    }
    return false;
}

bool TableWindow::isUncommitted() const
{
    return ui->dataView->isUncommitted() || isModified();
}

QString TableWindow::getQuitUncommittedConfirmMessage() const
{
    QString title = getMdiWindow()->windowTitle();
    if (ui->dataView->isUncommitted() && isModified())
        return tr("Table window \"%1\" has uncommitted structure modifications and data.").arg(title);
    else if (ui->dataView->isUncommitted())
        return tr("Table window \"%1\" has uncommitted data.").arg(title);
    else if (isModified())
        return tr("Table window \"%1\" has uncommitted structure modifications.").arg(title);
    else
    {
        qCritical() << "Unhandled message case in TableWindow::getQuitUncommittedConfirmMessage().";
        return QString();
    }
}

void TableWindow::useCurrentTableAsBaseForNew()
{
    newTable();
    ui->tableNameEdit->clear();
    updateWindowTitle();
    ui->tableNameEdit->setFocus();
    updateAfterInit();
}

Db* TableWindow::getAssociatedDb() const
{
    return db;
}

QPair<Db*, QString> TableWindow::getSoftDbObjectAssociation() const
{
    if (!existingTable)
        return {db, QString()};

    return {db, table};
}

bool TableWindow::isWindowClosingBlocked() const
{
    return structureExecutor->isExecuting() || dataModel->isExecutionInProgress() ||
        (ui->tabWidget->currentIndex() == getDataTabIdx() && !ui->dataView->getNavigationState());
}

void TableWindow::updateFont()
{
    QFont f = CFG_UI.Fonts.DataView.get();
    QFontMetrics fm(f);

    QTableView* views[] = {ui->structureView, ui->tableConstraintsView, ui->indexList, ui->triggerList};
    for (QTableView* view : views)
    {
        view->setFont(f);
        view->horizontalHeader()->setFont(f);
        view->verticalHeader()->setFont(f);
        view->verticalHeader()->setDefaultSectionSize(fm.height() + 4);
    }
}

void TableWindow::dbChanged()
{
    if (db)
        disconnect(db, SIGNAL(dbObjectDeleted(QString,QString,DbObjectType)), this, SLOT(checkIfTableDeleted(QString,QString,DbObjectType)));

    db = ui->dbCombo->currentDb();
    dataModel->setDb(db);

    if (db)
        connect(db, SIGNAL(dbObjectDeleted(QString,QString,DbObjectType)), this, SLOT(checkIfTableDeleted(QString,QString,DbObjectType)));
}

void TableWindow::handlePossibleIdxOrTrgRename(Db* db, const QString& database, const QString& oldObject, const QString& newObject)
{
    Q_UNUSED(database);
    if (db != this->db)
        return;

    for (int i = 0, total = ui->indexList->rowCount(); i < total; ++i)
    {
        if (ui->indexList->item(i, 0)->text().compare(oldObject, Qt::CaseInsensitive) == 0)
        {
            ui->indexList->item(i, 0)->setText(newObject);
            return;
        }
    }

    for (int i = 0, total = ui->triggerList->rowCount(); i < total; ++i)
    {
        if (ui->triggerList->item(i, 0)->text().compare(oldObject, Qt::CaseInsensitive) == 0)
        {
            ui->triggerList->item(i, 0)->setText(newObject);
            return;
        }
    }
}

void TableWindow::handlePossibleColumnRename(Db* db, const QString& database, const QString& table, const QString& oldObject, const QString& newObject)
{
    if (db != this->db || table.compare(this->table, Qt::CaseInsensitive) != 0)
        return;

    refreshStructure();
}
