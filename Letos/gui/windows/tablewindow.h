#ifndef TABLEWINDOW_H
#define TABLEWINDOW_H

#include "db/db.h"
#include "abstracttablewindow.h"
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

class GUI_API_EXPORT TableWindow : public AbstractTableWindow
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

        void init() override;
        SqliteCreateTablePtr getTableStatement() const;
        bool handleInitialFocus() override;
        void useCurrentTableAsBaseForNew();

    protected:
        TableWindow(QWidget *parent, Db* db, const QString& database, const QString& table, bool existingTable);

        QVariant saveSession() override;
        bool restoreSession(const QVariant& sessionValue) override;
        Icon* getIconNameForMdiWindow() override;
        QString getTitleTemplateForMdiWindow() override;
        void createActions() override;
        void setupDefShortcuts() override;
        bool restoreSessionNextTime() override;
        QToolBar* getToolBar(int toolbar) const override;

        void parseDdl();
        virtual bool resolveCreateTableStatement();
        virtual bool resolveOriginalCreateTableStatement();
        void initDbAndTable();
        void createStructureActions();
        void createDataGridActions();
        void createDataFormActions();
        void createIndexActions();
        void createTriggerActions();
        bool editColumn(const QModelIndex& idx);
        void delColumn(const QModelIndex& idx);
        void editConstraint(const QModelIndex& idx);
        void delConstraint(const QModelIndex& idx);
        virtual void executeStructureChanges() override;
        QStringList generateStructureChangeStatements();
        QString updateWindowAfterStructureChanged() override;
        void updateAfterInit();
        QModelIndex structureCurrentIndex() const;
        void addConstraint(ConstraintDialog::Constraint mode);
        bool validate(bool skipWarning = false) override;
        bool isModified() const override;
        TokenList indexColumnTokens(SqliteCreateIndexPtr index);
        QString getCurrentIndex() const;
        QString getCurrentTrigger() const;
        void resizeStructureViewColumns();
        bool hasAnyPkDefined() const;
        virtual void defineCurrentContextDb();
        QList<QTableView*> getViewsForFontUpdate() const override;

        Ui::TableWindow *ui = nullptr;
        SqliteCreateTablePtr createTable;
        SqliteCreateTablePtr originalCreateTable;
        TableStructureModel* structureModel = nullptr;
        TableConstraintsModel* structureConstraintsModel = nullptr;
        ConstraintTabModel* constraintTabModel = nullptr;
        TableModifier* tableModifier = nullptr;
        CenteredIconItemDelegate* constraintColumnsDelegate = nullptr;
        IconPositionItemDelegate* iconPositionDelegate = nullptr;
        DbComboBox* dbCombo = nullptr;

    protected slots:
        void dbClosedFinalCleanup() override;
        void checkIfTableDeleted(const QString& database, const QString& object, DbObjectType type) override;
        void checkIfIndexDeleted(const QString& object);
        void checkIfTriggerDeleted(const QString& object);
        void changesSuccessfullyCommittedHandleDeps(const QString& oldTable) override;
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
        void createSimilarTable();
        void updateStructureToolbarState();
        void updateStructureCommitState() override;
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
        void handlePossibleIdxOrTrgRename(Db* db, const QString& database, const QString& oldObject, const QString& newObject);
        void handlePossibleColumnRename(Db* db, const QString& database, const QString& table, const QString& oldObject, const QString& newObject);

    public slots:
        void refreshStructure();
        void updateIndexes();
        void updateTriggers();
        void addColumn();
        void editColumn(const QString& columnName);
        void delColumn(const QString& columnName);

    signals:
        void modifyStatusChanged();
};

#endif // TABLEWINDOW_H
