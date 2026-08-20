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
    AbstractTableWindow(parent),
    ui(new Ui::VirtualTableWindow)
{
}

VirtualTableWindow::VirtualTableWindow(Db* db, QWidget* parent) :
    AbstractTableWindow(parent),
    ui(new Ui::VirtualTableWindow)
{
    this->db = db;

    newTable();
}

VirtualTableWindow::VirtualTableWindow(const VirtualTableWindow& win) :
    AbstractTableWindow(win.parentWidget()),
    ui(new Ui::VirtualTableWindow)
{
    this->db = win.db;
    this->database = win.database;
    this->table = win.table;
}

VirtualTableWindow::VirtualTableWindow(QWidget* parent, Db* db, const QString& database, const QString& table) :
    AbstractTableWindow(parent),
    ui(new Ui::VirtualTableWindow)
{
    this->db = db;
    this->database = database;
    this->table = table;
}

VirtualTableWindow::~VirtualTableWindow()
{
    delete ui;
}

void VirtualTableWindow::init()
{
    ui->setupUi(this);
    ABSTRACT_TABLE_WINDOW_COMMON_UI();

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
    initDbCombo();

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

    initDbAndTable();
    updateAfterInit();
}

void VirtualTableWindow::initDbAndTable()
{
    if (table.isNull())
        return;

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

QString VirtualTableWindow::getTitleTemplateForMdiWindow()
{
    return tr("New virtual table %1");
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

QList<QTableView*> VirtualTableWindow::getViewsForFontUpdate() const
{
    return {ui->columnsView};
}

void VirtualTableWindow::nameChanged()
{
    if (!createTable)
        return;

    createTable->table = ui->tableNameEdit->text();
    updateDdlTab();
}

void VirtualTableWindow::createSimilarTable()
{
    DbObjectDialogs dialog(db);
    dialog.addVirtualTableSimilarTo(QString(), table);
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

void VirtualTableWindow::updateDdlTab()
{
    createTable->rebuildTokens();
    QString ddl = LETOS->getCodeFormatter()->format("sql", createTable->detokenize(), db);
    ui->ddlEdit->setPlainText(ddl);
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

    AbstractTableWindow::checkIfTableDeleted(database, object, type);
}

void VirtualTableWindow::refreshStructure()
{
    parseDdl();
}
