#include "tablestructuremodel.h"
#include "datagrid/sqlquerymodelcolumn.h"
#include "iconmanager.h"
#include "parser/ast/sqlitecreatetable.h"
#include "uiconfig.h"
#include "common/iconpositionitemdelegate.h"
#include <QFont>
#include <QDebug>
#include <QMimeData>
#include <QApplication>
#include <QStyle>

TableStructureModel::TableStructureModel(QObject *parent) :
    QAbstractTableModel(parent)
{
}

int TableStructureModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    if (createTable.isNull())
        return 0;

    return createTable->columns.size();
}

int TableStructureModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    if (createTable.isNull())
        return 0;

    return 10;
}

QVariant TableStructureModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (createTable.isNull())
        return QVariant();

    if (!isValidColumnIdx(index.column()))
        return QVariant();

    int row = index.row();
    if (createTable->columns.size() <= row)
        return QVariant();

    if (role == Qt::ToolTipRole)
        return getToolTip(row, getHeaderColumn(index.column()));

    switch (getHeaderColumn(index.column()))
    {
        case TableStructureModel::Columns::NAME:
        {
            if (role == IconPositionItemDelegate::DecorationPositionRole)
                return static_cast<int>(QStyleOptionViewItem::Right);

            if (role == Qt::DecorationRole && isColumnHidden(row))
                return ICONS.EYE_CLOSED;

            if (role != Qt::DisplayRole)
                break;

            return getColumnName(row);
        }
        case TableStructureModel::Columns::TYPE:
        {
            if (role != Qt::DisplayRole)
                break;

            return getColumnType(row);
        }
        case TableStructureModel::Columns::PK:
        {
            if (role != Qt::DecorationRole)
                break;

            return getColumnPk(row);
        }
        case TableStructureModel::Columns::FK:
        {
            if (role != Qt::DecorationRole)
                break;

            return getColumnFk(row);
        }
        case TableStructureModel::Columns::UNIQUE:
        {
            if (role != Qt::DecorationRole)
                break;

            return getColumnUnique(row);
        }
        case TableStructureModel::Columns::CHECK:
        {
            if (role != Qt::DecorationRole)
                break;

            return getColumnCheck(row);
        }
        case TableStructureModel::Columns::NOTNULL:
        {
            if (role != Qt::DecorationRole)
                break;

            return getColumnNotNull(row);
        }
        case TableStructureModel::Columns::COLLATE:
        {
            if (role != Qt::DecorationRole)
                break;

            return getColumnCollate(row);
        }
        case TableStructureModel::Columns::GENERATED:
        {
            if (role != Qt::DecorationRole)
                break;

            return getColumnGenerate(row);
        }
        case TableStructureModel::Columns::DEFAULT:
        {
            if (role == Qt::FontRole)
                return getColumnDefaultFont(row);

            if (role == Qt::ForegroundRole)
                return getColumnDefaultColor(row);

            if (role == Qt::DisplayRole)
                return getColumnDefaultValue(row);

            break;
        }
    }
    return QVariant();
}

QVariant TableStructureModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (orientation == Qt::Vertical)
            return section + 1;

        // Now it's horizontal orientation with DisplayRole
        return columnLabel(section);
    }

    return QAbstractTableModel::headerData(section, orientation, role);
}

TableStructureModel::Columns TableStructureModel::getHeaderColumn(int colIdx) const
{
    return static_cast<Columns>(colIdx);
}

bool TableStructureModel::isValidColumnIdx(int colIdx) const
{
    return colIdx >= 0 && colIdx < 10;
}

bool TableStructureModel::isColumnHidden(int row) const
{
    SqliteColumnType* type = getColumn(row)->type;
    return type && type->hidden;
}

SqliteCreateTable::Column* TableStructureModel::getColumn(int colIdx) const
{
    if (createTable.isNull())
        return nullptr;

    return createTable->columns[colIdx];
}

void TableStructureModel::replaceColumn(int colIdx, SqliteCreateTable::Column* column)
{
    if (createTable.isNull())
        return;

    SqliteCreateTable::Column* oldColumn = createTable->columns[colIdx];
    QString oldColumnName = oldColumn->name;

    delete oldColumn;
    createTable->columns[colIdx] = column;
    column->setParent(createTable);
    modified = true;

    emit modifiyStateChanged();
    emit dataChanged(createIndex(colIdx, 0), createIndex(colIdx, columnCount()-1));
    emit columnModified(oldColumnName, column);
}

void TableStructureModel::insertColumn(int colIdx, SqliteCreateTable::Column* column)
{
    if (createTable.isNull())
        return;

    beginInsertRows(QModelIndex(), colIdx, colIdx);
    createTable->columns.insert(colIdx, column);
    column->setParent(createTable);
    endInsertRows();

    modified = true;
    emit modifiyStateChanged();
}

void TableStructureModel::appendColumn(SqliteCreateTable::Column* column)
{
    if (createTable.isNull())
        return;

    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    createTable->columns.append(column);
    column->setParent(createTable);
    endInsertRows();

    modified = true;
    emit modifiyStateChanged();
}

void TableStructureModel::delColumn(int colIdx)
{
    if (createTable.isNull())
        return;

    QString name = createTable->columns[colIdx]->name;

    beginRemoveRows(QModelIndex(), colIdx, colIdx);
    delete createTable->columns[colIdx];
    createTable->columns.removeAt(colIdx);
    endRemoveRows();

    modified = true;
    emit modifiyStateChanged();
    emit columnDeleted(name);
}

void TableStructureModel::renameColumn(int colIdx, const QString& newName)
{
    if (createTable.isNull())
        return;

    SqliteCreateTable::Column* column = createTable->columns[colIdx];
    QString oldColumnName = column->name;
    column->name = newName;
    modified = true;

    QModelIndex idx = createIndex(colIdx, getHeaderColumnIdx(Columns::NAME));
    emit modifiyStateChanged();
    emit dataChanged(idx, idx);
    emit columnModified(oldColumnName, column);
}

void TableStructureModel::moveColumnUp(int colIdx)
{
    moveColumnTo(colIdx, colIdx-1);
}

void TableStructureModel::moveColumnDown(int colIdx)
{
    moveColumnTo(colIdx, colIdx+1);
}

void TableStructureModel::moveColumnTo(int colIdx, int newIdx)
{
    if (createTable.isNull())
        return;

    if (newIdx == colIdx)
        return;

    int totalCols = createTable->columns.size();
    if (colIdx + 1 == totalCols && newIdx == totalCols) // Moving last column out of range? Nothing to do.
        return;

    if (newIdx == colIdx+1)
    {
        // From Qt docs: "you must ensure that the destinationChild is not within the range of sourceFirst and sourceLast + 1".
        // So in this case - which is easy to handle - we will invert operation. We will move target index one level up,
        // instead of moving source index down.
        int tmpIdx = newIdx;
        newIdx = colIdx;
        colIdx = tmpIdx;
    }

    beginMoveRows(QModelIndex(), colIdx, colIdx, QModelIndex(), newIdx);
    if (newIdx >= totalCols)
    {
        SqliteCreateTable::Column* col = createTable->columns.takeAt(colIdx);
        createTable->columns.append(col);
    }
    else
        createTable->columns.move(colIdx, newIdx);

    endMoveRows();

    modified = true;
    emit modifiyStateChanged();
    emit columnsOrderChanged();
}

QModelIndex TableStructureModel::findColumn(const QString& columnName, Qt::CaseSensitivity cs) const
{
    int row = 0;
    for (SqliteCreateTable::Column* col : createTable->columns)
    {
        if (col->name.compare(columnName, cs) == 0)
            return createIndex(row, 0);

        row++;
    }
    return QModelIndex();
}

bool TableStructureModel::hasTableStructureItem(const QMimeData* data)
{
    return data && data->hasFormat(mimeType);
}

int TableStructureModel::getHeaderColumnIdx(Columns headerColumn) const
{
    return static_cast<int>(headerColumn);
}

QString TableStructureModel::columnLabel(int column) const
{
    switch (getHeaderColumn(column))
    {
        case Columns::NAME:
            return tr("Name", "table structure columns");
        case Columns::TYPE:
            return tr("Data type", "table structure columns");
        case Columns::PK:
            return tr("Primary\nKey", "table structure columns");
        case Columns::FK:
            return tr("Foreign\nKey", "table structure columns");
        case Columns::UNIQUE:
            return tr("Unique", "table structure columns");
        case Columns::CHECK:
            return tr("Check", "table structure columns");
        case Columns::NOTNULL:
            return tr("Not\nNULL", "table structure columns");
        case Columns::COLLATE:
            return tr("Collate", "table structure columns");
        case Columns::GENERATED:
            return tr("Generated", "table structure columns");
        case Columns::DEFAULT:
            return tr("Default value", "table structure columns");
    }
    return QString();
}

QVariant TableStructureModel::getColumnName(int row) const
{
    return getColumn(row)->name;
}

QVariant TableStructureModel::getColumnType(int row) const
{
    SqliteColumnType* type = getColumn(row)->type;
    return type ? type->detokenizeWithoutHidden() : "";
}

QVariant TableStructureModel::getColumnPk(int row) const
{
    if (isColumnPk(getColumn(row)))
        return ICONS.CONSTRAINT_PRIMARY_KEY;

    return QVariant();
}

QVariant TableStructureModel::getColumnFk(int row) const
{
    if (isColumnFk(getColumn(row)))
        return ICONS.CONSTRAINT_FOREIGN_KEY;

    return QVariant();
}

QVariant TableStructureModel::getColumnUnique(int row) const
{
    if (isColumnUnique(getColumn(row)))
        return ICONS.CONSTRAINT_UNIQUE;

    return QVariant();
}

QVariant TableStructureModel::getColumnCheck(int row) const
{
    if (isColumnCheck(getColumn(row)))
        return ICONS.CONSTRAINT_CHECK;

    return QVariant();
}

QVariant TableStructureModel::getColumnNotNull(int row) const
{
    if (isColumnNotNull(getColumn(row)))
        return ICONS.CONSTRAINT_NOT_NULL;

    return QVariant();
}

QVariant TableStructureModel::getColumnCollate(int row) const
{
    if (isColumnCollate(getColumn(row)))
        return ICONS.CONSTRAINT_COLLATION;

    return QVariant();
}

QVariant TableStructureModel::getColumnGenerate(int row) const
{
    SqliteCreateTable::Column* column = getColumn(row);
    SqliteCreateTable::Column::Constraint* constr = column->getConstraint(SqliteCreateTable::Column::Constraint::GENERATED);
    if (!constr)
        return QVariant();

    switch (constr->generatedType) {
        case SqliteCreateTable::Column::Constraint::GeneratedType::STORED:
            return ICONS.CONSTRAINT_GENERATED_STORED;
        case SqliteCreateTable::Column::Constraint::GeneratedType::VIRTUAL:
        case SqliteCreateTable::Column::Constraint::GeneratedType::null:
            break;
    }
    return ICONS.CONSTRAINT_GENERATED_VIRTUAL;
}

QVariant TableStructureModel::getColumnDefaultValue(int row) const
{
    QVariant value = getColumnDefault(row);
    if (isNull(value))
        return "NULL";

    return value;
}

QVariant TableStructureModel::getColumnDefaultFont(int row) const
{
    QVariant value = getColumnDefault(row);
    if (isNull(value))
    {
        QFont font;
        font.setItalic(true);
        return font;
    }
    return QVariant();
}

QVariant TableStructureModel::getColumnDefaultColor(int row) const
{
    QVariant value = getColumnDefault(row);
    if (isNull(value))
        return Cfg::getSyntaxCommentFormat().foreground().color();

    return QVariant();
}

QVariant TableStructureModel::getColumnDefault(int row) const
{
    SqliteCreateTable::Column::Constraint* constr = getColumn(row)->getConstraint(SqliteCreateTable::Column::Constraint::DEFAULT);
    if (!constr)
        return QVariant();

    if (!constr->id.isNull())
        return constr->id;
    else if (!isNull(constr->literalValue))
        return constr->literalValue;
    else if (!constr->ctime.isNull())
        return constr->ctime;
    else if (constr->expr)
        return constr->expr->detokenize();
    else
        return QVariant();
}

bool TableStructureModel::isColumnPk(SqliteCreateTable::Column* column) const
{
    if (column->hasConstraint(SqliteCreateTable::Column::Constraint::PRIMARY_KEY))
        return true;

    QList<SqliteCreateTable::Constraint*> constraints = createTable->getConstraints(SqliteCreateTable::Constraint::PRIMARY_KEY);
    for (SqliteCreateTable::Constraint* constr : constraints)
        if (constr->doesAffectColumn(column->name))
            return true;

    return false;
}

bool TableStructureModel::isColumnFk(SqliteCreateTable::Column* column) const
{
    if (column->hasConstraint(SqliteCreateTable::Column::Constraint::FOREIGN_KEY))
        return true;

    QList<SqliteCreateTable::Constraint*> constraints = createTable->getConstraints(SqliteCreateTable::Constraint::FOREIGN_KEY);
    for (SqliteCreateTable::Constraint* constr : constraints)
        if (constr->doesAffectColumn(column->name))
            return true;

    return false;
}

bool TableStructureModel::isColumnUnique(SqliteCreateTable::Column* column) const
{
    if (column->hasConstraint(SqliteCreateTable::Column::Constraint::UNIQUE))
        return true;

    QList<SqliteCreateTable::Constraint*> constraints = createTable->getConstraints(SqliteCreateTable::Constraint::UNIQUE);
    for (SqliteCreateTable::Constraint* constr : constraints)
        if (constr->doesAffectColumn(column->name))
            return true;

    return false;
}

bool TableStructureModel::isColumnCheck(SqliteCreateTable::Column* column) const
{
    if (column->hasConstraint(SqliteCreateTable::Column::Constraint::CHECK))
        return true;

    QList<SqliteCreateTable::Constraint*> constraints = createTable->getConstraints(SqliteCreateTable::Constraint::CHECK);
    for (SqliteCreateTable::Constraint* constr : constraints)
        if (constr->expr->getContextColumns(false).contains(column->name, Qt::CaseInsensitive))
            return true;

    return false;
}

bool TableStructureModel::isColumnNotNull(SqliteCreateTable::Column* column) const
{
    if (column->hasConstraint(SqliteCreateTable::Column::Constraint::NOT_NULL))
        return true;

    return false;
}

bool TableStructureModel::isColumnCollate(SqliteCreateTable::Column* column) const
{
    if (column->hasConstraint(SqliteCreateTable::Column::Constraint::COLLATE))
        return true;

    return false;
}

bool TableStructureModel::isColumnGenerate(SqliteCreateTable::Column* column) const
{
    if (column->hasConstraint(SqliteCreateTable::Column::Constraint::GENERATED))
        return true;

    return false;
}

QString TableStructureModel::getToolTip(int row, Columns modelColumn) const
{
    static_qstring(tooltipTpl, R"(<table><tr><td width=16><img src="%1" width="16" height="16"/></td><td style="white-space: pre"><b>%2</b></td><td>%3</td></tr></table>)");

    if (row >= createTable->columns.size())
        return QString();

    SqliteCreateTable::Column* col = createTable->columns[row];

    if (modelColumn == Columns::NAME && isColumnHidden(row))
        return tooltipTpl.arg(ICONS.EYE_CLOSED.toUrl(), "", tr("This is a hidden column."));

    if (col->constraints.isEmpty() && createTable->constraints.isEmpty())
        return QString();

    SqliteCreateTable::Column::Constraint::Type lookFor;
    SqliteCreateTable::Constraint::Type lookForTableType;
    bool tableConstrDefined = false;
    switch (modelColumn)
    {
        case Columns::PK:
            lookFor = SqliteCreateTable::Column::Constraint::Type::PRIMARY_KEY;
            lookForTableType = SqliteCreateTable::Constraint::Type::PRIMARY_KEY;
            tableConstrDefined = true;
            break;
        case Columns::FK:
            lookFor = SqliteCreateTable::Column::Constraint::Type::FOREIGN_KEY;
            lookForTableType = SqliteCreateTable::Constraint::Type::FOREIGN_KEY;
            tableConstrDefined = true;
            break;
        case Columns::UNIQUE:
            lookFor = SqliteCreateTable::Column::Constraint::Type::UNIQUE;
            lookForTableType = SqliteCreateTable::Constraint::Type::UNIQUE;
            tableConstrDefined = true;
            break;
        case Columns::CHECK:
            lookFor = SqliteCreateTable::Column::Constraint::Type::CHECK;
            // Not defined for Table-level constraint, because it cannot (easily) determin if it's affected by table CHECK.
            break;
        case Columns::NOTNULL:
            lookFor = SqliteCreateTable::Column::Constraint::Type::NOT_NULL;
            break;
        case Columns::COLLATE:
            lookFor = SqliteCreateTable::Column::Constraint::Type::COLLATE;
            break;
        case Columns::GENERATED:
            lookFor = SqliteCreateTable::Column::Constraint::Type::GENERATED;
            break;
        case Columns::DEFAULT:
            lookFor = SqliteCreateTable::Column::Constraint::Type::DEFAULT;
            break;
        case Columns::NAME:
        case Columns::TYPE:
            return QString();
    }

    SqliteCreateTable::Column::Constraint* constraint = col->constraints |
                                                        FIND_FIRST(constr, {return constr->type == lookFor;});

    SqliteCreateTable::Constraint* tableConstraint = nullptr;
    if (tableConstrDefined)
    {
        tableConstraint = createTable->constraints |
                          FIND_FIRST(constr, {return constr->type == lookForTableType;});
    }

    if (!constraint && !tableConstraint)
        return QString();

    if (constraint)
    {
        SqlQueryModelColumn::Constraint* constr = SqlQueryModelColumn::Constraint::create(constraint);
        QString tooltip = tooltipTpl.arg(constr->getIcon()->toUrl(), constr->getTypeString(), constr->getDetails());
        delete constr;
        return tooltip;
    }

    SqlQueryModelColumn::Constraint* constr = SqlQueryModelColumn::Constraint::create(createTable->columns[row]->name, tableConstraint);
    if (!constr)
        return QString();

    QString tooltip = tooltipTpl.arg(constr->getIcon()->toUrl(), constr->getTypeString(), constr->getDetails());
    delete constr;
    return tooltip;
}

void TableStructureModel::setCreateTable(SqliteCreateTable* value)
{
    beginResetModel();
    createTable = value;
    endResetModel();

    modified = false;
    emit modifiyStateChanged();
}

bool TableStructureModel::isModified() const
{
    return modified;
}

Qt::DropActions TableStructureModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

Qt::DropActions TableStructureModel::supportedDragActions() const
{
    return Qt::CopyAction|Qt::MoveAction;
}


QStringList TableStructureModel::mimeTypes() const
{
    return {mimeType};
}

QMimeData* TableStructureModel::mimeData(const QModelIndexList& indexes) const
{
    if (indexes.size() < 1)
        return nullptr;

    QModelIndex idx = indexes.first();

    QMimeData *data = new QMimeData();

    QByteArray output;
    QDataStream stream(&output, QIODevice::WriteOnly);

    stream << idx.row();
    data->setData(mimeType, output);

    return data;
}


bool TableStructureModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(action);
    Q_UNUSED(row);
    Q_UNUSED(column);
    Q_UNUSED(parent);

    if (!data)
        return false;

    if (!data->hasFormat(mimeType))
        return false;

    return true;
}

bool TableStructureModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
    Q_UNUSED(column);

    if (action == Qt::IgnoreAction)
        return true;

    if (!data)
        return false;

    if (!data->hasFormat(mimeType))
        return false;

    if (action != Qt::MoveAction)
        return false;

    if (row < 0)
    {
        if (!parent.isValid() && !createTable.isNull())
            row = createTable->columns.size();
        else
            row = parent.row();
    }

    if (row < 0)
        return false;

    QByteArray byteData = data->data(mimeType);
    QDataStream stream(&byteData, QIODevice::ReadOnly);
    int oldRow;
    stream >> oldRow;

    moveColumnTo(oldRow, row);
    return true;
}

Qt::ItemFlags TableStructureModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags defFlags = QAbstractItemModel::flags(index);
    if (!index.isValid())
        return defFlags|Qt::ItemIsDropEnabled;

    return defFlags|Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled;
}
