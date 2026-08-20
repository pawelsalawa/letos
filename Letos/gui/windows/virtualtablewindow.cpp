#include "virtualtablewindow.h"
#include "common/iconpositionitemdelegate.h"
#include "common/widgetcover.h"
#include "datagrid/sqltablemodel.h"
#include "dbobjectdialogs.h"
#include "dialogs/ddlpreviewdialog.h"
#include "dialogs/exportdialog.h"
#include "dialogs/importdialog.h"
#include "dialogs/populatedialog.h"
#include "iconmanager.h"
#include "services/codeformatter.h"
#include "services/dbmanager.h"
#include "services/notifymanager.h"
#include "statusfield.h"
#include "themetuner.h"
#include "uiconfig.h"
#include "mainwindow.h"
#include "common/dbcombobox.h"
#include "ui_virtualtablewindow.h"
#include "db/chainexecutor.h"
#include "schemaresolver.h"
#include "dbtree/dbtree.h"
#include <QMessageBox>
#include <QStandardItem>
#include <QPushButton>

CFG_KEYS_DEFINE(VirtualTableWindow)

VirtualTableWindow::VirtualTableWindow(QWidget *parent) :
    MdiChild(parent),
    ui(new Ui::VirtualTableWindow)
{
    init();
}

VirtualTableWindow::VirtualTableWindow(Db* db, QWidget* parent) :
    MdiChild(parent),
    db(db),
    ui(new Ui::VirtualTableWindow)
{
    newTable();
    init();
    initDbAndTable();
}

VirtualTableWindow::VirtualTableWindow(const VirtualTableWindow& win) :
    MdiChild(win.parentWidget()),
    db(win.db),
    database(win.database),
    table(win.table),
    ui(new Ui::VirtualTableWindow)
{
    init();
    initDbAndTable();
}

VirtualTableWindow::VirtualTableWindow(QWidget* parent, Db* db, const QString& database, const QString& table) :
    MdiChild(parent),
    db(db),
    database(database),
    table(table),
    ui(new Ui::VirtualTableWindow)
{
    init();
    initDbAndTable();
}

VirtualTableWindow::~VirtualTableWindow()
{
    delete ui;
}

void VirtualTableWindow::init()
{
    ui->setupUi(this);

    ui->columnsView->horizontalHeader()->setSectionsClickable(false);
    iconPositionDelegate = new IconPositionItemDelegate(this);

#ifdef Q_OS_MACX
    QStyle *fusion = QStyleFactory::create("Fusion");
    ui->structureToolBar->setStyle(fusion);
    ui->structureTab->layout()->setSpacing(0);
#endif

    ui->argsEdit->setVirtualSqlExpression("CREATE VIRTUAL TABLE vt USING modname (%1);");

    dataModel = new SqlTableModel(this);
    dataModel->setSupportsReturningDelete(false);
    ui->dataView->init(dataModel);

    initActions();
    updateTabsOrder();
    createDbCombo();

    MAINWINDOW->installToolbarSizeWheelHandler(ui->structureToolBar);
    MAINWINDOW->installToolbarSizeWheelHandler(ui->dataView->getToolBar(DataView::TOOLBAR_GRID));
    MAINWINDOW->installToolbarSizeWheelHandler(ui->dataView->getToolBar(DataView::TOOLBAR_FORM));

    connect(dataModel, SIGNAL(executionSuccessful()), this, SLOT(executionSuccessful()));
    connect(dataModel, SIGNAL(executionFailed(QString)), this, SLOT(executionFailed(QString)));
    connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));
    connect(this, SIGNAL(modifyStatusChanged()), this, SLOT(updateStructureCommitState()));
    connect(ui->tableNameEdit, SIGNAL(textChanged(QString)), this, SIGNAL(modifyStatusChanged()));
    connect(ui->tableNameEdit, SIGNAL(textChanged(QString)), this, SLOT(nameChanged()));
    connect(ui->moduleCombo, SIGNAL(currentTextChanged(QString)), this, SIGNAL(modifyStatusChanged()));
    connect(ui->argsEdit, SIGNAL(textChanged()), this, SIGNAL(modifyStatusChanged()));
    connect(CFG_UI.General.DataTabAsFirstInTables, SIGNAL(changed(const QVariant&)), this, SLOT(updateTabsOrder()));
    connect(CFG_UI.Fonts.DataView, SIGNAL(changed(QVariant)), this, SLOT(updateFont()));

    connect(ui->shadowTablesView, &QListWidget::doubleClicked, this, [this](const QModelIndex& idx)
    {
        DBTREE->openTable(db, database, idx.data().toString());
    });

    structureExecutor = new ChainExecutor(this);
    connect(structureExecutor, SIGNAL(success(SqlQueryPtr)), this, SLOT(changesSuccessfullyCommitted()));
    connect(structureExecutor, SIGNAL(failure(int,QString)), this, SLOT(changesFailedToCommit(int,QString)));

    THEME_TUNER->manageCompactLayout({
                                         ui->structureTab,
                                         ui->dataTab,
                                         ui->ddlTab
                                     });

    updateFont();
    setupCoverWidget();
    updateAfterInit();
}

void VirtualTableWindow::initDbAndTable()
{
    ui->columnsView->setItemDelegateForColumn(0, iconPositionDelegate);

    defineCurrentContextDb();

    if (db)
    {
        // Should always be true, but for sake of future possibility of re-allowing null db
        dataModel->setDb(db);
        ui->dbCombo->setDisabled(true);

        SchemaResolver resolver(db);
        QStringList extensions = resolver.getAllExtensions();
        extensions.sort(Qt::CaseInsensitive);
        ui->moduleCombo->addItems(extensions);
    }

    if (database.isEmpty())
        database = "main";

    if (existingTable)
        dataModel->setDatabaseAndTable(database, table);

    ui->tableNameEdit->setText(table); // TODO no attached/temp db name support here

    if (columnsModel)
    {
        delete columnsModel;
        columnsModel = nullptr;
    }

    columnsModel = new QStandardItemModel(this);
    columnsModel->setColumnCount(2);
    columnsModel->setHeaderData(0, Qt::Horizontal, tr("Name"));
    columnsModel->setHeaderData(1, Qt::Horizontal, tr("Data type"));

    connect(ui->argsEdit, SIGNAL(textChanged()), this, SLOT(updateDdlTab()));
    connect(ui->tableNameEdit, SIGNAL(textChanged(QString)), this, SLOT(updateDdlTab()));
    connect(ui->moduleCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(updateDdlTab()));

    ui->columnsView->setModel(columnsModel);
    ui->columnsView->verticalHeader()->setDefaultSectionSize(ui->columnsView->fontMetrics().height() + 8);

    parseDdl();

    // (Re)connect to DB signals
    connect(db, SIGNAL(dbObjectDeleted(QString,QString,DbObjectType)), this, SLOT(checkIfTableDeleted(QString,QString,DbObjectType)));
}

void VirtualTableWindow::staticInit()
{
    qRegisterMetaType<VirtualTableWindow>("VirtualTableWindow");
}

bool VirtualTableWindow::isUncommitted() const
{
    // TODO
    return false;
}

QString VirtualTableWindow::getQuitUncommittedConfirmMessage() const
{
    // TODO
    return QString();
}

bool VirtualTableWindow::isWindowClosingBlocked() const
{
    // TODO
    return false;
}

QString VirtualTableWindow::getTable() const
{
    return table;
}

Db* VirtualTableWindow::getDb() const
{
    return db;
}

void VirtualTableWindow::useCurrentTableAsBaseForNew()
{
    newTable();
    ui->tableNameEdit->clear();
    columnsModel->clear();
    ui->shadowTablesView->clear();
    updateWindowTitle();
    ui->tableNameEdit->setFocus();
    updateAfterInit();
}

void VirtualTableWindow::changeEvent(QEvent* e)
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

void VirtualTableWindow::showEvent(QShowEvent* e)
{
    if (!shownAtLEastOnce)
    {
        applyInitialTab();
        shownAtLEastOnce = true;
    }
    QWidget::showEvent(e);
}

void VirtualTableWindow::newTable()
{
    existingTable = false;
    table = "";
}

void VirtualTableWindow::parseDdl()
{
    if (!resolveCreateTableStatement())
        return;

    if (!resolveOriginalCreateTableStatement())
        return;

    originalArgsValue = originalCreateTable->args.join(",\n");

    loadFromStmt();
}

void VirtualTableWindow::loadFromStmt()
{
    ui->tableNameEdit->setText(table);
    ui->argsEdit->setPlainText(createTable->args.join(",\n"));
    ui->moduleCombo->setCurrentText(createTable->module);

    SchemaResolver resolver(db);
    QStringList realColumns = resolver.getColumnsUsingPragma(database, table, true);
    QList<QPair<QString, QString>> colsAndTypes = resolver.getColumnsAndDataTypesUsingPragma(database, table, false);

    columnsModel->setRowCount(0);
    columnsModel->setRowCount(colsAndTypes.size());
    int row = 0;
    for (auto&& colAndType : colsAndTypes)
    {
        columnsModel->setData(columnsModel->index(row, 0), colAndType.first);
        columnsModel->setData(columnsModel->index(row, 1), colAndType.second);

        if (!realColumns.contains(colAndType.first, Qt::CaseInsensitive))
        {
            columnsModel->setData(columnsModel->index(row, 0), ICONS.EYE_CLOSED.toQIcon(), Qt::DecorationRole);
            columnsModel->setData(columnsModel->index(row, 0), static_cast<int>(QStyleOptionViewItem::Right), IconPositionItemDelegate::DecorationPositionRole);
        }

        row++;
    }

    ui->shadowTablesView->clear();
    QStringList shadowTables = resolver.getShadowTablesForVirtualTable(database, table);
    ui->shadowTablesView->addItems(shadowTables);
    for (int row = 0; row < ui->shadowTablesView->count(); row++)
        ui->shadowTablesView->item(row)->setIcon(ICONS.SHADOW_TABLE);

    updateStructureCommitState();
    updateStructureToolbarState();
    updateDdlTab();
}

void VirtualTableWindow::createStructureActions()
{
    createAction(REFRESH_STRUCTURE, ICONS.RELOAD, tr("Refresh structure", "table window"), this, SLOT(refreshStructure()), ui->structureToolBar, ui->structureTab);
    separatorAfterAction[REFRESH_STRUCTURE] = ui->structureToolBar->addSeparator();
    createAction(COMMIT_STRUCTURE, ICONS.COMMIT, tr("Commit structure changes", "table window"), this, SLOT(commitStructure()), ui->structureToolBar, ui->structureTab);
    createAction(ROLLBACK_STRUCTURE, ICONS.ROLLBACK, tr("Rollback structure changes", "table window"), this, SLOT(rollbackStructure()), ui->structureToolBar, ui->structureTab);
    separatorAfterAction[ROLLBACK_STRUCTURE] = ui->structureToolBar->addSeparator();
    createAction(IMPORT, ICONS.TABLE_IMPORT, tr("Import data to the table", "table window"), this, SLOT(importTable()), ui->structureToolBar, ui->structureTab);
    createAction(EXPORT, ICONS.TABLE_EXPORT, tr("Export table", "table window"), this, SLOT(exportTable()), ui->structureToolBar, ui->structureTab);
    createAction(POPULATE, ICONS.TABLE_POPULATE, tr("Populate table", "table window"), this, SLOT(populateTable()), ui->structureToolBar, ui->structureTab);
    separatorAfterAction[POPULATE] = ui->structureToolBar->addSeparator();
    createAction(CREATE_SIMILAR, ICONS.TABLE_CREATE_SIMILAR, tr("Create similar table", "table window"), this, SLOT(createSimilarTable()), ui->structureToolBar);
}

void VirtualTableWindow::createDataGridActions()
{
    QAction* before = ui->dataView->getAction(DataView::FILTER_VALUE);
    ui->dataView->getToolBar(DataView::TOOLBAR_GRID)->insertAction(before, actionMap[IMPORT]);
    ui->dataView->getToolBar(DataView::TOOLBAR_GRID)->insertAction(before, actionMap[EXPORT]);
    ui->dataView->getToolBar(DataView::TOOLBAR_GRID)->insertAction(before, actionMap[POPULATE]);
    ui->dataView->getToolBar(DataView::TOOLBAR_GRID)->insertSeparator(before);
}

void VirtualTableWindow::createDataFormActions()
{
}

void VirtualTableWindow::createDbCombo()
{
    ui->dbCombo->setFixedWidth(100);
    ui->dbCombo->setToolTip(tr("Database"));
    connect(ui->dbCombo, SIGNAL(verifiedDbChanged()), this, SLOT(dbChanged()));
}

void VirtualTableWindow::setupCoverWidget()
{
    widgetCover = new WidgetCover(this);
    widgetCover->initWithInterruptContainer();
    widgetCover->hide();
    connect(widgetCover, SIGNAL(cancelClicked()), structureExecutor, SLOT(interrupt()));
}

bool VirtualTableWindow::resolveCreateTableStatement()
{
    if (existingTable)
    {
        SchemaResolver resolver(db);
        SqliteQueryPtr parsedObject = resolver.getParsedObject(database, table, SchemaResolver::TABLE);
        if (!parsedObject.dynamicCast<SqliteCreateVirtualTable>())
        {
            notifyError(tr("Could not process the %1 virtual table correctly. Unable to open a table window.").arg(table));
            invalid = true;
            return false;
        }

        createTable = parsedObject.dynamicCast<SqliteCreateVirtualTable>();
    }
    else
    {
        createTable = SqliteCreateVirtualTablePtr::create();
        createTable->table = table;
    }
    return true;
}

bool VirtualTableWindow::resolveOriginalCreateTableStatement()
{
    originalCreateTable = SqliteCreateVirtualTablePtr::create(*createTable);
    return true;
}

QVariant VirtualTableWindow::saveSession()
{
    if (!db || !existingTable)
        return QVariant();

    QHash<QString,QVariant> sessionValue;
    sessionValue["table"] = table;
    sessionValue["db"] = db->getName();
    sessionValue["dataView"] = ui->dataView->getSessionValue();
    return sessionValue;
}

bool VirtualTableWindow::restoreSession(const QVariant& sessionValue)
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

Icon* VirtualTableWindow::getIconNameForMdiWindow()
{
    return ICONS.VIRTUAL_TABLE;
}

QString VirtualTableWindow::getTitleForMdiWindow()
{
    QString dbSuffix = (!db ? "" : (" (" + db->getName() + ")"));
    if (existingTable)
        return table + dbSuffix;

    QStringList existingNames = MAINWINDOW->getMdiArea()->getWindowTitles();
    if (existingNames.contains(windowTitle()))
        return windowTitle();

    // Generate new name
    QString title = tr("New virtual table %1").arg(newTableWindowNum++);
    while (existingNames.contains(title))
        title = tr("New virtual table %1").arg(newTableWindowNum++);

    title += dbSuffix;
    return title;
}

void VirtualTableWindow::createActions()
{
    createStructureActions();
    createDataGridActions();
    createDataFormActions();

    createAction(NEXT_TAB, "next tab", this, SLOT(nextTab()), this);
    createAction(PREV_TAB, "prev tab", this, SLOT(prevTab()), this);
}

void VirtualTableWindow::setupDefShortcuts()
{
    // Widget context
    setShortcutContext({
                           COMMIT_STRUCTURE,
                           ROLLBACK_STRUCTURE,
                           REFRESH_STRUCTURE
                       },
                       Qt::WidgetWithChildrenShortcut);

    BIND_SHORTCUTS(VirtualTableWindow, Action);
}

bool VirtualTableWindow::restoreSessionNextTime()
{
    return existingTable && db;
}

QToolBar* VirtualTableWindow::getToolBar(int toolbar) const
{
    switch (static_cast<ToolBar>(toolbar))
    {
        case TOOLBAR_STRUCTURE:
            return ui->structureToolBar;
    }
    return nullptr;
}

int VirtualTableWindow::getDataTabIdx() const
{
    return ui->tabWidget->indexOf(ui->dataTab);
}

int VirtualTableWindow::getStructureTabIdx() const
{
    return ui->tabWidget->indexOf(ui->structureTab);
}

void VirtualTableWindow::updateAfterInit()
{
    updateStructureCommitState();
    updateStructureToolbarState();
    updateNewTableState();
}

bool VirtualTableWindow::isModified() const
{
    return (originalCreateTable &&
                (
                    originalCreateTable->table != ui->tableNameEdit->text() ||
                    originalArgsValue != ui->argsEdit->toPlainText() ||
                    originalCreateTable->module != ui->moduleCombo->currentText()
                )
            ) ||
            (!existingTable && !ui->tableNameEdit->text().isEmpty() && !ui->moduleCombo->currentText().isEmpty());
}

void VirtualTableWindow::defineCurrentContextDb()
{
    ui->dbCombo->setCurrentDb(db);
}

bool VirtualTableWindow::validate(bool skipWarning)
{
    if (!existingTable && !skipWarning && ui->tableNameEdit->text().isEmpty())
    {
        int res = QMessageBox::warning(this, tr("Empty name"), tr("A blank name for the table is allowed in SQLite, but it is not recommended.\n"
            "Are you sure you want to create a table with blank name?"), QMessageBox::Yes, QMessageBox::No);

        if (res != QMessageBox::Yes)
            return false;
    }

    static_qstring(argsValidationTpl, "CREATE VIRTUAL TABLE vt USING modname (%1);");
    Parser parser;
    if (!parser.parse(argsValidationTpl.arg(ui->argsEdit->toPlainText())))
    {
        notifyError(tr("Invalid syntax of module arguments. Details: %1").arg(parser.getErrorString()));
        return false;
    }

    if (existingTable)
    {
        int res = QMessageBox::warning(this, tr("Recreate Virtual Table"), tr("Modifying a virtual table requires dropping and recreating it. This may result in data loss, depending on the virtual table module.\n\n"
            "Do you want to continue?"), QMessageBox::Yes, QMessageBox::No);

        if (res != QMessageBox::Yes)
            return false;
    }

    return true;
}

void VirtualTableWindow::executeStructureChanges()
{
    static_qstring(dropTpl, "DROP TABLE IF EXISTS %1;");
    static_qstring(createTpl, "CREATE VIRTUAL TABLE %1 USING %2 (%3);");

    QString createSql = createTpl.arg(
                wrapObjIfNeeded(ui->tableNameEdit->text()),
                ui->moduleCombo->currentText(),
                ui->argsEdit->toPlainText()
                );

    Parser parser;
    if (!parser.parse(createSql) || parser.getQueries().isEmpty())
    {
        qCritical() << "Failed to parse CREATE VIRTUAL TABLE for committing it:" << createSql << ", error is:" << parser.getErrorString();
        return;
    }

    createTable = parser.getQueries()[0].dynamicCast<SqliteCreateVirtualTable>();
    if (createTable.isNull())
    {
        qCritical() << "Failed to parse CREATE VIRTUAL TABLE for committing it:" << createSql
                    << ". Parsed statement is not SqliteCreateVirtualTable, but instead it's:"
                    << sqliteQueryTypeToString(parser.getQueries()[0]->queryType);
        return;
    }

    QStringList sqls;
    if (existingTable)
        sqls << dropTpl.arg(wrapObjIfNeeded(ui->tableNameEdit->text()));

    sqls << createSql;

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

QString VirtualTableWindow::updateWindowAfterStructureChanged()
{
    originalCreateTable = createTable;
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

void VirtualTableWindow::applyInitialTab()
{
    if (existingTable && !table.isNull() && CFG_UI.General.OpenTablesOnData.get())
        ui->tabWidget->setCurrentIndex(getDataTabIdx());
    else
        ui->tabWidget->setCurrentIndex(getStructureTabIdx());
}

void VirtualTableWindow::updateTabsOrder()
{
    tabsMoving = true;
    ui->tabWidget->removeTab(getDataTabIdx());
    int idx = 1;
    if (CFG_UI.General.DataTabAsFirstInTables.get())
        idx = 0;

    ui->tabWidget->insertTab(idx, ui->dataTab, tr("Data"));
    tabsMoving = false;
}

void VirtualTableWindow::executionSuccessful()
{
    dataLoaded = true;
}

void VirtualTableWindow::executionFailed(const QString& errorText)
{
    notifyError(tr("Could not load data for table %1. Error details: %2").arg(table, errorText));
}

void VirtualTableWindow::nameChanged()
{
    if (!createTable)
        return;

    createTable->table = ui->tableNameEdit->text();
    updateDdlTab();
}

void VirtualTableWindow::exportTable()
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

void VirtualTableWindow::importTable()
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

void VirtualTableWindow::populateTable()
{
    PopulateDialog dialog(this);
    dialog.setDbAndTable(db, table);
    if (dialog.exec() == QDialog::Accepted && dataLoaded)
        ui->dataView->refreshData(false);
}

void VirtualTableWindow::createSimilarTable()
{
    DbObjectDialogs dialog(db);
    dialog.addVirtualTableSimilarTo(QString(), table);
}

void VirtualTableWindow::tabChanged(int newTab)
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

void VirtualTableWindow::updateStructureCommitState()
{
    bool modified = isModified();
    actionMap[COMMIT_STRUCTURE]->setEnabled(modified);
    actionMap[ROLLBACK_STRUCTURE]->setEnabled(modified && existingTable);
}

void VirtualTableWindow::updateStructureToolbarState()
{
}

void VirtualTableWindow::updateNewTableState()
{
    ui->tabWidget->setTabEnabled(getDataTabIdx(), existingTable);
    actionMap[EXPORT]->setEnabled(existingTable);
    actionMap[IMPORT]->setEnabled(existingTable);
    actionMap[POPULATE]->setEnabled(existingTable);
    actionMap[CREATE_SIMILAR]->setEnabled(existingTable);
    actionMap[REFRESH_STRUCTURE]->setEnabled(existingTable);
}

void VirtualTableWindow::updateFont()
{
    QFont f = CFG_UI.Fonts.DataView.get();
    QFontMetrics fm(f);

    ui->columnsView->setFont(f);
    ui->columnsView->horizontalHeader()->setFont(f);
    ui->columnsView->verticalHeader()->setFont(f);
    ui->columnsView->verticalHeader()->setDefaultSectionSize(fm.height() + 4);
}

void VirtualTableWindow::updateDdlTab()
{
    createTable->rebuildTokens();
    QString ddl = LETOS->getCodeFormatter()->format("sql", createTable->detokenize(), db);
    ui->ddlEdit->setPlainText(ddl);
}

bool VirtualTableWindow::commitStructure(bool skipWarning)
{
    if (!isModified())
    {
        qWarning() << "Called VirtualTableWindow::commitStructure(), but isModified() returned false.";
        updateStructureCommitState();
        return false;
    }

    if (!validate(skipWarning))
        return false;

    executeStructureChanges();
    return true;
}

void VirtualTableWindow::changesSuccessfullyCommitted()
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

    ui->dataView->resetSorting();
    if (ui->tabWidget->currentIndex() == getDataTabIdx())
        ui->dataView->refreshData();

    STATUSFIELD->releaseFadeOutFor(this);
}

void VirtualTableWindow::changesFailedToCommit(int errorCode, const QString& errorText)
{
    qDebug() << "VirtualTableWindow::changesFailedToCommit:" << errorCode << errorText;

    modifyingThisTable = false;
    widgetCover->hide();
    notifyError(tr("Could not commit table structure. Error message: %1", "table window").arg(errorText));

    STATUSFIELD->releaseFadeOutFor(this);
}

void VirtualTableWindow::rollbackStructure()
{
    createTable = SqliteCreateVirtualTablePtr::create(*originalCreateTable.data());
    loadFromStmt();
}

void VirtualTableWindow::checkIfTableDeleted(const QString& database, const QString& object, DbObjectType type)
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
        case DbObjectType::TRIGGER:
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

void VirtualTableWindow::nextTab()
{
    int idx = ui->tabWidget->currentIndex();
    idx++;
    ui->tabWidget->setCurrentIndex(idx);
}

void VirtualTableWindow::prevTab()
{
    int idx = ui->tabWidget->currentIndex();
    idx--;
    ui->tabWidget->setCurrentIndex(idx);
}

void VirtualTableWindow::focusStructureTab()
{
    ui->tabWidget->setCurrentIndex(getStructureTabIdx());
}

void VirtualTableWindow::focusDataTab()
{
    ui->tabWidget->setCurrentIndex(getDataTabIdx());
}

void VirtualTableWindow::refreshStructure()
{
    parseDdl();
}

void VirtualTableWindow::dbChanged()
{
    if (db)
        disconnect(db, SIGNAL(dbObjectDeleted(QString,QString,DbObjectType)), this, SLOT(checkIfTableDeleted(QString,QString,DbObjectType)));

    db = ui->dbCombo->currentDb();
    dataModel->setDb(db);

    if (db)
        connect(db, SIGNAL(dbObjectDeleted(QString,QString,DbObjectType)), this, SLOT(checkIfTableDeleted(QString,QString,DbObjectType)));
}
