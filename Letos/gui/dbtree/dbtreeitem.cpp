#include "dbtreeitem.h"
#include "iconmanager.h"
#include "dbtreemodel.h"
#include "services/dbmanager.h"
#include "dbtree.h"
#include <QDebug>
#include <db/invaliddb.h>

DbTreeItem::DbTreeItem(DbTreeItem::Type type, const Icon& icon, const QString& nodeName, QObject* parent)
    : DbTreeItem(type, nodeName, parent)
{
    setIcon(icon);
}

DbTreeItem::DbTreeItem(DbTreeItem::Type type, const QString& nodeName, QObject *parent)
    : QObject(parent)
{
    setText(nodeName);
    setType(type);
    init();
}

DbTreeItem::DbTreeItem(const DbTreeItem& item)
    : QObject(item.QObject::parent()), QStandardItem(item)
{
    init();
}

DbTreeItem::DbTreeItem()
{
    setType(Type::ITEM_PROTOTYPE);
    init();
}

void DbTreeItem::initMeta()
{
    qRegisterMetaType<DbTreeItem*>("DbTreeItem*");
}

DbTreeItem::Type DbTreeItem::getType() const
{
    return static_cast<Type>(type());
}

void DbTreeItem::setType(Type type)
{
    setData(static_cast<int>(type), DataRole::TYPE);
}

int DbTreeItem::type() const
{
    return data(DataRole::TYPE).toInt();
}

DbTreeItem* DbTreeItem::findItem(DbTreeItem::Type type, const QString& name)
{
    return DbTreeModel::findItem(this, type, name);
}

DbTreeItem* DbTreeItem::findFirstItem(Type type)
{
    return DbTreeModel::findFirstItem(this, type);
}

QStandardItem* DbTreeItem::clone() const
{
    return new DbTreeItem(*this);
}

QList<QStandardItem *> DbTreeItem::childs() const
{
    QList<QStandardItem *> results;
    for (int i = 0; i < rowCount(); i++)
         results += child(i);

    return results;
}

QList<DbTreeItem*> DbTreeItem::dbTreeChilds() const
{
    QList<DbTreeItem*> results;
    for (int i = 0; i < rowCount(); i++)
         results += dynamic_cast<DbTreeItem*>(child(i));

    return results;
}

QStringList DbTreeItem::childNames() const
{
    QStringList results;
    for (int i = 0; i < rowCount(); i++)
         results += child(i)->text();

    return results;
}

QString DbTreeItem::getTable() const
{
    const DbTreeItem* item = getParentItem(Type::TABLE);
    if (!item)
        return QString();

    return item->text();
}

QString DbTreeItem::getColumn() const
{
    if (getType() != Type::COLUMN)
        return QString();

    return text();
}

QString DbTreeItem::getIndex() const
{
    const DbTreeItem* item = getParentItem(Type::INDEX);
    if (!item)
        return QString();

    return item->text();
}

QString DbTreeItem::getTrigger() const
{
    const DbTreeItem* item = getParentItem(Type::TRIGGER);
    if (!item)
        return QString();

    return item->text();
}

QString DbTreeItem::getView() const
{
    const DbTreeItem* item = getParentItem(Type::VIEW);
    if (!item)
        return QString();

    return item->text();
}

void DbTreeItem::setColumnType(const QString& columnType)
{
    setData(columnType, DataRole::COLUMN_TYPE);
}

QString DbTreeItem::getColumnType() const
{
    return data(DataRole::COLUMN_TYPE).toString();
}

void DbTreeItem::setData(const QVariant& value, int role)
{
    QStandardItem::setData(value, role);
    if (role == Qt::DisplayRole || role == DataRole::TYPE)
        updateSignatureValue();
}

QStandardItem *DbTreeItem::parentItem() const
{
    if (!QStandardItem::parent())
        return model()->invisibleRootItem();

    return QStandardItem::parent();
}

DbTreeItem *DbTreeItem::parentDbTreeItem() const
{
    QStandardItem* parentItem = QStandardItem::parent();
    if (!parentItem)
        return nullptr;

    return dynamic_cast<DbTreeItem*>(parentItem);
}

QList<DbTreeItem *> DbTreeItem::getPathToRoot()
{
    QList<DbTreeItem *> path;
    getPathToRoot(path);
    return path;
}

QList<DbTreeItem*> DbTreeItem::getPathToParentItem(DbTreeItem::Type type)
{
    QList<DbTreeItem*> path;
    getPathToParentItem(path, type);
    return path;
}

QList<DbTreeItem*> DbTreeItem::getPathToParentItem(DbTreeItem::Type type, const QString& name)
{
    QList<DbTreeItem*> path;
    getPathToParentItem(path, type, name);
    return path;
}

DbTreeItem* DbTreeItem::findParentItem(DbTreeItem::Type type)
{
    DbTreeItem* parent = parentDbTreeItem();
    if (!parent)
        return nullptr;

    if (parent->getType() == type)
        return parent;

    return parent->findParentItem(type);
}

DbTreeItem* DbTreeItem::findParentItem(DbTreeItem::Type type, const QString& name)
{
    DbTreeItem* parent = parentDbTreeItem();
    if (!parent)
        return nullptr;

    if (parent->getType() == type && name == parent->text())
        return parent;

    return parent->findParentItem(type);
}

void DbTreeItem::getPathToRoot(QList<DbTreeItem *> &path)
{
    path << this;
    if (parentDbTreeItem())
        parentDbTreeItem()->getPathToRoot(path);
}

QString DbTreeItem::pathSignature() const
{
    return pathSignatureParts().join("_");
}

QStringList DbTreeItem::pathSignatureParts() const
{
    QStringList parts;
    pathSignatureParts(parts);
    return parts;
}

void DbTreeItem::pathSignatureParts(QStringList& parts) const
{
    if (parentDbTreeItem())
        parentDbTreeItem()->pathSignatureParts(parts);

    parts += signature();
}

QString DbTreeItem::signature() const
{
    return data(static_cast<int>(DataRole::SIGNATURE_OF_THIS)).toString();
}

void DbTreeItem::getPathToParentItem(QList<DbTreeItem*>& path, DbTreeItem::Type type)
{
    path << this;
    if (getType() == type)
        return;

    if (parentDbTreeItem())
        parentDbTreeItem()->getPathToParentItem(path, type);
}

void DbTreeItem::getPathToParentItem(QList<DbTreeItem*>& path, DbTreeItem::Type type, const QString& name)
{
    path << this;
    if (getType() == type && name == text())
        return;

    if (parentDbTreeItem())
        parentDbTreeItem()->getPathToParentItem(path, type, name);
}

const DbTreeItem* DbTreeItem::getParentItem(DbTreeItem::Type type) const
{
    if (getType() == type)
        return this;

    DbTreeItem* parent = parentDbTreeItem();
    if (parent)
        return parent->getParentItem(type);

    return nullptr;
}

void DbTreeItem::updateSignatureValue()
{
    setData(QString::number(type()) + "." + QString::fromLatin1(text().toUtf8().toBase64()), static_cast<int>(DataRole::SIGNATURE_OF_THIS));
}

Db* DbTreeItem::getDb() const
{
    QString dbName = data(DataRole::DB).toString();
    return DBLIST->getByName(dbName);
}

void DbTreeItem::setDb(Db* value)
{
    setDb(value->getName());
}

void DbTreeItem::setDb(const QString& dbName)
{
    setData(dbName, DataRole::DB);
    updateDbIcon();
}

void DbTreeItem::updateDbIcon()
{
    if (getType() != DbTreeItem::Type::DB)
        return;

    Db* db = getDb();
    if (db->isValid())
    {
        if (db->isOpen())
            setIcon(ICONS.DATABASE_ONLINE);
        else
            setIcon(ICONS.DATABASE_OFFLINE);
    }
    else
        setIcon(ICONS.DATABASE_INVALID);
}

const Icon* DbTreeItem::getIcon() const
{
    return data(DataRole::ICON_PTR).value<const Icon*>();
}

void DbTreeItem::setHidden(bool hidden)
{
    setData(hidden, DataRole::HIDDEN);
    dynamic_cast<DbTreeModel*>(model())->itemChangedVisibility(this);
}

bool DbTreeItem::isHidden() const
{
    return data(DataRole::HIDDEN).toBool();
}

void DbTreeItem::setIcon(const Icon& icon)
{
    setData(QVariant::fromValue(&icon), DataRole::ICON_PTR);
    if (!icon.isNull())
        QStandardItem::setIcon(icon);
}

bool DbTreeItem::isSchemaReady() const
{
    return data(DataRole::SCHEMA_READY).toBool();
}

void DbTreeItem::setSchemaReady(bool ready)
{
    setData(ready, DataRole::SCHEMA_READY);
}

void DbTreeItem::setRestoredExpandState(bool state)
{
    setData(state, DataRole::EXPAND_STATE_RESTORED);
}

bool DbTreeItem::getRestoredExpandState() const
{
    return data(DataRole::EXPAND_STATE_RESTORED).toBool();
}

void DbTreeItem::setShadowTable(bool shadow)
{
    setData(shadow, DataRole::SHADOW_TABLE);
}

bool DbTreeItem::isShadowTable() const
{
    return data(DataRole::SHADOW_TABLE).toBool();
}

void DbTreeItem::init()
{
    Type type = getType();
    switch (type)
    {
        case DbTreeItem::Type::DIR:
        case DbTreeItem::Type::DB:
        case DbTreeItem::Type::TABLE:
        case DbTreeItem::Type::INDEX:
        case DbTreeItem::Type::TRIGGER:
        case DbTreeItem::Type::VIEW:
        case DbTreeItem::Type::VIRTUAL_TABLE:
        case DbTreeItem::Type::COLUMN:
            setEditable(true);
            break;
        case DbTreeItem::Type::TABLES:
        case DbTreeItem::Type::INDEXES:
        case DbTreeItem::Type::TRIGGERS:
        case DbTreeItem::Type::VIEWS:
        case DbTreeItem::Type::COLUMNS:
        case DbTreeItem::Type::ITEM_PROTOTYPE:
            setEditable(false);
            break;
    }

    setData(false, DataRole::HIDDEN);

    Qt::ItemFlags f = flags();
    if (DbTree::isItemDraggable(this))
        f |= Qt::ItemIsDragEnabled;
    else
        f ^= Qt::ItemIsDragEnabled;

    setFlags(f);
}

QDataStream &operator <<(QDataStream &out, const DbTreeItem *item)
{
    out << item->pathSignatureParts();
    return out;
}

QDataStream &operator >>(QDataStream &in, DbTreeItem *&item)
{
    QStringList signature;
    in >> signature;
    item = DBTREE->getModel()->findItemBySignature(signature);
    return in;
}

size_t qHash(DbTreeItem::Type type)
{
    return static_cast<size_t>(type);
}
