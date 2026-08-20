#ifndef VIRTUALTABLEWINDOW_H
#define VIRTUALTABLEWINDOW_H

#include "dbobjecttype.h"
#include "mdichild.h"
#include "parser/ast/sqlitecreatevirtualtable.h"
#include <QWidget>

class SqlTableModel;
class WidgetCover;
class ChainExecutor;
class IconPositionItemDelegate;

class QStandardItemModel;
namespace Ui {
    class VirtualTableWindow;
}

CFG_KEY_LIST(VirtualTableWindow, QObject::tr("Virtual Table window"),
     CFG_KEY_ENTRY(COMMIT_STRUCTURE,        QKeySequence::Save,           QObject::tr("Commit the table structure"))
     CFG_KEY_ENTRY(ROLLBACK_STRUCTURE,      QKeySequence::Cancel,         QObject::tr("Rollback pending changes in the table structure"))
     CFG_KEY_ENTRY(REFRESH_STRUCTURE,       Qt::Key_F5,                   QObject::tr("Refresh table structure"))
     CFG_KEY_ENTRY(EXPORT,                  Qt::CTRL | Qt::Key_E,         QObject::tr("Export table data"))
     CFG_KEY_ENTRY(IMPORT,                  Qt::CTRL | Qt::Key_I,         QObject::tr("Import data to the table"))
     CFG_KEY_ENTRY(NEXT_TAB,                Qt::ALT | Qt::Key_Right,      QObject::tr("Go to next tab"))
     CFG_KEY_ENTRY(PREV_TAB,                Qt::ALT | Qt::Key_Left,       QObject::tr("Go to previous tab"))
)

/**
 * The VirtualTableWindow does copy-paste significant part of code from TableWindow.
 * At first glance it may appear as a big code smell. However extracting common parts into a base class
 * would depend on the Ui::*, thus it would require a template class with a defined C++20 concept parameter,
 * and since it's a template class, it could not use Qt signals/slots, therefore yet another level
 * of abstraction would be required with base-base class leveraging the Q_OBJECT macro.
 * This would make the code much more complex and harder to maintain.
 */
class VirtualTableWindow : public MdiChild
{
        Q_OBJECT

    public:
        enum Action
        {
            // Structure tab
            REFRESH_STRUCTURE,
            COMMIT_STRUCTURE,
            ROLLBACK_STRUCTURE,
            EXPORT,
            IMPORT,
            POPULATE,
            CREATE_SIMILAR,
            // All tabs
            NEXT_TAB,
            PREV_TAB
        };
        Q_ENUM(Action)

        enum ToolBar
        {
            TOOLBAR_STRUCTURE,
        };

        explicit VirtualTableWindow(QWidget *parent = nullptr);
        VirtualTableWindow(Db* db, QWidget *parent = 0);
        VirtualTableWindow(const VirtualTableWindow& win);
        VirtualTableWindow(QWidget *parent, Db* db, const QString& database, const QString& table);
        ~VirtualTableWindow();

        static void staticInit();

        bool isUncommitted() const override;
        QString getQuitUncommittedConfirmMessage() const override;
        bool isWindowClosingBlocked() const override;
        QString getTable() const;
        Db* getDb() const;
        void useCurrentTableAsBaseForNew();

    protected:
        void changeEvent(QEvent *e) override;
        void showEvent(QShowEvent *e) override;

        void newTable();
        void parseDdl();
        void loadFromStmt();
        void createStructureActions();
        void createDataGridActions();
        void createDataFormActions();
        void createDbCombo();
        void setupCoverWidget();
        virtual bool resolveCreateTableStatement();
        virtual bool resolveOriginalCreateTableStatement();
        QVariant saveSession() override;
        bool restoreSession(const QVariant& sessionValue) override;
        Icon* getIconNameForMdiWindow() override;
        QString getTitleForMdiWindow() override;
        void createActions() override;
        void setupDefShortcuts() override;
        bool restoreSessionNextTime() override;
        QToolBar* getToolBar(int toolbar) const override;
        int getDataTabIdx() const;
        int getStructureTabIdx() const;
        void updateAfterInit();
        bool isModified() const;
        virtual void defineCurrentContextDb();
        bool validate(bool skipWarning = false);
        virtual void executeStructureChanges();
        QString updateWindowAfterStructureChanged();
        virtual void applyInitialTab();

        Ui::VirtualTableWindow *ui;
        int newTableWindowNum = 1;
        bool shownAtLEastOnce = false;
        Db* db = nullptr;
        QString database;
        QString table;
        QString originalArgsValue;
        SqlTableModel* dataModel = nullptr;
        bool dataLoaded = false;
        bool existingTable = true;
        WidgetCover* widgetCover = nullptr;
        ChainExecutor* structureExecutor = nullptr;
        IconPositionItemDelegate* iconPositionDelegate = nullptr;
        bool tabsMoving = false;
        bool disableCommitOnTabChange = false;
        SqliteCreateVirtualTablePtr createTable;
        SqliteCreateVirtualTablePtr originalCreateTable;
        QStandardItemModel* columnsModel = nullptr;
        bool modifyingThisTable = false;
        QHash<Action, QAction*> separatorAfterAction;

    private:
        void init();
        void initDbAndTable();

    protected slots:
        void executionSuccessful();
        void executionFailed(const QString& errorText);
        void nameChanged();
        void exportTable();
        void importTable();
        void populateTable();
        void createSimilarTable();
        void tabChanged(int newTab);
        void updateStructureCommitState();
        void updateStructureToolbarState();
        void updateNewTableState();
        void updateFont();
        void updateTabsOrder();
        void updateDdlTab();
        void dbChanged();
        virtual bool commitStructure(bool skipWarning = false);
        virtual void changesSuccessfullyCommitted();
        void changesFailedToCommit(int errorCode, const QString& errorText);
        virtual void rollbackStructure();
        void checkIfTableDeleted(const QString& database, const QString& object, DbObjectType type);
        void nextTab();
        void prevTab();

    public slots:
        void focusStructureTab();
        void focusDataTab();
        void refreshStructure();

    signals:
        void modifyStatusChanged();
};

#endif // VIRTUALTABLEWINDOW_H
