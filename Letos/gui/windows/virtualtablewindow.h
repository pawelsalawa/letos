#ifndef VIRTUALTABLEWINDOW_H
#define VIRTUALTABLEWINDOW_H

#include "dbobjecttype.h"
#include "abstracttablewindow.h"
#include "parser/ast/sqlitecreatevirtualtable.h"
#include <QWidget>
#include <QTabWidget>

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

template<typename T>
concept TableWindowUi = requires(T* ui, QWidget* widget)
{
    { ui->tabWidget } -> std::convertible_to<QTabWidget*>;
    { ui->retranslateUi(widget) } -> std::same_as<void>;
};

/**
 * The VirtualTableWindow does copy-paste significant part of code from TableWindow.
 * At first glance it may appear as a big code smell. However extracting common parts into a base class
 * would depend on the Ui::*, thus it would require a template class with a defined C++20 concept parameter,
 * and since it's a template class, it could not use Qt signals/slots, therefore yet another level
 * of abstraction would be required with base-base class leveraging the Q_OBJECT macro.
 * This would make the code much more complex and harder to maintain.
 */
class VirtualTableWindow : public AbstractTableWindow
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

        void init() override;
        void useCurrentTableAsBaseForNew();

    protected:
        void parseDdl();
        void loadFromStmt();
        void createStructureActions();
        void createDataGridActions();
        void createDataFormActions();
        virtual bool resolveCreateTableStatement();
        virtual bool resolveOriginalCreateTableStatement();
        QVariant saveSession() override;
        bool restoreSession(const QVariant& sessionValue) override;
        Icon* getIconNameForMdiWindow() override;
        QString getTitleTemplateForMdiWindow() override;
        void createActions() override;
        void setupDefShortcuts() override;
        bool restoreSessionNextTime() override;
        QToolBar* getToolBar(int toolbar) const override;
        void updateAfterInit();
        bool isModified() const override;
        virtual void defineCurrentContextDb();
        bool validate(bool skipWarning = false) override;
        virtual void executeStructureChanges() override;
        QString updateWindowAfterStructureChanged() override;
        QList<QTableView*> getViewsForFontUpdate() const override;

        Ui::VirtualTableWindow *ui;

        QString originalArgsValue;
        SqliteCreateVirtualTablePtr createTable;
        SqliteCreateVirtualTablePtr originalCreateTable;
        QStandardItemModel* columnsModel = nullptr;
        IconPositionItemDelegate* iconPositionDelegate = nullptr;

    private:
        void initDbAndTable();

    protected slots:
        void nameChanged();
        void createSimilarTable();
        void updateStructureCommitState() override;
        void updateStructureToolbarState();
        void updateNewTableState();
        void updateDdlTab();
        virtual void rollbackStructure();
        void checkIfTableDeleted(const QString& database, const QString& object, DbObjectType type) override;

    public slots:
        void refreshStructure();

    signals:
        void modifyStatusChanged();
};

#endif // VIRTUALTABLEWINDOW_H
