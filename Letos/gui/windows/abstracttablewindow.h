#ifndef ABSTRACTTABLEWINDOW_H
#define ABSTRACTTABLEWINDOW_H

#include "dataview.h"
#include "dbobjecttype.h"
#include "mdichild.h"
#include "uiutils.h"
#include <QHash>
#include <QString>

class DbComboBox;
class SqlTableModel;
class WidgetCover;
class ChainExecutor;
class QAction;
class Db;
class QTabWidget;
class QTableView;

#define ABSTRACT_TABLE_WINDOW_COMMON_UI() \
    commonUi = { \
        .tabWidget = ui->tabWidget, \
        .dataTab = ui->dataTab, \
        .structureTab = ui->structureTab, \
        .dbCombo = ui->dbCombo, \
        .dataView = ui->dataView, \
        .retranslateUi = [ui = this->ui](QWidget* widget) {ui->retranslateUi(widget);} \
    };

class GUI_API_EXPORT AbstractTableWindow : public MdiChild, public RequiresExplicitInit
{
    Q_OBJECT

    public:
        explicit AbstractTableWindow(QWidget* parent = nullptr);
        virtual ~AbstractTableWindow();

        Db* getDb() const;
        QString getTable() const;
        bool isUncommitted() const override;
        QString getQuitUncommittedConfirmMessage() const override;
        bool isWindowClosingBlocked() const override;
        Db* getAssociatedDb() const override;
        QPair<Db*, QString> getSoftDbObjectAssociation() const override;

    protected:
        struct CommonUi
        {
            QTabWidget* tabWidget = nullptr;
            QWidget* dataTab = nullptr;
            QWidget* structureTab = nullptr;
            DbComboBox* dbCombo = nullptr;
            DataView *dataView = nullptr;
            std::function<void(QWidget*)> retranslateUi = nullptr;
        };

        void newTable();
        void changeEvent(QEvent *e) override;
        void showEvent(QShowEvent *e) override;
        void setupCoverWidget();
        int getDataTabIdx() const;
        int getStructureTabIdx() const;
        void initDbCombo();
        void applyInitialTab();
        virtual QList<QTableView*> getViewsForFontUpdate() const = 0;
        virtual QString updateWindowAfterStructureChanged() = 0;
        QString getTitleForMdiWindow() override;
        virtual QString getTitleTemplateForMdiWindow() = 0;

        bool shownAtLEastOnce = false;
        Db* db = nullptr;
        QString database;
        QString table;
        SqlTableModel* dataModel = nullptr;
        bool dataLoaded = false;
        bool existingTable = true;
        WidgetCover* widgetCover = nullptr;
        ChainExecutor* structureExecutor = nullptr;
        bool modifyingThisTable = false;
        bool tabsMoving = false;
        bool disableCommitOnTabChange = false;
        bool invalid = false;
        int newTableWindowNum = 1;
        QHash<int, QAction*> separatorAfterAction; // use int to avoid Action enum coupling
        CommonUi commonUi;

        virtual bool isModified() const = 0;
        virtual bool validate(bool skipWarning = false) = 0;
        virtual void executeStructureChanges() = 0;
        virtual void updateStructureCommitState() = 0;

    protected slots:
        void executionSuccessful();
        void executionFailed(const QString& errorText);
        virtual bool commitStructure(bool skipWarning = false);
        void tabChanged(int newTab);
        void dbChanged();
        void changesSuccessfullyCommitted();
        virtual void changesSuccessfullyCommittedHandleDeps(const QString& oldTable);
        void changesFailedToCommit(int errorCode, const QString& errorText);
        virtual void checkIfTableDeleted(const QString& database, const QString& object, DbObjectType type);
        void nextTab();
        void prevTab();
        void updateTabsOrder();
        void updateFont();
        void exportTable();
        void importTable();
        void populateTable();

    public slots:
        void focusStructureTab();
        void focusDataTab();
};

#endif // ABSTRACTTABLEWINDOW_H
