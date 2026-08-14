#include "sqlitestatement.h"
#include "../token.h"
#include "../lexer.h"
#include "services/dbmanager.h"
#include <QDebug>

SqliteStatement::SqliteStatement()
{
}

SqliteStatement::SqliteStatement(const SqliteStatement& other) :
    QObject(), tokens(other.tokens), tokensMap(other.tokensMap)
{

}

SqliteStatement::~SqliteStatement()
{
}

QString SqliteStatement::detokenize() const
{
    return tokens.detokenize();
}

QStringList SqliteStatement::getContextColumns(bool checkParent, bool checkChilds)
{
    return getContextColumns(this, checkParent, checkChilds);
}

QStringList SqliteStatement::getContextTables(bool checkParent, bool checkChilds)
{
    return getContextTables(this, checkParent, checkChilds);
}

QStringList SqliteStatement::getContextDatabases(bool checkParent, bool checkChilds)
{
    prepareDbNames();
    return getContextDatabases(this, checkParent, checkChilds);
}

TokenList SqliteStatement::getContextColumnTokens(bool checkParent, bool checkChilds)
{
    return getContextColumnTokens(this, checkParent, checkChilds);
}

TokenList SqliteStatement::getContextTableTokens(bool checkParent, bool checkChilds)
{
    return getContextTableTokens(this, checkParent, checkChilds);
}

TokenList SqliteStatement::getContextDatabaseTokens(bool checkParent, bool checkChilds)
{
    prepareDbNames();
    return getContextDatabaseTokens(this, checkParent, checkChilds);
}

QList<SqliteStatement::FullObject> SqliteStatement::getContextFullObjects(bool checkParent, bool checkChilds)
{
    QList<FullObject> fullObjects = getContextFullObjects(this, checkParent, checkChilds);

    FullObject fullObj;
    QMutableListIterator<FullObject> it(fullObjects);
    while (it.hasNext())
    {
        fullObj = it.next();
        if (fullObj.type == SqliteStatement::FullObject::NONE)
        {
            qWarning() << "FullObject of type NONE!";
            it.remove();
            continue;
        }

        if (fullObj.type != SqliteStatement::FullObject::DATABASE && !fullObj.object)
        {
            qWarning() << "No 'object' member in FullObject that is not of DATABASE type!";
            it.remove();
            continue;
        }

        if (fullObj.type == SqliteStatement::FullObject::DATABASE && !fullObj.database)
        {
            qWarning() << "No 'database' member in FullObject that is of DATABASE type!";
            it.remove();
            continue;
        }
    }

    return fullObjects;
}

SqliteStatementPtr SqliteStatement::detach()
{
    if (!parent())
        qWarning() << "Detaching " << this << ", but there's no parent!";

    setParent(nullptr);
    return SqliteStatementPtr(this);
}

void SqliteStatement::processPostParsing()
{
    evaluatePostParsing();
    for (SqliteStatement* stmt : childStatements())
        stmt->processPostParsing();
}

bool SqliteStatement::visit(Visitor& visitor)
{
    for (SqliteStatement* stmt : getContextStatements(this, false, true))
    {
        bool res = stmt->visit(visitor);
        if (!res)
            return false;
    }

    return visitor(this);
}

QStringList SqliteStatement::getContextColumns(SqliteStatement *caller, bool checkParent, bool checkChilds)
{
    QStringList results = getColumnsInStatement();
    for (SqliteStatement* stmt : getContextStatements(caller, checkParent, checkChilds))
        results += stmt->getContextColumns(this, checkParent, checkChilds);

    return results;
}

QStringList SqliteStatement::getContextTables(SqliteStatement *caller, bool checkParent, bool checkChilds)
{
    QStringList results = getTablesInStatement();
    for (SqliteStatement* stmt : getContextStatements(caller, checkParent, checkChilds))
        results += stmt->getContextTables(this, checkParent, checkChilds);

    return results;
}

QStringList SqliteStatement::getContextDatabases(SqliteStatement *caller, bool checkParent, bool checkChilds)
{
    QStringList results = getDatabasesInStatement();
    for (SqliteStatement* stmt : getContextStatements(caller, checkParent, checkChilds))
    {
        stmt->validDbNames = this->validDbNames;
        results += stmt->getContextDatabases(this, checkParent, checkChilds);
    }

    return results;
}

TokenList SqliteStatement::getContextColumnTokens(SqliteStatement *caller, bool checkParent, bool checkChilds)
{
    TokenList results = getColumnTokensInStatement();
    for (SqliteStatement* stmt : getContextStatements(caller, checkParent, checkChilds))
        results += stmt->getContextColumnTokens(this, checkParent, checkChilds);

    return results;
}

TokenList SqliteStatement::getContextTableTokens(SqliteStatement *caller, bool checkParent, bool checkChilds)
{
    TokenList results = getTableTokensInStatement();
    for (SqliteStatement* stmt : getContextStatements(caller, checkParent, checkChilds))
        results += stmt->getContextTableTokens(this, checkParent, checkChilds);

    return results;
}

TokenList SqliteStatement::getContextDatabaseTokens(SqliteStatement *caller, bool checkParent, bool checkChilds)
{
    TokenList results = getDatabaseTokensInStatement();
    for (SqliteStatement* stmt : getContextStatements(caller, checkParent, checkChilds))
    {
        stmt->validDbNames = this->validDbNames;
        results += stmt->getContextDatabaseTokens(this, checkParent, checkChilds);
    }

    return results;
}

QList<SqliteStatement::FullObject> SqliteStatement::getContextFullObjects(SqliteStatement* caller, bool checkParent, bool checkChilds)
{
    QList<SqliteStatement::FullObject> results = getFullObjectsInStatement();
    for (SqliteStatement* stmt : getContextStatements(caller, checkParent, checkChilds))
    {
        stmt->setContextDbForFullObject(dbTokenForFullObjects);
        results += stmt->getContextFullObjects(this, checkParent, checkChilds);
    }

    return results;
}

QStringList SqliteStatement::getColumnsInStatement()
{
    return QStringList();
}

QStringList SqliteStatement::getTablesInStatement()
{
    return QStringList();
}

QStringList SqliteStatement::getDatabasesInStatement()
{
    return QStringList();
}

TokenList SqliteStatement::getColumnTokensInStatement()
{
    return TokenList();
}

TokenList SqliteStatement::getTableTokensInStatement()
{
    return TokenList();
}

TokenList SqliteStatement::getDatabaseTokensInStatement()
{
    return TokenList();
}

QList<SqliteStatement::FullObject> SqliteStatement::getFullObjectsInStatement()
{
    return QList<SqliteStatement::FullObject>();
}

TokenList SqliteStatement::rebuildTokensFromContents(bool replaceStatementTokens) const
{
    Q_UNUSED(replaceStatementTokens);
    qCritical() << "called rebuildTokensFromContents() for SqliteStatement that has no implementation for it.";
    return TokenList();
}

void SqliteStatement::evaluatePostParsing()
{
}

QList<SqliteStatement*> SqliteStatement::getContextStatements(SqliteStatement *caller, bool checkParent, bool checkChilds)
{
    QList<SqliteStatement*> results;

    if (checkParent)
    {
        SqliteStatement* stmt = parentStatement();
        if (stmt && stmt != caller)
            results += stmt;
    }

    if (checkChilds)
    {
        for (SqliteStatement* childStmt : childStatements())
        {
            if (childStmt == caller)
                continue;

            results += childStmt;
        }
    }

    return results;
}

void SqliteStatement::prepareDbNames()
{
    validDbNames = DBLIST->getValidDbNames();
}

bool SqliteStatement::isPrintableToken(const TokenPtr &token, bool skipMeaningless)
{
    switch (token->type)
    {
        case Token::OTHER:
        case Token::STRING:
        case Token::FLOAT:
        case Token::INTEGER:
        case Token::BIND_PARAM:
        case Token::OPERATOR:
        case Token::PAR_LEFT:
        case Token::PAR_RIGHT:
        case Token::BLOB:
        case Token::KEYWORD:
            return true;
        case Token::COMMENT:
        case Token::SPACE:
            if (!skipMeaningless)
                return true;
        default:
            break;
    }
    return false;
}

TokenList SqliteStatement::extractPrintableTokens(const TokenList &tokens, bool skipMeaningless)
{
    return tokens | FILTER(token, {return isPrintableToken(token, skipMeaningless);});
}

TokenPtr SqliteStatement::extractPrintableToken(const TokenList &tokens, int idx, bool skipMeaningless)
{
    int currIdx = -1;
    for (auto&& token : tokens)
    {
        if (isPrintableToken(token, skipMeaningless))
            currIdx++;

        if (currIdx >= idx)
            return token;
    }
    return TokenPtr();
}

QStringList SqliteStatement::getStrListFromValue(const QString &value)
{
    QStringList list;
    if (!value.isNull())
        list << value;

    return list;
}

TokenList SqliteStatement::getTokenListFromNamedKey(const QString &tokensMapKey, int idx)
{
    TokenList list;
    if (tokensMap.contains(tokensMapKey))
    {
        if (idx < 0)
            list += extractPrintableTokens(tokensMap[tokensMapKey]);
        else if (tokensMap[tokensMapKey].size() > idx)
            list << extractPrintableToken(tokensMap[tokensMapKey], idx);
    }
    else
        qCritical() << "No '" << tokensMapKey << "' in tokens map when asked for it in getTokenListFromNamedKey().";

    return list;
}

TokenPtr SqliteStatement::getDbTokenFromFullname(const QString &tokensMapKey)
{
    if (!tokensMap.contains(tokensMapKey))
    {
        qCritical() << "No '" << tokensMapKey << "' in tokens map when asked for it getDbTokenFromFullname().";
        return TokenPtr();
    }

    TokenList tokens = extractPrintableTokens(tokensMap[tokensMapKey]);

    if (tokens.size() == 3)
        return tokens[0];
    else if (tokens.size() == 1)
        return TokenPtr();
    else
        qCritical() << "Expected 1 or 3 tokens in '" << tokensMapKey << "' in tokens map, but got" << tokens.size();

    return TokenPtr();
}

TokenPtr SqliteStatement::getObjectTokenFromFullname(const QString &tokensMapKey)
{
    if (!tokensMap.contains(tokensMapKey))
    {
        qCritical() << "No '" << tokensMapKey << "' in tokens map when asked for it.";
        return TokenPtr();
    }

    TokenList tokens = extractPrintableTokens(tokensMap[tokensMapKey]);
    if (tokens.size() == 3)
        return tokens[2];
    else if (tokens.size() == 1)
        return tokens[0];
    else
        qCritical() << "Expected 1 or 3 tokens in '" << tokensMapKey << "' in tokens map, but got" << tokens.size();

    return TokenPtr();
}

TokenPtr SqliteStatement::getDbTokenFromNmDbnm(const QString &tokensMapKey1, const QString &tokensMapKey2)
{
    if (!tokensMap.contains(tokensMapKey1))
    {
        qCritical() << "No '" << tokensMapKey1 << "' in tokens map when asked for it in getDbTokenFromNmDbnm().";
        return TokenPtr();
    }

    // It seems like checking tokensMapKey2 has no added value to the logic, while it prevents from reporting
    // database token in case of: SELECT * FROM dbName.    <- the valid query with "minor" error
    Q_UNUSED(tokensMapKey2);
//    if (!tokensMap.contains(tokensMapKey2))
//    {
//        qCritical() << "No '" << tokensMapKey2 << "' in tokens map when asked for it in getDbTokenFromNmDbnm().";
//        return TokenPtr();
//    }

//    if (tokensMap[tokensMapKey2].size() == 0)
//        return TokenPtr();

    TokenList listForKey1 = extractPrintableTokens(tokensMap[tokensMapKey1]);
    TokenList listForKey2 = extractPrintableTokens(tokensMap[tokensMapKey2]);
    if (!tokensMap.contains("DOT") && listForKey2.size() == 0)
    {
        // In this case the query is "SELECT * FROM test" and there is no database,
        // but if there was a dot after the "test", then the "test" is a database name,
        // so this block won't be executed. Instead the name of the database will be returned below.
        return TokenPtr();
    }

    return extractPrintableToken(listForKey1, 0);
}

TokenPtr SqliteStatement::getObjectTokenFromNmDbnm(const QString &tokensMapKey1, const QString &tokensMapKey2)
{
    if (!tokensMap.contains(tokensMapKey1))
    {
        qCritical() << "No '" << tokensMapKey1 << "' in tokens map when asked for it in getObjectTokenFromNmDbnm().";
        return TokenPtr();
    }

    if (!tokensMap.contains(tokensMapKey2))
    {
        qCritical() << "No '" << tokensMapKey2 << "' in tokens map when asked for it in getObjectTokenFromNmDbnm().";
        return TokenPtr();
    }

    TokenList listForKey1 = extractPrintableTokens(tokensMap[tokensMapKey1]);
    TokenList listForKey2 = extractPrintableTokens(tokensMap[tokensMapKey2]);
    if (listForKey2.size() <= 1)
    {
        if (listForKey1.isEmpty())
            return TokenPtr();

        return listForKey1[0];
    }

    return listForKey2[1];
}

TokenList SqliteStatement::getDbTokenListFromFullname(const QString &tokensMapKey)
{
    TokenList list;
    TokenPtr token = getDbTokenFromFullname(tokensMapKey);
    if (token)
        list << token;

    return list;
}

TokenList SqliteStatement::getObjectTokenListFromFullname(const QString &tokensMapKey)
{
    TokenList list;
    TokenPtr token = getObjectTokenFromFullname(tokensMapKey);
    if (token)
        list << token;

    return list;
}

TokenList SqliteStatement::getDbTokenListFromNmDbnm(const QString &tokensMapKey1, const QString &tokensMapKey2)
{
    TokenList list;
    TokenPtr token = getDbTokenFromNmDbnm(tokensMapKey1, tokensMapKey2);
    if (token)
        list << token;

    return list;
}

TokenList SqliteStatement::getObjectTokenListFromNmDbnm(const QString &tokensMapKey1, const QString &tokensMapKey2)
{
    TokenList list;
    TokenPtr token = getObjectTokenFromNmDbnm(tokensMapKey1, tokensMapKey2);
    if (token)
        list << token;

    return list;
}

SqliteStatement::FullObject SqliteStatement::getFullObjectFromFullname(SqliteStatement::FullObject::Type type, const QString& tokensMapKey)
{
    return getFullObject(type, getDbTokenFromFullname(tokensMapKey), getObjectTokenFromFullname(tokensMapKey));
}

SqliteStatement::FullObject SqliteStatement::getFullObjectFromNmDbnm(SqliteStatement::FullObject::Type type, const QString& tokensMapKey1, const QString& tokensMapKey2)
{
    return getFullObject(type, getDbTokenFromNmDbnm(tokensMapKey1, tokensMapKey2), getObjectTokenFromNmDbnm(tokensMapKey1, tokensMapKey2));
}

SqliteStatement::FullObject SqliteStatement::getFullObject(SqliteStatement::FullObject::Type type, TokenPtr dbToken, TokenPtr objToken)
{
    FullObject fullObj;
    if (!objToken)
        return fullObj;

    fullObj.database = dbToken;
    fullObj.object = objToken;
    fullObj.type = type;
    return fullObj;
}

void SqliteStatement::setContextDbForFullObject(TokenPtr dbToken)
{
    dbTokenForFullObjects = dbToken;
}

SqliteStatement::FullObject SqliteStatement::getFirstDbFullObject()
{
    TokenList dbTokens = getDatabaseTokensInStatement();
    return getDbFullObject(dbTokens.size() > 0 ? dbTokens[0] : TokenPtr());
}

SqliteStatement::FullObject SqliteStatement::getDbFullObject(TokenPtr dbToken)
{
    FullObject fullObj;
    if (!dbToken)
        return fullObj;

    fullObj.database = dbToken;
    fullObj.type = FullObject::DATABASE;
    return fullObj;
}

Range SqliteStatement::getRange()
{
    if (tokens.size() == 0)
        return Range(0, 0);

    return Range(tokens.first()->start, tokens.last()->end);
}

SqliteStatement *SqliteStatement::findStatementWithToken(TokenPtr token)
{
    SqliteStatement* stmtWithToken = nullptr;
    for (SqliteStatement* stmt : childStatements())
    {
        stmtWithToken = stmt->findStatementWithToken(token);
        if (stmtWithToken)
            return stmtWithToken;
    }

    if (tokens.contains(token))
        return this;

    return nullptr;
}

SqliteStatement *SqliteStatement::findStatementWithPosition(quint64 cursorPosition)
{
    TokenPtr token = tokens.atCursorPosition(cursorPosition);
    if (!token)
        return nullptr;

    return findStatementWithToken(token);
}

SqliteStatement *SqliteStatement::parentStatement() const
{
    if (!parent())
        return nullptr;

    return dynamic_cast<SqliteStatement*>(parent());
}

QList<SqliteStatement *> SqliteStatement::childStatements()
{
    QList<SqliteStatement*> results;
    for (QObject* obj : children())
        results += dynamic_cast<SqliteStatement*>(obj);

    return results;
}

void SqliteStatement::rebuildTokens()
{
    tokens.clear();
    tokensMap.clear();
    tokens = rebuildTokensFromContents(true);
    // TODO rebuild tokensMap as well
    // It shouldn't be hard to write unit tests that parse a query, remembers it tokensMap, then rebuilds tokens from contents
    // and then compare new tokens map with previous one. This way we should be able to get all maps correctly.
}

TokenList SqliteStatement::produceTokens() const
{
    return rebuildTokensFromContents(false);
}

void SqliteStatement::attach(SqliteStatement*& memberForChild, SqliteStatement* childStatementToAttach)
{
    memberForChild = childStatementToAttach;
    childStatementToAttach->setParent(this);
}

bool SqliteStatement::FullObject::isValid() const
{
    return (object != nullptr || (type == DATABASE && database != nullptr));
}
