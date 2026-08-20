#ifndef SQLITECREATEVIRTUALTABLE_H
#define SQLITECREATEVIRTUALTABLE_H

#include "sqlitequery.h"

#include <QString>
#include <QList>

class API_EXPORT SqliteCreateVirtualTable : public SqliteQuery
{
    Q_OBJECT

    public:
        SqliteCreateVirtualTable();
        SqliteCreateVirtualTable(const SqliteCreateVirtualTable& other);
        SqliteCreateVirtualTable(bool ifNotExists, const QString& name1, const QString& name2,
                                 const QString& name3);
        SqliteCreateVirtualTable(bool ifNotExists, const QString& name1, const QString& name2,
                                 const QString& name3, const QStringList& args);

        SqliteStatement* clone();

    protected:
        QStringList getTablesInStatement();
        QStringList getDatabasesInStatement();
        TokenList getTableTokensInStatement();
        TokenList getDatabaseTokensInStatement();
        QList<FullObject> getFullObjectsInStatement();
        TokenList rebuildTokensFromContents(bool replaceStatementTokens) const;

    private:
        void initName(const QString& name1, const QString& name2);

    public:
        bool ifNotExistsKw = false;
        QString database = QString();
        QString table = QString();
        QString module = QString();
        QStringList args;
};

typedef QSharedPointer<SqliteCreateVirtualTable> SqliteCreateVirtualTablePtr;

#endif // SQLITECREATEVIRTUALTABLE_H
