#include "formatcolumntype.h"
#include "parser/ast/sqlitecolumntype.h"
#include "sqlenterpriseformatter.h"

FormatColumnType::FormatColumnType(SqliteColumnType* colType) :
    FormatStatement(colType), colType(colType)
{
}

void FormatColumnType::formatInternal()
{
    if (colType->hidden)
        withId("HIDDEN");

    if (colType->name.isEmpty())
        return;

    withId(cfg->SqlEnterpriseFormatter.UppercaseDataTypes.get() ? colType->name.toUpper() : colType->name.toLower());

    if (!isNull(colType->scale))
    {
        withParExprLeft();
        if (colType->scale.userType() == QMetaType::Int)
            withInteger(colType->scale.toInt());
        else if (colType->scale.userType() == QMetaType::LongLong)
            withInteger(colType->scale.toLongLong());
        else if (colType->scale.userType() == QMetaType::Double)
            withFloat(colType->scale.toDouble());
        else
            withId(colType->scale.toString());

        if (!isNull(colType->precision))
        {
            withCommaOper();
            if (colType->precision.userType() == QMetaType::Int)
                withInteger(colType->precision.toInt());
            else if (colType->precision.userType() == QMetaType::LongLong)
                withInteger(colType->precision.toLongLong());
            else if (colType->precision.userType() == QMetaType::Double)
                withFloat(colType->precision.toDouble());
            else
                withId(colType->precision.toString());
        }
        withParExprRight();
    }
}
