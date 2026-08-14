#ifndef TABLEWINDOW_H
#define TABLEWINDOW_H

#include "db/db.h"
#include "mdichild.h"
#include "common/extactioncontainer.h"
#include "parser/ast/sqlitecreatetable.h"
#include "parser/ast/sqlitecreateindex.h"
#include "dialogs/constraintdialog.h"
#include "db/chainexecutor.h"
#include "gui_global.h"
#include <QPointer>

class TableModifier;
class SqlTableModel;
class ExtLineEdit;
class IntValidator;
class QLabel;
class TableStructureModel;
class TableConstraintsModel;
class QProgressBar;
class WidgetCover;
class SqliteSyntaxHighlighter;
class CenteredIconItemDelegate;
class ConstraintTabModel;
class DbComboBox;


class IconPositionItemDelegate;
namespace Ui {
    class TableWindow;
}

CFG_KEY_LIST(TableWindow, QObject::tr("Table window"),
     CFG_KEY_ENTRY(COMMIT_STRUCTURE,        QKeySequence::Save,           QObject::tr("Commit the table structure"))
     CFG_KEY_ENTRY(ROLLBACK_STRUCTURE,      QKeySequence::Cancel,         QObject::tr("Rollback pending changes in the table structure"))
     CFG_KEY_ENTRY(REFRESH_STRUCTURE,       Qt::Key_F5,                   QObject::tr("Refresh table structure"))
     CFG_KEY_ENTRY(ADD_COLUMN,              Qt::Key_Insert,               QObject::tr("Add new column"))
     CFG_KEY_ENTRY(EDIT_COLUMN,             Qt::Key_Return,               QObject::tr("Edit selected column"))
     CFG_KEY_ENTRY(DEL_COLUMN,              Qt::Key_Delete,               QObject::tr("Delete selected column"))
     CFG_KEY_ENTRY(EXPORT,                  Qt::CTRL | Qt::Key_E,         QObject::tr("Export table data"))
     CFG_KEY_ENTRY(IMPORT,                  Qt::CTRL | Qt::Key_I,         QObject::tr("Import data to the table"))
     CFG_KEY_ENTRY(ADD_TABLE_CONSTRAINT,    Qt::Key_Insert,               QObject::tr("Add new table constraint"))
     CFG_KEY_ENTRY(EDIT_TABLE_CONSTRAINT,   Qt::Key_Return,               QObject::tr("Edit selected table constraint"))
     CFG_KEY_ENTRY(DEL_TABLE_CONSTRAINT,    Qt::Key_Delete,               QObject::tr("Delete selected table constraint"))
     CFG_KEY_ENTRY(REFRESH_INDEXES,         Qt::Key_F5,                   QObject::tr("Refresh table index list"))
     CFG_KEY_ENTRY(ADD_INDEX,               Qt::Key_Insert,               QObject::tr("Add new index"))
     CFG_KEY_ENTRY(EDIT_INDEX,              Qt::Key_Return,               QObject::tr("Edit selected index"))
     CFG_KEY_ENTRY(DEL_INDEX,               Qt::Key_Delete,               QObject::tr("Delete selected index"))
     CFG_KEY_ENTRY(REFRESH_TRIGGERS,        Qt::Key_F5,                   QObject::tr("Refresh table trigger list"))
     CFG_KEY_ENTRY(ADD_TRIGGER,             Qt::Key_Insert,               QObject::tr("Add new trigger"))
     CFG_KEY_ENTRY(EDIT_TRIGGER,            Qt::Key_Return,               QObject::tr("Edit selected trigger"))
     CFG_KEY_ENTRY(DEL_TRIGGER,             Qt::Key_Delete,               QObject::tr("Delete selected trigger"))
     CFG_KEY_ENTRY(NEXT_TAB,                Qt::ALT | Qt::Key_Right,      QObject::tr("Go to next tab"))
     CFG_KEY_ENTRY(PREV_TAB,                Qt::ALT | Qt::Key_Left,       QObject::tr("Go to previous tab"))
)

class GUI_API_EXPORT TableWindow : public MdiChild
{
    Q_OBJECT

    public:
        enum Action
        {
            // Structure tab
            REFRESH_STRUCTURE,
            COMMIT_STRUCTURE,
            ROLLBACK_STRUCTURE,
            ADD_COLUMN,
            EDIT_COLUMN,
            DEL_COLUMN,
            MOVE_COLUMN_UP,
            MOVE_COLUMN_DOWN,
            ADD_TABLE_CONSTRAINT,
            EDIT_TABLE_CONSTRAINT,
            DEL_TABLE_CONSTRAINT,
            ADD_TABLE_PK,
            ADD_TABLE_FK,
            ADD_TABLE_UNIQUE,
            ADD_TABLE_CHECK,
            MOVE_CONSTRAINT_UP,
            MOVE_CONSTRAINT_DOWN,
            EXPORT,
            IMPORT,
            POPULATE,
            CREATE_SIMILAR,
            RESET_AUTOINCREMENT,
            // Indexes tab
            REFRESH_INDEXES,
            ADD_INDEX,
            EDIT_INDEX,
            DEL_INDEX,
            // Triggers tab
            REFRESH_TRIGGERS,
            ADD_TRIGGER,
            EDIT_TRIGGER,
            DEL_TRIGGER,
            // All tabs
            NEXT_TAB,
            PREV_TAB
        };
        Q_ENUM(Action)

        enum ToolBar
        {
            TOOLBAR_STRUCTURE,
            TOOLBAR_INDEXES,
            TOOLBAR_TRIGGERS
        };

        explicit TableWindow(QWidget *parent = 0);
        TableWindow(Db* db, QWidget *parent = 0);
        TableWindow(const TableWindow& win);
        TableWindow(QWidget *parent, Db* db, const QString& database, const QString& table);
        ~TableWindow();

        static void staticInit();
        static void insertAction(ExtActionPrototype* action, ToolBar toolbar = TOOLBAR_STRUCTURE);
        static void insertActionBefore(ExtActionPrototype* action, Action beforeAction, ToolBar toolbar = TOOLBAR_STRUCTURE);
        static void insertActionAfter(ExtActionPrototype* action, Action afterAction, ToolBar toolbar = TOOLBAR_STRUCTURE);
        static void removeAction(ExtActionPrototype* action, ToolBar toolbar = TOOLBAR_STRUCTURE);

        QString getTable() const;
        Db* getDb() const;
        SqliteCreateTablePtr getTableStatement() const;
        bool handleInitialFocus();
        bool isUncommitted() const;
        QString getQuitUncommittedConfirmMessage() const;
        void useCurrentTableAsBaseForNew();
        Db* getAssociatedDb() const;
        QPair<Db*, QString> getSoftDbObjectAssociation() const;
        bool isWindowClosingBlocked() const;

    protected:
        TableWindow(QWidget *parent, Db* db, const QString& database, const QString& table, bool existingTable);

        void changeEvent(QEvent *e);
        void showEvent(QShowEvent *e);
        QVariant saveSession();
        bool restoreSession(const QVariant& sessionValue);
        Icon* getIconNameForMdiWindow();
        QString getTitleForMdiWindow();
        void createActions();
        void setupDefShortcuts();
        bool restoreSessionNextTime();
        QToolBar* getToolBar(int toolbar) const;

        void init();
        void newTable();
        void parseDdl();
        virtual bool resolveCreateTableStatement();
        virtual bool resolveOriginalCreateTableStatement();
        void createDbCombo();
        void initDbAndTable();
        void setupCoverWidget();
        void createStructureActions();
        void createDataGridActions();
        void createDataFormActions();
        void createIndexActions();
        void createTriggerActions();
        bool editColumn(const QModelIndex& idx);
        void delColumn(const QModelIndex& idx);
        void editConstraint(const QModelIndex& idx);
        void delConstraint(const QModelIndex& idx);
        virtual void executeStructureChanges();
        QStringList generateStructureChangeStatements();
        QString updateWindowAfterStructureChanged();
        void updateAfterInit();
        QModelIndex structureCurrentIndex() const;
        void addConstraint(ConstraintDialog::Constraint mode);
        bool validate(bool skipWarning = false);
        bool isModified() const;
        TokenList indexColumnTokens(SqliteCreateIndexPtr index);
        QString getCurrentIndex() const;
        QString getCurrentTrigger() const;
        virtual void applyInitialTab();
        void resizeStructureViewColumns();
        int getDataTabIdx() const;
        int getStructureTabIdx() const;
        bool hasAnyPkDefined() const;
        virtual void defineCurrentContextDb();

        int newTableWindowNum = 1;
        bool shownAtLEastOnce = false;
        Db* db = nullptr;
        QString database;
        QString table;
        Ui::TableWindow *ui = nullptr;
        SqlTableModel* dataModel = nullptr;
        bool dataLoaded = false;
        bool existingTable = true;
        SqliteCreateTablePtr createTable;
        SqliteCreateTablePtr originalCreateTable;
        TableStructureModel* structureModel = nullptr;
        TableConstraintsModel* structureConstraintsModel = nullptr;
        ConstraintTabModel* constraintTabModel = nullptr;
        WidgetCover* widgetCover = nullptr;
        ChainExecutor* structureExecutor = nullptr;
        TableModifier* tableModifier = nullptr;
        bool modifyingThisTable = false;
        CenteredIconItemDelegate* constraintColumnsDelegate = nullptr;
        IconPositionItemDelegate* iconPositionDelegate = nullptr;
        bool tabsMoving = false;
        bool disableCommitOnTabChange = false;
        DbComboBox* dbCombo = nullptr;
        QHash<Action, QAction*> separatorAfterAction;
        
    protected slots:
        void executionSuccessful();
        void executionFailed(const QString& errorText);
        void dbClosedFinalCleanup();
        void checkIfTableDeleted(const QString& database, const QString& object, DbObjectType type);
        void checkIfIndexDeleted(const QString& object);
        void checkIfTriggerDeleted(const QString& object);
        virtual bool commitStructure(bool skipWarning = false);
        virtual void changesSuccessfullyCommitted();
        void changesFailedToCommit(int errorCode, const QString& errorText);
        virtual void rollbackStructure();
        void resetAutoincrement();
        void editColumn();
        void delColumn();
        void moveColumnUp();
        void moveColumnDown();
        void addConstraint();
        void editConstraint();
        void delConstraint();
        void moveConstraintUp();
        void moveConstraintDown();
        void addPk();
        void addFk();
        void addUnique();
        void addCheck();
        void exportTable();
        void importTable();
        void populateTable();
        void createSimilarTable();
        void tabChanged(int newTab);
        void updateStructureToolbarState();
        void updateStructureCommitState();
        void updateTableConstraintsToolbarState();
        void updateDdlTab();
        void updateNewTableState();
        void structureViewDoubleClicked(const QModelIndex &index);
        void constraintsViewDoubleClicked(const QModelIndex &index);
        void nameChanged();
        void withOutRowIdChanged();
        void strictChanged();
        void addIndex();
        void editCurrentIndex();
        void indexViewDoubleClicked(const QModelIndex& idx);
        void triggerViewDoubleClicked(const QModelIndex& idx);
        void delIndex();
        void addTrigger();
        void editTrigger();
        void delTrigger();
        void updateIndexesState();
        void updateTriggersState();
        void nextTab();
        void prevTab();
        void updateTabsOrder();
        void updateFont();
        void dbChanged();
        void handlePossibleIdxOrTrgRename(Db* db, const QString& database, const QString& oldObject, const QString& newObject);
        void handlePossibleColumnRename(Db* db, const QString& database, const QString& table, const QString& oldObject, const QString& newObject);

    public slots:
        void refreshStructure();
        void updateIndexes();
        void updateTriggers();
        void addColumn();
        void editColumn(const QString& columnName);
        void delColumn(const QString& columnName);
        void focusStructureTab();
        void focusDataTab();

    signals:
        void modifyStatusChanged();
};

#endif // TABLEWINDOW_H
