#ifndef SQLITESTATEMENT_H
#define SQLITESTATEMENT_H

#include "common/utils.h"
#include "parser/token.h"
#include <QList>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QStringList>
#include <QSharedPointer>

// TODO start using attach() in most cases where setParent() is used

class SqliteStatement;
typedef QSharedPointer<SqliteStatement> SqliteStatementPtr;

/**
 * @defgroup sqlite_statement Parser result containers
 */

/**
 * @ingroup sqlite_statement
 *
 * @brief General container for any type of parsed object.
 *
 * Statements can have multiple children statements and single parent statement.
 * This way they create a tree of objects. Since those statements represent various language
 * statements (here SQL), such tree is also called an Abstract Syntax Tree (aka AST).
 *
 * The AST lets you to examine structure of the query in a logical manner.
 * In other words, once you parse query into AST, you have a tree of object, where each oject in
 * the tree represents some part of the query and provides all its parameters as member variables.
 *
 * Deleting single statement causes all it's children to be deleted automatically.
 *
 * @section output_from_parser SqliteStatement as output from Parser
 *
 * The SqliteStatement is the most generic representation of Parser processing results.
 * Every parsed query is represented by its specialized, derived class, but it's also
 * the SqliteQuery and every SqliteQuery is also the SqliteStatement.
 *
 * Apart from SqliteQuery objects, which represent complete SQLite queries, there are also
 * other statements, like expressions (http://sqlite.org/lang_expr.html) and others.
 * Those statements don't inherit from SqliteQuery, but directly from SqliteStatement.
 *
 * Every parsed statement contains list of tokens that were used to parse this statement
 * in SqliteStatement::tokens.
 *
 * There is also SqliteStatement::tokensMap, which is a table mapping grammar rule name
 * into  tokens used to fulfill that rule. To learn possible keys for each SqliteStatement,
 * you have to look into sqlite3.y file and see definition of the statement,
 * that you're examining SqliteStatement::tokensMap for.
 *
 * @note SqliteStatement::tokensMap is a low level API and it's not very predictible,
 * unless you get to know it very well. That's why it's not recommended to use it.
 *
 * Example of working with SqliteStatement::tokensMap: you have a SqliteAttachPtr from the parser.
 * You can learn the "attach name" from SqliteAttach::name, like this:
 * @code
 * QString name;
 * if (attachPtr->name)
 *     name = attachPtr->name->detokenize();
 * @endcode
 * or you can use tokensMap like this:
 * @code
 * QString name;
 * if (attachPtr->tokensMap["expr2"])
 *     name = attachPtr->tokensMap["expr2"]->detokenize();
 * @endcode
 *
 * Why using tokensMap, when you can read values from object member easly? Well, object members
 * have plain values (string, integer, etc), while tokensMap has tokens, so you can examine
 * at which character exactly was the value placed, where it ended, etc.
 *
 * @section query_generation SqliteStatement as utility for generating query string
 *
 * Generating query string with SqliteStatement makes sense only in case, when you have parsed
 * query and got SqliteStatement as a result. You can modify some parameters of the query
 * and detokenize it back to SQL string. This is done in 4 steps:
 * <ul>
 * <li>Parse SQL query string,</li>
 * <li>Modify values in parsed statements,</li>
 * <li>Re-generate tokens in all modified statements,</li>
 * <li>Detokenize tokens from statements.</li>
 * </ul>
 *
 * This is how it's usually done:
 * @code
 * // STEP 1
 * Parser parser;
 * if (!parser.parse("SELECT column FROM test WHERE value = 5") || parser.getQueries().size() == 0)
 * {
 *     // handle parsing error, or no queries parsed (which is also some kind of error)
 *     return;
 * }
 *
 * SqliteQueryPtr query = parser.getQueries().first();
 * SqliteSelectPtr select = query.dynamicCast<SqliteSelect>();
 * if (!select)
 * {
 *     // we want to deal with the SELECT only
 *     return;
 * }
 *
 * // STEP 2
 * SqliteSelect::Core* core = select->coreSelects.first();
 *
 * // Prepare new result column statement
 * SqliteSelect::Core::ResultColumn* resCol = new SqliteSelect::Core::ResultColumn();
 *
 * SqliteExpr* expr = new SqliteExpr();       // create expression for result column
 * expr->initLiteral("test value");           // let the expression be a constant literal value
 *
 * resCol->attach(resCol->expr, expr);        // put the defined expression into result column statement
 * core->attach(core->resultColumns, resCol); // add new result column to rest of columns
 *
 * // STEP 3
 * select->rebuildTokens();
 *
 * // STEP 4
 * QString newQuery = select->detokenize();
 * @endcode
 *
 * In the result, the newQuery will contain: <tt>SELECT column, 'test value' FROM test WHERE value = 5</tt>.
 *
 * @warning It is important to use SqliteStatement::attach() and SqliteStatement::detach()
 * when modifying AST layout. The tree hierarchy is used to delete objects recurrently,
 * so deleting the SqliteSelect will also delete all it's children. Assembling or disassembling statements
 * manually (without SqliteStatement::attach() and SqliteStatement::detach()) is not safe and will most likely
 * result in a memory leak, or application crash.
 *
 * For example of SqliteStatement::detach() usage see section below.
 *
 * @section ptr_vs_shared_ptr C++ pointer to SqliteStatement vs. SqliteStatementPtr
 *
 * SqliteStatementPtr is a shared pointer (QSharedPointer) to SqliteStatement. All derived classes
 * also have variations of their types as shared pointers. However only the top level objects
 * returned from the Parser are actually provided as shared pointers. All children objects are
 * regular C++ pointers. The reason behind this is to avoid memory leaks. Top level objects from Parser
 * are shared pointers, so you can use them casually, without worrying who and when should delete them.
 * On the other hand, any children of those top level objects are regular pointers, cause they will
 * be deleted automatically when their parent is deleted.
 *
 * Sometimes you might want to use just some child statement from parsed query. Normally you would need to
 * assign that child statement to some local variable and reset its parent to nullptr. Fortunately
 * SqliteStatement provides handful method SqliteStatement::detach(), which does all that for you.
 * It also provides detached statement as a new shared pointer, so it's easier to manage it.
 * Additionally there's a template version of detach() method which can return detached statement
 * as provided statement type.
 *
 * Example:
 * @code
 * Parser parser;
 * if (!parser.parse("SELECT column FROM test WHERE value = 5") || parser.getQueries().size() == 0)
 * {
 *     // handle parsing error, or no queries parsed (which is also some kind of error)
 *     return SqliteExprPtr();
 * }
 *
 * SqliteQueryPtr query = parser.getQueries().first();
 * SqliteSelectPtr select = query.dynamicCast<SqliteSelect>();
 * if (!select)
 * {
 *     // we want to deal with the SELECT only
 *     return SqliteExprPtr();
 * }
 *
 * SqliteSelect::Core* core = select->coreSelects.first();
 *
 * // Our point is to get the SqliteExpr which represents the result column "column".
 * SqliteExprPtr expr = core->resultColumns.first().detach<SqliteExpr>();
 * return expr;
 * @endcode
 *
 * After the above <tt>parser</tt> goes out of scope, so it's deleted and all its parsed
 * queries get deleted as well, because their shared pointers were not copied anywhere else.
 * The captured <tt>expr</tt> would normally also be deleted, but when we detached it, it became
 * an independed entity, with its own lifetime.
 *
 * For the opposite operation, use SqliteStatement::attach().
 */
class API_EXPORT SqliteStatement : public QObject
{
    Q_OBJECT

    public:
        struct FullObject
        {
            enum Type
            {
                TABLE,
                INDEX,
                TRIGGER,
                VIEW,
                DATABASE,
                NONE
            };

            bool isValid() const;

            Type type = NONE;
            TokenPtr database;
            TokenPtr object;
        };

        typedef std::function<bool(SqliteStatement*)> Visitor;

        SqliteStatement();
        SqliteStatement(const SqliteStatement& other);
        virtual ~SqliteStatement();

        QString detokenize() const;
        Range getRange();

        template <class T>
        T* findTypedStatementWithToken(TokenPtr token)
        {
            T* stmtWithToken = nullptr;
            for (SqliteStatement* stmt : childStatements())
            {
                stmtWithToken = stmt->template findTypedStatementWithToken<T>(token);
                if (stmtWithToken)
                    return stmtWithToken;
            }

            T* thisTyped = dynamic_cast<T*>(this);
            if (thisTyped && tokens.contains(token))
                return thisTyped;

            return nullptr;
        }
        SqliteStatement* findStatementWithToken(TokenPtr token);

        template <class T>
        T* findTypedStatementWithPosition(quint64 cursorPosition)
        {
            TokenPtr token = tokens.atCursorPosition(cursorPosition);
            if (!token)
                return nullptr;

            return findTypedStatementWithToken<T>(token);
        }
        SqliteStatement* findStatementWithPosition(quint64 cursorPosition);

        template <class T>
        T* findClosestTypedParentStatement()
        {
            SqliteStatement* parent = parentStatement();
            while (parent)
            {
                T* casted = dynamic_cast<T*>(parent);
                if (casted)
                    return casted;

                parent = parent->parentStatement();
            }
            return nullptr;
        }

        SqliteStatement* parentStatement() const;
        QList<SqliteStatement*> childStatements();
        QStringList getContextColumns(bool checkParent = true, bool checkChilds = true);
        QStringList getContextTables(bool checkParent = true, bool checkChilds = true);
        QStringList getContextDatabases(bool checkParent = true, bool checkChilds = true);
        TokenList getContextColumnTokens(bool checkParent = true, bool checkChilds = true);
        TokenList getContextTableTokens(bool checkParent = true, bool checkChilds = true);
        TokenList getContextDatabaseTokens(bool checkParent = true, bool checkChilds = true);
        QList<FullObject> getContextFullObjects(bool checkParent = true, bool checkChilds = true);
        void rebuildTokens();
        /**
         * @brief Generates tokens according to statement's current state.
         * @return Generated tokens.
         *
         * This differs from rebuildTokens() by not replacing internal token list.
         */
        TokenList produceTokens() const;
        void attach(SqliteStatement*& memberForChild, SqliteStatement* childStatementToAttach);
        SqliteStatementPtr detach();
        void processPostParsing();
        virtual SqliteStatement* clone() = 0;

        template <class T>
        T* typeClone()
        {
            return dynamic_cast<T*>(clone());
        }

        template <class T>
        QSharedPointer<T> typeCloneShared()
        {
            return QSharedPointer<T>(dynamic_cast<T*>(clone()));
        }

        template <class T>
        void attach(QList<T*>& listMemberForChild, T* childStatementToAttach)
        {
            listMemberForChild << childStatementToAttach;
            childStatementToAttach->setParent(this);
        }

        template <class X>
        QSharedPointer<X> detach() {return detach().dynamicCast<X>();}

        template <class T>
        QList<T*> getAllTypedStatements()
        {
            QList<T*> results;

            T* casted = dynamic_cast<T*>(this);
            if (casted)
                results << casted;

            for (SqliteStatement* stmt : getContextStatements(this, false, true))
                results += stmt->getAllTypedStatements<T>();

            return results;
        }

        bool visit(Visitor& visitor);

        /**
         * @brief Ordered list of all tokens for this statement.
         * An ordered list of tokens that represent current statement. Tokens include
         * Token::SPACE and Token::COMMENT types, so detokenizing this list results
         * in the equal SQL string as was passed to the parser at the begining.
         */
        TokenList tokens;

        /**
         * @brief Map of grammar terminals and non-terminals into their tokens.
         * This is map of ordered token lists that represent each node of SQLite grammar definition
         * used to build this statement. For example grammar definition:
         *   test ::= value1 TERMINAL_TOKEN value2
         * will result in tokens map containing following entries:
         *   value1 = {list of tokens from value1}
         *   TERMINAL_TOKEN = {list of only one token: TERMINAL_TOKEN}
         *   value2 = {list of tokens from value2}
         *
         * In case there are two non-terminals with same name used (for example "name name name"),
         * then first non-terminal is used as a key just as is, but second is renamed to "name2",
         * and third to "name3". If we had example:
         *   test ::= value TERMINAL_TOKEN value
         * then it will result in tokens map containing following entries:
         *   value  = {list of tokens from first value}
         *   TERMINAL_TOKEN = {list of only one token: TERMINAL_TOKEN}
         *   value2 = {list of tokens from second value}
         */
        QHash<QString,TokenList> tokensMap;

    protected:
        QStringList getContextColumns(SqliteStatement* caller, bool checkParent, bool checkChilds);
        QStringList getContextTables(SqliteStatement* caller, bool checkParent, bool checkChilds);
        QStringList getContextDatabases(SqliteStatement* caller, bool checkParent, bool checkChilds);
        TokenList getContextColumnTokens(SqliteStatement* caller, bool checkParent, bool checkChilds);
        TokenList getContextTableTokens(SqliteStatement* caller, bool checkParent, bool checkChilds);
        TokenList getContextDatabaseTokens(SqliteStatement* caller, bool checkParent, bool checkChilds);
        QList<FullObject> getContextFullObjects(SqliteStatement* caller, bool checkParent, bool checkChilds);

        virtual QStringList getColumnsInStatement();
        virtual QStringList getTablesInStatement();
        virtual QStringList getDatabasesInStatement();
        virtual TokenList getColumnTokensInStatement();
        virtual TokenList getTableTokensInStatement();
        virtual TokenList getDatabaseTokensInStatement();
        virtual QList<FullObject> getFullObjectsInStatement();
        virtual TokenList rebuildTokensFromContents(bool replaceStatementTokens) const;
        virtual void evaluatePostParsing();

        static TokenList extractPrintableTokens(const TokenList& tokens, bool skipMeaningless = true);
        static TokenPtr extractPrintableToken(const TokenList& tokens, int idx, bool skipMeaningless = true);
        QStringList getStrListFromValue(const QString& value);
        TokenList getTokenListFromNamedKey(const QString& tokensMapKey, int idx = 0);
        TokenPtr getDbTokenFromFullname(const QString& tokensMapKey = "fullname");
        TokenPtr getObjectTokenFromFullname(const QString& tokensMapKey = "fullname");
        TokenPtr getDbTokenFromNmDbnm(const QString& tokensMapKey1 = "nm", const QString& tokensMapKey2 = "dbnm");
        TokenPtr getObjectTokenFromNmDbnm(const QString& tokensMapKey1 = "nm", const QString& tokensMapKey2 = "dbnm");
        TokenList getDbTokenListFromFullname(const QString& tokensMapKey = "fullname");
        TokenList getObjectTokenListFromFullname(const QString& tokensMapKey = "fullname");
        TokenList getDbTokenListFromNmDbnm(const QString& tokensMapKey1 = "nm", const QString& tokensMapKey2 = "dbnm");
        TokenList getObjectTokenListFromNmDbnm(const QString& tokensMapKey1 = "nm", const QString& tokensMapKey2 = "dbnm");
        FullObject getFullObjectFromFullname(FullObject::Type type, const QString& tokensMapKey = "fullname");
        FullObject getFullObjectFromNmDbnm(FullObject::Type type, const QString& tokensMapKey1 = "nm", const QString& tokensMapKey2 = "dbnm");
        FullObject getFirstDbFullObject();
        FullObject getDbFullObject(TokenPtr dbToken);
        FullObject getFullObject(SqliteStatement::FullObject::Type type, TokenPtr dbToken, TokenPtr objToken);
        void setContextDbForFullObject(TokenPtr dbToken);

        /**
         * @brief Token representing a database.
         * Keeps db context for getFullObjectsInStatement(), so for example "CREATE TABLE xyz.abc (id)" will know,
         * that for column "id" the database is "xyz" and table "abc". The value is spread across childrens
         * of this statement when getContextFullObjects() is called.
         * The value of this variable is defined in overwritten implementation of getFullObjectsInStatement() method.
         */
        TokenPtr dbTokenForFullObjects;

        /**
         * @brief List of database names as seen in the side Database List.
         * It is resoled at top-level statement being queried for databases and then it's propagated down to all child statements.
         * It helps to identify whether the xyz in "xyz." is a table or a database prefix.
         */
        QStringList validDbNames;

    private:
        QList<SqliteStatement*> getContextStatements(SqliteStatement* caller, bool checkParent, bool checkChilds);
        void prepareDbNames();

        static bool isPrintableToken(const TokenPtr& token, bool skipMeaningless);
};

#endif // SQLITESTATEMENT_H
