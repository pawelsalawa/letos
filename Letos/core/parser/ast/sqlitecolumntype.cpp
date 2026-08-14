#include "sqlitecolumntype.h"
#include "parser/statementtokenbuilder.h"
#include "parser/lexer.h"
#include <QRegularExpression>

SqliteColumnType::SqliteColumnType()
{
}

SqliteColumnType::SqliteColumnType(const SqliteColumnType& other) :
    SqliteStatement(other), name(other.name), scale(other.scale), precision(other.precision), hidden(other.hidden)
{
}

SqliteColumnType::SqliteColumnType(const QString &name) :
    SqliteColumnType()
{
    this->name = removeHiddenFromTypeName(name, &hidden);
}

SqliteColumnType::SqliteColumnType(const QString &name, const QVariant& scale) :
    SqliteColumnType(name)
{
    this->scale = scale;
}

SqliteColumnType::SqliteColumnType(const QString &name, const QVariant& scale, const QVariant& precision) :
    SqliteColumnType(name, scale)
{
    this->precision = precision;
}

SqliteStatement* SqliteColumnType::clone()
{
    return new SqliteColumnType(*this);
}

bool SqliteColumnType::isPrecisionDouble()
{
    return !isNull(precision) && precision.toString().indexOf(".") > -1;
}

bool SqliteColumnType::isScaleDouble()
{
    return !isNull(scale) && scale.toString().indexOf(".") > -1;
}

TokenList SqliteColumnType::rebuildTokensFromContents(bool replaceStatementTokens) const
{
    StatementTokenBuilder builder(replaceStatementTokens);

    if (name.isEmpty())
    {
        if (hidden)
            builder.withOther("HIDDEN");

        return builder.build();
    }

    TokenList resultTokens = Lexer::tokenize(name);

    if (!isNull(scale))
    {
        builder.withSpace().withParLeft();
        if (scale.userType() == QMetaType::Int)
            builder.withInteger(scale.toInt());
        else if (scale.userType() == QMetaType::LongLong)
            builder.withInteger(scale.toLongLong());
        else if (scale.userType() == QMetaType::Double)
            builder.withFloat(scale);
        else
            builder.withOther(scale.toString());

        if (!isNull(precision))
        {
            builder.withOperator(",").withSpace();
            if (precision.userType() == QMetaType::Int)
                builder.withInteger(precision.toInt());
            else if (precision.userType() == QMetaType::LongLong)
                builder.withInteger(precision.toLongLong());
            else if (precision.userType() == QMetaType::Double)
                builder.withFloat(precision);
            else
                builder.withOther(precision.toString());
        }
        builder.withParRight();
    }

    TokenList hiddenTokens = hidden ? Lexer::tokenize("HIDDEN ") : TokenList();

    return hiddenTokens + resultTokens + builder.build();
}

DataType SqliteColumnType::toDataType() const
{
    return DataType(name, scale, precision);
}

QString SqliteColumnType::detokenizeWithoutHidden() const
{
    QString value = detokenize();
    return removeHiddenFromTypeName(value);
}

QString SqliteColumnType::removeHiddenFromTypeName(const QString& typeName, bool* isHidden)
{
    QStringList parts = typeName.split(QRegularExpression("\\s+"));
    if (parts.contains("hidden", Qt::CaseInsensitive))
    {
        parts.removeIf([](const QString& part) { return part.compare("hidden", Qt::CaseInsensitive) == 0;});
        if (isHidden)
            *isHidden = true;

        return parts.join(" ");
    }
    return typeName;
}
