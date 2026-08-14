#include "columndialog.h"
#include "services/notifymanager.h"
#include "ui_columndialog.h"
#include "columndialogconstraintsmodel.h"
#include "iconmanager.h"
#include "newconstraintdialog.h"
#include "dialogs/constraintdialog.h"
#include "constraints/constraintpanel.h"
#include "datatype.h"
#include "uiutils.h"
#include "common/dialogsizehandler.h"
#include "schemaresolver.h"
#include <QDebug>
#include <QCheckBox>
#include <QMessageBox>
#include <QDebug>
#include <QPushButton>

ColumnDialog::ColumnDialog(Db* db, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ColumnDialog),
    db(db)
{
    init();
}

ColumnDialog::~ColumnDialog()
{
    delete ui;
}

void ColumnDialog::init()
{
    ui->setupUi(this);
    limitDialogWidth(this);
    setWindowIcon(ICONS.COLUMN);

    ui->scale->setStrict(true, true);
    ui->precision->setStrict(true, true);

    connect(ui->typeCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(updateDataType()));

    constraintsModel = new ColumnDialogConstraintsModel();
    ui->constraintsView->setModel(constraintsModel);
    initActions();

    setupConstraintCheckBoxes();

    connect(ui->advancedCheck, SIGNAL(toggled(bool)), this, SLOT(switchMode(bool)));

    connect(ui->constraintsView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(updateConstraintsToolbarState()));
    connect(ui->constraintsView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(editConstraint(QModelIndex)));
    connect(constraintsModel, SIGNAL(constraintsChanged()), this, SLOT(updateValidations()));
    connect(constraintsModel, SIGNAL(constraintsChanged()), this, SLOT(updateState()));
    connect(ui->typeCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(updateValidations()));
    connect(ui->scale, SIGNAL(modified()), this, SLOT(updateValidations()));
    connect(ui->precision, SIGNAL(modified()), this, SLOT(updateValidations()));

    connect(ui->pkButton, SIGNAL(clicked()), this, SLOT(configurePk()));
    connect(ui->fkButton, SIGNAL(clicked()), this, SLOT(configureFk()));
    connect(ui->checkButton, SIGNAL(clicked()), this, SLOT(configureCheck()));
    connect(ui->defaultButton, SIGNAL(clicked()), this, SLOT(configureDefault()));
    connect(ui->generatedButton, SIGNAL(clicked()), this, SLOT(configureGenerated()));
    connect(ui->notNullButton, SIGNAL(clicked()), this, SLOT(configureNotNull()));
    connect(ui->collateButton, SIGNAL(clicked()), this, SLOT(configureCollate()));
    connect(ui->uniqueButton, SIGNAL(clicked()), this, SLOT(configureUnique()));

    updateState();
}

void ColumnDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
        case QEvent::LanguageChange:
            ui->retranslateUi(this);
            break;
        default:
            break;
    }
}

void ColumnDialog::createActions()
{
    createAction(ADD_CONSTRAINT, ICONS.COLUMN_CONSTRAINT_ADD, tr("Add constraint", "column dialog"), this, SLOT(addConstraint()), ui->constraintsToolbar);
    createAction(EDIT_CONSTRAINT, ICONS.COLUMN_CONSTRAINT_EDIT, tr("Edit constraint", "column dialog"), this, SLOT(editConstraint()), ui->constraintsToolbar);
    createAction(DEL_CONSTRAINT, ICONS.COLUMN_CONSTRAINT_DEL, tr("Delete constraint", "column dialog"), this, SLOT(delConstraint()), ui->constraintsToolbar);
    createAction(MOVE_CONSTRAINT_UP, ICONS.MOVE_UP, tr("Move constraint up", "column dialog"), this, SLOT(moveConstraintUp()), ui->constraintsToolbar);
    createAction(MOVE_CONSTRAINT_DOWN, ICONS.MOVE_DOWN, tr("Move constraint down", "column dialog"), this, SLOT(moveConstraintDown()), ui->constraintsToolbar);
    ui->constraintsToolbar->addSeparator();
    createAction(ADD_PK, ICONS.CONSTRAINT_PRIMARY_KEY_ADD, tr("Add a primary key", "column dialog"), this, SLOT(addPk()), ui->constraintsToolbar);
    createAction(ADD_FK, ICONS.CONSTRAINT_FOREIGN_KEY_ADD, tr("Add a foreign key", "column dialog"), this, SLOT(addFk()), ui->constraintsToolbar);
    createAction(ADD_UNIQUE, ICONS.CONSTRAINT_UNIQUE_ADD, tr("Add an unique constraint", "column dialog"), this, SLOT(addUnique()), ui->constraintsToolbar);
    createAction(ADD_CHECK, ICONS.CONSTRAINT_CHECK_ADD, tr("Add a check constraint", "column dialog"), this, SLOT(addCheck()), ui->constraintsToolbar);
    createAction(ADD_NOT_NULL, ICONS.CONSTRAINT_NOT_NULL_ADD, tr("Add a not null constraint", "column dialog"), this, SLOT(addNotNull()), ui->constraintsToolbar);
    createAction(ADD_COLLATE, ICONS.CONSTRAINT_COLLATION_ADD, tr("Add a collate constraint", "column dialog"), this, SLOT(addCollate()), ui->constraintsToolbar);
    createAction(ADD_GENERATED, ICONS.CONSTRAINT_GENERATED_ADD, tr("Add a generated value constraint", "column dialog"), this, SLOT(addGenerated()), ui->constraintsToolbar);
    createAction(ADD_DEFAULT, ICONS.CONSTRAINT_DEFAULT_ADD, tr("Add a default constraint", "column dialog"), this, SLOT(addDefault()), ui->constraintsToolbar);
}

void ColumnDialog::setupDefShortcuts()
{
}

void ColumnDialog::showEvent(QShowEvent* e)
{
    adjustSize();
    DialogSizeHandler::applyFor(this, DialogSizeHandler::HORIZONTAL);
}

void ColumnDialog::updateConstraintsToolbarState()
{
    QModelIndex idx = ui->constraintsView->selectionModel()->currentIndex();
    bool hasSelected = idx.isValid();
    bool isFirst = false;
    bool isLast = false;
    if (constraintsModel->rowCount() > 0)
    {
        isFirst = (idx.row() == 0);
        isLast = (idx.row() == (constraintsModel->rowCount() - 1));
    }

    actionMap[EDIT_CONSTRAINT]->setEnabled(hasSelected);
    actionMap[DEL_CONSTRAINT]->setEnabled(hasSelected);
    actionMap[MOVE_CONSTRAINT_UP]->setEnabled(hasSelected && !isFirst);
    actionMap[MOVE_CONSTRAINT_DOWN]->setEnabled(hasSelected && !isLast);
}

void ColumnDialog::updateState()
{
    ui->pkButton->setEnabled(ui->pkCheck->isChecked());
    ui->autoIncrCheck->setEnabled(ui->pkCheck->isChecked());
    ui->fkButton->setEnabled(ui->fkCheck->isChecked());
    ui->uniqueButton->setEnabled(ui->uniqueCheck->isChecked());
    ui->notNullButton->setEnabled(ui->notNullCheck->isChecked());
    ui->checkButton->setEnabled(ui->checkCheck->isChecked());
    ui->collateButton->setEnabled(ui->collateCheck->isChecked());
    ui->generatedButton->setEnabled(ui->generatedCheck->isChecked());
    ui->defaultButton->setEnabled(ui->defaultCheck->isChecked());
    updateConstraintsToolbarState();
}

void ColumnDialog::addConstraint(ConstraintDialog::Constraint mode)
{
    NewConstraintDialog dialog(mode, column.data(), db, this);
    for (ConstraintDialog::Constraint constraint : disabledConstraints)
        dialog.disableMode(constraint);

    if (dialog.exec() != QDialog::Accepted)
        return;

    SqliteStatement* constrStmt = dialog.getConstraint();
    SqliteCreateTable::Column::Constraint* constr = dynamic_cast<SqliteCreateTable::Column::Constraint*>(constrStmt);
    if (!constr)
    {
        qCritical() << "Constraint returned from ConstraintDialog was not of column type, while we're trying to add column constraint.";
        return;
    }

    constraintsModel->appendConstraint(constr);
    ui->constraintsView->resizeColumnToContents(0);
    ui->constraintsView->resizeColumnToContents(1);
    updateTypeForAutoIncr();
}

void ColumnDialog::setupConstraintCheckBoxes()
{
    ui->pkCheck->setIcon(ICONS.CONSTRAINT_PRIMARY_KEY);
    ui->compoundPkCheck->setIcon(ICONS.CONSTRAINT_PRIMARY_KEY);
    ui->fkCheck->setIcon(ICONS.CONSTRAINT_FOREIGN_KEY);
    ui->compoundFkCheck->setIcon(ICONS.CONSTRAINT_FOREIGN_KEY);
    ui->uniqueCheck->setIcon(ICONS.CONSTRAINT_UNIQUE);
    ui->compoundUniqueCheck->setIcon(ICONS.CONSTRAINT_UNIQUE);
    ui->notNullCheck->setIcon(ICONS.CONSTRAINT_NOT_NULL);
    ui->checkCheck->setIcon(ICONS.CONSTRAINT_CHECK);
    ui->collateCheck->setIcon(ICONS.CONSTRAINT_COLLATION);
    ui->generatedCheck->setIcon(ICONS.CONSTRAINT_GENERATED);
    ui->defaultCheck->setIcon(ICONS.CONSTRAINT_DEFAULT);

    connect(ui->pkCheck, SIGNAL(clicked(bool)), this, SLOT(pkToggled(bool)));
    connect(ui->autoIncrCheck, SIGNAL(clicked(bool)), this, SLOT(pkAutoIncrToggled(bool)));
    connect(ui->fkCheck, SIGNAL(clicked(bool)), this, SLOT(fkToggled(bool)));
    connect(ui->uniqueCheck, SIGNAL(clicked(bool)), this, SLOT(uniqueToggled(bool)));
    connect(ui->notNullCheck, SIGNAL(clicked(bool)), this, SLOT(notNullToggled(bool)));
    connect(ui->checkCheck, SIGNAL(clicked(bool)), this, SLOT(checkToggled(bool)));
    connect(ui->collateCheck, SIGNAL(clicked(bool)), this, SLOT(collateToggled(bool)));
    connect(ui->generatedCheck, SIGNAL(clicked(bool)), this, SLOT(generatedToggled(bool)));
    connect(ui->defaultCheck, SIGNAL(clicked(bool)), this, SLOT(defaultToggled(bool)));

    for (QCheckBox* cb : {
         ui->pkCheck,
         ui->fkCheck,
         ui->uniqueCheck,
         ui->notNullCheck,
         ui->checkCheck,
         ui->collateCheck,
         ui->generatedCheck,
         ui->defaultCheck
    })
    {
        connect(cb, SIGNAL(toggled(bool)), this, SLOT(updateState()));
    }
}

void ColumnDialog::addConstraint()
{
    addConstraint(ConstraintDialog::UNKNOWN);
}

void ColumnDialog::editConstraint()
{
    QModelIndex idx = ui->constraintsView->currentIndex();
    editConstraint(idx);
}

void ColumnDialog::delConstraint()
{
    QModelIndex idx = ui->constraintsView->currentIndex();
    delConstraint(idx);
}

void ColumnDialog::editConstraint(const QModelIndex& idx)
{
    if (!idx.isValid())
        return;

    SqliteCreateTable::Column::Constraint* constr = constraintsModel->getConstraint(idx.row());
    editConstraint(constr);
}

void ColumnDialog::editConstraint(SqliteCreateTable::Column::Constraint* constraint)
{
    ConstraintDialog dialog(ConstraintDialog::EDIT, constraint, column.data(), db, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    ui->constraintsView->resizeColumnToContents(0);
    ui->constraintsView->resizeColumnToContents(1);
    updateValidations();
    updateTypeForAutoIncr();
}

void ColumnDialog::delConstraint(const QModelIndex& idx)
{
    if (!idx.isValid())
        return;

    SqliteCreateTable::Column::Constraint* constr = constraintsModel->getConstraint(idx.row());

    QString arg = constr->name.isNull() ? constr->typeString() : constr->name;
    QString msg = tr("Are you sure you want to delete constraint '%1'?", "column dialog").arg(arg);
    int btn = QMessageBox::question(this, tr("Delete constraint", "column dialog"), msg);
    if (btn != QMessageBox::Yes)
        return;

    constraintsModel->delConstraint(idx.row());
}

void ColumnDialog::configureConstraint(SqliteCreateTable::Column::Constraint::Type type)
{
    SqliteCreateTable::Column::Constraint* constraint = column->getConstraint(type);
    if (!constraint)
    {
        qCritical() << "Called ColumnDialog::configureConstraint(), but there's no specified type constraint in the column!";
        return;
    }
    editConstraint(constraint);
}

void ColumnDialog::addEmptyConstraint(SqliteCreateTable::Column::Constraint::Type type)
{
    SqliteCreateTable::Column::Constraint* constr = new SqliteCreateTable::Column::Constraint();
    constr->type = type;
    constraintsModel->appendConstraint(constr);
    constr->rebuildTokens();
}

void ColumnDialog::delAllConstraint(SqliteCreateTable::Column::Constraint::Type type)
{
    SqliteCreateTable::Column::Constraint* constr = nullptr;
    while ((constr = column->getConstraint(type)) != nullptr)
        constraintsModel->delConstraint(constr);
}

void ColumnDialog::constraintToggled(SqliteCreateTable::Column::Constraint::Type type, bool enabled)
{
    if (enabled)
        addEmptyConstraint(type);
    else
        delAllConstraint(type);
}

void ColumnDialog::updateConstraintState(SqliteCreateTable::Column::Constraint* constraint)
{
    QToolButton* toolButton = getToolButtonForConstraint(constraint);
    if (!toolButton)
        return;

    bool result = true;
    ConstraintPanel* panel = ConstraintPanel::produce(constraint);
    QString warning;
    if (!panel)
    {
        qCritical() << "Could not produce ConstraintPanel for constraint validation in ColumnDialog::updateConstraintState().";
    }
    else
    {
        panel->setDb(db);
        panel->setColumnStmt(column.data());
        panel->setConstraint(constraint);
        result = panel->validateOnly();
        warning = panel->validateForWarning();
        delete panel;
    }

    QString errMsg = tr("Correct the constraint's configuration.");
    setValidState(toolButton, result, errMsg);

    if (!result)
    {
        QPushButton* btn = ui->buttonBox->button(QDialogButtonBox::Ok);
        btn->setEnabled(false);
    }
    else if (!warning.isEmpty())
        setValidStateWarning(toolButton, warning);
}

QCheckBox* ColumnDialog::getCheckBoxForConstraint(SqliteCreateTable::Column::Constraint* constraint)
{
    switch (constraint->type)
    {
        case SqliteCreateTable::Column::Constraint::PRIMARY_KEY:
            return ui->pkCheck;
        case SqliteCreateTable::Column::Constraint::NOT_NULL:
            return ui->notNullCheck;
        case SqliteCreateTable::Column::Constraint::UNIQUE:
            return ui->uniqueCheck;
        case SqliteCreateTable::Column::Constraint::CHECK:
            return ui->checkCheck;
        case SqliteCreateTable::Column::Constraint::DEFAULT:
            return ui->defaultCheck;
        case SqliteCreateTable::Column::Constraint::COLLATE:
            return ui->collateCheck;
        case SqliteCreateTable::Column::Constraint::GENERATED:
            return ui->generatedCheck;
        case SqliteCreateTable::Column::Constraint::FOREIGN_KEY:
            return ui->fkCheck;
        case SqliteCreateTable::Column::Constraint::NULL_:
        case SqliteCreateTable::Column::Constraint::NAME_ONLY:
        case SqliteCreateTable::Column::Constraint::DEFERRABLE_ONLY:
            break;
    }
    return nullptr;
}

QToolButton* ColumnDialog::getToolButtonForConstraint(SqliteCreateTable::Column::Constraint* constraint)
{
    switch (constraint->type)
    {
        case SqliteCreateTable::Column::Constraint::PRIMARY_KEY:
            return ui->pkButton;
        case SqliteCreateTable::Column::Constraint::NOT_NULL:
            return ui->notNullButton;
        case SqliteCreateTable::Column::Constraint::UNIQUE:
            return ui->uniqueButton;
        case SqliteCreateTable::Column::Constraint::CHECK:
            return ui->checkButton;
        case SqliteCreateTable::Column::Constraint::DEFAULT:
            return ui->defaultButton;
        case SqliteCreateTable::Column::Constraint::COLLATE:
            return ui->collateButton;
        case SqliteCreateTable::Column::Constraint::GENERATED:
            return ui->generatedButton;
        case SqliteCreateTable::Column::Constraint::FOREIGN_KEY:
            return ui->fkButton;
        case SqliteCreateTable::Column::Constraint::NULL_:
        case SqliteCreateTable::Column::Constraint::NAME_ONLY:
        case SqliteCreateTable::Column::Constraint::DEFERRABLE_ONLY:
            break;
    }
    return nullptr;
}

void ColumnDialog::updateTypeValidations()
{
    QString scaleErrorMsg = tr("Scale is not allowed for INTEGER PRIMARY KEY columns.");
    QString precisionErrorMsg = tr("Precision cannot be defined without the scale.");
    QString typeErrorMsg = tr("Cannot use type other than INTEGER if AUTOINCREMENT is enabled in PRIMARY KEY.");
    QString integerEnforcedMsg = tr("INTEGER type was enforced due to enabled AUTOINCREMENT in PRIMARY KEY.");

    QVariant scale = ui->scale->getValue();
    QVariant precision = ui->precision->getValue();

    bool scaleDefined = !scale.toString().isEmpty();
    bool precisionDefined = !precision.toString().isEmpty();

    bool precisionOk = !(precisionDefined && !scaleDefined);
    bool scaleOk = true;
    bool typeOk = true;

    bool hasPk = column->getConstraint(SqliteCreateTable::Column::Constraint::PRIMARY_KEY) != nullptr;
    bool isInteger = ui->typeCombo->currentText().toUpper() == "INTEGER";
    if (hasPk && isInteger)
    {
        if (scaleDefined)
            scaleOk = false;

        if (precisionDefined)
        {
            precisionOk = false;
            precisionErrorMsg = tr("Precision is not allowed for INTEGER PRIMARY KEY columns.");
        }
    }

    if (!isInteger && hasAutoIncr())
        typeOk = false;

    setValidState(ui->scale, scaleOk, scaleErrorMsg);
    setValidState(ui->precision, precisionOk, precisionErrorMsg);
    setValidState(ui->typeCombo, typeOk, typeErrorMsg);

    if (typeOk && integerTypeEnforced)
    setValidStateTooltip(ui->typeCombo, integerEnforcedMsg);

    if (!scaleOk || !precisionOk || !typeOk)
    {
        QPushButton* btn = ui->buttonBox->button(QDialogButtonBox::Ok);
        btn->setEnabled(false);
    }
}

void ColumnDialog::updateTypeForAutoIncr()
{
    bool hasAuto = hasAutoIncr();
    if (hasAuto && ui->typeCombo->currentText().toUpper() != "INTEGER")
    {
        ui->typeCombo->setCurrentText("INTEGER");
        integerTypeEnforced = true;
    }
    else if (!hasAuto)
        integerTypeEnforced = false;

    updateTypeValidations();
}

bool ColumnDialog::hasAutoIncr() const
{
    for (SqliteCreateTable::Column::Constraint* constr : column->getConstraints(SqliteCreateTable::Column::Constraint::PRIMARY_KEY))
    {
        if (constr->autoincrKw)
            return true;
    }

    return false;
}

void ColumnDialog::validateFkTypeMatch()
{
    QString fkTypeWarningMsg = tr("Referenced column type (%1) is different than type declared in this column. It may cause issues while inserting or updating data.");

    for (SqliteCreateTable::Column::Constraint*& constr : column->getConstraints(SqliteCreateTable::Column::Constraint::FOREIGN_KEY))
    {
        if (!constr->foreignKey || constr->foreignKey->indexedColumns.isEmpty() || constr->foreignKey->foreignTable.isNull())
            continue;

        QString fkTable = constr->foreignKey->foreignTable;
        QString fkColumn = constr->foreignKey->indexedColumns.first()->name;
        if (!fkTableTypesCache.contains(fkTable, Qt::CaseInsensitive))
        {
            SchemaResolver resolver(db);
            fkTableTypesCache[fkTable] = resolver.getTableColumnDataTypesByName(fkTable);;
        }

        StrHash<DataType> fkTypes = fkTableTypesCache.value(fkTable, Qt::CaseInsensitive);
        if (fkTypes.isEmpty())
            continue;

        DataType fkType = fkTypes.value(fkColumn, Qt::CaseInsensitive);
        if (fkType.toString().toLower().trimmed() != ui->typeCombo->currentText().toLower().trimmed())
        {
            auto fkButton = getToolButtonForConstraint(constr);
            if (!fkButton)
                continue;

            if (isValidStateIndicatorVisible(fkButton))
                continue;

            setValidStateWarning(fkButton, fkTypeWarningMsg.arg(fkType.toString()));
            break;
        }
    }
}

void ColumnDialog::moveConstraintUp()
{
    QModelIndex idx = ui->constraintsView->currentIndex();
    if (!idx.isValid())
        return;

    constraintsModel->moveConstraintUp(idx.row());
}

void ColumnDialog::moveConstraintDown()
{
    QModelIndex idx = ui->constraintsView->currentIndex();
    if (!idx.isValid())
        return;

    constraintsModel->moveConstraintDown(idx.row());
}

void ColumnDialog::addPk()
{
    addConstraint(ConstraintDialog::PK);
}

void ColumnDialog::addFk()
{
    addConstraint(ConstraintDialog::FK);
}

void ColumnDialog::addUnique()
{
    addConstraint(ConstraintDialog::UNIQUE);
}

void ColumnDialog::addCheck()
{
    addConstraint(ConstraintDialog::CHECK);
}

void ColumnDialog::addCollate()
{
    addConstraint(ConstraintDialog::COLLATE);
}

void ColumnDialog::addNotNull()
{
    addConstraint(ConstraintDialog::NOTNULL);
}

void ColumnDialog::addGenerated()
{
    addConstraint(ConstraintDialog::GENERATED);
}

void ColumnDialog::addDefault()
{
    addConstraint(ConstraintDialog::DEFAULT);
}

void ColumnDialog::configurePk()
{
    configureConstraint(SqliteCreateTable::Column::Constraint::PRIMARY_KEY);
}

void ColumnDialog::configureFk()
{
    configureConstraint(SqliteCreateTable::Column::Constraint::FOREIGN_KEY);
}

void ColumnDialog::configureUnique()
{
    configureConstraint(SqliteCreateTable::Column::Constraint::UNIQUE);
}

void ColumnDialog::configureCheck()
{
    configureConstraint(SqliteCreateTable::Column::Constraint::CHECK);
}

void ColumnDialog::configureCollate()
{
    configureConstraint(SqliteCreateTable::Column::Constraint::COLLATE);
}

void ColumnDialog::configureNotNull()
{
    configureConstraint(SqliteCreateTable::Column::Constraint::NOT_NULL);
}

void ColumnDialog::configureGenerated()
{
    configureConstraint(SqliteCreateTable::Column::Constraint::GENERATED);
}

void ColumnDialog::configureDefault()
{
    configureConstraint(SqliteCreateTable::Column::Constraint::DEFAULT);
}

void ColumnDialog::pkToggled(bool enabled)
{
    constraintToggled(SqliteCreateTable::Column::Constraint::PRIMARY_KEY, enabled);
    if (!enabled)
        ui->autoIncrCheck->setChecked(false);

    updateTypeForAutoIncr();
}

void ColumnDialog::pkAutoIncrToggled(bool enabled)
{
    SqliteCreateTable::Column::Constraint* constraint = column->getConstraint(SqliteCreateTable::Column::Constraint::PRIMARY_KEY);
    if (!constraint)
    {
        qCritical() << "Called ColumnDialog::pkAutoIncrToggled(), but there's no PK constraint in the column!";
        return;
    }

    constraint->autoincrKw = enabled;
    if (enabled && constraint->sortOrder == SqliteSortOrder::DESC)
        constraint->sortOrder = SqliteSortOrder::null;

    updateValidations();
    updateTypeForAutoIncr();
}

void ColumnDialog::fkToggled(bool enabled)
{
    constraintToggled(SqliteCreateTable::Column::Constraint::FOREIGN_KEY, enabled);
}

void ColumnDialog::uniqueToggled(bool enabled)
{
    constraintToggled(SqliteCreateTable::Column::Constraint::UNIQUE, enabled);
}

void ColumnDialog::checkToggled(bool enabled)
{
    constraintToggled(SqliteCreateTable::Column::Constraint::CHECK, enabled);
}

void ColumnDialog::collateToggled(bool enabled)
{
    constraintToggled(SqliteCreateTable::Column::Constraint::COLLATE, enabled);
}

void ColumnDialog::generatedToggled(bool enabled)
{
    constraintToggled(SqliteCreateTable::Column::Constraint::GENERATED, enabled);
}

void ColumnDialog::notNullToggled(bool enabled)
{
    constraintToggled(SqliteCreateTable::Column::Constraint::NOT_NULL, enabled);
}

void ColumnDialog::defaultToggled(bool enabled)
{
    constraintToggled(SqliteCreateTable::Column::Constraint::DEFAULT, enabled);
}

void ColumnDialog::switchMode(bool advanced)
{
    ui->constraintModesWidget->setCurrentWidget(advanced ? ui->advancedPage : ui->simplePage);
}

void ColumnDialog::updateValidations()
{
    QPushButton* btn = ui->buttonBox->button(QDialogButtonBox::Ok);
    btn->setEnabled(true);

    for (QCheckBox* cb : {
         ui->pkCheck,
         ui->fkCheck,
         ui->uniqueCheck,
         ui->notNullCheck,
         ui->checkCheck,
         ui->collateCheck,
         ui->generatedCheck,
         ui->defaultCheck
    })
    {
        cb->setChecked(false);
    }

    for (QToolButton* tb : {
         ui->pkButton,
         ui->fkButton,
         ui->uniqueButton,
         ui->notNullButton,
         ui->checkButton,
         ui->collateButton,
         ui->generatedButton,
         ui->defaultButton
    })
    {
        setValidState(tb, true);
    }

    for (SqliteCreateTable::Column::Constraint*& constr : column->constraints)
        updateConstraint(constr);

    updateTypeValidations();
    validateFkTypeMatch();
}

void ColumnDialog::updateCompoundConstraints()
{
    SqliteCreateTable* createTable = column->findClosestTypedParentStatement<SqliteCreateTable>();
    if (!createTable)
        return;

    bool isPk = false;
    bool isFk = false;
    bool isUnique = false;
    QList<SqliteCreateTable::Constraint*> tableConstraints = createTable->getTableConstraintsOnColumn(column->name);
    for (auto constr : tableConstraints)
    {
        switch (constr->type)
        {
            case SqliteCreateTable::Constraint::PRIMARY_KEY:
                isPk = true;
                break;
            case SqliteCreateTable::Constraint::UNIQUE:
                isUnique = true;
                break;
            case SqliteCreateTable::Constraint::FOREIGN_KEY:
                isFk = true;
                break;
            case SqliteCreateTable::Constraint::CHECK:
            case SqliteCreateTable::Constraint::NAME_ONLY:
                break;
        }
    }

    ui->compoundPkCheck->setVisible(isPk);
    ui->compoundFkCheck->setVisible(isFk);
    ui->compoundUniqueCheck->setVisible(isUnique);
}

void ColumnDialog::updateConstraint(SqliteCreateTable::Column::Constraint* constraint)
{
    QCheckBox* checkBox = getCheckBoxForConstraint(constraint);
    if (checkBox)
    {
        checkBox->setChecked(true);
        if (constraint->type == SqliteCreateTable::Column::Constraint::PRIMARY_KEY)
            ui->autoIncrCheck->setChecked(constraint->autoincrKw);

        updateConstraintState(constraint);
    }
}

void ColumnDialog::setColumn(SqliteCreateTable::Column* value)
{
    column = SqliteCreateTable::ColumnPtr::create(*value);
    column->setParent(value->parent());
    constraintsModel->setColumn(column.data());

    ui->name->setText(value->name);
    ui->hiddenCheck->setChecked(value->type && value->type->hidden);

    SqliteCreateTable* createTable = dynamic_cast<SqliteCreateTable*>(value->parentStatement());
    if (createTable->strict)
    {
        ui->typeCombo->setEditable(false);
        for (DataType::Enum& type : DataType::getStrictValues())
            ui->typeCombo->addItem(DataType::toString(type));

        ui->scale->setVisible(false);
        ui->precision->setVisible(false);
        ui->sizeLabel->setVisible(false);
        ui->sizeCommaLabel->setVisible(false);

        if (value->type)
        {
            int idx = ui->typeCombo->findText(value->type->name, Qt::MatchFixedString);
            if (idx > -1)
                ui->typeCombo->setCurrentIndex(idx);
            else
                notifyError(tr("Could not match valid STRICT table datatype from declared type: %1.").arg(value->type->name));
        }
    }
    else
    {
        ui->typeCombo->addItem("");
        for (DataType::Enum& type : DataType::getAllTypesForUiDropdown())
            ui->typeCombo->addItem(DataType::toString(type));

        if (value->type)
        {
            ui->typeCombo->setEditText(value->type->name);
            ui->scale->setValue(value->type->scale, false);
            ui->precision->setValue(value->type->precision, false);
        }
    }

    updateValidations();
    updateCompoundConstraints();
}

SqliteCreateTable::Column* ColumnDialog::getModifiedColumn()
{
    column->name = ui->name->text();
    updateDataType();
    column->rebuildTokens();

    return new SqliteCreateTable::Column(*column);
}

QToolBar* ColumnDialog::getToolBar(int toolbar) const
{
    Q_UNUSED(toolbar);
    return nullptr;
}

void ColumnDialog::disableConstraint(ConstraintDialog::Constraint constraint)
{
    disabledConstraints << constraint;
    switch (constraint) {
        case ConstraintDialog::PK:
            ui->pkCheck->setEnabled(false);
            actionMap[ADD_PK]->setEnabled(false);
            break;
        case ConstraintDialog::FK:
            ui->fkCheck->setEnabled(false);
            actionMap[ADD_FK]->setEnabled(false);
            break;
        case ConstraintDialog::UNIQUE:
            ui->uniqueCheck->setEnabled(false);
            actionMap[ADD_UNIQUE]->setEnabled(false);
            break;
        case ConstraintDialog::NOTNULL:
            ui->notNullCheck->setEnabled(false);
            actionMap[ADD_NOT_NULL]->setEnabled(false);
            break;
        case ConstraintDialog::CHECK:
            ui->checkCheck->setEnabled(false);
            actionMap[ADD_CHECK]->setEnabled(false);
            break;
        case ConstraintDialog::COLLATE:
            ui->collateCheck->setEnabled(false);
            actionMap[ADD_COLLATE]->setEnabled(false);
            break;
        case ConstraintDialog::GENERATED:
            ui->generatedCheck->setEnabled(false);
            actionMap[ADD_GENERATED]->setEnabled(false);
            break;
        case ConstraintDialog::DEFAULT:
            ui->defaultCheck->setEnabled(false);
            actionMap[ADD_DEFAULT]->setEnabled(false);
            break;
        case ConstraintDialog::UNKNOWN:
            break;
    }
}

void ColumnDialog::updateDataType()
{
    if (!column)
        return;

    integerTypeEnforced = false;
    QString typeTxt = ui->typeCombo->currentText();
    QString scaleTxt = ui->scale->getValue().toString();
    QString precisionTxt = ui->precision->getValue().toString();
    if (!typeTxt.isEmpty() || ui->hiddenCheck->isChecked())
    {
        if (!column->type)
        {
            column->type = new SqliteColumnType();
            column->type->setParent(column.data());
        }

        column->type->name = typeTxt;
        column->type->hidden = ui->hiddenCheck->isChecked();

        if (!scaleTxt.isEmpty())
            column->type->scale = ui->scale->getValue();
        else
            column->type->scale = QVariant();

        if (!precisionTxt.isEmpty())
            column->type->precision = ui->precision->getValue();
        else
            column->type->precision = QVariant();

        column->type->rebuildTokens();
    }
    else if (column->type) // there was a type, but there's not now
    {
        delete column->type;
        column->type = nullptr;
    }

}
