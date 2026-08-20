#include "dbobjectdialogs.h"
#include "mainwindow.h"
#include "dialogs/indexdialog.h"
#include "dialogs/triggerdialog.h"
#include "common/utils_sql.h"
#include "dbtree/dbtree.h"
#include "schemaresolver.h"
#include "services/notifymanager.h"
#include "mdiarea.h"
#include "mdiwindow.h"
#include "windows/tablewindow.h"
#include "windows/viewwindow.h"
#include "db/sqlquery.h"
#include "services/config.h"
#include "windows/virtualtablewindow.h"
#include <QMessageBox>
#include <QDebug>
#include <tablemodifier.h>

DbObjectDialogs::DbObjectDialogs(Db* db) :
    db(db)
{
    mainWindow = MainWindow::getInstance();
    mdiArea = mainWindow->getMdiArea();
    parentWidget = mainWindow;
}

DbObjectDialogs::DbObjectDialogs(Db* db, QWidget* parentWidget) :
    db(db), parentWidget(parentWidget)
{
    mainWindow = MainWindow::getInstance();
    mdiArea = mainWindow->getMdiArea();
}

void DbObjectDialogs::addIndex(const QString& table)
{
    IndexDialog dialog(db, parentWidget);
    if (!table.isNull())
        dialog.setTable(table);

    dialog.exec();
}

void DbObjectDialogs::editIndex(const QString& index)
{
    if (index.isNull())
    {
        qWarning() << "Tried to edit null index.";
        return;
    }

    IndexDialog dialog(db, index, parentWidget);
    dialog.exec();
}

void DbObjectDialogs::addTriggerOnTable(const QString& table)
{
    addTrigger(table, QString());
}

void DbObjectDialogs::addTriggerOnView(const QString& view)
{
    addTrigger(QString(), view);
}

void DbObjectDialogs::addTrigger(const QString& table, const QString& view)
{
    TriggerDialog dialog(db, parentWidget);
    if (!table.isNull())
        dialog.setParentTable(table);
    else if (!view.isNull())
        dialog.setParentView(view);
    else
        return;

    dialog.exec();
}

void DbObjectDialogs::editTrigger(const QString& trigger)
{
    if (trigger.isNull())
    {
        qWarning() << "Tried to edit null trigger.";
        return;
    }

    TriggerDialog dialog(db, parentWidget);
    dialog.setTrigger(trigger);
    dialog.exec();
}

ViewWindow* DbObjectDialogs::addView(const QString &initialSelect)
{
    ViewWindow* win = new ViewWindow(db, mdiArea);
    win->setSelect(initialSelect);
    mdiArea->addSubWindow(win);
    return win;
}

ViewWindow* DbObjectDialogs::editView(const QString& database, const QString& view)
{
    ViewWindow* win = nullptr;
    for (MdiWindow* mdiWin : mdiArea->getWindows())
    {
        win = dynamic_cast<ViewWindow*>(mdiWin->getMdiChild());
        if (!win)
            continue;

        if (win->getDb() == db && win->getView() == view)
        {
            mdiArea->setActiveSubWindow(mdiWin);
            return win;
        }
    }

    win = new ViewWindow(mdiArea, db, database, view);
    if (win->isInvalid())
    {
        delete win;
        return nullptr;
    }

    mdiArea->addSubWindow(win);
    return win;
}

void DbObjectDialogs::editObject(Type type, const QString& name)
{
    editObject(type, "main", name);
}

void DbObjectDialogs::editObject(Type type, const QString& database, const QString& name)
{
    SchemaResolver schemaResolver(db);
    QString normalizedName = schemaResolver.normalizeCaseObjectName(database, name);
    if (type == Type::UNKNOWN)
        type = getObjectType(database, normalizedName);

    switch (type)
    {
        case Type::TABLE:
            editTable(database, normalizedName);
            break;
        case Type::INDEX:
            editIndex(normalizedName);
            break;
        case Type::TRIGGER:
            editTrigger(normalizedName);
            break;
        case Type::VIEW:
            editView(database, normalizedName);
            break;
        default:
        {
            qCritical() << "Unknown object type while trying to edit object. Object name:" << database << "." << name;
            return;
        }
    }
}

bool DbObjectDialogs::dropObject(Type type, const QString& name)
{
    return dropObject(type, "main", name);
}

bool DbObjectDialogs::dropObject(Type type, const QString& database, const QString& name)
{
    if (type == Type::UNKNOWN)
        type = getObjectType(database, name);

    QString title;
    QString message;
    QString typeForSql;
    switch (type)
    {
        case Type::TABLE:
            title = tr("Delete table");
            message = tr("Are you sure you want to delete table %1?");
            typeForSql = "TABLE";
            break;
        case Type::INDEX:
            title = tr("Delete index");
            message = tr("Are you sure you want to delete index %1?");
            typeForSql = "INDEX";
            break;
        case Type::TRIGGER:
            title = tr("Delete trigger");
            message = tr("Are you sure you want to delete trigger %1?");
            typeForSql = "TRIGGER";
            break;
        case Type::VIEW:
            title = tr("Delete view");
            message = tr("Are you sure you want to delete view %1?");
            typeForSql = "VIEW";
            break;
        default:
        {
            qCritical() << "Unknown object type while trying to drop object. Object name:" << database << "." << name;
            return false;
        }
    }

    if (!noConfirmation)
    {
        QMessageBox::StandardButton resp = QMessageBox::question(parentWidget, title, message.arg(name));
        if (resp != QMessageBox::Yes)
            return false;
    }


    QStringList finalSql = buildDropSql(typeForSql, database, name);
    ChainExecutor* executor = executeSql(db, finalSql);
    if (!executor->getSuccessfulExecution())
    {
        notifyError(tr("Error while dropping %1: %2").arg(name, executor->getErrorsMessages().join(",\n")));
        return false;
    }

    CFG->addDdlHistory(finalSql.join("\n"), db->getName(), db->getPath());
    if (!noSchemaRefreshing)
        DBTREE->refreshSchema(db);

    return true;
}

bool DbObjectDialogs::dropObjects(const QStringList& names)
{
    QHash<QString, QStringList> objects;
    objects["main"] = names;
    return dropObjects(objects);
}

QHash<QString, QHash<QString, QStringList>> DbObjectDialogs::groupObjects(const QHash<QString, QStringList>& objects)
{
    SchemaResolver resolver(db);
    QHash<QString, QHash<QString, QStringList>> groupedObjects;
    for (QHash<QString, QStringList>::const_iterator dbIt = objects.begin(); dbIt != objects.end(); ++dbIt)
    {
        for (const QString& name : dbIt.value())
        {
            QString typeForSql;
            switch (getObjectType(dbIt.key(), name))
            {
                case Type::TABLE:
                    typeForSql = "TABLE";
                    break;
                case Type::INDEX:
                    typeForSql = "INDEX";
                    break;
                case Type::TRIGGER:
                    typeForSql = "TRIGGER";
                    break;
                case Type::VIEW:
                    typeForSql = "VIEW";
                    break;
                default:
                {
                    qCritical() << "Unknown object type while trying to drop object. Object name:" << dbIt.key() << "." << name;
                    return QHash<QString, QHash<QString, QStringList>>();
                }
            }
            groupedObjects[dbIt.key()][typeForSql] << name;
        }

        if (groupedObjects[dbIt.key()]["TABLE"].size() > 1)
        {
            // Sort by most owned FKs per table (children/referencing tables) first, so they get deleted first.
            // There will be less FK maintenance/renaming, so that resulting DDL is the simplest possible.
            QStringList tables = groupedObjects[dbIt.key()]["TABLE"];
            QHash<QString, int> fkTableCount;
            for (QString& table : tables)
                fkTableCount[table] = resolver.getFkReferencedTables(table).size();

            sSort(tables, [fkTableCount](const QString& t1, const QString& t2)
            {
                return fkTableCount[t1] > fkTableCount[t2];
            });

            groupedObjects[dbIt.key()]["TABLE"] = tables;
        }
    }
    return groupedObjects;
}

QStringList DbObjectDialogs::buildDropSql(const QString& type, const QString& database, const QString& name)
{
    if (type == "TABLE")
    {
        TableModifier tabMod(db, database, name);
        tabMod.dropTable();
        for (auto&& err : tabMod.getErrors())
            qCritical() << err;

        for (auto&& warn : tabMod.getWarnings())
            qWarning() << warn;

        return tabMod.getGeneratedSqls();
    }

    static_qstring(dropSql, "DROP %1 IF EXISTS %2.%3;");
    return {dropSql.arg(type, wrapObjIfNeeded(database), wrapObjIfNeeded(name))};
}

ChainExecutor* DbObjectDialogs::executeSql(Db* db, const QStringList& sqls)
{
    ChainExecutor* executor = new ChainExecutor(this);
    executor->setDb(db);
    executor->setAsync(false);
    executor->setDisableForeignKeys(false);
    executor->setQueries(sqls);
    executor->exec();
    executor->deleteLater();
    return executor;
}

bool DbObjectDialogs::dropObjects(const QHash<QString, QStringList>& objects)
{
    QStringList names = concat(objects.values());
    QHash<QString, QHash<QString, QStringList>> groupedObjects = groupObjects(objects);

    if (!noConfirmation)
    {
        QMessageBox::StandardButton resp = QMessageBox::question(parentWidget, tr("Delete objects"),
                                                                 tr("Are you sure you want to delete following objects:\n%1").arg(names.join(", ")));
        if (resp != QMessageBox::Yes)
            return false;
    }

    if (!db->begin())
    {
        notifyError(tr("Cannot start transaction. Details: %1").arg(db->getErrorText()));
        return false;
    }

    // Iterate through dbNames, then through db object types and finally through object names. Drop them.
    for (auto dbIt = groupedObjects.begin(); dbIt != groupedObjects.end(); ++dbIt)
    {
        QHash<QString, QStringList> typeToNames = dbIt.value();
        for (auto typeIt = typeToNames.begin(); typeIt != typeToNames.end(); ++typeIt)
        {
            for (const QString& name : typeIt.value())
            {
                QStringList finalSql = buildDropSql(typeIt.key(), dbIt.key(), name);
                ChainExecutor* executor = executeSql(db, finalSql);
                if (!executor->getSuccessfulExecution())
                {
                    notifyError(tr("Error while dropping %1: %2").arg(name, executor->getErrorsMessages().join(",\n")));
                    return false;
                }

                CFG->addDdlHistory(finalSql.join("\n"), db->getName(), db->getPath());
            }
        }
    }

    if (!db->commit())
    {
        notifyError(tr("Cannot commit transaction. Details: %1").arg(db->getErrorText()));
        return false;
    }

    if (!noSchemaRefreshing)
        DBTREE->refreshSchema(db);

    return true;
}

DbObjectDialogs::Type DbObjectDialogs::getObjectType(const QString& database, const QString& name)
{
    static const QString typeSql = "SELECT type FROM %1.sqlite_master WHERE name = ?;";
    static const QStringList types = {"table", "index", "trigger", "view"};

    QString dbName = wrapObjIfNeeded(database);
    SqlQueryPtr results = db->exec(typeSql.arg(dbName), {name});
    if (results->isError())
    {
        qCritical() << "Could not get object type. Object name:" << database << "." << name << ", error:"
                    << results->getErrorText();
        return Type::UNKNOWN;
    }

    QString typeStr = results->getSingleCell().toString();
    return static_cast<Type>(types.indexOf(typeStr));
}
bool DbObjectDialogs::getNoSchemaRefreshing() const
{
    return noSchemaRefreshing;
}

void DbObjectDialogs::setNoSchemaRefreshing(bool value)
{
    noSchemaRefreshing = value;
}

bool DbObjectDialogs::getNoConfirmation() const
{
    return noConfirmation;
}

void DbObjectDialogs::setNoConfirmation(bool value)
{
    noConfirmation = value;
}

MdiChild* DbObjectDialogs::editTable(const QString& database, const QString& table)
{
    SchemaResolver resolver(db);
    if (resolver.isVirtualTable(database, table))
        return editVirtualTable(database, table);
    else
        return editRegularTable(database, table);
}

VirtualTableWindow* DbObjectDialogs::editVirtualTable(const QString& database, const QString& table)
{
    VirtualTableWindow* win = nullptr;
    for (MdiWindow*& mdiWin : mdiArea->getWindows())
    {
        win = dynamic_cast<VirtualTableWindow*>(mdiWin->getMdiChild());
        if (!win)
            continue;

        if (win->getDb() == db && win->getTable() == table)
        {
            mdiArea->setActiveSubWindow(mdiWin);
            return win;
        }
    }

    win = new VirtualTableWindow(mdiArea, db, database, table);
    win->init();
    if (win->isInvalid())
    {
        delete win;
        return nullptr;
    }

    mdiArea->addSubWindow(win);
    return win;
}

TableWindow* DbObjectDialogs::editRegularTable(const QString& database, const QString& table)
{
    TableWindow* win = nullptr;
    for (MdiWindow*& mdiWin : mdiArea->getWindows())
    {
        win = dynamic_cast<TableWindow*>(mdiWin->getMdiChild());
        if (!win)
            continue;

        if (win->getDb() == db && win->getTable() == table)
        {
            mdiArea->setActiveSubWindow(mdiWin);
            return win;
        }
    }

    win = new TableWindow(mdiArea, db, database, table);
    win->init();
    if (win->isInvalid())
    {
        delete win;
        return nullptr;
    }

    mdiArea->addSubWindow(win);
    return win;
}

TableWindow *DbObjectDialogs::addTableSimilarTo(const QString &database, const QString &table)
{
    TableWindow* win = new TableWindow(mdiArea, db, database, table);
    win->init();
    mdiArea->addSubWindow(win);
    win->useCurrentTableAsBaseForNew();
    return win;
}

VirtualTableWindow* DbObjectDialogs::addVirtualTableSimilarTo(const QString& database, const QString& table)
{
    VirtualTableWindow* win = new VirtualTableWindow(mdiArea, db, database, table);
    win->init();
    mdiArea->addSubWindow(win);
    win->useCurrentTableAsBaseForNew();
    return win;
}

TableWindow* DbObjectDialogs::addTable()
{
    TableWindow* win = new TableWindow(db, mdiArea);
    win->init();
    mdiArea->addSubWindow(win);
    return win;
}

VirtualTableWindow* DbObjectDialogs::addVirtualTable()
{
    VirtualTableWindow* win = new VirtualTableWindow(db, mdiArea);
    win->init();
    mdiArea->addSubWindow(win);
    return win;
}
