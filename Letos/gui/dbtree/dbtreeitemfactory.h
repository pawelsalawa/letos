#ifndef DBTREEITEMFACTORY_H
#define DBTREEITEMFACTORY_H

#include "gui_global.h"
#include "dbtree/dbtreeitem.h"

class GUI_API_EXPORT DbTreeItemFactory
{
    public:
        static DbTreeItem* createDir(const QString& name, QObject *parent = nullptr);
        static DbTreeItem* createDb(const QString& name, QObject *parent = nullptr);
        static DbTreeItem* createTable(const QString& name, QObject *parent = nullptr);
        static DbTreeItem* createShadowTable(const QString& name, QObject *parent = nullptr);
        static DbTreeItem* createVirtualTable(const QString& name, QObject *parent = nullptr);
        static DbTreeItem* createIndex(const QString& name, QObject *parent = nullptr);
        static DbTreeItem* createTrigger(const QString& name, QObject *parent = nullptr);
        static DbTreeItem* createView(const QString& name, QObject *parent = nullptr);
        static DbTreeItem* createColumn(const QString& name, QObject *parent = nullptr);
        static DbTreeItem* createColumn(const QString& name, const QString& type, bool hidden, QObject *parent = nullptr);
        static DbTreeItem* createTables(QObject *parent = nullptr);
        static DbTreeItem* createIndexes(QObject *parent = nullptr);
        static DbTreeItem* createTriggers(QObject *parent = nullptr);
        static DbTreeItem* createViews(QObject *parent = nullptr);
        static DbTreeItem* createColumns(QObject *parent = nullptr);
        static DbTreeItem* createPrototype(QObject *parent = nullptr);
};

#endif // DBTREEITEMFACTORY_H
