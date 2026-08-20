#ifndef SQLDATASOURCEQUERYMODEL_H
#define SQLDATASOURCEQUERYMODEL_H

#include "gui_global.h"
#include "sqlquerymodel.h"

/**
 * @brief The SqlDataSourceQueryModel class is an abstract class for models that are based on a single data source, like a table or a view.
 */
class GUI_API_EXPORT SqlDataSourceQueryModel : public SqlQueryModel
{
    public:
        explicit SqlDataSourceQueryModel(QObject *parent = 0);

        QString getDatabase() const;
        Features features() const;
        void updateTablesInUse(const QString& inUse);

        void applySqlFilter(const QString& value);
        void applyStringFilter(const QString& value);
        void applyStringFilter(const QStringList& values);
        void applyRegExpFilter(const QString& value);
        void applyRegExpFilter(const QStringList& values);
        void applyStrictFilter(const QString& value);
        void applyStrictFilter(const QStringList& values);
        void resetFilter();
        void setSupportsReturningDelete(bool newSupportsReturningDelete); // virtual tables do not support it

    protected:
        class CommitDeleteQueryBuilder : public CommitUpdateQueryBuilder
        {
            public:
                QString build();

                bool noReturning = false;
        };

        class SelectColumnsQueryBuilder : public CommitUpdateQueryBuilder
        {
            public:
                QString build();
                void addColumn(const QString& col);
        };

        typedef std::function<QString(const QString&)> FilterValueProcessor;

        static QString stringFilterValueProcessor(const QString& value);
        static QString strictFilterValueProcessor(const QString& value);
        static QString regExpFilterValueProcessor(const QString& value);

        bool commitAddedRow(const QList<SqlQueryItem*>& itemsInRow, QList<CommitSuccessfulHandler>& successfulCommitHandlers);
        bool commitDeletedRow(const QList<SqlQueryItem*>& itemsInRow, QList<CommitSuccessfulHandler>& successfulCommitHandlers);
        void prepareColumnsAndBindParams(const QList<SqlQueryItem*>& itemsInRow,
                                    QStringList& colNameList, QStringList& sqlValues, QList<QVariant>& args);
        QString getInsertSql(QStringList& colNameList, QStringList& sqlValues);
        QString getDatabasePrefix();
        void applyFilter(const QString& value, FilterValueProcessor valueProc);
        void applyFilter(const QStringList& values, FilterValueProcessor valueProc);

        virtual bool commitAddedRowPostprocess(const QList<SqlQueryItem*>& itemsInRow, const QList<QVariant>& rowValues, const RowId& rowId,
                                        QList<SqlQueryModel::CommitSuccessfulHandler>& successfulCommitHandlers) = 0;
        virtual bool commitDeletedRowPostprocess(const QList<SqlQueryItem*>& itemsInRow, int deletedRowCount,
                                        QList<SqlQueryModel::CommitSuccessfulHandler>& successfulCommitHandlers) = 0;
        virtual RowId getRowIdForRow(const QList<SqlQueryItem*>& itemsInRow) = 0;

        /**
         * @brief Get the data source for this object.
         * Default implementation returns an empty string. Working implementation
         * (i.e. for a table) should return the data source string (i.e. dbname.table),
         * both parts of name already wrapped if needed.
         */
        virtual QString getDataSource() = 0;

        /**
         * @brief Just table or view part of the datasource
         * @return
         */
        virtual QString getTableOrView() = 0;

        QString database;
        bool supportsReturningDelete = true;
};

#endif // SQLDATASOURCEQUERYMODEL_H
