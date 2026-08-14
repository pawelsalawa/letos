#include "dbtreeitemfactory.h"
#include "iconmanager.h"

DbTreeItem *DbTreeItemFactory::createDir(const QString &name, QObject *parent)
{
    return new DbTreeItem(DbTreeItem::Type::DIR, ICONS.DIRECTORY, name, parent);
}

DbTreeItem *DbTreeItemFactory::createDb(const QString &name, QObject *parent)
{
    return new DbTreeItem(DbTreeItem::Type::DB, ICONS.DATABASE_OFFLINE, name, parent);
}

DbTreeItem *DbTreeItemFactory::createTable(const QString &name, QObject *parent)
{
    return new DbTreeItem(DbTreeItem::Type::TABLE, ICONS.TABLE, name, parent);
}

DbTreeItem* DbTreeItemFactory::createShadowTable(const QString& name, QObject* parent)
{
    DbTreeItem* item = new DbTreeItem(DbTreeItem::Type::TABLE, ICONS.SHADOW_TABLE, name, parent);
    item->setShadowTable(true);
    return item;
}

DbTreeItem* DbTreeItemFactory::createVirtualTable(const QString& name, QObject* parent)
{
    return new DbTreeItem(DbTreeItem::Type::VIRTUAL_TABLE, ICONS.VIRTUAL_TABLE, name, parent);
}

DbTreeItem *DbTreeItemFactory::createIndex(const QString &name, QObject *parent)
{
    return new DbTreeItem(DbTreeItem::Type::INDEX, ICONS.INDEX, name, parent);
}

DbTreeItem *DbTreeItemFactory::createTrigger(const QString &name, QObject *parent)
{
    return new DbTreeItem(DbTreeItem::Type::TRIGGER, ICONS.TRIGGER, name, parent);
}

DbTreeItem *DbTreeItemFactory::createView(const QString &name, QObject *parent)
{
    return new DbTreeItem(DbTreeItem::Type::VIEW, ICONS.VIEW, name, parent);
}

DbTreeItem *DbTreeItemFactory::createColumn(const QString &name, QObject *parent)
{
    return new DbTreeItem(DbTreeItem::Type::COLUMN, ICONS.COLUMN, name, parent);
}

DbTreeItem* DbTreeItemFactory::createColumn(const QString& name, const QString& type, bool hidden, QObject* parent)
{
    auto item = new DbTreeItem(DbTreeItem::Type::COLUMN, hidden ? ICONS.HIDDEN_COLUMN : ICONS.COLUMN, name, parent);
    item->setColumnType(type);
    return item;
}

DbTreeItem *DbTreeItemFactory::createTables(QObject *parent)
{
    return new DbTreeItem(DbTreeItem::Type::TABLES, ICONS.TABLES, QObject::tr("Tables"), parent);
}

DbTreeItem *DbTreeItemFactory::createIndexes(QObject *parent)
{
    return new DbTreeItem(DbTreeItem::Type::INDEXES, ICONS.INDEXES, QObject::tr("Indexes"), parent);
}

DbTreeItem *DbTreeItemFactory::createTriggers(QObject *parent)
{
    return new DbTreeItem(DbTreeItem::Type::TRIGGERS, ICONS.TRIGGERS, QObject::tr("Triggers"), parent);
}

DbTreeItem *DbTreeItemFactory::createViews(QObject *parent)
{
    return new DbTreeItem(DbTreeItem::Type::VIEWS, ICONS.VIEWS, QObject::tr("Views"), parent);
}

DbTreeItem *DbTreeItemFactory::createColumns(QObject *parent)
{
    return new DbTreeItem(DbTreeItem::Type::COLUMNS, ICONS.COLUMNS, QObject::tr("Columns"), parent);
}

DbTreeItem *DbTreeItemFactory::createPrototype(QObject *parent)
{
    Q_UNUSED(parent);
    return new DbTreeItem();
}
