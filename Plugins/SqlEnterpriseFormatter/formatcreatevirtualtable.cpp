#include "formatcreatevirtualtable.h"
#include "parser/ast/sqlitecreatevirtualtable.h"
#include "parser/lexer.h"

FormatCreateVirtualTable::FormatCreateVirtualTable(SqliteCreateVirtualTable* cvt) :
    FormatStatement(cvt), cvt(cvt)
{
}

void FormatCreateVirtualTable::formatInternal()
{
    handleExplainQuery(cvt);
    withKeyword("CREATE").withKeyword("VIRTUAL").withKeyword("TABLE");
    if (cvt->ifNotExistsKw)
        withKeyword("IF").withKeyword("NOT").withKeyword("EXISTS");

    if (!cvt->database.isNull())
        withId(cvt->database).withIdDot();

    withId(cvt->table).withKeyword("USING").withId(cvt->module);
    if (!cvt->args.isEmpty())
    {
        withParDefLeft();
        int i = 0;
        for (const QString& arg : cvt->args)
        {
            if (i > 0)
                withListComma();

            for (const TokenPtr& tk : Lexer::tokenize(arg))
                handleToken(tk);

            i++;
        }
        withParDefRight();
    }

    withSemicolon();
}

void FormatCreateVirtualTable::handleToken(const TokenPtr& token)
{
    switch (token->type)
    {
        case Token::OTHER:
        case Token::STRING:
        case Token::KEYWORD:
            withId(token->value, false);
            break;
        case Token::COMMENT:
            // TODO Format comment here
            break;
        case Token::FLOAT:
            withFloat(token->value.toDouble());
            break;
        case Token::INTEGER:
            withInteger(token->value.toLongLong());
            break;
        case Token::BIND_PARAM:
            withBindParam(token->value);
            break;
        case Token::OPERATOR:
            withOperator(token->value);
            break;
        case Token::PAR_LEFT:
            withParDefLeft();
            break;
        case Token::PAR_RIGHT:
            withParDefRight();
            break;
        case Token::BLOB:
            withBlob(token->value);
            break;
        case Token::SPACE:
        case Token::INVALID:
        case Token::CTX_COLUMN:
        case Token::CTX_TABLE:
        case Token::CTX_DATABASE:
        case Token::CTX_FUNCTION:
        case Token::CTX_COLLATION:
        case Token::CTX_INDEX:
        case Token::CTX_TRIGGER:
        case Token::CTX_VIEW:
        case Token::CTX_JOIN_OPTS:
        case Token::CTX_TABLE_NEW:
        case Token::CTX_INDEX_NEW:
        case Token::CTX_VIEW_NEW:
        case Token::CTX_TRIGGER_NEW:
        case Token::CTX_ALIAS:
        case Token::CTX_TRANSACTION:
        case Token::CTX_COLUMN_NEW:
        case Token::CTX_COLUMN_TYPE:
        case Token::CTX_CONSTRAINT:
        case Token::CTX_FK_MATCH:
        case Token::CTX_PRAGMA:
        case Token::CTX_ROWID_KW:
        case Token::CTX_STRICT_KW:
        case Token::CTX_NEW_KW:
        case Token::CTX_OLD_KW:
        case Token::CTX_ERROR_MESSAGE:
        case Token::CTX_JOIN_EXPR:
            break;
    }
}

