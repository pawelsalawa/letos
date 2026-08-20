#include "sqldatasourcequerymodel.h"
#include "common/utils_sql.h"
#include "services/notifymanager.h"
#include "querygenerator.h"

SqlDataSourceQueryModel::SqlDataSourceQueryModel(QObject *parent) :
    SqlQueryModel(parent)
{
}

QString SqlDataSourceQueryModel::getDatabase() const
{
    return database;
}

SqlQueryModel::Features SqlDataSourceQueryModel::features() const
{
    return FILTERING;
}

void SqlDataSourceQueryModel::updateTablesInUse(const QString& inUse)
{
    QString dbName = database;
    if (database.toLower() == "main" || database.isEmpty())
        dbName = QString();

    tablesInUse.clear();
    tablesInUse << DbAndTable(db, dbName, inUse);
}

void SqlDataSourceQueryModel::applyFilter(const QString& value, FilterValueProcessor valueProc)
{
    if (value.isEmpty())
    {
        resetFilter();
        return;
    }

    QStringList conditions;
    for (SqlQueryModelColumnPtr& column : columns)
        conditions << wrapObjIfNeeded(column->getAliasedName())+" "+valueProc(value);

    queryExecutor->setFilters(conditions.join(" OR "));
    executeQuery(true);
}

void SqlDataSourceQueryModel::applyFilter(const QStringList& values, FilterValueProcessor valueProc)
{
    if (values.isEmpty())
    {
        resetFilter();
        return;
    }

    if (values.size() != columns.size())
    {
        qCritical() << "Asked to per-column filter, but number columns"
                    << columns.size() << "is different than number of values" << values.size();
        return;
    }

    QStringList conditions;
    for (int i = 0, total = columns.size(); i < total; ++i)
    {
        if (values[i].isEmpty())
            continue;

        conditions << wrapObjIfNeeded(columns[i]->getAliasedName())+" "+valueProc(values[i]);
    }

    queryExecutor->setFilters(conditions.join(" AND "));
    executeQuery();
}

void SqlDataSourceQueryModel::setSupportsReturningDelete(bool value)
{
    supportsReturningDelete = value;
}

QString SqlDataSourceQueryModel::stringFilterValueProcessor(const QString& value)
{
    static_qstring(pattern, "LIKE '%%1%'");
    return pattern.arg(escapeString(value));
}

QString SqlDataSourceQueryModel::strictFilterValueProcessor(const QString& value)
{
    static_qstring(pattern, "= '%1'");
    return pattern.arg(escapeString(value));
}

QString SqlDataSourceQueryModel::regExpFilterValueProcessor(const QString& value)
{
    static_qstring(pattern, "REGEXP '%1'");
    return pattern.arg(escapeString(value));
}

bool SqlDataSourceQueryModel::commitAddedRow(const QList<SqlQueryItem*>& itemsInRow, QList<CommitSuccessfulHandler>& successfulCommitHandlers)
{
    if (columns.size() != itemsInRow.size())
    {
        qCritical() << "Tried to SqlDataSourceQueryModel::commitAddedRow() with number of columns in argument different than model resolved for the table.";
        return false;
    }

    // Check that just in case:
    if (columns.size() == 0)
    {
        qCritical() << "Tried to SqlDataSourceQueryModel::commitAddedRow() with number of resolved columns in the table equal to 0!";
        return false;
    }

    // Prepare column placeholders and their values
    QStringList colNameList;
    QStringList bindParams;
    QList<QVariant> args;
    prepareColumnsAndBindParams(itemsInRow, colNameList, bindParams, args);

    // Prepare SQL query
    QString sql = getInsertSql(colNameList, bindParams);

    // Execute query
    SqlQueryPtr result = db->exec(sql, args);

    // Handle error
    if (result->isError())
    {
        QString errMsg = tr("Error while committing new row: %1").arg(result->getErrorText());
        for (SqlQueryItem* item : itemsInRow)
            item->setCommittingError(true, errMsg);

        notifyError(errMsg);
        return false;
    }

    // Take values from RETURNING clause to get actual values (because of DEFAULT, AUTOINCR).
    QList<SqlResultsRowPtr> rows = result->getAll();
    if (rows.size() != 1)
    {
        qCritical() << "After inserting new row to the table, number of rows returned from the query != 1. This should not happen. Number:" << rows.size();
        return false;
    }

    QList<QVariant> rowValues = rows.first()->valueList();
    if (columns.size() != rowValues.size())
    {
        qCritical() << "Tried to SqlDataSourceQueryModel::commitAddedRow() with number of columns in argument different than RETURNING values from the INSERT.";
        return false;
    }

    return commitAddedRowPostprocess(itemsInRow, rowValues, result->getInsertRowId(), successfulCommitHandlers);
}

bool SqlDataSourceQueryModel::commitDeletedRow(const QList<SqlQueryItem*>& itemsInRow, QList<CommitSuccessfulHandler>& successfulCommitHandlers)
{
    if (itemsInRow.size() == 0)
    {
        qCritical() << "Tried to SqlDataSourceQueryModel::commitDeletedRow() with number of items equal to 0!";
        return false;
    }

    RowId rowId = getRowIdForRow(itemsInRow);
    if (rowId.isEmpty())
        return false;

    CommitDeleteQueryBuilder queryBuilder;
    queryBuilder.setTableOrView(getTableOrView());
    queryBuilder.setRowId(rowId);
    queryBuilder.noReturning = !supportsReturningDelete;

    QString sql = queryBuilder.build();
    QHash<QString, QVariant> args = queryBuilder.getQueryArgs();

    SqlQueryPtr result = db->exec(sql, args);
    if (result->isError())
    {
        QString errMsg = tr("Error while deleting row from %1: %2").arg(getTableOrView(), result->getErrorText());
        for (SqlQueryItem* item : itemsInRow)
            item->setCommittingError(true, errMsg);

        notifyError(errMsg);
        return false;
    }

    if (!SqlQueryModel::commitDeletedRow(itemsInRow, successfulCommitHandlers))
        qCritical() << "Could not delete row from SqlQueryView while committing row deletion.";

    quint64 rowsAffected = supportsReturningDelete ? result->getAll().size() : result->rowsAffected();
    return commitDeletedRowPostprocess(itemsInRow, rowsAffected, successfulCommitHandlers);
}

QString SqlDataSourceQueryModel::getInsertSql(QStringList& colNameList, QStringList& sqlValues)
{
    static_qstring(insertTpl, "INSERT INTO %1 %2 %3 RETURNING *");
    static_qstring(valuesTpl, "VALUES (%1)");
    static_qstring(columnsTpl, "(%1)");

    QString wrappedTableName = getDataSource();
    QString colNames = colNameList.join(", ");
    if (colNameList.isEmpty())
        return insertTpl.arg(wrappedTableName, "", "DEFAULT VALUES");
    else
        return insertTpl.arg(wrappedTableName, columnsTpl.arg(colNames), valuesTpl.arg(sqlValues.join(", ")));
}

void SqlDataSourceQueryModel::prepareColumnsAndBindParams(const QList<SqlQueryItem*>& itemsInRow, QStringList& colNameList,
                                           QStringList& bindParams, QList<QVariant>& args)
{
    SqlQueryItem* item = nullptr;
    int i = 0;
    for (SqlQueryModelColumnPtr& modelColumn : columns)
    {
        item = itemsInRow[i++];
        if (!modelColumn->canEdit())
            continue;

        if (item->isUntouched() && modelColumn->hasDefaultValueForInsert())
            continue;

        colNameList << wrapObjIfNeeded(modelColumn->column);
        bindParams << ":arg" + QString::number(i);
        args << item->getValue();
    }
}

void SqlDataSourceQueryModel::applySqlFilter(const QString& value)
{
    if (value.isEmpty())
    {
        resetFilter();
        return;
    }

//    setQuery("SELECT * FROM "+getDataSource()+" WHERE "+value);
    queryExecutor->setFilters(value);
    executeQuery();
}

void SqlDataSourceQueryModel::applyStringFilter(const QString& value)
{
    applyFilter(value, &stringFilterValueProcessor);
}

void SqlDataSourceQueryModel::applyStringFilter(const QStringList& values)
{
    applyFilter(values, &stringFilterValueProcessor);
}

void SqlDataSourceQueryModel::applyRegExpFilter(const QString& value)
{
    applyFilter(value, &regExpFilterValueProcessor);
}

void SqlDataSourceQueryModel::applyRegExpFilter(const QStringList& values)
{
    applyFilter(values, &regExpFilterValueProcessor);
}

void SqlDataSourceQueryModel::applyStrictFilter(const QString& value)
{
    applyFilter(value, &strictFilterValueProcessor);
}

void SqlDataSourceQueryModel::applyStrictFilter(const QStringList& values)
{
    applyFilter(values, &strictFilterValueProcessor);
}

void SqlDataSourceQueryModel::resetFilter()
{
//    setQuery("SELECT * FROM "+getDataSource());
    queryExecutor->setFilters(QString());
    executeQuery(true);
}

QString SqlDataSourceQueryModel::getDatabasePrefix()
{
    if (database.isNull())
        return ""; // not "main.", because the "main." doesn't work for TEMP tables, such as sqlite_temp_master

    return wrapObjIfNeeded(database) + ".";
}

QString SqlDataSourceQueryModel::CommitDeleteQueryBuilder::build()
{
    QString dbAndTable = QueryGenerator::toFullObjectName(database, tableOrView);
    QString conditions = RowIdConditionBuilder::build();

    static_qstring(sql, "DELETE FROM %1 WHERE %2 RETURNING 1;");
    static_qstring(sqlNoReturning, "DELETE FROM %1 WHERE %2;");
    return (noReturning ? sqlNoReturning : sql).arg(dbAndTable, conditions);
}


QString SqlDataSourceQueryModel::SelectColumnsQueryBuilder::build()
{
    QString dbAndTable = QueryGenerator::toFullObjectName(database, tableOrView);
    QString conditions = RowIdConditionBuilder::build();

    static_qstring(sql, "SELECT %1 FROM %2 WHERE %3 LIMIT 1;");
    return sql.arg(columns.join(", "), dbAndTable, conditions);
}

void SqlDataSourceQueryModel::SelectColumnsQueryBuilder::addColumn(const QString& col)
{
    columns << col;
}
