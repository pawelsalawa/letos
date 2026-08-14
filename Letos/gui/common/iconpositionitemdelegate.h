#ifndef ICONPOSITIONITEMDELEGATE_H
#define ICONPOSITIONITEMDELEGATE_H

#include <QStyledItemDelegate>

class IconPositionItemDelegate : public QStyledItemDelegate
{
    public:
        explicit IconPositionItemDelegate(QObject *parent = nullptr);

        void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;

        static constexpr const int DecorationPositionRole = Qt::UserRole + 3000;
};

#endif // ICONPOSITIONITEMDELEGATE_H
