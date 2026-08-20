#include "erdtablewindow.h"
#include "scene/erdentity.h"
#include "ui_tablewindow.h"
#include "changes/erdchangemodifyentity.h"
#include "changes/erdchangenewentity.h"
#include "windows/tablestructuremodel.h"
#include "services/notifymanager.h"
#include "mainwindow.h"
#include "statusfield.h"
#include <QMessageBox>
#include <QTimer>
#include <QPushButton>

ErdTableWindow::ErdTableWindow(Db* db, ErdEntity* entity, QWidget* parent)
    : TableWindow(parent, db, QString(), entity->getTableName(), entity->isExistingTable()),
      entity(entity)
{
}

ErdTableWindow::~ErdTableWindow()
{
}

QString ErdTableWindow::getQuitUncommittedConfirmMessage() const
{
    if (isModified())
        return tr("ERD side panel for table \"%1\" has uncommitted modifications.").arg(entity->getTableName());

    qCritical() << "Unhandled message case in ErdTableWindow::getQuitUncommittedConfirmMessage().";
    return QString();
}

bool ErdTableWindow::commitErdChange()
{
    if (entity->isBeingDeleted())
        return true;

    return commitStructure(true);
}

void ErdTableWindow::abortErdChange()
{
    rollbackStructure();
}

bool ErdTableWindow::editColumn(const QString& columnName)
{
    QModelIndex idx = structureModel->findColumn(columnName);
    return TableWindow::editColumn(idx);
}

bool ErdTableWindow::resolveOriginalCreateTableStatement()
{
    bool res = TableWindow::resolveOriginalCreateTableStatement();
    if (res)
        originalContent = originalCreateTable->produceTokens().detokenize();

    return res;
}

bool ErdTableWindow::resolveCreateTableStatement()
{
    createTable = SqliteCreateTablePtr::create(*entity->getTableModel());
    return true;
}

void ErdTableWindow::applyInitialTab()
{
    ui->tabWidget->setCurrentIndex(getStructureTabIdx());
}

void ErdTableWindow::defineCurrentContextDb()
{
    // No-op, as we use in-memory db for this window and it's not on the dropdown list.
    // The dropdown list is hidden anyway.
}

bool ErdTableWindow::handleFailedStructureChanges(bool skipWarning)
{
    Q_UNUSED(skipWarning);

    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle(tr("Invalid table changes", "ERD editor"));
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(tr("<b>The table contains invalid changes</b>", "ERD editor"));
    msgBox.setInformativeText(tr(
                                  "Some of the changes you made cannot be applied because they contain errors.<br><br>"
                                  "<b>Errors:</b><br>"
                                  "<code>%1</code><br><br>"
                                  "You can <b>return to editing</b> and fix the problems, "
                                  "or <b>discard your changes</b> and restore the previous state of the table.",
                                  "ERD editor"
                                 ).arg(recordedErrors.join("\n")));

    auto* fixBtn = msgBox.addButton(tr("Fix errors", "ERD editor"), QMessageBox::AcceptRole);
    msgBox.addButton(QMessageBox::Discard);
    msgBox.setDefaultButton(fixBtn);
    msgBox.setEscapeButton(fixBtn);
    msgBox.exec();

    if (msgBox.clickedButton() == fixBtn)
    {
        emit requestReEditForEntity(entity);
        return false;
    }

    if (entity->isExistingTable())
    {
        entity->modelUpdated();
        return true;
    }

    emit editedEntityShouldBeDeleted(entity);
    return true;
}

void ErdTableWindow::setErrorRecording(bool enabled)
{
    if (enabled)
    {
        recordedErrors.clear();
        connect(NOTIFY_MANAGER, SIGNAL(notifyError(QString)), this, SLOT(errorRecorded(QString)));
        MAINWINDOW->getStatusField()->suspend();
    }
    else
    {
        MAINWINDOW->getStatusField()->resume();
        disconnect(NOTIFY_MANAGER, SIGNAL(notifyError(QString)), this, SLOT(errorRecorded(QString)));
    }
}

ErdEntity* ErdTableWindow::getEntity() const
{
    return entity;
}

void ErdTableWindow::init()
{
    TableWindow::init();

    ui->dbCombo->setEnabled(false);
    ui->dbCombo->setVisible(false);
    ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(ui->dataTab), false);
    ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(ui->indexesTab), false);
    ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(ui->triggersTab), false);

    ui->structureToolBar->removeAction(actionMap[REFRESH_STRUCTURE]);
    ui->structureToolBar->removeAction(actionMap[CREATE_SIMILAR]);
    ui->structureToolBar->removeAction(actionMap[RESET_AUTOINCREMENT]);
    ui->structureToolBar->removeAction(separatorAfterAction[MOVE_COLUMN_DOWN]);

    QString commitText = tr("Apply changes to diagram", "ERD editor");
    QString cancelText = tr("Abort changes", "ERD editor");
    actionMap[COMMIT_STRUCTURE]->setText(commitText);
    actionMap[COMMIT_STRUCTURE]->setToolTip(commitText);
    actionMap[ROLLBACK_STRUCTURE]->setText(cancelText);
    actionMap[ROLLBACK_STRUCTURE]->setToolTip(cancelText);

    disableCommitOnTabChange = true;}

void ErdTableWindow::nameEditedInline(const QString& newName)
{
    createTable->table = newName;
    ui->tableNameEdit->setText(newName);
    updateStructureCommitState();
    updateDdlTab();
}

void ErdTableWindow::columnEditedInline(int columnIdx, const QString& newName)
{
    while (createTable->columns.size() <= columnIdx)
    {
        SqliteCreateTable::Column* column = new SqliteCreateTable::Column("", nullptr, {});
        structureModel->appendColumn(column);
    }

    structureModel->renameColumn(columnIdx, newName);
    resizeStructureViewColumns();
}

void ErdTableWindow::columnDeletedInline(int columnIdx)
{
    if (createTable->columns.size() <= columnIdx)
    {
        qCritical() << "Column indexed" << columnIdx << "of entity" << entity->getTableName()
                    << "deleted inline, but this index is out of range for ErdTableWindow, which has"
                    << createTable->columns.size() << "columns.";
        return;
    }

    structureModel->delColumn(columnIdx);
    resizeStructureViewColumns();
}

void ErdTableWindow::changesSuccessfullyCommitted()
{
    modifyingThisTable = false;
    updateWindowAfterStructureChanged();
}

bool ErdTableWindow::commitStructure(bool skipWarning)
{
    entity->stopInlineEditing();
    if (!isModified())
        return true;

    if (entity->isExistingTable())
    {
        QString newContent = createTable->produceTokens().detokenize();
        if (originalContent == newContent)
            return true;
    }

    setErrorRecording(true);
    bool result = TableWindow::commitStructure(skipWarning);
    result &= structureExecutor->getSuccessfulExecution();
    setErrorRecording(false);
    if (!result)
        return handleFailedStructureChanges(skipWarning);

    return true;
}

void ErdTableWindow::rollbackStructure()
{
    TableWindow::rollbackStructure();
    entity->modelUpdated();
}

void ErdTableWindow::executeStructureChanges()
{
    ErdChange* change;
    if (entity->isExistingTable())
    {
        QString desc = ErdChangeModifyEntity::defaultDescription(originalCreateTable->table);
        change = new ErdChangeModifyEntity(db, originalCreateTable, createTable, desc);
    }
    else
    {
        QString desc = ErdChangeNewEntity::defaultDescription(createTable->table);
        change = new ErdChangeNewEntity(db, originalCreateTable->table, createTable, entity->pos(), desc);
    }

    structureExecutor->setTransaction(false);
    structureExecutor->setAsync(false);
    structureExecutor->setDb(db);
    structureExecutor->setQueries(change->toDdl());
    structureExecutor->setRollbackOnErrorTo(change->getTransactionId());
    structureExecutor->setDisableForeignKeys(true);
    structureExecutor->setDisableObjectDropsDetection(true);
    structureExecutor->exec();

    if (structureExecutor->getSuccessfulExecution())
        emit changeCreated(change);
    else
        qWarning() << "Failed to execute entity changes on in-memory database:" << structureExecutor->getErrorsMessages();
}

void ErdTableWindow::errorRecorded(const QString& msg)
{
    recordedErrors << msg;
}
