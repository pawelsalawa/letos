#include "sqltablemodel.h"
#include "common/utils_sql.h"
#include "sqlqueryitem.h"
#include "services/notifymanager.h"
#include "uiconfig.h"
#include <QDebug>
#include <QApplication>
#include <schemaresolver.h>
#include <querygenerator.h>

SqlTableModel::SqlTableModel(QObject *parent) :
    SqlDataSourceQueryModel(parent)
{
}

QString SqlTableModel::getTable() const
{
    return table;
}

void SqlTableModel::setDatabaseAndTable(const QString& database, const QString& table)
{
    this->database = database;
    this->table = table;

    SchemaResolver resolver(db);
    isWithOutRowIdTable = resolver.isWithoutRowIdTable(database, table);

    QStringList cols = resolver.getTableColumns(database, table, false) | MAP(c, {return wrapObjIfNeeded(c);});
    setQuery(QString("SELECT %1 FROM %2").arg(cols.join(", "), getDataSource()));

    updateTablesInUse(table);
}

SqlQueryModel::Features SqlTableModel::features() const
{
    return INSERT_ROW|DELETE_ROW|FILTERING;
}

bool SqlTableModel::commitAddedRowPostprocess(const QList<SqlQueryItem*>& itemsInRow, const QList<QVariant>& rowValues, const RowId& insertRowId, QList<CommitSuccessfulHandler>& successfulCommitHandlers)
{
    // ROWID - either compound ID for WITHOUT ROWID table, or just ROWID for regular table
    RowId rowId;
    if (isWithOutRowIdTable)
    {
        int i = 0;
        for (SqlQueryModelColumnPtr& modelColumn : columns)
        {
            if (modelColumn->isPk())
                rowId[modelColumn->column] = rowValues[i];

            i++;
        }
    }
    else
        rowId = insertRowId;

    // After all items are committed successfully, update data/metadata for inserted rows/items
    successfulCommitHandlers << [this, itemsInRow, rowValues, rowId]()
    {
        int colIdx = 0;
        for (SqlQueryItem* itemToUpdate : itemsInRow)
        {
            updateItem(itemToUpdate, rowValues[colIdx], columns[colIdx], rowId);
            colIdx++;
        }
    };

    return true;
}

bool SqlTableModel::commitDeletedRowPostprocess(const QList<SqlQueryItem*>& itemsInRow, int deletedRowCount, QList<SqlQueryModel::CommitSuccessfulHandler>& successfulCommitHandlers)
{
    Q_UNUSED(itemsInRow);
    Q_UNUSED(deletedRowCount);
    Q_UNUSED(successfulCommitHandlers);
    return true;
}

bool SqlTableModel::supportsModifyingQueriesInMenu() const
{
    return true;
}

RowId SqlTableModel::getRowIdForRow(const QList<SqlQueryItem*>& itemsInRow)
{
    return itemsInRow[0]->getRowId();
}

QString SqlTableModel::generateSelectQueryForItems(const QList<SqlQueryItem*>& items)
{
    QHash<QString, QVariantList> values = toValuesGroupedByColumns(items);

    QueryGenerator generator;
    return generator.generateSelectFromTable(db, database, table, values);
}

QString SqlTableModel::generateInsertQueryForItems(const QList<SqlQueryItem*>& items)
{
    QHash<QString, QVariantList> values = toValuesGroupedByColumns(items);

    QueryGenerator generator;
    return generator.generateInsertToTable(db, database, table, values);
}

QString SqlTableModel::generateUpdateQueryForItems(const QList<SqlQueryItem*>& items)
{
    QHash<QString, QVariantList> values = toValuesGroupedByColumns(items);

    QueryGenerator generator;
    return generator.generateUpdateOfTable(db, database, table, values);
}

QString SqlTableModel::generateDeleteQueryForItems(const QList<SqlQueryItem*>& items)
{
    QHash<QString, QVariantList> values = toValuesGroupedByColumns(items);

    QueryGenerator generator;
    return generator.generateDeleteFromTable(db, database, table, values);
}

QString SqlTableModel::getDataSource()
{
    return getDatabasePrefix() + wrapObjIfNeeded(table);
}

QString SqlTableModel::getTableOrView()
{
    return table;
}
