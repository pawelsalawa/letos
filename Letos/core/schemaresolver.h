#ifndef SCHEMARESOLVER_H
#define SCHEMARESOLVER_H

#include "parser/parser.h"
#include "parser/ast/sqlitequerytype.h"
#include "parser/ast/sqlitecreatetable.h"
#include "parser/ast/sqlitecreateindex.h"
#include "parser/ast/sqlitecreateview.h"
#include "parser/ast/sqlitecreatetrigger.h"
#include "core_global.h"
#include "common/utils_sql.h"
#include "selectresolver.h"
#include "db/sqlresultsrow.h"
#include "db/sqlquery.h"
#include "db/db.h"
#include "common/strhash.h"
#include "common/expiringcache.h"
#include "parser/ast/sqlitequerytype.h"
#include <QStringList>

class SqliteCreateTable;

// TODO add cache

class API_EXPORT SchemaResolver
{
    public:
        enum ObjectType
        {
            TABLE,
            INDEX,
            TRIGGER,
            VIEW,
            ANY
        };

        struct ObjectDetails
        {
            QString name;
            ObjectType type;
            QString ddl;
            QString refObject; // referenced object, i.e. parent table/view for index or trigger
        };

        struct TableListItem
        {
            enum Type
            {
                TABLE,
                VIRTUAL_TABLE,
                SHADOW_TABLE,
                VIEW,
                UNKNOWN
            };

            Type type;
            QString name;
        };

        struct ObjectCacheKey
        {
            enum Type
            {
                OBJECT_NAMES,
                OBJECT_DETAILS,
                TABLE_LIST_ITEM,
                OBJECT_DDL
            };

            ObjectCacheKey(Type type, Db* db, const QString& value1 = QString(), const QString& value2 = QString(), const QString& value3 = QString());
            ObjectCacheKey(Type type, Db* db, bool skipSystemObj, const QString& value1 = QString(), const QString& value2 = QString(), const QString& value3 = QString());

            Type type;
            Db* db;
            bool skipSystemObj;
            QString value1;
            QString value2;
            QString value3;
        };

        explicit SchemaResolver(Db* db);
        SchemaResolver(const SchemaResolver&) = delete;
        SchemaResolver& operator=(const SchemaResolver&) = delete;

        SchemaResolver(SchemaResolver&&) = delete;
        SchemaResolver& operator=(SchemaResolver&&) = delete;
        virtual ~SchemaResolver();

        QStringList getTables(const QString& database = QString());
        QStringList getIndexes(const QString& database = QString());
        QStringList getTriggers(const QString& database = QString());
        QStringList getViews(const QString& database = QString());
        StrHash<QStringList> getGroupedIndexes(const QString& database = QString());
        StrHash<QStringList> getGroupedTriggers(const QString& database = QString());
        QSet<QString> getDatabases();
        QStringList getObjects(const QString& type);
        QStringList getObjects(const QString& database, const QString& type);
        QStringList getAllObjects();
        QStringList getAllObjects(const QString& database);
        QString getUniqueName(const QString& database, const QString& namePrefix, const QStringList& forbiddenNames = QStringList());
        QString getUniqueName(const QString& namePrefix = QString(), const QStringList& forbiddenNames = QStringList());
        QStringList getFkReferencingTables(const QString& table);
        QStringList getFkReferencingTables(const QString& database, const QString& table);
        QStringList getFkReferencedTables(const QString& table);
        QStringList getFkReferencedTables(const QString& database, const QString& table);

        QStringList getIndexesForTable(const QString& database, const QString& table);
        QStringList getIndexesForTable(const QString& table);
        QStringList getTriggersForTable(const QString& database, const QString& table);
        QStringList getTriggersForTable(const QString& table);
        QStringList getTriggersForView(const QString& database, const QString& view);
        QStringList getTriggersForView(const QString& view);
        QStringList getViewsForTable(const QString& database, const QString& table);
        QStringList getViewsForTable(const QString& table);
        QStringList getIndexDdlsForTable(const QString& database, const QString& table);
        QStringList getIndexDdlsForTable(const QString& table);
        QStringList getTriggerDdlsForTableOrView(const QString& database, const QString& table);
        QStringList getTriggerDdlsForTableOrView(const QString& table);
        QStringList getShadowTablesForVirtualTable(const QString& database, const QString& table);
        QStringList getShadowTablesForVirtualTable(const QString& table);

        QList<TableListItem> getAllTableListItems();
        QList<TableListItem> getAllTableListItems(const QString& database);

        StrHash<ObjectDetails> getAllObjectDetails();
        StrHash<ObjectDetails> getAllObjectDetails(const QString& database);

        SqliteCreateTablePtr getParsedTable(const QString& name, bool autoConvertVirtTable = false);
        SqliteCreateTablePtr getParsedTable(const QString& database, const QString& name, bool autoConvertVirtTable = false);
        SqliteCreateIndexPtr getParsedIndex(const QString& name);
        SqliteCreateIndexPtr getParsedIndex(const QString& database, const QString& name);
        SqliteCreateTriggerPtr getParsedTrigger(const QString& name);
        SqliteCreateTriggerPtr getParsedTrigger(const QString& database, const QString& name);
        SqliteCreateViewPtr getParsedView(const QString& name);
        SqliteCreateViewPtr getParsedView(const QString& database, const QString& name);

        QList<SqliteCreateIndexPtr> getParsedIndexesForTable(const QString& database, const QString& table);
        QList<SqliteCreateIndexPtr> getParsedIndexesForTable(const QString& table);
        /**
         * @param includeContentReferences Whether result should include triggers that refer table/view in its implementation code, not just in the "ON" clause
         */
        QList<SqliteCreateTriggerPtr> getParsedTriggersForTable(const QString& database, const QString& table, bool includeContentReferences = false);
        QList<SqliteCreateTriggerPtr> getParsedTriggersForTable(const QString& table, bool includeContentReferences = false);
        QList<SqliteCreateTriggerPtr> getParsedTriggersForView(const QString& database, const QString& view, bool includeContentReferences = false);
        QList<SqliteCreateTriggerPtr> getParsedTriggersForView(const QString& view, bool includeContentReferences = false);
        QList<SqliteCreateViewPtr> getParsedViewsForTable(const QString& database, const QString& table);
        QList<SqliteCreateViewPtr> getParsedViewsForTable(const QString& table);

        bool isWithoutRowIdTable(const QString& table);
        bool isWithoutRowIdTable(const QString& database, const QString& table);
        bool isVirtualTable(const QString& database, const QString& table);
        bool isVirtualTable(const QString& table);
        SqliteCreateTablePtr resolveVirtualTableAsRegularTable(const QString& table);
        SqliteCreateTablePtr resolveVirtualTableAsRegularTable(const QString& database, const QString& table);

        QStringList getRowIdTableColumns(const QString& table);
        QStringList getRowIdTableColumns(const QString& database, const QString& table);

        /**
         * @brief getTableColumns Get column names for a table.
         * @param table Table to query.
         * @param onlyReal If true, the method will skip columns that are not real (like GENERATED columns).
         * @return List of column names of the table.
         * This does something similar to getting list of columns with
         * PRAGMA table_info(), but the pragma in Sqlite2 doesn't support
         * queries for attached databases, therefore this method is provided
         * to make this possible. It finds table's DDL and parses it,
         * then extracts list of columns from parsing results.
         */
        QStringList getTableColumns(const QString& table, bool onlyReal = false);

        /**
         * @brief getTableColumns Get column names for a table.
         * @param database Attached database name.
         * @param table Table to query.
         * @param onlyReal If true, the method will skip columns that are not real (like GENERATED columns).
         * @return List of column names of the table.
         * @overload
         */
        QStringList getTableColumns(const QString& database, const QString& table, bool onlyReal = false);

        StrHash<DataType> getTableColumnDataTypesByName(const QString& table);
        StrHash<DataType> getTableColumnDataTypesByName(const QString& database, const QString& table);
        QList<DataType> getTableColumnDataTypes(const QString& table, int expectedNumberOfTypes = -1);
        QList<DataType> getTableColumnDataTypes(const QString& database, const QString& table, int expectedNumberOfTypes = -1);

        StrHash<QStringList> getAllTableColumns(const QString& database = QString());

        QStringList getViewColumns(const QString& view);
        QStringList getViewColumns(const QString& database, const QString& view);
        QList<SelectResolver::Column> getViewColumnObjects(const QString& view);
        QList<SelectResolver::Column> getViewColumnObjects(const QString& database, const QString& view);

        QString getObjectDdl(const QString& name, ObjectType type);
        QString getObjectDdl(const QString& database, const QString& name, ObjectType type);

        QStringList getColumnsFromDdlUsingPragma(const QString& ddl);
        QStringList getColumnsUsingPragma(const QString& tableOrView, bool onlyReal = false);
        QStringList getColumnsUsingPragma(const QString& database, const QString& tableOrView, bool onlyReal = false);
        QStringList getColumnsUsingPragma(SqliteCreateTable* createTable);
        QStringList getColumnsUsingPragma(SqliteCreateView* createView);
        QList<QPair<QString, QString>> getColumnsAndDataTypesUsingPragma(const QString& tableOrView, bool onlyReal = false);
        QList<QPair<QString, QString>> getColumnsAndDataTypesUsingPragma(const QString& database, const QString& tableOrView, bool onlyReal = false);

        /**
         * @brief Parses given object's DDL.
         * @param name Name of the object in the database.
         * @return Parsed object, or null pointer if named object was not in the database, or parsing error occurred.
         *
         * Returned query has to be deleted outside!
         */
        SqliteQueryPtr getParsedObject(const QString& name, ObjectType type);

        /**
         * @brief Parses given object's DDL.
         * @param database Database that the object is in (the attach name of the database).
         * @param name Name of the object in the database.
         * @return Parsed object, or null pointer if named object was not in the database, or parsing error occurred.
         * @overload
         */
        SqliteQueryPtr getParsedObject(const QString& database, const QString& name, ObjectType type);

        StrHash<SqliteQueryPtr> getAllParsedObjects();
        StrHash<SqliteQueryPtr> getAllParsedObjects(const QString& database);
        StrHash<SqliteCreateTablePtr> getAllParsedTables();
        StrHash<SqliteCreateTablePtr> getAllParsedTables(const QString& database);
        StrHash<SqliteCreateIndexPtr> getAllParsedIndexes();
        StrHash<SqliteCreateIndexPtr> getAllParsedIndexes(const QString& database);
        StrHash<SqliteCreateTriggerPtr> getAllParsedTriggers();
        StrHash<SqliteCreateTriggerPtr> getAllParsedTriggers(const QString& database);
        StrHash<SqliteCreateTriggerPtr> getAllParsedViewTriggers();
        StrHash<SqliteCreateTriggerPtr> getAllParsedViewTriggers(const QString& database);
        StrHash<SqliteCreateViewPtr> getAllParsedViews();
        StrHash<SqliteCreateViewPtr> getAllParsedViews(const QString& database);
        QStringList getTablePrimaryKeyColumns(const QString& table);
        QStringList getTablePrimaryKeyColumns(const QString& database, const QString& table);
        bool isColumnPkOrUnique(const QString& table, const QString& column);
        bool isColumnPkOrUnique(const QString& database, const QString& table, const QString& column);

        QString getSqliteAutoIndexDdl(const QString& database, const QString& index);
        static QString getSqliteMasterDdl(bool schema, bool temp = false);
        static ObjectType objectTypeFromQueryType(const SqliteQueryType& queryType);

        QStringList getCollations();
        QStringList getAllExtensions();

        bool getIgnoreSystemObjects() const;
        void setIgnoreSystemObjects(bool value);

        bool getNoDbLocking() const;
        void setNoDbLocking(bool value);

        QString normalizeCaseObjectName(const QString& name);
        QString normalizeCaseObjectName(const QString& database, const QString& name);

        static QString objectTypeToString(ObjectType type);
        static ObjectType stringToObjectType(const QString& type);
        static TableListItem::Type stringToTableListItemType(const QString& type);
        static void staticInit();

        static_char* USE_SCHEMA_CACHING = "useSchemaCaching";

    private:
        bool usesCache();
        SqliteQueryPtr getParsedDdl(const QString& ddl);
        SqliteCreateTablePtr virtualTableAsRegularTable(const QString& database, const QString& table);
        StrHash< QStringList> getGroupedObjects(const QString &database, const QStringList& inputList, SqliteQueryType type);
        bool isFilteredOut(const QString& value, const QString& type);
        void filterSystemIndexes(QStringList& indexes);
        QList<SqliteCreateTriggerPtr> getParsedTriggersForTableOrView(const QString& database, const QString& tableOrView, bool includeContentReferences);
        QString getObjectDdlWithDifficultName(const QString& dbName, const QString& lowerName, QString targetTable, ObjectType type);
        QString getObjectDdlWithSimpleName(const QString& dbName, const QString& lowerName, QString targetTable, ObjectType type);
        StrHash<QString> getIndexesWithTables(const QString& database = QString());
        QString normalizeCaseObjectNameByQuery(const QString& query, const QString& name);
        QStringList getObjectDdlsReferencingTableOrView(const QString& database, const QString& table, ObjectType type);
        Parser* getParser();

        template <class T>
        StrHash<QSharedPointer<T>> getAllParsedObjectsForType(const QString& database, const QString& type, const QString& extraPredicate = QString());

        Db* db = nullptr;
        Parser* parser = nullptr;
        bool ignoreSystemObjects = false;
        Db::Flags dbFlags;

        static ExpiringCache<ObjectCacheKey,QVariant> cache;
        static ExpiringCache<QString, QString> autoIndexDdlCache;
};

size_t qHash(const SchemaResolver::ObjectCacheKey& key);
int operator==(const SchemaResolver::ObjectCacheKey& k1, const SchemaResolver::ObjectCacheKey& k2);

template <class T>
StrHash<QSharedPointer<T>> SchemaResolver::getAllParsedObjectsForType(const QString& database, const QString& type, const QString& extraPredicate)
{
    static_qstring(queryTpl, "SELECT name, type, sql FROM %1.sqlite_master%2;");
    static_qstring(typeTpl, "type = '%1'");
    static_qstring(whereTpl, " WHERE %1");

    QString dbName = getPrefixDb(database);

    QStringList predicates;
    if (!type.isNull())
        predicates << typeTpl.arg(type);

    if (!extraPredicate.isNull())
        predicates << extraPredicate;

    QString conditions = predicates.isEmpty() ? "" : whereTpl.arg(predicates.join(" AND "));
    SqlQueryPtr results = db->exec(queryTpl.arg(dbName, conditions));

    StrHash<QSharedPointer<T>> parsedObjects;
    for (SqlResultsRowPtr& row : results->getAll())
    {
        QString name = row->value("name").toString();
        SqliteQueryPtr parsedObject = getParsedDdl(row->value("sql").toString());
        if (!parsedObject)
            continue;

        if (isFilteredOut(name, row->value("type").toString()))
            continue;

        QSharedPointer<T> castedObject = parsedObject.dynamicCast<T>();
        if (castedObject)
            parsedObjects[name] = castedObject;
    }

    return parsedObjects;
}

#endif // SCHEMARESOLVER_H
