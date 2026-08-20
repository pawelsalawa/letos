#include "abstracttablewindow.h"
#include "common/dbcombobox.h"
#include "common/widgetcover.h"
#include "db/chainexecutor.h"
#include "datagrid/sqltablemodel.h"
#include "dbtree/dbtree.h"
#include "dialogs/exportdialog.h"
#include "dialogs/importdialog.h"
#include "dialogs/populatedialog.h"
#include "mainwindow.h"
#include "mdiarea.h"
#include "services/config.h"
#include "services/notifymanager.h"
#include "statusfield.h"
#include "uiconfig.h"
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QHeaderView>

AbstractTableWindow::AbstractTableWindow(QWidget* parent) :
    MdiChild(parent)
{
}

AbstractTableWindow::~AbstractTableWindow()
{
}


Db* AbstractTableWindow::getAssociatedDb() const
{
    return db;
}

QPair<Db*, QString> AbstractTableWindow::getSoftDbObjectAssociation() const
{
    if (!existingTable)
        return {db, QString()};

    return {db, table};
}

void AbstractTableWindow::newTable()
{
    existingTable = false;
    table.clear();
}

void AbstractTableWindow::changeEvent(QEvent* e)
{
    QWidget::changeEvent(e);
    switch (e->type())
    {
        case QEvent::LanguageChange:
            commonUi.retranslateUi(this);
            break;
        default:
            break;
    }
}

void AbstractTableWindow::showEvent(QShowEvent* e)
{
    if (!shownAtLEastOnce)
    {
        applyInitialTab();
        shownAtLEastOnce = true;
    }
    QWidget::showEvent(e);
}

void AbstractTableWindow::executionSuccessful()
{
    dataLoaded = true;
}

void AbstractTableWindow::executionFailed(const QString& errorText)
{
    notifyError(tr("Could not load data for table %1. Error details: %2").arg(table, errorText));
}

void AbstractTableWindow::setupCoverWidget()
{
    widgetCover = new WidgetCover(this);
    widgetCover->initWithInterruptContainer();
    widgetCover->hide();
    if (structureExecutor)
        connect(widgetCover, SIGNAL(cancelClicked()), structureExecutor, SLOT(interrupt()));
}

Db* AbstractTableWindow::getDb() const
{
    return db;
}

QString AbstractTableWindow::getTable() const
{
    return table;
}

bool AbstractTableWindow::isUncommitted() const
{
    return commonUi.dataView->isUncommitted() || isModified();
}

QString AbstractTableWindow::getQuitUncommittedConfirmMessage() const
{
    QString title = getMdiWindow()->windowTitle();
    if (commonUi.dataView->isUncommitted() && isModified())
        return tr("Table window \"%1\" has uncommitted structure modifications and data.").arg(title);
    else if (commonUi.dataView->isUncommitted())
        return tr("Table window \"%1\" has uncommitted data.").arg(title);
    else if (isModified())
        return tr("Table window \"%1\" has uncommitted structure modifications.").arg(title);
    else
    {
        qCritical() << "Unhandled message case in AbstractTableWindow::getQuitUncommittedConfirmMessage().";
        return QString();
    }
}

bool AbstractTableWindow::isWindowClosingBlocked() const
{
    return structureExecutor->isExecuting() || dataModel->isExecutionInProgress() ||
        (commonUi.tabWidget->currentIndex() == getDataTabIdx() && !commonUi.dataView->getNavigationState());
}

bool AbstractTableWindow::commitStructure(bool skipWarning)
{
    if (!isModified())
    {
        qWarning() << "Called commitStructure(), but isModified() returned false.";
        updateStructureCommitState();
        return false;
    }

    if (!validate(skipWarning))
        return false;

    executeStructureChanges();
    return true;
}

void AbstractTableWindow::tabChanged(int newTab)
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
            commonUi.dataView->refreshData(false);
    }
}

void AbstractTableWindow::dbChanged()
{
    if (db)
        disconnect(db, SIGNAL(dbObjectDeleted(QString,QString,DbObjectType)), this, SLOT(checkIfTableDeleted(QString,QString,DbObjectType)));

    db = commonUi.dbCombo->currentDb();
    dataModel->setDb(db);

    if (db)
        connect(db, SIGNAL(dbObjectDeleted(QString,QString,DbObjectType)), this, SLOT(checkIfTableDeleted(QString,QString,DbObjectType)));
}

void AbstractTableWindow::changesSuccessfullyCommitted()
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

    changesSuccessfullyCommittedHandleDeps(oldTable);

    commonUi.dataView->resetSorting();
    if (commonUi.tabWidget->currentIndex() == getDataTabIdx())
        commonUi.dataView->refreshData();

    STATUSFIELD->releaseFadeOutFor(this);
}

void AbstractTableWindow::changesSuccessfullyCommittedHandleDeps(const QString& oldTable)
{
}

void AbstractTableWindow::changesFailedToCommit(int errorCode, const QString& errorText)
{
    qDebug() << "AbstractTableWindow::changesFailedToCommit:" << errorCode << errorText;

    modifyingThisTable = false;
    widgetCover->hide();
    notifyError(tr("Could not commit table structure. Error message: %1", "table window").arg(errorText));

    STATUSFIELD->releaseFadeOutFor(this);
}

void AbstractTableWindow::checkIfTableDeleted(const QString& database, const QString& object, DbObjectType type)
{
    if (modifyingThisTable)
        return;

    if (object.compare(table, Qt::CaseInsensitive) == 0)
    {
        dbClosedFinalCleanup();
        MDIAREA->enforceCurrentTaskSelectionAfterWindowClose();
        getMdiWindow()->close();
    }
}

int AbstractTableWindow::getDataTabIdx() const
{
    return commonUi.tabWidget->indexOf(commonUi.dataTab);
}

int AbstractTableWindow::getStructureTabIdx() const
{
    return commonUi.tabWidget->indexOf(commonUi.structureTab);
}

void AbstractTableWindow::initDbCombo()
{
    commonUi.dbCombo->setFixedWidth(100);
    commonUi.dbCombo->setToolTip(tr("Database"));
    connect(commonUi.dbCombo, SIGNAL(verifiedDbChanged()), this, SLOT(dbChanged()));
}

void AbstractTableWindow::applyInitialTab()
{
    if (existingTable && !table.isNull() && CFG_UI.General.OpenTablesOnData.get())
        commonUi.tabWidget->setCurrentIndex(getDataTabIdx());
    else
        commonUi.tabWidget->setCurrentIndex(getStructureTabIdx());
}

QString AbstractTableWindow::getTitleForMdiWindow()
{
    QString dbSuffix = (!db ? "" : (" (" + db->getName() + ")"));
    if (existingTable)
        return table + dbSuffix;

    QStringList existingNames = MAINWINDOW->getMdiArea()->getWindowTitles();
    if (existingNames.contains(windowTitle()))
        return windowTitle();

    // Generate new name
    QString tpl = getTitleTemplateForMdiWindow();
    QString title = tpl.arg(newTableWindowNum++);
    while (existingNames.contains(title))
        title = tpl.arg(newTableWindowNum++);

    title += dbSuffix;
    return title;
}

void AbstractTableWindow::nextTab()
{
    int idx = commonUi.tabWidget->currentIndex();
    idx++;
    commonUi.tabWidget->setCurrentIndex(idx);
}

void AbstractTableWindow::prevTab()
{
    int idx = commonUi.tabWidget->currentIndex();
    idx--;
    commonUi.tabWidget->setCurrentIndex(idx);
}

void AbstractTableWindow::updateTabsOrder()
{
    tabsMoving = true;
    commonUi.tabWidget->removeTab(getDataTabIdx());
    int idx = 1;
    if (CFG_UI.General.DataTabAsFirstInTables.get())
        idx = 0;

    commonUi.tabWidget->insertTab(idx, commonUi.dataTab, tr("Data"));
    tabsMoving = false;
}

void AbstractTableWindow::updateFont()
{
    QFont f = CFG_UI.Fonts.DataView.get();
    QFontMetrics fm(f);

    for (QTableView* view : getViewsForFontUpdate())
    {
        view->setFont(f);
        view->horizontalHeader()->setFont(f);
        view->verticalHeader()->setFont(f);
        view->verticalHeader()->setDefaultSectionSize(fm.height() + 4);
    }
}

void AbstractTableWindow::focusStructureTab()
{
    commonUi.tabWidget->setCurrentIndex(getStructureTabIdx());
}

void AbstractTableWindow::focusDataTab()
{
    commonUi.tabWidget->setCurrentIndex(getDataTabIdx());
}

void AbstractTableWindow::exportTable()
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

void AbstractTableWindow::importTable()
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
        commonUi.dataView->refreshData(false);
}

void AbstractTableWindow::populateTable()
{
    PopulateDialog dialog(this);
    dialog.setDbAndTable(db, table);
    if (dialog.exec() == QDialog::Accepted && dataLoaded)
        commonUi.dataView->refreshData(false);
}
