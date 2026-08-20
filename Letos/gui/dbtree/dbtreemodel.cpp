#include "dbtreemodel.h"
#include "services/dbmanager.h"
#include "dbtreeview.h"
#include "iconmanager.h"
#include "uiconfig.h"
#include "schemaresolver.h"
#include "dbtreeitemfactory.h"
#include "plugins/dbplugin.h"
#include "dbobjectorganizer.h"
#include "dialogs/dbdialog.h"
#include "dialogs/errorsconfirmdialog.h"
#include "dialogs/versionconvertsummarydialog.h"
#include "db/invaliddb.h"
#include "services/notifymanager.h"
#include <QMimeData>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QInputDialog>
#include <QCheckBox>
#include <QWidgetAction>
#include <QClipboard>
#include <QElapsedTimer>

const QString DbTreeModel::toolTipTableTmp = R"(<table>%1</table>)";
const QString DbTreeModel::toolTipHdrRowTmp = R"(<tr><th><img src="%1" width="16" height="16"/></th><th colspan=2>%2</th></tr>)";
const QString DbTreeModel::toolTipRowTmp = R"(<tr><td></td><td>%1</td><td align="right">%2</td></tr>)";
const QString DbTreeModel::toolTipSecondaryHdrRowTmp = R"(<tr><td colspan=3><p align="center">%1</p></td></tr>)";
const QString DbTreeModel::toolTipIconRowTmp = R"(<tr><td><img src="%1" width="16" height="16"/></td><td>%2</td><td align="right">%3</td></tr>)";
const QString DbTreeModel::toolTipFooterRowTmp = R"(<tr><td></td><td colspan=2><p align="center"><i>%1</i></p></td></tr>)";

DbTreeModel::DbTreeModel()
{
    setItemPrototype(DbTreeItemFactory::createPrototype());
    connectDbManagerSignals();

    connect(CFG, SIGNAL(massSaveBegins()), this, SLOT(massSaveBegins()));
    connect(CFG, SIGNAL(massSaveCommitted()), this, SLOT(massSaveCommitted()));
    connect(CFG_UI.DbList.ShowSystemObjects, SIGNAL(changed(QVariant)), this, SLOT(markSchemaReloadingRequired()));
    connect(CFG_UI.DbList.ShowRegularTableLabels, SIGNAL(changed(QVariant)), this, SLOT(markSchemaReloadingRequired()));

    dbOrganizer = new DbObjectOrganizer(confirmReferencedTables, resolveNameConflict, confirmConversion, confirmConversionErrors);
    dbOrganizer->setAutoDelete(false);
    connect(dbOrganizer, SIGNAL(finishedDbObjectsCopy(bool,Db*,Db*)), this, SLOT(dbObjectsCopyFinished(bool,Db*,Db*)));
    connect(dbOrganizer, SIGNAL(finishedDbObjectsMove(bool,Db*,Db*)), this, SLOT(dbObjectsMoveFinished(bool,Db*,Db*)));
}

DbTreeModel::~DbTreeModel()
{
}

void DbTreeModel::connectDbManagerSignals()
{
    connect(DBLIST, SIGNAL(dbAdded(Db*)), this, SLOT(dbAdded(Db*)));
    connect(DBLIST, SIGNAL(dbUpdated(QString,Db*)), this, SLOT(dbUpdated(QString,Db*)));
    connect(DBLIST, SIGNAL(dbRemoved(Db*)), this, SLOT(dbRemoved(Db*)));
    connect(DBLIST, SIGNAL(dbConnected(Db*)), this, SLOT(dbConnected(Db*)));
    connect(DBLIST, SIGNAL(dbDisconnected(Db*)), this, SLOT(dbDisconnected(Db*)));
    connect(DBLIST, SIGNAL(dbLoaded(Db*)), this, SLOT(dbLoaded(Db*)));
    connect(DBLIST, SIGNAL(dbUnloaded(Db*)), this, SLOT(dbUnloaded(Db*)));
}

void DbTreeModel::move(QStandardItem *itemToMove, QStandardItem *newParentItem, int newRow)
{
    QStandardItem* currParent = dynamic_cast<DbTreeItem*>(itemToMove)->parentItem();
    if (!newParentItem)
        newParentItem = root();

    if (newParentItem == currParent)
    {
        move(itemToMove, newRow);
        return;
    }

    int oldRow = itemToMove->index().row();
    currParent->takeRow(oldRow);

    if (newRow > newParentItem->rowCount() || newRow < 0)
        newParentItem->appendRow(itemToMove);
    else
        newParentItem->insertRow(newRow, itemToMove);
}

void DbTreeModel::move(QStandardItem *itemToMove, int newRow)
{
    QStandardItem* currParent = dynamic_cast<DbTreeItem*>(itemToMove)->parentItem();
    int oldRow = itemToMove->index().row();
    currParent->takeRow(oldRow);
    if (newRow > currParent->rowCount() || newRow  < 0)
        currParent->appendRow(itemToMove);
    else if (oldRow < newRow)
        currParent->insertRow(newRow - 1, itemToMove);
    else
        currParent->insertRow(newRow, itemToMove);
}

void DbTreeModel::deleteGroup(QStandardItem *groupItem)
{
    QStandardItem* parentItem = dynamic_cast<DbTreeItem*>(groupItem)->parentItem();
    if (!parentItem)
        parentItem = root();

    int newRow = groupItem->index().row();
    for (QStandardItem* child : dynamic_cast<DbTreeItem*>(groupItem)->childs())
        move(child, parentItem, newRow);

    parentItem->removeRow(groupItem->row());
}

void DbTreeModel::deleteIndexesAfterMove(const QList<DbTreeItem*>& items)
{
    for (DbTreeItem* item: items)
    {
        QStandardItem* par = item->parentItem();
        par->removeRow(item->row());
    }
}

DbTreeItem* DbTreeModel::createGroup(const QString& name, QStandardItem* parent)
{
    if (!parent)
        parent = root();

    DbTreeItem* item = DbTreeItemFactory::createDir(name, this);
    parent->appendRow(item);
    return item;
}

QStringList DbTreeModel::getGroupFor(QStandardItem *item)
{
    QStringList group;
    while ((item = item->parent()) != nullptr)
    {
        if (dynamic_cast<DbTreeItem*>(item)->getType() == DbTreeItem::Type::DIR)
            group.prepend(item->text());
    }
    return group;
}

void DbTreeModel::applyFilter(const QString &filter)
{
    applyFilter(root(), filter);
}

void DbTreeModel::applyFilter(QStandardItem *parentItem, const QString &filter)
{
    queuedFilterValue.clear();
    if (filteringInProgress) // already processing the (possibly interrupted) filtering - subsequent call is possible after filter edit field is cleared
    {
        if (filter != currentFilter)
        {
            queuedFilterValue = filter;
            filteringInProgress->interrupt();
        }
        return;
    }

    filteringInProgress = new ItemFiltering();
    interruptableStarted(filteringInProgress);

    currentFilter = filter;
    applyFilterRecursively(parentItem, filter);

    if (filteringInProgress->interrupted && queuedFilterValue.isNull())
    {
        applyFilterRecursively(parentItem, "");
        currentFilter = "";
        emit filteringInterrupted();
    }
    interruptableFinished(filteringInProgress);

    safe_delete(filteringInProgress);

    // Subsequent filtering is user modified filter value during filtering in progress
    while (!queuedFilterValue.isNull())
    {
        QString newFilterValue = queuedFilterValue;
        queuedFilterValue.clear();
        applyFilter(root(), newFilterValue);
    }
}

bool DbTreeModel::applyFilterRecursively(QStandardItem *parentItem, const QString &filter)
{
    bool empty = filter.isEmpty();
    bool visibilityForParent = filteringInProgress->interrupted; // if interrupted, this is true by default, otherwise it's false at start and calculated later
    for (int i = 0; i < parentItem->rowCount(); i++)
    {
        DbTreeItem* item = dynamic_cast<DbTreeItem*>(parentItem->child(i));
        if (filteringInProgress->interrupted)
        {
            applyFilterRecursively(item, filter);
            QModelIndex index = item->index();
            treeView->setRowHidden(index.row(), index.parent(), false);
            continue;
        }

        if (!filter.isEmpty())
        {
            if (item->getType() == DbTreeItem::Type::TABLE)
                loadTableSchema(item);
            else if (item->getType() == DbTreeItem::Type::VIEW)
                loadViewSchema(item);
        }

        QModelIndex index = item->index();
        bool subFilterResult = applyFilterRecursively(item, filter);
        bool matched = empty || subFilterResult || item->text().contains(filter, Qt::CaseInsensitive);
        treeView->setRowHidden(index.row(), index.parent(), !matched);

        if (matched)
            visibilityForParent = true;

        if (i % 100 == 99)
            qApp->processEvents();
    }
    return visibilityForParent;
}

void DbTreeModel::storeGroups()
{
    QList<Config::DbGroupPtr> groups = childsToConfig(invisibleRootItem());
    CFG->storeGroups(groups);
}

void DbTreeModel::readGroups(QList<Db*> dbList)
{
    QList<Config::DbGroupPtr> groups = CFG->getGroups();
    for (Config::DbGroupPtr& group : groups)
        restoreGroup(group, &dbList);

    // Add rest of databases, not mentioned in groups
    Config::DbGroupPtr group;
    for (Db*& db : dbList)
    {
        group = Config::DbGroupPtr::create();
        group->referencedDbName = db->getName();
        restoreGroup(group);
    }
}

QList<Config::DbGroupPtr> DbTreeModel::childsToConfig(QStandardItem *item)
{
    QList<Config::DbGroupPtr> groups;
    Config::DbGroupPtr group;
    DbTreeItem* dbTreeItem = nullptr;
    for (int i = 0; i < item->rowCount(); i++)
    {
        dbTreeItem = dynamic_cast<DbTreeItem*>(item->child(i));
        switch (dbTreeItem->getType())
        {
            case DbTreeItem::Type::DIR:
            {
                group = Config::DbGroupPtr::create();
                group->name = dbTreeItem->text();
                group->order = i;
                group->open = treeView->isExpanded(dbTreeItem->index());
                group->childs = childsToConfig(dbTreeItem);
                groups += group;
                break;
            }
            case DbTreeItem::Type::DB:
            {
                group = Config::DbGroupPtr::create();
                group->referencedDbName = dbTreeItem->text();
                group->order = i;
                group->open = dbTreeItem->getDb()->isOpen();
                group->dbExpanded = treeView->isExpanded(dbTreeItem->index());
                groups += group;
                break;
            }
            default:
                // no-op
                break;
        }
    }
    return groups;
}

void DbTreeModel::restoreGroup(const Config::DbGroupPtr& group, QList<Db*>* dbList, QStandardItem* parent)
{
    Db* db = nullptr;
    DbTreeItem* item = nullptr;
    if (group->referencedDbName.isNull())
    {
        item = DbTreeItemFactory::createDir(group->name, this);
        item->setRestoredExpandState(group->open);
    }
    else
    {
        db = DBLIST->getByName(group->referencedDbName);

        // Databases referenced in groups must exist on database list. If not, we ignore them.
        // Even invalid (no plugin, no file) databases have entry in dblist.
        if (!db)
            return;

        item = DbTreeItemFactory::createDb(group->referencedDbName, this);
        item->setDb(group->referencedDbName);
        if (dbList)
            dbList->removeOne(db);
    }

    if (!parent)
        parent = invisibleRootItem();

    parent->appendRow(item);

    if (item->getType() == DbTreeItem::Type::DIR)
    {
        for (const Config::DbGroupPtr& childGroup : group->childs)
            restoreGroup(childGroup, dbList, item);
    }

    if (group->open)
    {
        if (db)
        {
            // If the db was stored in cfg as open, it was already open by DbManager.
            // Now the DbTreeModel didn't catch that (as it didn't exist yet), so we need to
            // call handler for 'connected' event, instead of forcing another open call.
            // Otherwise the database that could not be open would be requested to open twice:
            // 1. when restoring DbManager
            // 2. here
            // Instead of that, we just check if the database is already open (by DbManager)
            // and call proper handler to refresh database's schema and create tree nodes.
            if (db->isOpen())
                dbConnected(db, group->dbExpanded);
        }
        else
        {
            treeView->expand(item->index());
        }
    }
}

void DbTreeModel::expanded(const QModelIndex &index)
{
    QStandardItem* item = itemFromIndex(index);
    if (!item->hasChildren())
    {
        treeView->collapse(index);
        return;
    }

    DbTreeItem* dbTreeItem = dynamic_cast<DbTreeItem*>(item);
    if (dbTreeItem->getType() == DbTreeItem::Type::TABLE || dbTreeItem->getType() == DbTreeItem::Type::VIRTUAL_TABLE)
        loadTableSchema(dbTreeItem);

    if (dbTreeItem->getType() == DbTreeItem::Type::VIEW)
        loadViewSchema(dbTreeItem);

    if (dbTreeItem->getType() == DbTreeItem::Type::DIR)
        itemFromIndex(index)->setIcon(ICONS.DIRECTORY_OPEN);
}

void DbTreeModel::collapsed(const QModelIndex &index)
{
    QStandardItem* item = itemFromIndex(index);
    if (dynamic_cast<DbTreeItem*>(item)->getType() == DbTreeItem::Type::DIR)
        item->setIcon(ICONS.DIRECTORY);
}

void DbTreeModel::dbAdded(Db* db)
{
    DbTreeItem* item = DbTreeItemFactory::createDb(db->getName(), this);
    item->setDb(db);
    root()->appendRow(item);
    emit dbItemAdded(item);
}

void DbTreeModel::dbUpdated(const QString& oldName, Db* db)
{
    DbTreeItem* item = dynamic_cast<DbTreeItem*>(findItem(DbTreeItem::Type::DB, oldName));
    if (!item)
    {
        qWarning() << "Updated database in db model that couldn't be found in the model:" << oldName;
        return;
    }

    item->setText(db->getName());
    item->setDb(db->getName());
    item->updateDbIcon();
}

void DbTreeModel::dbRemoved(Db* db)
{
    dbRemoved(db->getName());
}

void DbTreeModel::dbRemoved(const QString& name)
{
    QStandardItem* item = findItem(DbTreeItem::Type::DB, name);
    if (!item)
    {
        qWarning() << "Removed database from db model that couldn't be found in the model:" << name;
        return;
    }
    dbRemoved(item);
}

void DbTreeModel::dbRemoved(QStandardItem* item)
{
    QStandardItem* parent = item->parent();
    if (!parent)
        parent = root();

    parent->removeRow(item->index().row());
    if (!parent->hasChildren())
        treeView->collapse(parent->index());
}

void DbTreeModel::interrupt()
{
    for (Interruptable* i : interruptables)
        i->interrupt();
}

void DbTreeModel::refreshSchema(Db* db)
{
    QStandardItem* item = findItem(DbTreeItem::Type::DB, db);
    if (!item)
    {
        qWarning() << "Refreshing schema of db that couldn't be found in the model:" << db->getName();
        return;
    }
    refreshSchema(db, item);
    applyFilter(item, currentFilter);
}

QList<DbTreeItem*> DbTreeModel::getAllItemsAsFlatList() const
{
    return getChildsAsFlatList(root());
}

QList<DbTreeItem*> DbTreeModel::getChildsAsFlatList(QStandardItem* item) const
{
    QList<DbTreeItem*> items;
    QStandardItem* child = nullptr;
    for (int i = 0; i < item->rowCount(); i++)
    {
        child = item->child(i);
        items << dynamic_cast<DbTreeItem*>(child);
        items += getChildsAsFlatList(child);
    }
    return items;
}

QVariant DbTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QStandardItemModel::data(index, role);;

    DbTreeItem* item = dynamic_cast<DbTreeItem*>(itemFromIndex(index));
    switch (role)
    {
        case Qt::ToolTipRole:
        {
            return getToolTip(item);
        }
    }
    return QStandardItemModel::data(index, role);
}

QString DbTreeModel::getToolTip(DbTreeItem* item) const
{
    if (!item)
        return QString();

    switch (item->getType())
    {
        case DbTreeItem::Type::DB:
            return getDbToolTip(item);
        case DbTreeItem::Type::TABLE:
        case DbTreeItem::Type::VIRTUAL_TABLE:
            return getTableToolTip(item);
        default:
            break;
    }
    return QString();
}

QString DbTreeModel::getDbToolTip(DbTreeItem* item) const
{
    QStringList rows;

    Db* db = item->getDb();
    QString iconPath = db->isValid() ? ICONS.DATABASE.toImgSrc() : ICONS.DATABASE_INVALID.toImgSrc();
    int fileSize = -1;

    QUrl url(db->getPath());
    if (url.scheme().isEmpty() || url.scheme() == "file")
        fileSize = QFile(db->getPath()).size();

    rows << toolTipHdrRowTmp.arg(iconPath, tr("Database: %1", "dbtree tooltip").arg(db->getName()));
    rows << toolTipRowTmp.arg(tr("URI:", "dbtree tooltip"), toNativePath(db->getPath()));

    if (db->isValid())
    {
        rows << toolTipRowTmp.arg(tr("Format:", "dbtree tooltip"), db->getTypeLabel());

        if (fileSize > -1)
            rows << toolTipRowTmp.arg(tr("File size:", "dbtree tooltip"), formatFileSize(fileSize));

        if (db->isOpen())
            rows << toolTipRowTmp.arg(tr("Encoding:", "dbtree tooltip"), db->getEncoding());
    }
    else
    {
        InvalidDb* idb = dynamic_cast<InvalidDb*>(db);
        rows << toolTipRowTmp.arg(tr("Error:", "dbtree tooltip"), idb->getError());
    }

    if (!db->isValid() || db->isOpen())
        rows << toolTipFooterRowTmp.arg(tr("Double-click to edit this database", "dbtree tooltip"));
    else
        rows << toolTipFooterRowTmp.arg(tr("Double-click to connect to this database", "dbtree tooltip"));

    return toolTipTableTmp.arg(rows.join(""));
}

QString DbTreeModel::getTableToolTip(DbTreeItem* item) const
{
    const_cast<DbTreeModel*>(this)->loadTableSchema(item); // not nice to const_cast, but nothing better we can do about this

    QStringList rows;
    rows << toolTipHdrRowTmp.arg(ICONS.TABLE.getPath(), tr("Table : %1", "dbtree tooltip").arg(item->text()));

    if (item->getType() == DbTreeItem::Type::VIRTUAL_TABLE)
        rows << toolTipSecondaryHdrRowTmp.arg(tr("(virtual table)", "dbtree tooltip"));
    else if (item->isShadowTable())
        rows << toolTipSecondaryHdrRowTmp.arg(tr("(shadow table)", "dbtree tooltip"));

    QStandardItem* columnsItem = item->child(0);
    QStandardItem* indexesItem = item->child(1);
    QStandardItem* triggersItem = item->child(2);

    int columnCnt = columnsItem->rowCount();
    int indexesCount = indexesItem->rowCount();
    int triggersCount = triggersItem->rowCount();

    QStringList columns;
    for (int i = 0; i < columnCnt; i++)
        columns << columnsItem->child(i)->text();

    QStringList indexes;
    for (int i = 0; i < indexesCount; i++)
        indexes << indexesItem->child(i)->text();

    QStringList triggers;
    for (int i = 0; i < triggersCount; i++)
        triggers << triggersItem->child(i)->text();

    rows << toolTipIconRowTmp.arg(ICONS.COLUMN.getPath(),
                                  tr("Columns (%1):", "dbtree tooltip").arg(columnCnt),
                                  columns.join(", "));
    rows << toolTipIconRowTmp.arg(ICONS.INDEX.getPath(),
                                  tr("Indexes (%1):", "dbtree tooltip").arg(indexesCount),
                                  indexes.join(", "));
    rows << toolTipIconRowTmp.arg(ICONS.TRIGGER.getPath(),
                                  tr("Triggers (%1):", "dbtree tooltip").arg(triggersCount),
                                  triggers.join(", "));

    return toolTipTableTmp.arg(rows.join(""));
}

void DbTreeModel::refreshSchema(Db* db, QStandardItem *item)
{
    if (!db->isOpen())
        return;

    // Remember expanded state of this branch
    QHash<QString, bool> expandedState;
    collectExpandedState(expandedState, item);

    // Remember selection state
    QHash<QString, QVariant> selectionState = collectSelectionState();

    // Delete child nodes
    while (item->rowCount() > 0)
        item->removeRow(0);

    // Now prepare to create new branch
    SchemaResolver resolver(db);
    resolver.setIgnoreSystemObjects(!CFG_UI.DbList.ShowSystemObjects.get());

    // Collect all db objects and build the db branch
    bool sort = CFG_UI.DbList.SortObjects.get();
    bool showShadow = CFG_UI.DbList.ShowShadowTables.get();
    QList<SchemaResolver::TableListItem> tableListItems = resolver.getAllTableListItems();
    QStringList tables;
    QStringList views;
    QSet<QString> virtualTables;
    QSet<QString> shadowTables;

    for (SchemaResolver::TableListItem& tableListItem : tableListItems)
    {
        switch (tableListItem.type)
        {
            case SchemaResolver::TableListItem::VIRTUAL_TABLE:
                virtualTables << tableListItem.name;
                tables << tableListItem.name;
                break;
            case SchemaResolver::TableListItem::SHADOW_TABLE:
            {
                if (showShadow)
                {
                    shadowTables << tableListItem.name;
                    tables << tableListItem.name;
                }
                break;
            }
            case SchemaResolver::TableListItem::TABLE:
                tables << tableListItem.name;
                break;
            case SchemaResolver::TableListItem::VIEW:
                views << tableListItem.name;
                break;
            case SchemaResolver::TableListItem::UNKNOWN:
                break;
        }
    }

    QList<QStandardItem*> tableItems = refreshSchemaTables(tables, virtualTables, shadowTables, sort);
    QList<QStandardItem*> viewItems = refreshSchemaViews(views, sort);
    refreshSchemaBuild(item, tableItems, viewItems);
    populateChildItemsWithDb(item, db);

    if (CFG_UI.DbList.ShowRegularTableLabels.get())
    {
        for (QStandardItem*& item : tableItems)
            loadTableSchema(dynamic_cast<DbTreeItem*>(item));

        for (QStandardItem*& item : viewItems)
            loadViewSchema(dynamic_cast<DbTreeItem*>(item));
    }

    restoreExpandedState(expandedState, item);
    restoreSelectionState(selectionState);
}

void DbTreeModel::collectExpandedState(QHash<QString, bool> &state, QStandardItem *parentItem)
{
    if (!parentItem)
        parentItem = root();

    DbTreeItem* dbTreeItem = dynamic_cast<DbTreeItem*>(parentItem);
    if (dbTreeItem)
        state[dbTreeItem->pathSignature()] = treeView->isExpanded(dbTreeItem->index());

    for (int i = 0; i < parentItem->rowCount(); i++)
        collectExpandedState(state, parentItem->child(i));
}

QList<QStandardItem *> DbTreeModel::refreshSchemaTables(const QStringList &tables, const QSet<QString>& virtualTables, const QSet<QString>& shadowTables, bool sort)
{
    QStringList sortedTables = tables;
    if (sort)
        sortedTables.sort(Qt::CaseInsensitive);

    QList<QStandardItem *> items;
    for (const QString& table : sortedTables)
    {
        if (virtualTables.contains(table))
            items += DbTreeItemFactory::createVirtualTable(table, this);
        else if (shadowTables.contains(table))
            items += DbTreeItemFactory::createShadowTable(table, this);
        else
            items += DbTreeItemFactory::createTable(table, this);
    }

    return items;
}

QList<QStandardItem*> DbTreeModel::refreshSchemaTableOrViewColumns(const QList<QPair<QString, QString>>& columns)
{
    bool doSort = CFG_UI.DbList.SortColumns.get();

    auto sortedColumns = columns;
    if (doSort)
        ::sSort(sortedColumns, [](auto& p1, auto& p2) {return p1.first < p2.first;});

    QList<QStandardItem*> items;
    for (const QPair<QString, QString>& column : sortedColumns)
    {
        bool hidden = false;
        QString typeName = SqliteColumnType::removeHiddenFromTypeName(column.second, &hidden);
        items += DbTreeItemFactory::createColumn(column.first, typeName, hidden, this);
    }

    return items;
}

QList<QStandardItem*> DbTreeModel::refreshSchemaIndexes(const QStringList& indexes, bool sort)
{
    QStringList sortedIndexes = indexes;
    if (sort)
        sortedIndexes.sort(Qt::CaseInsensitive);

    QList<QStandardItem*> items;
    for (const QString& index : sortedIndexes)
        items += DbTreeItemFactory::createIndex(index, this);

    return items;
}

QList<QStandardItem*> DbTreeModel::refreshSchemaTriggers(const QStringList& triggers, bool sort)
{
    QStringList sortedTriggers = triggers;
    if (sort)
        sortedTriggers.sort(Qt::CaseInsensitive);

    QList<QStandardItem*> items;
    for (const QString& trigger : sortedTriggers)
        items += DbTreeItemFactory::createTrigger(trigger, this);

    return items;
}

QList<QStandardItem *> DbTreeModel::refreshSchemaViews(const QStringList &views, bool sort)
{
    QStringList sortedViews = views;
    if (sort)
        sortedViews.sort(Qt::CaseInsensitive);

    QList<QStandardItem *> items;
    for (const QString& view : sortedViews)
        items += DbTreeItemFactory::createView(view, this);

    return items;
}

void DbTreeModel::populateChildItemsWithDb(QStandardItem *parentItem, Db* db)
{
    QStandardItem* childItem = nullptr;
    for (int i = 0; i < parentItem->rowCount(); i++)
    {
        childItem = parentItem->child(i);
        dynamic_cast<DbTreeItem*>(childItem)->setDb(db);
        populateChildItemsWithDb(childItem, db);
    }
}

void DbTreeModel::loadTableSchema(DbTreeItem* tableItem)
{
    if (tableItem->isSchemaReady())
        return;

    Db* db = tableItem->getDb();
    QString table = tableItem->text();

    SchemaResolver resolver(db);
    resolver.setIgnoreSystemObjects(!CFG_UI.DbList.ShowSystemObjects.get());

    bool sort = CFG_UI.DbList.SortObjects.get();

    DbTreeItem* columnsItem = tableItem->findFirstItem(DbTreeItem::Type::COLUMNS);
    DbTreeItem* indexesItem = tableItem->findFirstItem(DbTreeItem::Type::INDEXES);
    DbTreeItem* triggersItem = tableItem->findFirstItem(DbTreeItem::Type::TRIGGERS);

    QList<QStandardItem*> tableColumns = refreshSchemaTableOrViewColumns(resolver.getColumnsAndDataTypesUsingPragma(table));
    QList<QStandardItem*> indexItems = refreshSchemaIndexes(resolver.getIndexesForTable(table), sort);
    QList<QStandardItem*> triggerItems = refreshSchemaTriggers(resolver.getTriggersForTable(table), sort);

    for (QStandardItem* columnItem : tableColumns)
        columnsItem->appendRow(columnItem);

    for (QStandardItem* indexItem : indexItems)
        indexesItem->appendRow(indexItem);

    for (QStandardItem* triggerItem : triggerItems)
        triggersItem->appendRow(triggerItem);

    populateChildItemsWithDb(columnsItem, db);
    populateChildItemsWithDb(indexesItem, db);
    populateChildItemsWithDb(triggersItem, db);

    tableItem->setSchemaReady(true);
}

void DbTreeModel::loadViewSchema(DbTreeItem* viewItem)
{
    if (viewItem->isSchemaReady())
        return;

    Db* db = viewItem->getDb();
    QString view = viewItem->text();

    SchemaResolver resolver(db);
    resolver.setIgnoreSystemObjects(!CFG_UI.DbList.ShowSystemObjects.get());

    bool sort = CFG_UI.DbList.SortObjects.get();

    DbTreeItem* columnsItem = viewItem->findFirstItem(DbTreeItem::Type::COLUMNS);
    DbTreeItem* triggersItem = viewItem->findFirstItem(DbTreeItem::Type::TRIGGERS);

    QList<QStandardItem*> viewColumns = refreshSchemaTableOrViewColumns(resolver.getColumnsAndDataTypesUsingPragma(view));
    for (QStandardItem* columnItem : viewColumns)
    {
        columnItem->setEditable(false);
        columnsItem->appendRow(columnItem);
    }

    QList<QStandardItem*> triggerItems = refreshSchemaTriggers(resolver.getTriggersForView(view), sort);
    for (QStandardItem* triggerItem : triggerItems)
        triggersItem->appendRow(triggerItem);

    populateChildItemsWithDb(columnsItem, db);
    populateChildItemsWithDb(triggersItem, db);

    viewItem->setSchemaReady(true);
}

void DbTreeModel::refreshSchemaBuild(QStandardItem *dbItem,
                                     QList<QStandardItem*> tables,
                                     QList<QStandardItem*> views)
{
    DbTreeItem* tablesItem = DbTreeItemFactory::createTables(this);
    DbTreeItem* viewsItem = DbTreeItemFactory::createViews(this);

    dbItem->appendRow(tablesItem);
    dbItem->appendRow(viewsItem);

    DbTreeItem* columnsItem = nullptr;
    DbTreeItem* indexesItem = nullptr;
    DbTreeItem* triggersItem = nullptr;
    for (QStandardItem* tableItem : tables)
    {
        tablesItem->appendRow(tableItem);

        columnsItem = DbTreeItemFactory::createColumns(this);
        indexesItem = DbTreeItemFactory::createIndexes(this);
        triggersItem = DbTreeItemFactory::createTriggers(this);

        tableItem->appendRow(columnsItem);
        tableItem->appendRow(indexesItem);
        tableItem->appendRow(triggersItem);

        dynamic_cast<DbTreeItem*>(tableItem)->setSchemaReady(false);
    }
    for (QStandardItem* viewItem : views)
    {
        viewsItem->appendRow(viewItem);

        columnsItem = DbTreeItemFactory::createColumns(this);
        triggersItem = DbTreeItemFactory::createTriggers(this);

        viewItem->appendRow(columnsItem);
        viewItem->appendRow(triggersItem);

        dynamic_cast<DbTreeItem*>(viewItem)->setSchemaReady(false);
    }
}

void DbTreeModel::restoreExpandedState(const QHash<QString, bool>& expandedState, QStandardItem* parentItem)
{
    DbTreeItem* parentDbTreeItem = dynamic_cast<DbTreeItem*>(parentItem);
    QString sig = parentDbTreeItem->pathSignature();
    if (expandedState.contains(sig) && expandedState[sig])
        treeView->expand(parentItem->index());

    for (QStandardItem* child : parentDbTreeItem->childs())
        restoreExpandedState(expandedState, child);
}

void DbTreeModel::dbConnected(Db* db, bool expandItem)
{
    QStandardItem* item = findItem(DbTreeItem::Type::DB, db);
    if (!item)
    {
        qWarning() << "Connected to db that couldn't be found in the model:" << db->getName();
        return;
    }
    refreshSchema(db, item);
    if (expandItem)
    {
        treeView->expand(item->index());
        if (CFG_UI.DbList.ExpandTables.get())
            treeView->expand(item->model()->index(0, 0, item->index())); // also expand tables

        if (CFG_UI.DbList.ExpandViews.get())
            treeView->expand(item->model()->index(1, 0, item->index())); // also expand views
    }
    treeView->setCurrentIndex(item->index());
}

void DbTreeModel::dbDisconnected(Db* db)
{
    QStandardItem* item = findItem(DbTreeItem::Type::DB, db);
    if (!item)
    {
        qWarning() << "Disconnected from db that couldn't be found in the model:" << db->getName();
        return;
    }

    while (item->rowCount() > 0)
        item->removeRow(0);

    treeView->collapse(item->index());
}

void DbTreeModel::dbUnloaded(Db* db)
{
    DbTreeItem* item = findItem(DbTreeItem::Type::DB, db->getName());
    if (!item)
    {
        qCritical() << "No DB item found to update icon:" << db->getName();
        return;
    }
    item->updateDbIcon();
}

void DbTreeModel::dbLoaded(Db* db)
{
    if (ignoreDbLoadedSignal)
        return;

    DbTreeItem* item = findItem(DbTreeItem::Type::DB, db->getName());
    if (!item)
    {
        qCritical() << "No DB item found to update icon:" << db->getName();
        return;
    }
    item->updateDbIcon();
}

void DbTreeModel::massSaveBegins()
{
    requireSchemaReloading = false;
}

void DbTreeModel::massSaveCommitted()
{
    if (requireSchemaReloading)
    {
        for (Db* db : DBLIST->getDbList())
        {
            if (db->isOpen())
                refreshSchema(db);
        }
    }
}

void DbTreeModel::markSchemaReloadingRequired()
{
    requireSchemaReloading = true;
}

DbTreeItem* DbTreeModel::findItem(DbTreeItem::Type type, const QString &name)
{
    return findItem(root(), type, name);
}

DbTreeItem *DbTreeModel::findItem(QStandardItem* parentItem, DbTreeItem::Type type, const QString& name)
{
    DbTreeItem* item = nullptr;
    DbTreeItem* subItem = nullptr;
    for (int i = 0; i < parentItem->rowCount(); i++)
    {
         item = dynamic_cast<DbTreeItem*>(parentItem->child(i));

         // Search recursively
         if (item->hasChildren())
         {
             subItem = findItem(item, type, name);
             if (subItem)
                 return subItem;
         }

         if (item->getType() != type)
             continue;

         if (item->text() != name)
             continue;

         return item;
    }

    return nullptr;
}

DbTreeItem *DbTreeModel::findItem(DbTreeItem::Type type, Db* db)
{
    return findItem(root(), type, db);
}

DbTreeItem* DbTreeModel::findFirstItemOfType(DbTreeItem::Type type)
{
    return findFirstItem(root(), type);
}

DbTreeItem *DbTreeModel::findItemBySignature(const QStringList &signature) const
{
    QStringList pair;
    DbTreeItem* currItem = nullptr;
    DbTreeItem::Type type;
    QString name;
    for (const QString& part : signature)
    {
        pair = part.split(".");
        type = static_cast<DbTreeItem::Type>(pair.first().toInt());
        name = QString::fromUtf8(QByteArray::fromBase64(pair.last().toLatin1()));
        currItem = findItem((currItem ? currItem : root()), type, name);
        if (!currItem)
            return nullptr; // not found the target item
    }
    return currItem;
}

QList<DbTreeItem*> DbTreeModel::findItems(DbTreeItem::Type type)
{
    return findItems(root(), type);
}

DbTreeItem *DbTreeModel::findItem(QStandardItem* parentItem, DbTreeItem::Type type, Db* db)
{
    DbTreeItem* item = nullptr;
    DbTreeItem* subItem = nullptr;
    for (int i = 0; i < parentItem->rowCount(); i++)
    {
        item = dynamic_cast<DbTreeItem*>(parentItem->child(i));

        // Search recursively
        if (item->hasChildren())
        {
            subItem = findItem(item, type, db);
            if (subItem)
                return subItem;
        }

        if (item->getType() != type)
            continue;

        if (item->text() != db->getName())
            continue;

        return item;
    }

    return nullptr;
}

QList<DbTreeItem*> DbTreeModel::findItems(QStandardItem* parentItem, DbTreeItem::Type type)
{
    QList<DbTreeItem*> items;
    DbTreeItem* item = nullptr;
    for (int i = 0; i < parentItem->rowCount(); i++)
    {
        item = dynamic_cast<DbTreeItem*>(parentItem->child(i));

        // Search recursively
        if (item->hasChildren())
            items += findItems(item, type);

        if (item->getType() != type)
            continue;

        items += item;
    }

    return items;
}

DbTreeItem* DbTreeModel::findFirstItem(QStandardItem* parentItem, DbTreeItem::Type type)
{
    for (int i = 0; i < parentItem->rowCount(); i++)
    {
        DbTreeItem* item = dynamic_cast<DbTreeItem*>(parentItem->child(i));

        if (item->hasChildren())
        {
            DbTreeItem* child = findFirstItem(item, type);
            if (child)
                return child;
        }

        if (item->getType() != type)
            continue;

        return item;
    }

    return nullptr;
}

QStandardItem* DbTreeModel::root() const
{
    return invisibleRootItem();
}

void DbTreeModel::loadDbList()
{
    clear();
    readGroups(DBLIST->getDbList());
}

void DbTreeModel::itemChangedVisibility(DbTreeItem* item)
{
    emit updateItemHidden(item);
}

void DbTreeModel::setTreeView(DbTreeView *value)
{
    treeView = value;
    connect(treeView, &QTreeView::expanded, this, &DbTreeModel::expanded);
    connect(treeView, &QTreeView::collapsed, this, &DbTreeModel::collapsed);
    connect(this, SIGNAL(updateItemHidden(DbTreeItem*)), treeView, SLOT(updateItemHidden(DbTreeItem*)));
}

QStringList DbTreeModel::mimeTypes() const
{
    QStringList types = QStandardItemModel::mimeTypes();
    types << MIMETYPE;
    return types;
}

QMimeData *DbTreeModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *data = QStandardItemModel::mimeData(indexes);
    if (!data)
        return nullptr;

    if (indexes.size() == 0)
        return nullptr;

    QByteArray output;
    QDataStream stream(&output, QIODevice::WriteOnly);

    QList<QUrl> urlList;
    QStringList textList;

    DbTreeItem* item = nullptr;
    qint32 indexesSize = indexes.size();
    stream << indexesSize;
    for (const QModelIndex& idx : indexes)
    {
        item = dynamic_cast<DbTreeItem*>(itemFromIndex(idx));
        stream << item->pathSignatureParts();

        textList << item->text();
        if (item->getType() == DbTreeItem::Type::DB)
            urlList << QUrl("file://"+item->getDb()->getPath());
    }
    data->setData(MIMETYPE, output);
    data->setText(textList.join("\n"));
    data->setUrls(urlList);

    return data;
}

DbTreeItem* DbTreeModel::getDbTreeItemOf(int row, int column, const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        QModelIndex idx = index(row, column, parent);
        if (idx.isValid())
            return dynamic_cast<DbTreeItem*>(itemFromIndex(idx));
        else // drop on top of the parent
            return dynamic_cast<DbTreeItem*>(itemFromIndex(parent));
    }

    return dynamic_cast<DbTreeItem*>(item(row, column));
}

bool DbTreeModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(action);
    Q_UNUSED(row);
    Q_UNUSED(column);
    Q_UNUSED(parent);
    if (data->formats().contains(MIMETYPE))
    {
        QList<DbTreeItem*> srcItems = getDragItems(data);
        DbTreeItem* dstItem = getDbTreeItemOf(row, column, parent);
        QList<DbTreeItem*> deniedItems = srcItems | FILTER(item,
                         {
                             return !DbTree::isAcceptedDropItem(item) ||
                                    (dstItem && dstItem->getDb() && item->getDb() == dstItem->getDb());
                         });

        return deniedItems.isEmpty();
    }

    return data->hasUrls();
}

bool DbTreeModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
    Q_UNUSED(action);
    // The result means: do we want the old item to be removed from the tree?
    bool invokeStdAction = false;
    bool res = pasteData(data, row, column, parent, Qt::IgnoreAction, &invokeStdAction);
    if (!invokeStdAction)
        return res;

    return QStandardItemModel::dropMimeData(data, action, row, column, parent);
}

bool DbTreeModel::pasteData(const QMimeData* data, int row, int column, const QModelIndex& parent, Qt::DropAction defaultAction, bool* invokeStdAction)
{
    // The result means: do we want the old item to be removed from the tree?
    DbTreeItem* dstItem = getDbTreeItemOf(row, column, parent);
    if (data->formats().contains(MIMETYPE))
        return dropDbTreeItem(getDragItems(data), dstItem, defaultAction, invokeStdAction);
    else if (data->hasUrls())
        return dropUrls(data->urls());
    else
        return false;
}

void DbTreeModel::interruptableStarted(Interruptable* obj)
{
    if (interruptables.size() == 0)
        treeView->getDbTree()->showRefreshWidgetCover();

    interruptables << obj;
}

void DbTreeModel::interruptableFinished(Interruptable* obj)
{
    interruptables.removeOne(obj);
    if (interruptables.size() == 0)
        treeView->getDbTree()->hideRefreshWidgetCover();
}

QList<DbTreeItem*> DbTreeModel::getDragItems(const QMimeData* data) const
{
    QList<DbTreeItem*> items;
    QByteArray byteData = data->data(MIMETYPE);
    QDataStream stream(&byteData, QIODevice::ReadOnly);

    qint32 itemCount;
    stream >> itemCount;

    DbTreeItem* item = nullptr;
    QStringList signature;
    for (qint32 i = 0; i < itemCount; i++)
    {
        stream >> signature;
        item = findItemBySignature(signature);
        if (item)
            items << item;
    }

    return items;
}

QList<DbTreeItem*> DbTreeModel::getItemsForIndexes(const QModelIndexList& indexes) const
{
    QList<DbTreeItem*> items;
    for (const QModelIndex& idx : indexes)
    {
        if (idx.isValid())
            items << dynamic_cast<DbTreeItem*>(itemFromIndex(idx));
    }

    return items;
}

DbTreeItem* DbTreeModel::getItemForIndex(const QModelIndex& index) const
{
    if (index.isValid())
        return dynamic_cast<DbTreeItem*>(itemFromIndex(index));

    return nullptr;
}

QHash<QString, QVariant> DbTreeModel::collectSelectionState()
{
    DbTreeItem* activeItem = treeView->currentItem();
    QStringList currentItem = activeItem ? activeItem->pathSignatureParts() : QStringList();
    QList<QVariant> selectedItems = treeView->selectionItems() | MAP(item, {return QVariant(item->pathSignatureParts());});

    QHash<QString, QVariant> selectionState;
    selectionState["currentItem"] = currentItem;
    selectionState["selectedItems"] = selectedItems;
    return selectionState;
}

void DbTreeModel::restoreSelectionState(const QHash<QString, QVariant>& selectionState)
{
    QHash<QStringList, DbTreeItem*> allItemMap = getAllItemsWithSignatures();

    // Current/Active
    QStringList currentSig = selectionState["currentItem"].toStringList();
    DbTreeItem* currentItem = findDeepestExistingItemBySignature(currentSig, allItemMap);
    if (currentItem)
        treeView->setCurrentItem(currentItem);

    // Selected items
    QList<QStringList> selectedSignatures = selectionState["selectedItems"].toList() | MAP(v, {return v.toStringList();});
    QList<DbTreeItem*> selectedItems = selectedSignatures | MAP(sig, {return findDeepestExistingItemBySignature(sig, allItemMap);});
    selectedItems = selectedItems | FILTER(item, {return !!item;});
    treeView->selectItems(selectedItems);
}

void DbTreeModel::staticInit()
{
}

bool DbTreeModel::dropDbTreeItem(const QList<DbTreeItem*>& srcItems, DbTreeItem* dstItem, Qt::DropAction defaultAction, bool *invokeStdDropAction)
{
    // The result means: do we want the old item to be removed from the tree?
    if (srcItems.size() == 0)
        return false;

    DbTreeItem* srcItem = srcItems.first();
    switch (srcItem->getType())
    {
        case DbTreeItem::Type::TABLE:
        case DbTreeItem::Type::VIEW:
        {
            if (!dstItem)
                return false;

            if (srcItem->getDb() == dstItem->getDb() && invokeStdDropAction)
                return true;

            return dropDbObjectItem(srcItems, dstItem, defaultAction);
        }
        case DbTreeItem::Type::DB:
        case DbTreeItem::Type::DIR:
        {
            if (invokeStdDropAction)
                *invokeStdDropAction = true;

            break;
        }
        case DbTreeItem::Type::COLUMN:
        case DbTreeItem::Type::TABLES:
        case DbTreeItem::Type::INDEXES:
        case DbTreeItem::Type::INDEX:
        case DbTreeItem::Type::TRIGGERS:
        case DbTreeItem::Type::TRIGGER:
        case DbTreeItem::Type::VIEWS:
        case DbTreeItem::Type::COLUMNS:
        case DbTreeItem::Type::VIRTUAL_TABLE:
        case DbTreeItem::Type::ITEM_PROTOTYPE:
            break;
    }

    return false;
}

bool DbTreeModel::dropDbObjectItem(const QList<DbTreeItem*>& srcItems, DbTreeItem* dstItem, Qt::DropAction defaultAction)
{
    if (defaultAction == Qt::CopyAction)
        moveOrCopyDbObjects(srcItems, dstItem, false, true, true, true);
    else if (defaultAction == Qt::MoveAction)
        moveOrCopyDbObjects(srcItems, dstItem, true, true, true, true);
    else
    {
        // This trick is necessary since Qt 6, because mouse release event was posted after the menu was popped up,
        // therefore "copy" action was immediately triggered as first action in the menu.
        // By posting this code through QTimer (i.e. through event queue) we make the order of events correct.
        QPoint pos = treeView->mapToGlobal(treeView->getLastDropPosition());
        QTimer::singleShot(1, [this, pos, srcItems, dstItem]()
        {
            bool copy = false;
            bool move = false;
            bool includeData = false;
            bool includeIndexes = false;
            bool includeTriggers = false;

            QMenu menu;
            QAction* copyAction = menu.addAction(ICONS.ACT_COPY, tr("Copy"));
            QAction* moveAction = menu.addAction(ICONS.ACT_CUT, tr("Move"));
            menu.addSeparator();
            QCheckBox *includeDataCheck = createCopyOrMoveMenuCheckBox(&menu, tr("Include data"));
            QCheckBox *includeIndexesCheck = createCopyOrMoveMenuCheckBox(&menu, tr("Include indexes"));
            QCheckBox *includeTriggersCheck = createCopyOrMoveMenuCheckBox(&menu, tr("Include triggers"));
            menu.addSeparator();
            menu.addAction(ICONS.ACT_ABORT, tr("Abort"));

            connect(moveAction, &QAction::triggered, [&move]() {move = true;});
            connect(copyAction, &QAction::triggered, [&copy]() {copy = true;});

            menu.exec(pos);
            if (!copy && !move)
                return;

            includeData = includeDataCheck->isChecked();
            includeIndexes = includeIndexesCheck->isChecked();
            includeTriggers = includeTriggersCheck->isChecked();

            moveOrCopyDbObjects(srcItems, dstItem, move, includeData, includeIndexes, includeTriggers);
        });
    }

    return false;
}

QCheckBox* DbTreeModel::createCopyOrMoveMenuCheckBox(QMenu* menu, const QString& label)
{
    QWidget* parentWidget = new QWidget(menu);
    parentWidget->setLayout(new QVBoxLayout());
    QMargins margins = parentWidget->layout()->contentsMargins();
    parentWidget->layout()->setContentsMargins(margins.left(), 0, margins.right(), 0);

    QCheckBox *cb = new QCheckBox(label);
    cb->setChecked(true);
    parentWidget->layout()->addWidget(cb);

    QWidgetAction *action = new QWidgetAction(menu);
    action->setDefaultWidget(parentWidget);
    menu->addAction(action);
    return cb;
}

bool DbTreeModel::dropUrls(const QList<QUrl>& urls)
{
    QString filePath;
    bool autoTest = false;
    for (const QUrl& url : urls)
    {
        if (!url.isLocalFile())
        {
            qDebug() << url.toString() + "skipped, not a local file.";
            continue;
        }

        autoTest = false;
        filePath = url.toLocalFile();
        if (CFG_UI.DbList.BypassDbDialogWhenPossible.get())
        {
            if (quickAddDroppedDb(filePath))
            {
                continue;
            }
            else
            {
                notifyWarn(tr("Could not add dropped database file '%1' automatically. Manual setup is necessary.").arg(filePath));
                autoTest = true;
            }
        }

        DbDialog dialog(DbDialog::ADD, MAINWINDOW);
        dialog.setPath(filePath);
        dialog.setDoAutoTest(autoTest);
        dialog.exec();
    }
    return false;
}

bool DbTreeModel::quickAddDroppedDb(const QString& filePath)
{
    DbPlugin* plugin = DBLIST->getPluginForDbFile(filePath);
    if (!plugin)
        return false;

    return quickAddDroppedDb(filePath, plugin);
}

bool DbTreeModel::quickAddDroppedDb(const QString& filePath, DbPlugin* plugin)
{
    QString name = DBLIST->generateUniqueDbName(plugin, filePath);
    QHash<QString,QVariant> opts;
    opts[DB_PLUGIN] = plugin->getName();
    return DBLIST->addDb(name, filePath, opts, !CFG_UI.DbList.NewDbNotPermanentByDefault.get());
}

void DbTreeModel::loadTableOrViewSchema(DbTreeItem* tableOrViewItem)
{
    if (tableOrViewItem->getType() == DbTreeItem::Type::TABLE || tableOrViewItem->getType() == DbTreeItem::Type::VIRTUAL_TABLE)
        loadTableSchema(tableOrViewItem);
    else if (tableOrViewItem->getType() == DbTreeItem::Type::VIEW)
        loadViewSchema(tableOrViewItem);
    else
        qWarning() << "Called DbTreeModel::loadTableOrViewSchema with item that is not a table or view:" << tableOrViewItem->text();
}

void DbTreeModel::moveOrCopyDbObjects(const QList<DbTreeItem*>& srcItems, DbTreeItem* dstItem, bool move, bool includeData, bool includeIndexes, bool includeTriggers)
{
    if (srcItems.size() == 0)
        return;

    DbTreeItem* srcItem = srcItems.first();
    Db* srcDb = srcItem->getDb();
    Db* dstDb = dstItem->getDb();

    QStringList srcNames;
    for (DbTreeItem* item : srcItems)
        srcNames << item->text();

    interruptableStarted(dbOrganizer);
    if (move)
        dbOrganizer->moveObjectsToDb(srcDb, srcNames, dstDb, includeData, includeIndexes, includeTriggers);
    else
        dbOrganizer->copyObjectsToDb(srcDb, srcNames, dstDb, includeData, includeIndexes, includeTriggers);
}

QHash<QStringList, DbTreeItem*> DbTreeModel::getAllItemsWithSignatures() const
{
    QList<DbTreeItem*> allItems = getAllItemsAsFlatList();
    return allItems | TO_HASH(item, {return item->pathSignatureParts();});
}

DbTreeItem* DbTreeModel::findDeepestExistingItemBySignature(QStringList signature, const QHash<QStringList, DbTreeItem*>& allItemsWithSignatures) const
{
    DbTreeItem* item = allItemsWithSignatures[signature];
    while (!item && !signature.isEmpty())
    {
        signature.removeLast();
        item = allItemsWithSignatures[signature];
    }
    return item;
}

bool DbTreeModel::confirmReferencedTables(const QStringList& tables)
{
    QMessageBox::StandardButton result = QMessageBox::question(MAINWINDOW, tr("Referenced tables"),
                                                               tr("Do you want to include following referenced tables as well:\n%1").arg(tables.join(", ")));

    return result == QMessageBox::Yes;
}

bool DbTreeModel::resolveNameConflict(QString& nameInConflict)
{
    bool ok = false;
    QInputDialog tmpDialog; // just for a cancel button text
    QString result = QInputDialog::getText(MAINWINDOW, tr("Name conflict"),
        tr("Following object already exists in the target database.\nPlease enter new, unique name, or "
           "press '%1' to abort the operation:").arg(tmpDialog.cancelButtonText()),
        QLineEdit::Normal, nameInConflict, &ok);

    if (ok)
        nameInConflict = result;

    return ok;
}

bool DbTreeModel::confirmConversion(const QList<QPair<QString, QString> >& diffs)
{
    VersionConvertSummaryDialog dialog(MAINWINDOW);
    dialog.setWindowTitle(tr("SQL statements conversion"));
    dialog.setSides(diffs);
    return dialog.exec() == QDialog::Accepted;
}

bool DbTreeModel::confirmConversionErrors(const QHash<QString, QSet<QString> >& errors)
{
    ErrorsConfirmDialog dialog(MAINWINDOW);
    dialog.setTopLabel(tr("Following error occurred while converting SQL statements to the target SQLite version:"));
    dialog.setBottomLabel(tr("Would you like to ignore those errors and proceed?"));
    dialog.setErrors(errors);
    return dialog.exec() == QDialog::Accepted;
}

bool DbTreeModel::getIgnoreDbLoadedSignal() const
{
    return ignoreDbLoadedSignal;
}

void DbTreeModel::setIgnoreDbLoadedSignal(bool value)
{
    ignoreDbLoadedSignal = value;
}

bool DbTreeModel::hasDbTreeItem(const QMimeData *data)
{
    return data && data->formats().contains(MIMETYPE);
}

void DbTreeModel::dbObjectsMoveFinished(bool success, Db* srcDb, Db* dstDb)
{
    if (!success)
    {
        interruptableFinished(dbOrganizer);
        DBTREE->refreshSchema(srcDb);
        return;
    }

    DBTREE->refreshSchema(srcDb);
    DBTREE->refreshSchema(dstDb);
    interruptableFinished(dbOrganizer);
}

void DbTreeModel::dbObjectsCopyFinished(bool success, Db* srcDb, Db* dstDb)
{
    dbObjectsMoveFinished(success, srcDb, dstDb);
}
