#include "dbtreeview.h"
#include "dbtreemodel.h"
#include "dbtreeitemdelegate.h"
#include "mainwindow.h"
#include "services/dbmanager.h"
#include "uiconfig.h"
#include "windows/tablewindow.h"
#include "windows/viewwindow.h"
#include <QDragMoveEvent>
#include <QMenu>
#include <QList>
#include <QMimeData>
#include <QDebug>
#include <QDrag>
#include <QPainter>
#include <sqleditor.h>

DbTreeView::DbTreeView(QWidget *parent) :
    QTreeView(parent)
{
    contextMenu = new QMenu(this);
    connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showMenu(QPoint)));

    setHeaderHidden(true);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    itemDelegate = new DbTreeItemDelegate();
    setItemDelegate(itemDelegate);
}

DbTreeView::~DbTreeView()
{
    delete contextMenu;
    delete itemDelegate;
}

void DbTreeView::setDbTree(DbTree *dbTree)
{
    this->dbTree = dbTree;
}

DbTree* DbTreeView::getDbTree() const
{
    return dbTree;
}

DbTreeItem *DbTreeView::currentItem()
{
    return dynamic_cast<DbTreeItem*>(model()->itemFromIndex(currentIndex()));
}

void DbTreeView::setCurrentItem(DbTreeItem* item)
{
    expandToMakeVisible(item);
    setCurrentIndex(item->index());
}

DbTreeItem* DbTreeView::currentDbItem()
{
    DbTreeItem* item = currentItem();
    if (!item)
        return nullptr;

    if (item->getType() == DbTreeItem::Type::DB)
        return item;

    return item->findParentItem(DbTreeItem::Type::DB);
}

DbTreeItem *DbTreeView::itemAt(const QPoint &pos)
{
    return dynamic_cast<DbTreeItem*>(model()->itemFromIndex(indexAt(pos)));
}

QList<DbTreeItem *> DbTreeView::selectionItems()
{
    QList<DbTreeItem*> items;
    QModelIndexList selectedIndexes = selectionModel()->selectedIndexes();
    for (QModelIndex modIdx : selectedIndexes)
        items += dynamic_cast<DbTreeItem*>(model()->itemFromIndex(modIdx));

    return items;
}

void DbTreeView::selectItems(const QList<DbTreeItem*>& items)
{
    selectionModel()->clearSelection();
    for (DbTreeItem* item : items)
    {
        expandToMakeVisible(item);
        selectionModel()->select(item->index(), QItemSelectionModel::Select);
    }
}

DbTreeModel *DbTreeView::model() const
{
    return dynamic_cast<DbTreeModel*>(QTreeView::model());
}

void DbTreeView::showMenu(const QPoint &pos)
{
    contextMenu->clear();

    DbTreeItem* itemUnderCursor = itemAt(pos);
    if (!itemUnderCursor)
        selectionModel()->clear();

    DbTreeItem* item = getItemForAction();
    QList<DbTreeItem*> selItems = model()->getItemsForIndexes(getSelectedIndexes());
    dbTree->setupActionsForMenu(item, selItems, contextMenu);
    if (contextMenu->actions().size() == 0)
        return;

    dbTree->updateActionStates(item);
    contextMenu->popup(mapToGlobal(pos));
}

void DbTreeView::updateItemHidden(DbTreeItem* item)
{
    setRowHidden(item->index().row(), item->index().parent(), item->isHidden());
}

DbTreeItem *DbTreeView::getItemForAction(bool onlySelected) const
{
    QModelIndex idx = selectionModel()->currentIndex();
    if (onlySelected && !selectionModel()->isSelected(idx))
        return nullptr;

    return dynamic_cast<DbTreeItem*>(model()->itemFromIndex(idx));
}

void DbTreeView::dragEnterEvent(QDragEnterEvent* e)
{
    QTreeView::dragEnterEvent(e);
    if (e->mimeData()->hasUrls())
        e->acceptProposedAction();
}

void DbTreeView::dragMoveEvent(QDragMoveEvent *event)
{
    QTreeView::dragMoveEvent(event);

    if (event->dropAction() != Qt::MoveAction && event->mimeData()->formats().contains(DbTreeModel::MIMETYPE))
    {
        // Don't allow copying of items within the tree - only moving.
        // Eventual object copying is done later during the actual handling.
        //
        // Drag copying/linking is reserved for other types of actions, like dropping to SQL editor
        // with special treatment.
        event->ignore();
        return;
    }

    DbTreeItem* dstItem = itemAt(event->position().toPoint());

    // Depending on where we drop we need a type of item we drop ON,
    // or type of parent item if we drop ABOVE/BELOW. If we drop on empty space,
    // we leave type as default.
    if (dstItem)
    {
        QAbstractItemView::DropIndicatorPosition dropPosition = dropIndicatorPosition();
        switch (dropPosition)
        {
            case QAbstractItemView::OnItem:
                break;
            case QAbstractItemView::AboveItem:
            case QAbstractItemView::BelowItem:
            {
                dstItem = dstItem->parentDbTreeItem();
                break;
            }
            case QAbstractItemView::OnViewport:
                dstItem = nullptr;
                break;
        }
    }

    //qDebug() << event->mimeData()->formats();
    const QMimeData* data = event->mimeData();
    if (dbTree->isMimeDataValidForItem(data, dstItem))
        event->acceptProposedAction();
    else
        event->ignore();
}

void DbTreeView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        DbTreeItem* itemUnderCursor = itemAt(event->pos());
        if (itemUnderCursor && handleDoubleClick(itemUnderCursor))
            return;
    }
    if (event->button() == Qt::MiddleButton)
        return;

    QTreeView::mouseDoubleClickEvent(event);
}

void DbTreeView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton)
    {
        DbTreeItem* itemUnderCursor = itemAt(event->pos());
        if (itemUnderCursor && handleMiddleClick(itemUnderCursor))
            return;
    }
    QTreeView::mousePressEvent(event);
}

bool DbTreeView::handleDoubleClick(DbTreeItem *item)
{
    switch (item->getType())
    {
        case DbTreeItem::Type::DIR:
            break;
        case DbTreeItem::Type::DB:
            return handleDbDoubleClick(item);
        case DbTreeItem::Type::TABLES:
            break;
        case DbTreeItem::Type::VIRTUAL_TABLE:
            return handleVirtualTableDoubleClick(item);
        case DbTreeItem::Type::TABLE:
            return handleTableDoubleClick(item);
        case DbTreeItem::Type::INDEXES:
            break;
        case DbTreeItem::Type::INDEX:
            return handleIndexDoubleClick(item);
        case DbTreeItem::Type::TRIGGERS:
            break;
        case DbTreeItem::Type::TRIGGER:
            return handleTriggerDoubleClick(item);
        case DbTreeItem::Type::VIEWS:
            break;
        case DbTreeItem::Type::VIEW:
            return handleViewDoubleClick(item);
        case DbTreeItem::Type::COLUMNS:
            break;
        case DbTreeItem::Type::COLUMN:
            return handleColumnDoubleClick(item);
        case DbTreeItem::Type::ITEM_PROTOTYPE:
            break;
    }

    return false;
}

bool DbTreeView::handleDbDoubleClick(DbTreeItem *item)
{
    Db* db = item->getDb();
    if (!db->isValid() || db->isOpen())
    {
        dbTree->editDb(db);
        return true;
    }
    dbTree->getAction(DbTree::CONNECT_TO_DB)->trigger();
    return true;
}

bool DbTreeView::handleTableDoubleClick(DbTreeItem *item)
{
    dbTree->openTable(item);
    return true;
}

bool DbTreeView::handleVirtualTableDoubleClick(DbTreeItem* item)
{
    dbTree->openTable(item);
    return true;
}

bool DbTreeView::handleIndexDoubleClick(DbTreeItem *item)
{
    dbTree->editIndex(item);
    return true;
}

bool DbTreeView::handleTriggerDoubleClick(DbTreeItem *item)
{
    dbTree->editTrigger(item);
    return true;
}

bool DbTreeView::handleViewDoubleClick(DbTreeItem *item)
{
    dbTree->openView(item);
    return true;
}

bool DbTreeView::handleColumnDoubleClick(DbTreeItem *item)
{
    dbTree->editColumn(item);
    return true;
}

void DbTreeView::expandToMakeVisible(DbTreeItem* item)
{
    DbTreeItem* parentItem = item->parentDbTreeItem();
    while (parentItem)
    {
        expand(parentItem->index());
        parentItem = parentItem->parentDbTreeItem();
    }
}

DbTreeItemDelegate* DbTreeView::getItemDelegate() const
{
    return itemDelegate;
}

QPoint DbTreeView::getLastDropPosition() const
{
    return lastDropPosition;
}

QModelIndexList DbTreeView::getSelectedIndexes() const
{
    QModelIndexList idxList = selectedIndexes();
    if (currentIndex().isValid() && !idxList.contains(currentIndex()))
        idxList << currentIndex();

    return idxList;
}

void DbTreeView::dropEvent(QDropEvent* e)
{
    lastDropPosition = e->position().toPoint();

    QTreeView::dropEvent(e);
}

void DbTreeView::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions);

    const QModelIndexList indexes = selectedIndexes();
    if (indexes.isEmpty())
        return;

    QMimeData *data = model()->mimeData(indexes);
    if (!data)
        return;

    QDrag* drag = new QDrag(this);
    drag->setMimeData(data);

    QPixmap pixmap = createDragPixmap(indexes);
    drag->setPixmap(pixmap);
    drag->setHotSpot(QPoint(-10, -10));

    QList<DbTreeItem*> items = model()->getItemsForIndexes(indexes);
    bool targetIsThisTree = false;
    connect(drag, &QDrag::targetChanged, this, [this, drag, items, &targetIsThisTree](QObject *target)
    {
        QWidget* w = qobject_cast<QWidget*>(target);
        targetIsThisTree = false;
        if (auto *widget = qobject_cast<QWidget*>(target))
            targetIsThisTree = (widget == this || isAncestorOf(widget));

        if (w && qobject_cast<SqlEditor*>(w->parentWidget()))
        {
            SqlEditor* editor = qobject_cast<SqlEditor*>(w->parentWidget());
            drag->setDragCursor(editor->getDbItemDragMoveIcon(items), Qt::MoveAction);
            drag->setDragCursor(editor->getDbItemDragCopyIcon(items), Qt::CopyAction);
            drag->setDragCursor(editor->getDbItemDragLinkIcon(items), Qt::LinkAction);
            return;
        }

        drag->setDragCursor(QPixmap(), Qt::MoveAction);
        drag->setDragCursor(QPixmap(), Qt::CopyAction);
        drag->setDragCursor(QPixmap(), Qt::LinkAction);
    });

    Qt::DropAction res = drag->exec(Qt::MoveAction | Qt::CopyAction | Qt::LinkAction, Qt::MoveAction);
    if (res == Qt::MoveAction && targetIsThisTree)
        model()->deleteIndexesAfterMove(items);
}

QPixmap DbTreeView::createDragPixmap(const QModelIndexList& indexes)
{
    if (indexes.isEmpty())
        return QPixmap();

    QVector<QRect> rects;
    rects.reserve(indexes.size());

    int totalHeight = 0;
    int maxWidth = 0;
    for (const QModelIndex &idx : indexes)
    {
        QRect r = visualRect(idx);
        if (!r.isValid())
            continue;

        rects.push_back(r);
        totalHeight += r.height();
        maxWidth = std::max(maxWidth, r.width());
    }

    if (rects.isEmpty())
        return QPixmap();

    QPixmap pixmap(maxWidth, totalHeight);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    int yOffset = 0;
    for (const QRect &r : rects)
    {
        QPixmap part = viewport()->grab(r);
        painter.drawPixmap(0, yOffset, part);
        yOffset += r.height();
    }

    return pixmap;
}

bool DbTreeView::handleMiddleClick(DbTreeItem* item)
{
    switch (item->getType())
    {
        case DbTreeItem::Type::DB:
            return handleDbMiddleClick(item);
        case DbTreeItem::Type::TABLES:
            return handleTablesMiddleClick(item);
        case DbTreeItem::Type::TABLE:
            return handleTableMiddleClick(item);
        case DbTreeItem::Type::VIEWS:
            return handleViewsMiddleClick(item);
        case DbTreeItem::Type::VIEW:
            return handleViewMiddleClick(item);
        case DbTreeItem::Type::DIR:
        case DbTreeItem::Type::VIRTUAL_TABLE:
        case DbTreeItem::Type::INDEXES:
        case DbTreeItem::Type::INDEX:
        case DbTreeItem::Type::TRIGGERS:
        case DbTreeItem::Type::TRIGGER:
        case DbTreeItem::Type::COLUMNS:
        case DbTreeItem::Type::COLUMN:
        case DbTreeItem::Type::ITEM_PROTOTYPE:
            break;
    }

    return false;
}

bool DbTreeView::handleDbMiddleClick(DbTreeItem* item)
{
    Db* db = item->getDb();
    if (!db->isValid() || db->isOpen())
    {
        db->close();
        return true;
    }
    return false;
}

bool DbTreeView::handleTablesMiddleClick(DbTreeItem* item)
{
    return handleWindowClosingMiddleClick<TableWindow>(item, [](TableWindow* win, DbTreeItem* item)
    {
        return win->getDb() == item->getDb();
    });
}

bool DbTreeView::handleTableMiddleClick(DbTreeItem* item)
{
    return handleWindowClosingMiddleClick<TableWindow>(item, [](TableWindow* win, DbTreeItem* item)
    {
        return win->getDb() == item->getDb() && win->getTable() == item->getTable();
    });
}

bool DbTreeView::handleViewsMiddleClick(DbTreeItem* item)
{
    return handleWindowClosingMiddleClick<ViewWindow>(item, [](ViewWindow* win, DbTreeItem* item)
    {
        return win->getDb() == item->getDb();
    });
}

bool DbTreeView::handleViewMiddleClick(DbTreeItem* item)
{
    return handleWindowClosingMiddleClick<ViewWindow>(item, [](ViewWindow* win, DbTreeItem* item)
    {
        return win->getDb() == item->getDb() && win->getView() == item->getView();
    });
}
