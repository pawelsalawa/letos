#include "mainwindow.h"
#include "commandpalette/commandpalette.h"
#include "dbtree/dbtree.h"
#include "dbtree/dbtreemodel.h"
#include "dialogs/settingsexportdialog.h"
#include "dialogs/settingsimportdialog.h"
#include "iconmanager.h"
#include "windows/editorwindow.h"
#include "windows/functionseditor.h"
#include "windows/collationseditor.h"
#include "windows/ddlhistorywindow.h"
#include "windows/sqliteextensioneditor.h"
#include "mdiarea.h"
#include "statusfield.h"
#include "uiconfig.h"
#include "services/notifymanager.h"
#include "dialogs/configdialog.h"
#include "services/pluginmanager.h"
#include "formmanager.h"
#include "customconfigwidgetplugin.h"
#ifdef Q_OS_MACOS
#include "macmdiwindowswitcher.h"
#endif
#include "sqlitesyntaxhighlighter.h"
#include "qtscriptsyntaxhighlighter.h"
#include "services/exportmanager.h"
#include "services/importmanager.h"
#include "dialogs/exportdialog.h"
#include "dialogs/importdialog.h"
#include "multieditor/multieditorwidgetplugin.h"
#include "multieditor/multieditor.h"
#include "dialogs/dbdialog.h"
#include "uidebug.h"
#include "services/dbmanager.h"
#include "services/updatemanager.h"
#include "dialogs/aboutdialog.h"
#include "dialogs/newversiondialog.h"
#include "dialogs/quitconfirmdialog.h"
#include "dialogs/cssdebugdialog.h"
#include "themetuner.h"
#include "style.h"
#include "services/codeformatter.h"
#include "windows/codesnippeteditor.h"
#include "codesnippetspanel.h"
#include "sqleditor.h"
#include "uiutils.h"
#include "datagrid/cellrendererplugin.h"
#include "common/mouseshortcut.h"
#include "common/widgetcover.h"
#include <QMdiSubWindow>
#include <QDebug>
#include <QStyleFactory>
#include <QUiLoader>
#include <QInputDialog>
#include <QProgressBar>
#include <QLabel>
#include <QStyle>
#include <QApplication>
#include <QToolTip>
#include <QTimer>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QtSystemDetection>
#else
#include <qsystemdetection.h>
#endif
#include <QtGui>

CFG_KEYS_DEFINE(MainWindow)
MainWindow* MainWindow::instance = nullptr;
bool MainWindow::safeModeEnabled = false;
bool MainWindow::sessionRestoringFinished = false;
int MainWindow::defaultToolbarIconSize = 32;
QMimeDatabase MainWindow::mimeDb;

MainWindow::MainWindow() :
    QMainWindow(),
    ui(new Ui::MainWindow)
{
    init();
}

MainWindow::~MainWindow()
{
}

void MainWindow::init()
{
    ui->setupUi(this);
    connect(LETOS, SIGNAL(aboutToQuit()), this, SLOT(cleanUp()));

#ifdef Q_OS_WIN
    setWindowIcon(ICONS.LETOS_APP.toQIcon().pixmap(256, 256));
#else
    setWindowIcon(ICONS.LETOS_APP);
#endif

    setWindowTitle(QString("Letos (%1)").arg(LETOS->getVersionString()));

#ifdef Q_OS_MACX
    ui->centralWidget->layout()->setContentsMargins(0, 0, 0, 0);
#endif

    Committable::init(MainWindow::confirmQuit);
    updateCornerDocking();

    DbTreeModel::staticInit();
    dbTree = new DbTree(this);
    addDockWidget(Qt::LeftDockWidgetArea, dbTree);

    codeSnippetsPanel = new CodeSnippetsPanel(this);
    addDockWidget(Qt::LeftDockWidgetArea, codeSnippetsPanel);

    tabifyDockWidget(dbTree, codeSnippetsPanel);
    dbTree->raise();

    statusField = new StatusField(this);
    addDockWidget(Qt::BottomDockWidgetArea, statusField);
    if (!statusField->hasMessages())
        statusField->close();

    initActions();
    initToolbarSizeActionList();

    ui->mdiArea->setTaskBar(ui->taskBar);
    addToolBar(Qt::BottomToolBarArea, ui->taskBar);

#ifdef Q_OS_MACOS
    macMdiWindowSwitcher = new MacMdiWindowSwitcher(ui->mdiArea, this);
#endif

    addToolBar(Qt::TopToolBarArea, ui->viewToolbar);
    insertToolBar(ui->viewToolbar, ui->mainToolBar);
    insertToolBar(ui->mainToolBar, ui->structureToolbar);
    insertToolBar(ui->structureToolbar, ui->dbToolbar);
    ui->viewToolbar->setVisible(false);

    QSize tbIconSize(defaultToolbarIconSize, defaultToolbarIconSize);
    ui->viewToolbar->setIconSize(tbIconSize);
    ui->mainToolBar->setIconSize(tbIconSize);
    ui->structureToolbar->setIconSize(tbIconSize);
    ui->dbToolbar->setIconSize(tbIconSize);

    installToolbarSizeWheelHandler(ui->mainToolBar);
    installToolbarSizeWheelHandler(ui->viewToolbar);
    installToolbarSizeWheelHandler(ui->structureToolbar);
    installToolbarSizeWheelHandler(ui->dbToolbar);

    formManager = new FormManager();

    initMenuBar();

    PLUGINS->registerPluginType<CustomConfigWidgetPlugin>(tr("Configuration widgets"));
    PLUGINS->registerPluginType<SyntaxHighlighterPlugin>(tr("Syntax highlighting engines"));
    PLUGINS->registerPluginType<MultiEditorWidgetPlugin>(tr("Data editors"), "dataEditorsPluginsPage");
    PLUGINS->registerPluginType<CellRendererPlugin>(tr("Data renderers"), "dataRenderersPluginsPage");
    PLUGINS->loadBuiltInPlugin(new SqliteHighlighterPlugin);
    PLUGINS->loadBuiltInPlugin(new JavaScriptHighlighterPlugin);
    MultiEditor::loadBuiltInEditors();

    updateWindowActions();

    qApp->installEventFilter(this);

    if (isDebugEnabled())
    {
        if (isDebugConsoleEnabled())
            notifyInfo(tr("Running in debug mode. Press %1 or use 'Help / Open debug console' menu entry to open the debug console.").arg(shortcuts[OPEN_DEBUG_CONSOLE]->get()));
        else
            notifyInfo(tr("Running in debug mode. Debug messages are printed to the standard output."));
    }

#ifdef HAS_UPDATEMANAGER
    // For some reason these two signal-slot connections are not made correctly if method-reference syntax is used
    // and it affects Windows builds only. Therefore they have to be the old-fashion SIGNAL() and SLOT().
    connect(UPDATES, SIGNAL(updateAvailable(QString,QString)), this, SLOT(updateAvailable(QString,QString)));
    connect(UPDATES, SIGNAL(noUpdatesAvailable(bool)), this, SLOT(noUpdatesAvailable(bool)));
#endif
    connect(statusField, &StatusField::linkActivated, this, &MainWindow::statusFieldLinkClicked);

    connect(CFG_CORE.General.Language, SIGNAL(changed(QVariant)), this, SLOT(notifyAboutLanguageChange()));
    connect(CFG_UI.General.AllowMultipleSessions, SIGNAL(changed(QVariant)), this, SLOT(updateMultipleSessionsSetting(QVariant)));
    connect(CFG_UI.General.ToolBarIconSize, SIGNAL(changed(QVariant)), this, SLOT(updateToolbarStyle()));

    defaultToolbarIconSize = ui->structureToolbar->iconSize().width();
    updateToolbarStyle();
    qApp->installEventFilter(new ToolBarStyleEnforcer(this));

    updateMultipleSessionsSetting();
    fixFonts();
    fixToolbars();
    observeSessionChanges();

    connect(STYLE, SIGNAL(paletteChanged()), this, SLOT(refreshSyntaxColors()));

    initDropOverlay();
    initCommandPalette();

    // It looks like for some time now it is not beneficial to register this crash handler,
    // because if it happens during restoring previous session and some MDI window crashes,
    // this handler would overwrite session with incomplete/damaged session state.
    // At the same time, session saving is triggered frequently during the application
    // runtime, so it doesn't seem like saving is necessary during the crash.
    // LETOS->installCrashHandler([this]()
    // {
    //     saveSession();
    // });
}

void MainWindow::observeSessionChanges()
{
    saveSessionTimer = new QTimer(this);
    saveSessionTimer->setSingleShot(true);
    connect(saveSessionTimer, SIGNAL(timeout()), this, SLOT(saveSession()));

    for (QDockWidget* dock : QList<QDockWidget*>({dbTree, statusField}))
    {
        connect(dock, SIGNAL(topLevelChanged(bool)), this, SLOT(scheduleSessionSave()));
        connect(dock, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)), this, SLOT(scheduleSessionSave()));
        connect(dock, SIGNAL(visibilityChanged(bool)), this, SLOT(scheduleSessionSave()));
    }
    connect(dbTree, SIGNAL(sessionValueChanged()), this, SLOT(scheduleSessionSave()));
    connect(getMdiArea(), SIGNAL(sessionValueChanged()), this, SLOT(scheduleSessionSave()));
    connect(this, SIGNAL(sessionValueChanged()), this, SLOT(scheduleSessionSave()));
}

void MainWindow::cleanUp()
{
    if (LETOS->getImmediateQuit())
        return;

//    qDebug() << "MainWindow::cleanUp()";
    for (MdiWindow* win : getMdiArea()->getWindows())
        delete win;

    removeDockWidget(dbTree);
    removeDockWidget(statusField);

    safe_delete(dbTree);
    safe_delete(statusField);

    delete ui;

    ThemeTuner::cleanUp();

    safe_delete(formManager);
}

EditorWindow* MainWindow::openSqlEditor()
{
    EditorWindow* win = new EditorWindow(ui->mdiArea);
    if (win->isInvalid())
    {
        delete win;
        return nullptr;
    }

    ui->mdiArea->addSubWindow(win);
    return win;
}

void MainWindow::updateWindowActions()
{
    bool hasActiveTask = ui->mdiArea->activeSubWindow();
    actionMap[MDI_CASCADE]->setEnabled(hasActiveTask);
    actionMap[MDI_TILE]->setEnabled(hasActiveTask);
    actionMap[MDI_TILE_HORIZONTAL]->setEnabled(hasActiveTask);
    actionMap[MDI_TILE_VERTICAL]->setEnabled(hasActiveTask);
    actionMap[CLOSE_WINDOW]->setEnabled(hasActiveTask);
    actionMap[CLOSE_OTHER_WINDOWS]->setEnabled(hasActiveTask);
    actionMap[CLOSE_ALL_WINDOWS]->setEnabled(hasActiveTask);
    actionMap[CLOSE_ALL_WINDOWS_LEFT]->setEnabled(hasActiveTask);
    actionMap[CLOSE_ALL_WINDOWS_RIGHT]->setEnabled(hasActiveTask);
    actionMap[RENAME_WINDOW]->setEnabled(hasActiveTask);
    actionMap[RESTORE_WINDOW]->setEnabled(hasClosedWindowToRestore());
}

void MainWindow::notifyAboutLanguageChange()
{
    notifyInfo(tr("You need to restart application to make the language change take effect."));
}

MdiArea *MainWindow::getMdiArea() const
{
    return dynamic_cast<MdiArea*>(ui->mdiArea);
}

DbTree *MainWindow::getDbTree() const
{
    return dbTree;
}

StatusField *MainWindow::getStatusField() const
{
    return statusField;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (LETOS->getImmediateQuit())
    {
        closingApp = true;
        QMainWindow::closeEvent(event);
        return;
    }

    if (!Committable::canQuit())
    {
        event->ignore();
        return;
    }

    saveSessionTimer->stop();
    safe_delete(saveSessionTimer);

    closingApp = true;
    closeNonSessionWindows();
    saveSession(true);
    LETOS->cleanUp();
    QMainWindow::closeEvent(event);
}

void MainWindow::createActions()
{
    createAction(OPEN_SQL_EDITOR, ICONS.OPEN_SQL_EDITOR, tr("Open SQL &editor"), this, SLOT(openSqlEditorSlot()), ui->mainToolBar);
    createAction(OPEN_FUNCTION_EDITOR, ICONS.FUNCTIONS_EDITOR, tr("Open SQL &functions editor"), this, SLOT(openFunctionEditorSlot()), ui->mainToolBar);
    createAction(OPEN_SNIPPETS_EDITOR, ICONS.CODE_SNIPPETS, tr("Open code &snippets editor"), this, SLOT(openCodeSnippetsEditorSlot()), ui->mainToolBar);
    createAction(OPEN_COLLATION_EDITOR, ICONS.COLLATIONS_EDITOR, tr("Open &collations editor"), this, SLOT(openCollationEditorSlot()), ui->mainToolBar);
    createAction(OPEN_EXTENSION_MANAGER, ICONS.EXTENSION_EDITOR, tr("Open ex&tension manager"), this, SLOT(openExtensionManagerSlot()), ui->mainToolBar);
    ui->mainToolBar->addSeparator();
    createAction(IMPORT, ICONS.IMPORT, tr("&Import"), this, SLOT(importAnything()), ui->mainToolBar);
    createAction(EXPORT, ICONS.EXPORT, tr("E&xport"), this, SLOT(exportAnything()), ui->mainToolBar);
    createAction(OPEN_CONFIG, ICONS.CONFIGURE, tr("Open confi&guration dialog"), this, SLOT(openConfig()), ui->mainToolBar);

    createAction(MDI_TILE, ICONS.WIN_TILE, tr("&Tile windows"), ui->mdiArea, SLOT(tileSubWindows()), ui->viewToolbar);
    createAction(MDI_TILE_HORIZONTAL, ICONS.WIN_TILE_HORIZONTAL, tr("Tile windows &horizontally"), ui->mdiArea, SLOT(tileHorizontally()), ui->viewToolbar);
    createAction(MDI_TILE_VERTICAL, ICONS.WIN_TILE_VERTICAL, tr("Tile windows &vertically"), ui->mdiArea, SLOT(tileVertically()), ui->viewToolbar);
    createAction(MDI_CASCADE, ICONS.WIN_CASCADE, tr("&Cascade windows"), ui->mdiArea, SLOT(cascadeSubWindows()), ui->viewToolbar);
    createAction(PREV_TASK, tr("Previous window"), ui->taskBar, SLOT(prevTask()), this);
    createAction(NEXT_TASK, tr("Next window"), ui->taskBar, SLOT(nextTask()), this);
    createAction(MOVE_TASK_EARLIER, tr("Move window earlier"), ui->taskBar, SLOT(moveTaskEarlier()), this);
    createAction(MOVE_TASK_LATER, tr("Move window later"), ui->taskBar, SLOT(moveTaskLater()), this);
    createAction(HIDE_STATUS_FIELD, tr("Hide status field"), this, SLOT(hideStatusField()), this);

    createAction(CLOSE_WINDOW, ICONS.WIN_CLOSE, tr("Close current &window"), this, SLOT(closeSelectedWindow()), this);
    createAction(CLOSE_OTHER_WINDOWS, ICONS.WIN_CLOSE_OTHER, tr("Close &other windows"), this, SLOT(closeAllWindowsButSelected()), this);
    createAction(CLOSE_ALL_WINDOWS, ICONS.WIN_CLOSE_ALL, tr("Close &all windows"), this, SLOT(closeAllWindows()), this);
    createAction(CLOSE_ALL_WINDOWS_LEFT, ICONS.WIN_CLOSE_ALL_LEFT, tr("Close windows on the &left"), this, SLOT(closeAllLeftWindows()), this);
    createAction(CLOSE_ALL_WINDOWS_RIGHT, ICONS.WIN_CLOSE_ALL_RIGHT, tr("Close windows on the &right"), this, SLOT(closeAllRightWindows()), this);
    createAction(RESTORE_WINDOW, ICONS.WIN_RESTORE, tr("Re&store recently closed window"), this, SLOT(restoreLastClosedWindow()), this);
    createAction(RENAME_WINDOW, ICONS.WIN_RENAME, tr("Re&name selected window"), this, SLOT(renameWindow()), this);

    createAction(COMMAND_PALETTE, ICONS.COMMAND_PALETTE, tr("Open Command Palette"), this, SLOT(openCommandPalette()), this);
    createAction(OPEN_DEBUG_CONSOLE, tr("Open Debug Console"), this, SLOT(openDebugConsole()), this);
    createAction(OPEN_CSS_CONSOLE, tr("Open CSS Console"), this, SLOT(openCssConsole()), this);
    createAction(REPORT_BUG, ICONS.BUG, tr("Report a &bug"), this, SLOT(reportBug()), this);
    createAction(DONATE, ICONS.DONATE, tr("D&onate"), this, SLOT(donate()), this);
    createAction(FEATURE_REQUEST, ICONS.FEATURE_REQUEST, tr("Propose a new &feature"), this, SLOT(requestFeature()), this);
    createAction(ABOUT, ICONS.LETOS_APP16, tr("&About"), this, SLOT(aboutLetos()), this);
    createAction(LICENSES, ICONS.LICENSES, tr("&Licenses"), this, SLOT(licenses()), this);
    createAction(HOMEPAGE, ICONS.HOMEPAGE, tr("Open home &page"), this, SLOT(homepage()), this);
    createAction(USER_MANUAL, ICONS.USER_MANUAL, tr("User &Manual"), this, SLOT(userManual()), this);
    createAction(SQLITE_DOCS, ICONS.SQLITE_DOCS, tr("SQLite &documentation"), this, SLOT(sqliteDocs()), this);
    createAction(BUG_REPORT_HISTORY, ICONS.BUG_LIST, tr("Bugs and feature &requests"), this, SLOT(reportHistory()), this);
    createAction(QUIT, ICONS.QUIT, tr("Quit"), this, SLOT(quit()), this);
#ifdef HAS_UPDATEMANAGER
    createAction(CHECK_FOR_UPDATES, ICONS.GET_UPDATE, tr("Check for &updates"), this, SLOT(checkForUpdates()), this);
#endif
    createAction(OPEN_DDL_HISTORY, ICONS.DDL_HISTORY, tr("Open DDL &history"), this, SLOT(openDdlHistorySlot()), this);
    createAction(EXPORT_SETTINGS, ICONS.CONFIG_EXPORT, tr("Export configuration"), this, SLOT(exportConfig()), this);
    createAction(IMPORT_SETTINGS, ICONS.CONFIG_IMPORT, tr("Import configuration"), this, SLOT(importConfig()), this);

    actionMap[ABOUT]->setMenuRole(QAction::AboutRole);
    actionMap[OPEN_CONFIG]->setMenuRole(QAction::PreferencesRole);

    ui->dbToolbar->addAction(dbTree->getAction(DbTree::NEW_DB));
    ui->dbToolbar->addAction(dbTree->getAction(DbTree::OPEN_FILE));
    ui->dbToolbar->addAction(dbTree->getAction(DbTree::REFRESH_SCHEMA));

    attachActionInMenu(dbTree->getAction(DbTree::OPEN_FILE), dbTree->getAction(DbTree::ADD_DB), ui->dbToolbar);

    ui->structureToolbar->addAction(dbTree->getAction(DbTree::ADD_TABLE));
    attachActionInMenu(dbTree->getAction(DbTree::ADD_TABLE), dbTree->getAction(DbTree::ADD_VIRT_TABLE), ui->structureToolbar);
    ui->structureToolbar->addAction(dbTree->getAction(DbTree::ADD_VIEW));

    ui->taskBar->initContextMenu(this);

    // Extra info for CommandPalette
    setCommandPalleteContext(
                {MDI_TILE, MDI_TILE_HORIZONTAL, MDI_TILE_VERTICAL, MDI_CASCADE},
                {tr("arrange", "command palette entry")}
                );
}

void MainWindow::initMenuBar()
{
    // Database menu
    dbMenu = new QMenu(this);
    dbMenu->setTitle(tr("&Database", "menubar"));
    menuBar()->addMenu(dbMenu);

    dbMenu->addAction(dbTree->getAction(DbTree::CONNECT_TO_DB));
    dbMenu->addAction(dbTree->getAction(DbTree::DISCONNECT_FROM_DB));
    dbMenu->addSeparator();
    dbMenu->addAction(dbTree->getAction(DbTree::NEW_DB));
    dbMenu->addAction(dbTree->getAction(DbTree::OPEN_DB));
    dbMenu->addAction(dbTree->getAction(DbTree::ADD_DB));
    dbMenu->addAction(dbTree->getAction(DbTree::EDIT_DB));
    dbMenu->addAction(dbTree->getAction(DbTree::DELETE_DB));
    dbMenu->addSeparator();
    dbMenu->addAction(dbTree->getAction(DbTree::EXPORT_DB));
    dbMenu->addAction(dbTree->getAction(DbTree::VACUUM_DB));
    dbMenu->addAction(dbTree->getAction(DbTree::INTEGRITY_CHECK));
    dbMenu->addSeparator();
    dbMenu->addAction(dbTree->getAction(DbTree::REFRESH_SCHEMA));
    dbMenu->addAction(dbTree->getAction(DbTree::REFRESH_SCHEMAS));
#ifndef Q_OS_MACX
    dbMenu->addSeparator();
    dbMenu->addAction(actionMap[QUIT]);
#endif

    // Structure menu
    structMenu = new QMenu(this);
    structMenu->setTitle(tr("&Structure", "menubar"));
    menuBar()->addMenu(structMenu);

    structMenu->addAction(dbTree->getAction(DbTree::ADD_TABLE));
    structMenu->addAction(dbTree->getAction(DbTree::ADD_VIRT_TABLE));
    structMenu->addAction(dbTree->getAction(DbTree::EDIT_TABLE));
    structMenu->addAction(dbTree->getAction(DbTree::DEL_TABLE));
    structMenu->addSeparator();
    structMenu->addAction(dbTree->getAction(DbTree::ADD_INDEX));
    structMenu->addAction(dbTree->getAction(DbTree::EDIT_INDEX));
    structMenu->addAction(dbTree->getAction(DbTree::DEL_INDEX));
    structMenu->addSeparator();
    structMenu->addAction(dbTree->getAction(DbTree::ADD_TRIGGER));
    structMenu->addAction(dbTree->getAction(DbTree::EDIT_TRIGGER));
    structMenu->addAction(dbTree->getAction(DbTree::DEL_TRIGGER));
    structMenu->addSeparator();
    structMenu->addAction(dbTree->getAction(DbTree::ADD_VIEW));
    structMenu->addAction(dbTree->getAction(DbTree::EDIT_VIEW));
    structMenu->addAction(dbTree->getAction(DbTree::DEL_VIEW));

    // View menu
    // NOLINTBEGIN(clang-analyzer-optin.cplusplus.VirtualCall)
    viewMenu = createPopupMenu();
    // NOLINTEND(clang-analyzer-optin.cplusplus.VirtualCall)
    menuBar()->addMenu(viewMenu);

    mdiMenu = new QMenu(menuBar());
    connect(ui->mdiArea, &MdiArea::windowListChanged, this, &MainWindow::refreshMdiWindows);

    viewMenu->addSeparator();
    viewMenu->addAction(actionMap[MDI_TILE]);
    viewMenu->addAction(actionMap[MDI_TILE_HORIZONTAL]);
    viewMenu->addAction(actionMap[MDI_TILE_VERTICAL]);
    viewMenu->addAction(actionMap[MDI_CASCADE]);
    viewMenu->addSeparator();
    viewMenu->addAction(actionMap[CLOSE_WINDOW]);
    viewMenu->addAction(actionMap[CLOSE_ALL_WINDOWS]);
    viewMenu->addAction(actionMap[CLOSE_OTHER_WINDOWS]);
    viewMenu->addAction(actionMap[CLOSE_ALL_WINDOWS_LEFT]);
    viewMenu->addAction(actionMap[CLOSE_ALL_WINDOWS_RIGHT]);
    viewMenu->addSeparator();
    viewMenu->addAction(actionMap[PREV_TASK]);
    viewMenu->addAction(actionMap[NEXT_TASK]);
    viewMenu->addAction(actionMap[MOVE_TASK_EARLIER]);
    viewMenu->addAction(actionMap[MOVE_TASK_LATER]);
    viewMenu->addSeparator();
    viewMenu->addAction(actionMap[RESTORE_WINDOW]);
    viewMenu->addAction(actionMap[RENAME_WINDOW]);

    initWindowsMenu();

    CONFLICTING_MENU_HOTKEY_WORKAROUND(viewMenu, QKeySequence::Close, actionMap[CLOSE_WINDOW]);

    // Tools menu
    toolsMenu = new QMenu(this);
    toolsMenu->setTitle(tr("&Tools", "menubar"));
    toolsMenuAction = menuBar()->addMenu(toolsMenu);

    toolsMenu->addAction(actionMap[OPEN_SQL_EDITOR]);
    toolsMenu->addAction(actionMap[OPEN_DDL_HISTORY]);
    toolsMenu->addAction(actionMap[OPEN_FUNCTION_EDITOR]);
    toolsMenu->addAction(actionMap[OPEN_SNIPPETS_EDITOR]);
    toolsMenu->addAction(actionMap[OPEN_COLLATION_EDITOR]);
    toolsMenu->addAction(actionMap[OPEN_EXTENSION_MANAGER]);
    toolsMenu->addAction(dbTree->getAction(DbTree::EXEC_SQL_FROM_FILE));
    toolsMenu->addAction(actionMap[IMPORT]);
    toolsMenu->addAction(actionMap[EXPORT]);
    toolsMenu->addAction(actionMap[COMMAND_PALETTE]);
    toolsMenu->addSeparator();
    toolsMenu->addAction(actionMap[IMPORT_SETTINGS]);
    toolsMenu->addAction(actionMap[EXPORT_SETTINGS]);
    toolsMenu->addAction(actionMap[OPEN_CONFIG]);

    // Help menu
    letosMenu = new QMenu(this);
    letosMenu->setTitle(tr("&Help"));
    menuBar()->addMenu(letosMenu);
    if (isDebugEnabled() && isDebugConsoleEnabled())
    {
        letosMenu->addAction(actionMap[OPEN_DEBUG_CONSOLE]);
        letosMenu->addSeparator();
    }
    letosMenu->addAction(actionMap[USER_MANUAL]);
    letosMenu->addAction(actionMap[SQLITE_DOCS]);
    letosMenu->addAction(actionMap[HOMEPAGE]);
    letosMenu->addSeparator();
#ifdef HAS_UPDATEMANAGER
    if (UPDATES->isPlatformEligibleForUpdate())
    {
        letosMenu->addAction(actionMap[CHECK_FOR_UPDATES]);
        letosMenu->addSeparator();
    }
#endif
    letosMenu->addAction(actionMap[REPORT_BUG]);
    letosMenu->addAction(actionMap[FEATURE_REQUEST]);
    letosMenu->addAction(actionMap[BUG_REPORT_HISTORY]);
    letosMenu->addSeparator();
    letosMenu->addAction(actionMap[LICENSES]);
    letosMenu->addAction(actionMap[DONATE]);
    letosMenu->addAction(actionMap[ABOUT]);
}

void MainWindow::initWindowsMenu()
{
    for (QAction*& act : windowsMenuActions)
    {
        menuBar()->removeAction(act);
        viewMenu->removeAction(act);
    }
    windowsMenuActions.clear();

    if (CFG_UI.General.WindowsInMenuBar.get())
    {
        mdiMenu->setTitle(tr("&Windows", "menubar view menu"));
        windowsMenuActions << (toolsMenuAction ?
                                   menuBar()->insertMenu(toolsMenuAction, mdiMenu) :
                                   menuBar()->addMenu(mdiMenu)
                                );
    }
    else
    {
        mdiMenu->setTitle(tr("Window list", "menubar view menu"));
        windowsMenuActions << viewMenu->addSeparator();
        windowsMenuActions << viewMenu->addMenu(mdiMenu);
    }
}

void MainWindow::saveSession(MdiWindow* currWindow)
{
    /*
     * The currWindow is passed as parameter to the method to let hide the main window before
     * saving session, because saving might take a while.
     */
    QHash<QString,QVariant> sessionValue;
    sessionValue["state"] = saveState();
    sessionValue["geometry"] = saveGeometry();

    if (CFG_UI.General.RestoreSession.get())
    {
        QList<QVariant> windowSessions;
        for (MdiWindow* window : ui->mdiArea->getWindows())
            if (window->restoreSessionNextTime())
                windowSessions << window->saveSession();

        sessionValue["windowSessions"] = windowSessions;

        if (currWindow && currWindow->restoreSessionNextTime())
        {
            QString title = currWindow->windowTitle();
            sessionValue["activeWindowTitle"] = title;
        }
    }

    sessionValue["dbTree"] = dbTree->saveSession();
    sessionValue["snippetsPanel"] = codeSnippetsPanel->saveSession();
    sessionValue["style"] = currentStyle();

    CFG_UI.General.Session.set(sessionValue);
}

void MainWindow::restoreSession()
{
    bool styleSet = false;
    auto cleanup = qScopeGuard([this, &styleSet]()
    {
        sessionRestoringFinished = true;
#ifdef SUPPORT_REMINDERS
        checkForSupportReminder();
#endif

        if (!styleSet)
        {
#ifdef Q_OS_WIN
            setStyle("fusion");
#elif defined(Q_OS_MACOS)
            setStyle("macos");
            THEME_TUNER->tuneCurrentTheme();
#elif defined(Q_OS_UNIX)
            setStyle("fusion");
            THEME_TUNER->tuneCurrentTheme();
#endif
        }

        if (statusField->hasMessages())
            statusField->setVisible(true);

        emit sessionInitiallyRestored();
    });

    if (safeModeEnabled)
    {
        qInfo() << "Safe-Mode active. Skipping last saved session.";
        return;
    }

    QHash<QString,QVariant> sessionValue = CFG_UI.General.Session.get();
    if (sessionValue.size() == 0)
    {
        THEME_TUNER->tuneCurrentTheme();
        restoreState(saveState()); // workaround for probable Qt bug (?), reported in #3421
        return;
    }

    if (sessionValue.contains("style"))
    {
        QString styleName = sessionValue["style"].toString();
        setStyle(styleName);
        styleSet = true;
    }

    QString styleName = currentStyle();
    CFG_UI.General.Style.set(styleName);

    if (sessionValue.contains("geometry"))
        restoreGeometry(sessionValue["geometry"].toByteArray());

    if (sessionValue.contains("state"))
    {
        restoreState(sessionValue["state"].toByteArray());
        handlePostRestoreConfigUpdates();
    }
    else
        restoreState(saveState()); // workaround for probable Qt bug (?), reported in #3421

    if (sessionValue.contains("dbTree"))
        dbTree->restoreSession(sessionValue["dbTree"]);

    if (sessionValue.contains("snippetsPanel"))
        codeSnippetsPanel->restoreSession(sessionValue["snippetsPanel"]);

    if (CFG_UI.General.RestoreSession.get())
    {
        if (sessionValue.contains("windowSessions"))
            restoreWindowSessions(sessionValue["windowSessions"].toList());

        if (sessionValue.contains("activeWindowTitle"))
        {
            QString title = sessionValue["activeWindowTitle"].toString();
            MdiWindow* window = ui->mdiArea->getWindowByTitle(title);
            if (window)
                ui->mdiArea->setActiveSubWindow(window);
        }
    }

    updateCornerDocking();
    updateWindowActions();

    if (LETOS->getConfig()->getConfigMigrated())
    {
        // This is needed to fix the sessions that were saved with older versions of Letos,
        // where code snippets panel was not existing and the tabification gets lost upon session retoring from older vesion.
        // This will happen only once, after config is migrated.
        tabifyDockWidget(dbTree, codeSnippetsPanel);
        dbTree->raise();
    }
}

void MainWindow::restoreWindowSessions(const QList<QVariant>& windowSessions)
{
    if (windowSessions.size() == 0)
        return;

    for (const QVariant& winSession : windowSessions)
        restoreWindowSession(winSession);
}

MdiWindow* MainWindow::restoreWindowSession(const QVariant &windowSessions)
{
    QHash<QString, QVariant> winSessionHash = windowSessions.toHash();
    if (!winSessionHash.contains("class"))
        return nullptr;

    // Find out the type of stored session
    QByteArray classBytes = winSessionHash["class"].toString().toLatin1();
    char* className = classBytes.data();
    QMetaType metaType = QMetaType::fromName(className);
    if (!metaType.isValid())
    {
        qWarning() << "Could not restore window session, because type" << className
                   << "is not known to Qt meta subsystem.";
        return nullptr;
    }

    // Try to instantiate the object
    void* object = metaType.create();
    if (!object)
    {
        qWarning() << "Could not restore window session, because type" << className
                   << "could not be instantiated.";
        return nullptr;
    }

    // Switch to session aware window, so we can use its session aware interface.
    MdiChild* mdiChild = reinterpret_cast<MdiChild*>(object);
    RequiresExplicitInit* explicitInit = dynamic_cast<RequiresExplicitInit*>(mdiChild);
    if (explicitInit)
        explicitInit->init();

    if (mdiChild->isInvalid())
    {
        mdiChild->deleteLater();
        return nullptr;
    }

    // Add the window to MDI area and restore its session
    MdiWindow* window = ui->mdiArea->addSubWindow(mdiChild);
    if (!window->restoreSession(winSessionHash))
    {
        window->setCloseWithoutSessionSaving(true);
        window->deleteLater();
        return nullptr;
    }

    return window;
}

bool MainWindow::setStyle(const QString& styleName)
{
    QStyle* style = QStyleFactory::create(styleName);
    if (!style)
    {
        notifyWarn(tr("Could not set style: %1", "main window").arg(styleName));
        return false;
    }

    STYLE->setStyle(style, styleName);
    statusField->refreshColors();
    CFG_UI.General.Style.set(styleName);

    return true;
}

QString MainWindow::currentStyle() const
{
    return STYLE->name();
}

EditorWindow* MainWindow::openSqlEditor(Db* dbToSet, const QString& sql)
{
    EditorWindow* win = openSqlEditor();
    if (!win->setCurrentDb(dbToSet))
    {
        qCritical() << "Created EditorWindow had not got requested database:" << dbToSet->getName();
        win->getMdiWindow()->close();
        return nullptr;
    }

    win->setContents(FORMATTER->format("sql", sql, dbToSet));
    return win;
}

EditorWindow* MainWindow::openSqlEditorForFile(Db* dbToSet, const QString& fileName)
{
    if (!SqlEditor::confirmBigFileLoading(fileName))
        return nullptr;

    EditorWindow* win = openSqlEditor();
    if (dbToSet && dbToSet->isOpen() && !win->setCurrentDb(dbToSet))
    {
        qCritical() << "Created EditorWindow had not got requested database:" << dbToSet->getName();
        win->getMdiWindow()->close();
        return nullptr;
    }

    win->openFile(fileName);
    return win;
}

void MainWindow::installToolbarSizeWheelHandler(QToolBar* toolbar)
{
    if (!toolbarSizeWheelHandler)
        toolbarSizeWheelHandler = MouseShortcut::forWheel(Qt::ControlModifier, this, SLOT(toolbarSizeChangeRequested(int)), toolbar);
    else
        toolbar->installEventFilter(toolbarSizeWheelHandler);
}

QMenu* MainWindow::createPopupMenu()
{
    QMenu* m = QMainWindow::createPopupMenu();
    m->addSeparator();
    if (!tbStyleMenu)
        tbStyleMenu = createToolbarStyleMenu(m);

    m->addMenu(tbStyleMenu);
    m->setTitle(tr("&View", "menubar"));
    return m;
}

void MainWindow::saveSession(bool hide)
{
    MdiWindow* currWindow = ui->mdiArea->getCurrentWindow();
    if (hide)
        this->hide();

    saveSession(currWindow);
}

void MainWindow::saveSession()
{
    saveSession(false);
}

void MainWindow::scheduleSessionSave()
{
    if (saveSessionTimer)
        saveSessionTimer->start(saveSessionDelayMs);
}

void MainWindow::toolbarSizeChangeRequested(int steps)
{
    int idx = toolbarSizeActionList | INDEX_OF(act, {return actionMap[act]->isChecked();});
    if (steps > 0)
        idx++;
    else if (steps < 0)
        idx--;

    if (idx < 0 || idx >= toolbarSizeActionList.size())
        return;

    int percInt = toolbarSizes[toolbarSizeActionList[idx]];
    CFG_UI.General.ToolBarIconSize.set(percInt);
}

void MainWindow::refreshSyntaxColors()
{
    QHashIterator<QString, CfgEntry*> it = CFG_UI.Colors.getEntries();
    while (it.hasNext())
    {
        it.next();
        if (it.value()->getDefaultValue().metaType() != QMetaType::fromType<QColor>())
            continue;

        CfgEntry* customEntry = CFG_UI.Colors.getEntryByName(it.key() + "Custom");
        if (!customEntry)
        {
            qWarning() << "Color setting" << it.value()->getFullKey() << "has no associated"
                       << (it.value()->getFullKey() + "Custom") << "setting";
            continue;
        }

        if (customEntry->get().toBool())
            continue; // customized by user

        it.value()->reset();
    }

    QList<SyntaxHighlighterPlugin*> plugins = PLUGINS->getLoadedPlugins<SyntaxHighlighterPlugin>();
    auto pluginIt = plugins.begin();
    while (pluginIt != plugins.end())
    {
        (*pluginIt)->refreshFormats();
        pluginIt++;
    }
}

void MainWindow::exportConfig()
{
    SettingsExportDialog dialog(this);
    dialog.exec();
}

void MainWindow::importConfig()
{
    SettingsImportDialog dialog(this);
    dialog.exec();
}

void MainWindow::updateToolbarStyle()
{
    QList<QToolBar*> toolbars = this->findChildren<QToolBar*>();
    applyToolbarStyle(toolbars);
    updateToolbarStyleActionState();
}

void MainWindow::closeNonSessionWindows()
{
    for (MdiWindow* window : ui->mdiArea->getWindows())
        if (!window->restoreSessionNextTime())
            window->close();
}

FormManager* MainWindow::getFormManager() const
{
    return formManager;
}

void MainWindow::setupDefShortcuts()
{
    BIND_SHORTCUTS(MainWindow, Action);
}

void MainWindow::openSqlEditorSlot()
{
    openSqlEditor();
}

void MainWindow::refreshMdiWindows()
{
    mdiMenu->clear();

    QStringList actionNames;
    QHash<QString, QAction*> nameToAction;
    for (QAction* action : getMdiArea()->getTaskBar()->getTasks())
    {
        actionNames << action->text();
        nameToAction[action->text()] = action;
    }

    sSort(actionNames);

    for (QString& name : actionNames)
        mdiMenu->addAction(nameToAction[name]);

    fixToolbarTooltips(ui->viewToolbar);

    updateWindowActions();
}

void MainWindow::hideStatusField()
{
    statusField->close();
}

void MainWindow::openConfig()
{
    ConfigDialog config(this);
    config.exec();
}

void MainWindow::openDdlHistorySlot()
{
    openDdlHistory();
}

void MainWindow::openFunctionEditorSlot()
{
    openFunctionEditor();
}

void MainWindow::openCodeSnippetsEditorSlot()
{
    openCodeSnippetEditor();
}

void MainWindow::openCollationEditorSlot()
{
    openCollationEditor();
}

void MainWindow::openExtensionManagerSlot()
{
    openExtensionManager();
}

void MainWindow::exportAnything()
{
    if (!ExportManager::isAnyPluginAvailable())
    {
        notifyError(tr("Cannot export, because no export plugin is loaded."));
        return;
    }

    ExportDialog dialog(this);
    Db* db = DBTREE->getSelectedOpenDb();

    if (db)
        dialog.setPreselectedDb(db, true);

    dialog.exec();
}

void MainWindow::importAnything()
{
    if (!ImportManager::isAnyPluginAvailable())
    {
        notifyError(tr("Cannot import, because no import plugin is loaded."));
        return;
    }

    ImportDialog dialog(this);
    Db* db = DBTREE->getSelectedOpenDb();
    if (db)
        dialog.setDb(db);

    dialog.exec();
}

void MainWindow::closeAllWindows()
{
    ui->mdiArea->closeAllSubWindows();
}

void MainWindow::closeAllLeftWindows()
{
    ui->mdiArea->closeAllLeftToActive();
}

void MainWindow::closeAllRightWindows()
{
    ui->mdiArea->closeAllRightToActive();
}

void MainWindow::closeAllWindowsButSelected()
{
    ui->mdiArea->closeAllButActive();
}

void MainWindow::closeSelectedWindow()
{
    ui->mdiArea->closeActiveSubWindow();
}

void MainWindow::renameWindow()
{
    MdiWindow* win = ui->mdiArea->getCurrentWindow();
    if (!win)
        return;

    QString newTitle = QInputDialog::getText(this, tr("Rename window"), tr("Enter new name for the window:"), QLineEdit::Normal, win->windowTitle());
    if (newTitle == win->windowTitle() || newTitle.isEmpty())
        return;

    win->rename(newTitle);

}

void MainWindow::openDebugConsole()
{
    showUiDebugConsole();
}

void MainWindow::openCssConsole()
{
    CssDebugDialog* dialog = new CssDebugDialog;
    dialog->show();
}

void MainWindow::reportBug()
{
    QDesktopServices::openUrl(QUrl(LETOS->getNewIssuePage()));
}

void MainWindow::requestFeature()
{
    QDesktopServices::openUrl(QUrl(LETOS->getNewIssuePage()));
}

void MainWindow::aboutLetos()
{
    AboutDialog dialog(AboutDialog::ABOUT, this);
    dialog.exec();
}

void MainWindow::licenses()
{
    AboutDialog dialog(AboutDialog::LICENSES, this);
    dialog.exec();
}

void MainWindow::homepage()
{
    QDesktopServices::openUrl(QUrl(LETOS->getHomePage()));
}

void MainWindow::githubReleases()
{
    QDesktopServices::openUrl(QUrl(LETOS->getGitHubReleases()));
}

void MainWindow::userManual()
{
    QDesktopServices::openUrl(QUrl(LETOS->getUserManualPage()));
}

void MainWindow::sqliteDocs()
{
    QDesktopServices::openUrl(QUrl(LETOS->getSqliteDocsPage()));
}

void MainWindow::reportHistory()
{
    QDesktopServices::openUrl(QUrl(LETOS->getIssuesPage()));
}

void MainWindow::donate()
{
    QDesktopServices::openUrl(QUrl(LETOS->getDonatePage()));
}

void MainWindow::statusFieldLinkClicked(const QString& link)
{
#ifdef HAS_UPDATEMANAGER
    if (link == openUpdatesUrl && newVersionDialog)
    {
        newVersionDialog->exec();
        return;
    }
#endif
#ifdef SUPPORT_REMINDERS
    if (link == showDonateUrl)
    {
        CFG_CORE.SupportReminder.LastClicked.set(QDate::currentDate());
        QDesktopServices::openUrl(LETOS->getDonatePage());
        return;
    }
#endif
}

void MainWindow::quit()
{
    close();
}

void MainWindow::updateMultipleSessionsSetting(const QVariant& newValue)
{
    Config::getSettings()->setValue(ALLOW_MULTIPLE_SESSIONS_SETTING, newValue);
}

void MainWindow::updateMultipleSessionsSetting()
{
    Config::getSettings()->setValue(ALLOW_MULTIPLE_SESSIONS_SETTING, CFG_UI.General.AllowMultipleSessions.get());
}

#ifdef HAS_UPDATEMANAGER
void MainWindow::updateAvailable(const QString& version, const QString& url)
{
    newVersionDialog = new NewVersionDialog(this);
    newVersionDialog->setUpdate(version, url);
    notifyInfo(tr("New updates are available. <a href=\"%1\">Click here for details</a>.").arg(openUpdatesUrl));
}

void MainWindow::noUpdatesAvailable(bool enforced)
{
    if (enforced)
        notifyInfo(tr("You're running the most recent version. No updates are available."));
}

void MainWindow::checkForUpdates()
{
    UPDATES->checkForUpdates(true);
}
#endif

#ifdef SUPPORT_REMINDERS
void MainWindow::checkForSupportReminder()
{
    static const int FIRST_DISPLAY_DAYS = 100;
    static const int MIN_LAUNCHES = 30;
    static const int REPEAT_DAYS = 180;
    static const int AFTER_CLICK_DAYS = 365;

    QDate today = QDate::currentDate();
    auto& reminderCfg = CFG_CORE.SupportReminder;

    QDate firstLaunch = reminderCfg.FirstLaunchDate.get();
    if (!firstLaunch.isValid())
    {
        firstLaunch = today;
        reminderCfg.FirstLaunchDate.set(firstLaunch);
    }

    static bool alreadyCountedInThisProcess = false;
    int launchCount = reminderCfg.LaunchCount.get();
    if (!alreadyCountedInThisProcess)
    {
        launchCount++;
        reminderCfg.LaunchCount.set(launchCount);
        alreadyCountedInThisProcess = true;
    }

    if (!reminderCfg.ShowOccasional.get())
        return;

    bool oldEnough = firstLaunch.isValid() && firstLaunch.daysTo(today) >= FIRST_DISPLAY_DAYS;
    bool enoughLaunches = launchCount >= MIN_LAUNCHES;

    if (!oldEnough || !enoughLaunches)
        return;

    QDate lastClicked = reminderCfg.LastClicked.get();
    if (lastClicked.isValid() && lastClicked.daysTo(today) < AFTER_CLICK_DAYS)
        return;

    QDate lastShown = reminderCfg.LastShown.get();
    if (lastShown.isValid() && lastShown.daysTo(today) < REPEAT_DAYS)
        return;

    reminderCfg.LastShown.set(today);

    QString message = tr("Letos is free and open source. If it helps with your work, consider <a href=\"%1\">supporting the project</a>.")
            .arg(showDonateUrl);

    QTimer::singleShot(1000, this, [message] {notifyInfo(message);});
}
#endif

void MainWindow::updateCornerDocking()
{
    if (CFG_UI.General.DockLayout.get() == "vertical")
    {
        setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
        setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
        setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
        setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    }
    else
    {
        setCorner(Qt::TopLeftCorner, Qt::TopDockWidgetArea);
        setCorner(Qt::TopRightCorner, Qt::TopDockWidgetArea);
        setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);
        setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);
    }
}

void MainWindow::messageFromSecondaryInstance(quint32 instanceId, QByteArray message)
{
    Q_UNUSED(instanceId);
    activateWindow();
    if (isMinimized())
        showMaximized();

    raise();
    QString dbToOpen = deserializeFromBytes(message).toString();
    if (!dbToOpen.isNull())
        openDb(dbToOpen);
}

DdlHistoryWindow* MainWindow::openDdlHistory()
{
    return openMdiWindow<DdlHistoryWindow>();
}

FunctionsEditor* MainWindow::openFunctionEditor()
{
    return openMdiWindow<FunctionsEditor>();
}

CodeSnippetEditor* MainWindow::openCodeSnippetEditor()
{
    return openMdiWindow<CodeSnippetEditor>();
}

CollationsEditor* MainWindow::openCollationEditor()
{
    return openMdiWindow<CollationsEditor>();
}

SqliteExtensionEditor* MainWindow::openExtensionManager()
{
    return openMdiWindow<SqliteExtensionEditor>();
}

void MainWindow::fixFonts()
{
    CfgTypedEntry<QFont>* typed = nullptr;
    for (CfgEntry* cfg : CFG_UI.Fonts.getEntries())
    {
        typed = dynamic_cast<CfgTypedEntry<QFont>*>(cfg);
        if (typed->get().pointSize() == 0)
            cfg->set(cfg->getDefaultValue());
    }
}

void MainWindow::fixToolbars()
{
    fixToolbarTooltips(ui->viewToolbar);
    fixToolbarTooltips(ui->mainToolBar);
    fixToolbarTooltips(ui->structureToolbar);
    fixToolbarTooltips(ui->dbToolbar);
}

QMenu* MainWindow::createToolbarStyleMenu(QMenu* parentMenu)
{
    QMenu* menu = new QMenu(parentMenu);
    menu->setTitle(tr("Toolbar &icons", "menubar"));

    QActionGroup* tbIconSizeGroup = new QActionGroup(menu);
    tbIconSizeGroup->setExclusive(true);
    for (Action& actEnum : toolbarSizeActionList)
    {
        int percInt = toolbarSizes[actEnum];;
        actionMap[actEnum] = new QAction(tr("Size: %1%", "toolbar icons").arg(percInt), this);
        actionMap[actEnum]->setCheckable(true);
        tbIconSizeGroup->addAction(actionMap[actEnum]);
        menu->addAction(actionMap[actEnum]);

        if (percInt <= 0) // safety
            percInt = 100;

        connect(actionMap[actEnum], &QAction::triggered, this, [this, percInt]() {
            CFG_UI.General.ToolBarIconSize.set(percInt);
        });
    }
    return menu;
}

void MainWindow::applyToolbarStyle(QToolBar* tb)
{
    bool useDefault = tb->property(CONSTANT_ICON_SIZE).toBool();
    int iconSizePerc = CFG_UI.General.ToolBarIconSize.get();
    int iconSizeInt = useDefault || (iconSizePerc <= 0) ?
                defaultToolbarIconSize :
                (defaultToolbarIconSize * iconSizePerc) / 100;
    QSize iconSize = QSize(iconSizeInt, iconSizeInt);
    tb->setIconSize(iconSize);
}

void MainWindow::applyToolbarStyle(QList<QToolBar*> tbList)
{
    int iconSizePerc = CFG_UI.General.ToolBarIconSize.get();
    int iconSizeInt = (iconSizePerc <= 0) ? defaultToolbarIconSize : (defaultToolbarIconSize * iconSizePerc) / 100;
    QSize iconSize = QSize(iconSizeInt, iconSizeInt);
    QSize defIconSize = QSize(defaultToolbarIconSize, defaultToolbarIconSize);
    for (QToolBar* tb : tbList)
    {
        if (tb->property(CONSTANT_ICON_SIZE).toBool())
            tb->setIconSize(defIconSize);
        else
            tb->setIconSize(iconSize);
    }
}

void MainWindow::updateToolbarStyleActionState()
{
    int iconSizePerc = CFG_UI.General.ToolBarIconSize.get();
    if (!toolbarSizesReversed.contains(iconSizePerc))
    {
        qCritical() << "Unexpected toolbar icon size in config:" << iconSizePerc << ", resetting to 100%.";
        CFG_UI.General.ToolBarIconSize.set(100);
        return;
    }
    Action act = toolbarSizesReversed[iconSizePerc];
    actionMap[act]->setChecked(true);
}

void MainWindow::initToolbarSizeActionList()
{
    QMetaEnum actMetaEnum = QMetaEnum::fromType<Action>();
    for (int actEnum = 0, total = actMetaEnum.keyCount(); actEnum < total; actEnum++)
    {
        const char* key = actMetaEnum.key(actEnum);
        if (!QString::fromLatin1(key).startsWith("TOOLBAR_ICON_SIZE_"))
            continue;

        Action act = static_cast<Action>(actEnum);
        int percInt = QString::fromLatin1(actMetaEnum.valueToKey(actEnum)).split('_').last().toInt();
        toolbarSizeActionList << act;
        toolbarSizes[act] = percInt;
        toolbarSizesReversed[percInt] = act;
    }
}

void MainWindow::handlePostRestoreConfigUpdates()
{
    QVariantHash updates = CFG_CORE.General.PostRestoreConfigUpdates.get();
    if (updates.contains("HideTheViewToolbar"))
    {
        ui->viewToolbar->setVisible(false);
        updates.remove("HideTheViewToolbar");
        CFG_CORE.General.PostRestoreConfigUpdates.set(updates);
    }
}

void MainWindow::initDropOverlay()
{
    dropOverlay = new WidgetCover(this);
    dropOverlay->setTransparency(192);
    QGridLayout* dropLayout = dropOverlay->getContainerLayout();
    dropLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* dropLabel = new QLabel(tr("Drop files to open them"), dropOverlay);
    dropLabel->setStyleSheet("QLabel { font-size: 24px; color: white; font-weight: bold; }");
    dropLabel->setAlignment(Qt::AlignCenter);
    dropLayout->addWidget(dropLabel, 0, 0);

    dropDetails = new QLabel("", dropOverlay);
    dropDetails->setStyleSheet("QLabel {"
                               "    color: white;"
                               "    background-color: rgba(30, 30, 30, 192);"
                               "    padding: 10px;"
                               "    border-radius: 15px;"
                               "}");
    dropDetails->setAlignment(Qt::AlignCenter);
    dropLayout->addWidget(dropDetails, 1, 0);
}

void MainWindow::initCommandPalette()
{
    commandPaletteOverlay = new WidgetCover(this);
    commandPaletteOverlay->setTransparency(192);
    QGridLayout* overlayLayout = commandPaletteOverlay->getContainerLayout();
    overlayLayout->setContentsMargins(0, 0, 0, 0);
    overlayLayout->setSizeConstraint(QLayout::SetDefaultConstraint);
    overlayLayout->setAlignment(Qt::AlignTop|Qt::AlignHCenter);
    overlayLayout->setRowStretch(0, 0);
    overlayLayout->setRowStretch(1, 1);
    overlayLayout->setColumnStretch(0, 1);

    commandPalette = new CommandPalette(commandPaletteOverlay);
    commandPalette->setStyleSheet("#commandPalette {"
                                  "    border-radius: 5px;"
                                  "    background-color: palette(window);"
                                  "}");
    commandPalette->setAutoFillBackground(true);
    overlayLayout->addWidget(commandPalette, 0, 0, Qt::AlignTop|Qt::AlignHCenter);
    commandPalette->hide();

    connect(commandPalette, SIGNAL(hidden()), commandPaletteOverlay, SLOT(hide()));
    connect(commandPalette, &CommandPalette::hidden, this, [this]()
    {
        removeEventFilter(commandPalette);
    });
}

void MainWindow::openCommandPalette()
{
    commandPaletteOverlay->show();
    commandPalette->show();
    installEventFilter(commandPalette);
}

void MainWindow::handleExternalDragEnter(const QStringList& filePaths)
{
    // qDebug() << "enter" << filePaths;

    static_qstring(detailsTpl, "<table cellpadding=\"5\">%1</table>");
    static_qstring(rowTpl, "<tr><td align=\"right\"><code>%1</code></td><td width=\"20\"></td><td>%2</td></tr>");
    QStringList rows;
    for (const QString& filePath : filePaths)
    {
        DropFileContext ctx = fileToDropContext(filePath);
        QString desc = dropDescriptionByFileType(ctx);
        rows << rowTpl.arg(ctx.fileName, desc);
    }

    dropDetails->setText(detailsTpl.arg(rows.join("")));
    if (!dropOverlay->isVisible())
        dropOverlay->show();
}

void MainWindow::handleExternalDragLeave()
{
    // qDebug() << "leave";
    if (dropOverlay->isVisible())
        dropOverlay->hide();
}

void MainWindow::handleDroppedFile(const QString& filePath)
{
    DropFileType fileType = fileToFileType(filePath);
    switch (fileType)
    {
        case MainWindow::DropFileType::SQLITE3:
        case MainWindow::DropFileType::SQLITE3_POSSIBLE:
        case MainWindow::DropFileType::SQLITE3_EMPTY:
            DBTREE->openDb(filePath);
            break;
        case MainWindow::DropFileType::SQL:
        case MainWindow::DropFileType::TEXT:
            openSqlEditorForFile(nullptr, filePath);
            break;
        case MainWindow::DropFileType::CSV:
        {
            ImportDialog dialog(this);
            dialog.setFilePath(filePath);
            dialog.exec();
            break;
        }
        case MainWindow::DropFileType::SQLITE2:
            notifyError(tr("The dropped file appears to be a SQLite 2 database, which is not supported by this Letos version. Last version supporting SQLite 2 was 3.2.1."));
            break;
        case MainWindow::DropFileType::OTHER:
            notifyWarn(tr("The dropped file type is unsupported: %1 (%2)").arg(filePath, fileToDropContext(filePath).mimeValue));
            break;
    }
}

QString MainWindow::dropDescriptionByFileType(const DropFileContext& ctx)
{
    switch (ctx.type)
    {
        case MainWindow::DropFileType::SQLITE3:
            return tr("SQLite 3 database - add to database list and open");
        case MainWindow::DropFileType::SQLITE3_POSSIBLE:
            return tr("It may be an encrypted SQLite 3 database. You can try to open it.");
        case MainWindow::DropFileType::SQLITE3_EMPTY:
            return tr("Empty file, but also empty SQLite 3 database - open as database");
        case MainWindow::DropFileType::SQL:
            return tr("SQL file - open in SQL Editor");
        case MainWindow::DropFileType::TEXT:
            return tr("Text file - open in SQL Editor");
        case MainWindow::DropFileType::CSV:
            return tr("CSV file - import using Import Dialog");
        case MainWindow::DropFileType::SQLITE2:
            return tr("SQLite 2 database - not supported anymore");
        case MainWindow::DropFileType::OTHER:
            return tr("Unsupported file type");
    }
    return QString();
}

bool MainWindow::confirmQuit(const QList<Committable*>& instances)
{
    QuitConfirmDialog dialog(MAINWINDOW);

    for (Committable* c : instances)
    {
        if (c->isUncommitted())
            dialog.addMessage(c->getQuitUncommittedConfirmMessage());
    }

    if (dialog.getMessageCount() == 0)
        return true;

    if (dialog.exec() == QDialog::Accepted)
        return true;

    return false;
}

CommandPalette* MainWindow::getCommandPalette() const
{
    return commandPalette;
}

QList<int> MainWindow::getActionsForCommandPalette() const
{
    return {
        MDI_TILE,
        MDI_CASCADE,
        MDI_TILE_HORIZONTAL,
        MDI_TILE_VERTICAL,
        TOOLBAR_ICON_SIZE_50,
        TOOLBAR_ICON_SIZE_75,
        TOOLBAR_ICON_SIZE_100,
        TOOLBAR_ICON_SIZE_125,
        TOOLBAR_ICON_SIZE_150,
        TOOLBAR_ICON_SIZE_175,
        TOOLBAR_ICON_SIZE_200,
        TOOLBAR_ICON_SIZE_250,
        TOOLBAR_ICON_SIZE_300,
        OPEN_SQL_EDITOR,
        OPEN_CONFIG,
        OPEN_DDL_HISTORY,
        OPEN_SNIPPETS_EDITOR,
        OPEN_FUNCTION_EDITOR,
        OPEN_COLLATION_EDITOR,
        OPEN_EXTENSION_MANAGER,
        EXPORT,
        IMPORT,
        RESTORE_WINDOW,
        OPEN_DEBUG_CONSOLE,
        OPEN_CSS_CONSOLE,
        LICENSES,
        HOMEPAGE,
        USER_MANUAL,
        SQLITE_DOCS,
        REPORT_BUG,
        FEATURE_REQUEST,
        ABOUT,
        DONATE,
        BUG_REPORT_HISTORY,
        CHECK_FOR_UPDATES,
        QUIT,
        EXPORT_SETTINGS,
        IMPORT_SETTINGS
    };
}

bool MainWindow::isClosingApp() const
{
    return closingApp;
}

QToolBar* MainWindow::getToolBar(int toolbar) const
{
    switch (static_cast<ToolBar>(toolbar))
    {
        case TOOLBAR_MAIN:
            return ui->mainToolBar;
        case TOOLBAR_DATABASE:
            return ui->dbToolbar;
        case TOOLBAR_STRUCTURE:
            return ui->structureToolbar;
        case TOOLBAR_VIEW:
            return ui->viewToolbar;
    }
    return nullptr;
}

void MainWindow::openDb(const QString& path)
{
    Db* db = DBLIST->getByPath(path);
    if (db)
    {
        notifyInfo(tr("Database passed in command line parameters (%1) was already on the list under name: %2").arg(path, db->getName()));
        return;
    }

    QString name = DBLIST->quickAddDb(path, QHash<QString,QVariant>());
    if (!name.isNull())
    {
        notifyInfo(tr("Database passed in command line parameters (%1) has been temporarily added to the list under name: %2").arg(path, name));
        db = DBLIST->getByName(name);
        db->open();
    }
    else
        notifyError(tr("Could not add database %1 to list.").arg(path));
}

QMenu* MainWindow::getDatabaseMenu() const
{
    return dbMenu;
}

QMenu* MainWindow::getStructureMenu() const
{
    return structMenu;
}

QMenu* MainWindow::getViewMenu() const
{
    return viewMenu;
}

QMenu* MainWindow::getToolsMenu() const
{
    return toolsMenu;
}

QMenu* MainWindow::getLetosMenu() const
{
    return letosMenu;
}

MainWindow *MainWindow::getInstance()
{
    if (!instance)
        instance = new MainWindow();

    return instance;
}

void MainWindow::setSafeMode(bool enabled)
{
    safeModeEnabled = enabled;
}

bool MainWindow::isSafeMode()
{
    return safeModeEnabled;
}

bool MainWindow::isSessionRestoringFinished()
{
    return sessionRestoringFinished;
}

bool MainWindow::isInternalDrop(const QMimeData* data)
{
    for (const QString& format : data->formats())
    {
        if (format.startsWith("application/x-letos-"))
            return true;
    }
    return false;
}

MainWindow::DropFileType MainWindow::fileToFileType(const QString& filePath)
{
    QMimeType mimeType = mimeDb.mimeTypeForFile(filePath);
    return mimeToFileType(mimeType.name());
}

MainWindow::DropFileContext MainWindow::fileToDropContext(const QString& filePath)
{
    QMimeType mimeType = mimeDb.mimeTypeForFile(filePath);
    QString mimeName = mimeType.name();
    DropFileType fileType = mimeToFileType(mimeName);

    return DropFileContext{
        mimeName,
        fileType,
        QFileInfo(filePath).fileName(),
        filePath,
    };
}

MainWindow::DropFileType MainWindow::mimeToFileType(const QString& mimeValue)
{
    if (mimeValue == "application/vnd.sqlite3" || mimeValue == "application/x-sqlite3")
        return DropFileType::SQLITE3;

    if (mimeValue == "application/x-zerosize")
        return DropFileType::SQLITE3_EMPTY;

    if (mimeValue == "application/octet-stream")
        return DropFileType::SQLITE3_POSSIBLE;

    if (mimeValue == "application/sql" || mimeValue == "text/x-sql")
        return DropFileType::SQL;

    if (mimeValue == "text/plain")
        return DropFileType::TEXT;

    if (mimeValue == "text/csv" || mimeValue == "text/tab-separated-values")
        return DropFileType::CSV;

    if (mimeValue == "application/x-sqlite2")
        return DropFileType::SQLITE2;

    return DropFileType::OTHER;
}

bool MainWindow::eventFilter(QObject* obj, QEvent* e)
{
    Q_UNUSED(obj);
    static QHash<QString, int> objCnt;
    static int totalCnt = 0;

    switch (e->type())
    {
        case QEvent::FileOpen:
        {
            QUrl url = dynamic_cast<QFileOpenEvent*>(e)->url();
            if (!url.isLocalFile())
                return false;

            DbDialog dialog(DbDialog::ADD, this);
            dialog.setPath(url.toLocalFile());
            dialog.exec();
            return true;
        }
        case QEvent::DragEnter:
        {
            auto* dragEv = static_cast<QDragEnterEvent*>(e);
            if (dragEv->mimeData()->hasUrls() && !isInternalDrop(dragEv->mimeData()))
            {
                dragEv->acceptProposedAction();
                if (totalCnt == 0)
                    handleExternalDragEnter(dragEv->mimeData()->urls() | MAP(url, {return url.toLocalFile();}));

                objCnt[obj->objectName()]++;
                totalCnt++;
                return true;
            }
            break;
        }
        case QEvent::Drop:
        {
            objCnt.clear();
            totalCnt = 0;
            handleExternalDragLeave();
            auto* dropEv = static_cast<QDropEvent*>(e);
            if (dropEv->mimeData()->hasUrls() && !isInternalDrop(dropEv->mimeData()))
            {
                for (const QUrl& url : dropEv->mimeData()->urls())
                    handleDroppedFile(url.toLocalFile());

                dropEv->acceptProposedAction();
                return true;
            }
            break;
        }
        case QEvent::DragLeave:
        {
            if (objCnt.contains(obj->objectName()) && objCnt[obj->objectName()] > 0)
            {
                objCnt[obj->objectName()]--;
                totalCnt--;
            }

            if (totalCnt <= 0)
            {
                objCnt.clear();
                totalCnt = 0;
                handleExternalDragLeave();
            }
        }
        default:
            break;
    }
    return false;
}

void MainWindow::pushClosedWindowSessionValue(const QVariant &value)
{
    closedWindowSessionValues.enqueue(value);

    if (closedWindowSessionValues.size() > closedWindowsStackSize)
        closedWindowSessionValues.dequeue();
}

void MainWindow::restoreLastClosedWindow()
{
    if (closedWindowSessionValues.size() == 0)
        return;

    QMdiSubWindow* activeWin = ui->mdiArea->activeSubWindow();
    bool maximizedMode = activeWin && activeWin->isMaximized();

    QVariant winSession = closedWindowSessionValues.takeLast();
    if (maximizedMode)
    {
        QHash<QString, QVariant> winSessionHash = winSession.toHash();
        winSessionHash.remove("geometry");
        winSession = winSessionHash;
    }

    restoreWindowSession(winSession);
}

bool MainWindow::hasClosedWindowToRestore() const
{
    return closedWindowSessionValues.size() > 0;
}

MainWindow::ToolBarStyleEnforcer::ToolBarStyleEnforcer(QObject* parent) :
    QObject(parent)
{
}

bool MainWindow::ToolBarStyleEnforcer::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Polish)
    {
        if (QToolBar* tb = qobject_cast<QToolBar*>(obj))
            qobject_cast<MainWindow*>(parent())->applyToolbarStyle(tb);
    }
    return QObject::eventFilter(obj, event);
}
