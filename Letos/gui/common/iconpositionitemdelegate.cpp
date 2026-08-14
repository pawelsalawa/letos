#include "iconpositionitemdelegate.h"


IconPositionItemDelegate::IconPositionItemDelegate(QObject *parent)
    : QStyledItemDelegate{parent}
{

}

void IconPositionItemDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
{
    QStyledItemDelegate::initStyleOption(option, index);

    const QVariant v = index.data(DecorationPositionRole);
    if (v.isValid())
        option->decorationPosition = static_cast<QStyleOptionViewItem::Position>(v.toInt());
}
