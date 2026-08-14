#ifndef DBTREEITEM_H
#define DBTREEITEM_H

#include "db/db.h"
#include "iconmanager.h"
#include "gui_global.h"
#include <QStandardItem>
#include <QObject>

class GUI_API_EXPORT DbTreeItem : public QObject, public QStandardItem
{
    Q_OBJECT

    public:
        enum class Type
        {
            DIR = 1000,
            DB = 1001,
            TABLES = 1002,
            TABLE = 1003,
            INDEXES = 1004,
            INDEX = 1005,
            TRIGGERS = 1006,
            TRIGGER = 1007,
            VIEWS = 1008,
            VIEW = 1009,
            COLUMNS = 1010,
            COLUMN = 1011,
            VIRTUAL_TABLE = 1012,
            // Technical properties below
            ITEM_PROTOTYPE = 9999
        };

        DbTreeItem(Type type, const Icon& icon, const QString& nodeName, QObject* parent = 0);
        DbTreeItem(const DbTreeItem& item);
        DbTreeItem();

        static void initMeta();

        int type() const;
        DbTreeItem* findItem(Type type, const QString& name);
        DbTreeItem* findFirstItem(Type type);
        QStandardItem* clone() const;
        QList<QStandardItem*> childs() const;
        QList<DbTreeItem*> dbTreeChilds() const;
        QStringList childNames() const;
        QString getTable() const;
        QString getColumn() const;
        QString getIndex() const;
        QString getTrigger() const;
        QString getView() const;
        void setColumnType(const QString& columnType);
        QString getColumnType() const;
        void setData(const QVariant& value, int role);

        /**
         * @brief parentItem
         * @return Parent item for this item. Might be the "invisible root item" if this is the top level item. It will never be null.
         */
        QStandardItem* parentItem() const;

        /**
         * @brief parentDbTreeItem
         * @return Parent item that is always DbTreeItem. If there is no parent item (i.e. this is the top item), then null is returned.
         */
        DbTreeItem* parentDbTreeItem() const;
        QList<DbTreeItem*> getPathToRoot();
        QList<DbTreeItem*> getPathToParentItem(Type type);
        QList<DbTreeItem*> getPathToParentItem(Type type, const QString& name);
        DbTreeItem* findParentItem(Type type);
        DbTreeItem* findParentItem(Type type, const QString& name);
        QString pathSignature() const;
        QStringList pathSignatureParts() const;
        QString signature() const;

        Type getType() const;
        void setType(Type type);
        Db* getDb() const;
        void setDb(Db* value);
        void setDb(const QString& dbName);
        void updateDbIcon();
        const Icon* getIcon() const;
        void setHidden(bool hidden);
        bool isHidden() const;
        void setIcon(const Icon& icon);
        bool isSchemaReady() const;
        void setSchemaReady(bool ready);
        void setRestoredExpandState(bool state);
        bool getRestoredExpandState() const;
        void setShadowTable(bool shadow);
        bool isShadowTable() const;

    private:
        struct DataRole // not 'enum class' because we need autocasting to int for this one
        {
            enum Enum
            {
                TYPE = 1001,
                DB = 1002,
                ICON_PTR = 1003,
                HIDDEN = 1004,
                SCHEMA_READY = 1005,
                COLUMN_TYPE = 1006,
                SHADOW_TABLE = 1007,
                EXPAND_STATE_RESTORED = 9997,
                SIGNATURE_OF_THIS = 9998
            };
        };

        DbTreeItem(Type type, const QString& nodeName, QObject* parent = 0);

        void pathSignatureParts(QStringList& parts) const;
        void init();
        void getPathToRoot(QList<DbTreeItem*>& path);
        void getPathToParentItem(QList<DbTreeItem*>& path, Type type);
        void getPathToParentItem(QList<DbTreeItem*>& path, Type type, const QString& name);
        const DbTreeItem* getParentItem(Type type) const;
        void updateSignatureValue();

    signals:

    public slots:
};

GUI_API_EXPORT QDataStream &operator<<(QDataStream &out, const DbTreeItem* item);
GUI_API_EXPORT QDataStream &operator>>(QDataStream &in, DbTreeItem*& item);

GUI_API_EXPORT size_t qHash(DbTreeItem::Type type);

Q_DECLARE_METATYPE(DbTreeItem*)

#endif // DBTREEITEM_H
