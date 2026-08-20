#ifndef FORMATSTATEMENT_H
#define FORMATSTATEMENT_H

#include "parser/ast/sqlitestatement.h"
#include "parser/ast/sqlitesortorder.h"
#include "parser/ast/sqliteconflictalgo.h"
#include "common/utils_sql.h"
#include "sqlenterpriseformatter.h"
#include <QString>
#include <QStringList>
#include <QHash>
#include <QStack>
#include <QVariant>
#include <functional>

class FormatStatement
{
    public:
        struct FormatToken
        {
            enum Type
            {
                KEYWORD,
                LINED_UP_KEYWORD,
                ID,
                ID_NO_WRAP,
                STRING_OR_ID,
                OPERATOR,
                STAR,
                FLOAT,
                STRING,
                INTEGER,
                BLOB,
                BIND_PARAM,
                ID_DOT,
                PAR_DEF_LEFT,
                PAR_DEF_RIGHT,
                PAR_EXPR_LEFT,
                PAR_EXPR_RIGHT,
                PAR_FUNC_LEFT,
                PAR_FUNC_RIGHT,
                SEMICOLON,
                COMMA_LIST,
                COMMA_OPER, // for example in LIMIT
                FUNC_ID,
                DATA_TYPE,
                NEW_LINE,
                INDENT_MARKER,
                INCR_INDENT,
                SET_INDENT,
                DECR_INDENT,
                MARK_KEYWORD_LINEUP
            };

            enum Flag
            {
                NO_FLAG = 0x00,
                NO_SPACE_BEFORE = 0x01,
                NO_SPACE_AFTER = 0x02,
                NO_NEWLINE_BEFORE = 0x04,
                NO_NEWLINE_AFTER = 0x08,
                SINGLE_QUOTE_STRING = 0x10
            };

            Q_DECLARE_FLAGS(Flags, Flag)

            Type type;
            QVariant value;
            QVariant additionalValue;
            Flags flags;
        };

        enum class ListSeparator
        {
            NONE,
            COMMA,
            EXPR_COMMA,
            NEW_LINE,
            SEMICOLON
        };

        typedef std::function<void(FormatStatement*)> FormatStatementEnricher;

        FormatStatement(SqliteStatement *stmt);
        virtual ~FormatStatement();

        QString format();
        void setSelectedWrapper(NameWrapper wrapper);
        void setConfig(Cfg::SqlEnterpriseFormatterConfig* cfg);

        static FormatStatement* forQuery(SqliteStatement *query);

        FormatStatement& withKeyword(const QString& kw);
        FormatStatement& withLinedUpKeyword(const QString& kw, const QString& lineUpName = QString());
        FormatStatement& withId(const QString& id);
        FormatStatement& withId(const QString& id, bool wrapIfNeeded);
        FormatStatement& withIdList(const QStringList& names, const QString& indentName = QString(), ListSeparator sep = ListSeparator::COMMA);
        FormatStatement& withOperator(const QString& oper, FormatToken::Flags flags = FormatToken::Flag::NO_FLAG);
        FormatStatement& withStringOrId(const QString& id, FormatToken::Flags flags = FormatToken::Flag::NO_FLAG);
        FormatStatement& withIdDot(FormatToken::Flags flags = FormatToken::Flag::NO_FLAG);
        FormatStatement& withStar(FormatToken::Flags flags = FormatToken::Flag::NO_FLAG);
        FormatStatement& withFloat(double value);
        FormatStatement& withInteger(qint64 value);
        FormatStatement& withString(const QString& value);
        FormatStatement& withBlob(const QString& value);
        FormatStatement& withBindParam(const QString& name);
        FormatStatement& withParDefLeft(FormatToken::Flags flags = FormatToken::Flag::NO_FLAG);
        FormatStatement& withParDefRight(FormatToken::Flags flags = FormatToken::Flag::NO_FLAG);
        FormatStatement& withParExprLeft(FormatToken::Flags flags = FormatToken::Flag::NO_FLAG);
        FormatStatement& withParExprRight(FormatToken::Flags flags = FormatToken::Flag::NO_FLAG);
        FormatStatement& withParFuncLeft(FormatToken::Flags flags = FormatToken::Flag::NO_FLAG);
        FormatStatement& withParFuncRight(FormatToken::Flags flags = FormatToken::Flag::NO_FLAG);
        FormatStatement& withSemicolon(FormatToken::Flags flags = FormatToken::Flag::NO_FLAG);
        FormatStatement& withListComma(FormatToken::Flags flags = FormatToken::Flag::NO_FLAG);
        FormatStatement& withCommaOper(FormatToken::Flags flags = FormatToken::Flag::NO_FLAG);
        FormatStatement& withSortOrder(SqliteSortOrder sortOrder);
        FormatStatement& withConflict(SqliteConflictAlgo onConflict);
        FormatStatement& withFuncId(const QString& func);
        FormatStatement& withDataType(const QString& dataType);
        FormatStatement& withNewLine();
        FormatStatement& withLiteral(const QVariant& value);
        FormatStatement& withStatement(SqliteStatement* stmt, const QString& indentName = QString(), FormatStatementEnricher enricher = nullptr);
        FormatStatement& markIndent(const QString& name);
        FormatStatement& markAndKeepIndent(const QString& name);
        FormatStatement& withIncrIndent(int newIndent);
        FormatStatement& withIncrIndent(const QString& name = QString());
        FormatStatement& withDecrIndent();
        FormatStatement& markKeywordLineUp(const QString& keyword, const QString& lineUpName = QString());
        FormatStatement& withSeparator(ListSeparator sep, FormatToken::Flags flags = FormatToken::Flag::NO_FLAG);

        template <class T>
        FormatStatement& withStatementList(QList<T*> stmtList, FormatToken::Flags flags)
        {
            return withStatementList(stmtList, QString(), ListSeparator::COMMA, nullptr, flags);
        }

        template <class T>
        FormatStatement& withStatementList(QList<T*> stmtList, const QString& indentName = QString(), ListSeparator sep = ListSeparator::COMMA,
                                           FormatStatementEnricher enricher = nullptr, FormatToken::Flags flags = FormatToken::Flag::NO_FLAG)
        {
            if (!indentName.isNull())
                markAndKeepIndent(indentName);

            bool first = true;
            for (T* stmt : stmtList)
            {
                if (!first)
                    withSeparator(sep, flags);

                withStatement(stmt, QString(), enricher);
                first = false;
            }

            if (!indentName.isNull())
                withDecrIndent();

            return *this;
        }

        template <class T>
        T* getFormatStatement(SqliteStatement* stmt)
        {
            return dynamic_cast<T*>(forQuery(stmt, wrapper, cfg));
        }

    protected:
        void handleExplainQuery(SqliteQuery* query);

        virtual void formatInternal() = 0;
        virtual void resetInternal();

        NameWrapper wrapper = NameWrapper::BRACKET;
        Cfg::SqlEnterpriseFormatterConfig* cfg = nullptr;

    private:
        FormatToken* withToken(FormatToken::Type type, const QVariant& value, const QVariant& additionalValue = QVariant(), FormatToken::Flags flags = FormatToken::Flag::NO_FLAG);
        FormatToken* withToken(FormatToken::Type type, const QVariant& value, FormatToken::Flags flags);
        void cleanup();
        void buildTokens();
        bool applyIndent();
        void applySpace(FormatToken::Type type);
        bool isSpaceExpectingType(FormatToken::Type type);
        bool isMetaType(FormatToken::Type type);
        void newLine();
        void incrIndent(const QString& name = QString());
        void decrIndent();
        void setIndent(int newIndent);
        bool endsWithSpace();
        FormatToken* getLastRealToken(bool skipNewLines = false);
        FormatToken* getLastToken();
        QString detokenize();
        void detokenizeLeftPar(FormatToken* token, bool spaceBefore, bool spaceAfter, bool nlBefore, bool nlAfter);
        void detokenizeRightPar(FormatToken* token, bool spaceBefore, bool spaceAfter, bool nlBefore, bool nlAfter);
        void resetIndents();
        void removeAllSpaces();
        void removeAllSpacesFromLine();
        void updateLastToken(FormatToken* token);
        QString getFinalLineUpName(const QString& lineUpName);
        int predictCurrentIndent(FormatToken* currentMetaToken);
        bool willStartWithNewLine(FormatToken* token);
        void formatId(const QString& value, bool wrappingRequested);
        int getLineUpValue(const QString& lineUpName);

        static FormatStatement* forQuery(SqliteStatement* query, NameWrapper wrapper, Cfg::SqlEnterpriseFormatterConfig* cfg);

        QHash<QString,int> kwLineUpPosition;
        QHash<QString,int> namedIndents;
        QStack<int> indents;
        QList<FormatToken*> tokens;
        bool deleteTokens = true;
        QStringList lines;
        QString line;
        FormatToken* lastToken = nullptr;
        QString statementName;
        FormatStatement* parentFormatStatement = nullptr;
        SqliteStatement* thisStatement = nullptr;

        static qint64 nameSeq;
        static const QString SPACE;
        static const QString NEWLINE;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FormatStatement::FormatToken::Flags)

#endif // FORMATSTATEMENT_H
