#include "mainwindow.h"
#include "iconmanager.h"
#include "dbtree/dbtreeitem.h"
#include "datagrid/sqlquerymodelcolumn.h"
#include "datagrid/sqlquerymodel.h"
#include "sqleditor.h"
#include "windows/editorwindow.h"
#include "windows/tablewindow.h"
#include "windows/virtualtablewindow.h"
#include "windows/viewwindow.h"
#include "dataview.h"
#include "dbtree/dbtree.h"
#include "multieditor/multieditordatetime.h"
#include "multieditor/multieditortime.h"
#include "multieditor/multieditordate.h"
#include "multieditor/multieditorbool.h"
#include "uidebug.h"
#include "completionhelper.h"
#include "services/updatemanager.h"
#include "log.h"
#include "qio.h"
#include "translations.h"
#include "dialogs/languagedialog.h"
#include "dialogs/triggerdialog.h"
#include "services/pluginmanager.h"
#include "singleapplication/singleapplication.h"
#include "services/impl/configimpl.h"
#include "common/colorpickerpopup.h"
#include "datagrid/sqlqueryview.h"
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QApplication>
#include <QSplashScreen>
#include <QThread>
#include <QPluginLoader>
#include <QDebug>
#include <QMessageBox>
#include <QProcess>
#include <QFileDialog>
#include <QSettings>
#ifdef Q_OS_WIN
#   include <windef.h>
#   include <windows.h>
#endif

static bool listPlugins = false;
static bool doNotLoadPlugins = false;

QString uiHandleCmdLineArgs(bool applyOptions = true)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("GUI interface to Letos, a SQLite manager."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption masterConfigOption("master-config", QObject::tr("Points to the master configuration file. Read manual at wiki page for more details."), QObject::tr("settings file"));
    QCommandLineOption safeModeOption({"X", "safe-mode"}, QObject::tr("Starts the application in safe mode without restoring the previous session. Use this to bypass issues caused by a corrupted session."));
    QCommandLineOption noPluginsOption("no-plugins", QObject::tr("Do not load any plugins. Can be used alongside safe mode to further isolate potential issues."));
    QCommandLineOption debugOption({"d", "debug"}, QObject::tr("Enables debug messages in console (accessible with F12)."));
    QCommandLineOption debugStdOutOption({"do", "debug-stdout"}, QObject::tr("Redirects debug messages into standard output (forces debug mode)."));
    QCommandLineOption debugFileOption({"df", "debug-file"}, QObject::tr("Redirects debug messages into given file (forces debug mode)."), QObject::tr("log file"));
    QCommandLineOption lemonDebugOption("debug-lemon", QObject::tr("Enables Lemon parser debug messages for SQL code assistant."));
    QCommandLineOption sqlDebugOption({"ds", "debug-sql"}, QObject::tr("Enables debugging of every single SQL query being sent to any database."));
    QCommandLineOption sqlDebugDbNameOption("debug-sql-db", QObject::tr("Limits SQL query messages to only the given <database>."), QObject::tr("database"));
    QCommandLineOption executorDebugOption("debug-query-executor", QObject::tr("Enables debugging of Letos's query executor."));
    QCommandLineOption listPluginsOption("list-plugins", QObject::tr("Lists plugins installed in the Letos and quits."));
    parser.addOption(safeModeOption);
    parser.addOption(debugOption);
    parser.addOption(debugStdOutOption);
    parser.addOption(debugFileOption);
    parser.addOption(lemonDebugOption);
    parser.addOption(sqlDebugOption);
    parser.addOption(sqlDebugDbNameOption);
    parser.addOption(executorDebugOption);
    parser.addOption(masterConfigOption);
    parser.addOption(listPluginsOption);
    parser.addOption(noPluginsOption);

    parser.addPositionalArgument(QObject::tr("file"), QObject::tr("Database file to open"));

    parser.process(qApp->arguments());

    if (applyOptions)
    {
        bool enableDebug = parser.isSet(debugOption) || parser.isSet(debugStdOutOption) || parser.isSet(sqlDebugOption) || parser.isSet(debugFileOption);
        setUiDebug(enableDebug, !parser.isSet(debugStdOutOption), parser.value(debugFileOption));
        CompletionHelper::enableLemonDebug = parser.isSet(lemonDebugOption);
        setSqlLoggingEnabled(parser.isSet(sqlDebugOption));
        setExecutorLoggingEnabled(parser.isSet(executorDebugOption));
        if (parser.isSet(sqlDebugDbNameOption))
            setSqlLoggingFilter(parser.value(sqlDebugDbNameOption));

        if (parser.isSet(listPluginsOption))
            listPlugins = true;

        if (parser.isSet(noPluginsOption))
            doNotLoadPlugins = true;

        if (parser.isSet(masterConfigOption))
            Config::setMasterConfigFile(parser.value(masterConfigOption));

        if (parser.isSet(safeModeOption))
            MainWindow::setSafeMode(true);
    }

    QStringList args = parser.positionalArguments();
    if (args.size() > 0)
        return args[0];

    return QString();
}

void initUiScaling()
{
    if (qEnvironmentVariableIsEmpty("QT_SCALE_FACTOR"))
    {
        QString scale = Config::getSettings()->value(MainWindow::UI_SCALE_SETTING).toString();
        if (!scale.isEmpty())
        {
            scale = QString::number(scale.toDouble() / 100.0);
            qputenv("QT_SCALE_FACTOR", scale.toLatin1());
        }
    }

    if (qEnvironmentVariableIsEmpty("QT_SCALE_FACTOR_ROUNDING_POLICY"))
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
}

bool shouldAllowMultipleSessions()
{
    QVariant allowMultipleSessionsValue = Config::getSettings()->value(MainWindow::ALLOW_MULTIPLE_SESSIONS_SETTING);
    return allowMultipleSessionsValue.isValid() && allowMultipleSessionsValue.toBool();
}

int main(int argc, char *argv[])
{
    qunsetenv("QT_PLUGIN_PATH"); // #4241 ignore user's QT_PLUGIN_PATH, because it can break Letos if it's incompatible Qt path

    QCoreApplication::setApplicationName("Letos");
    QCoreApplication::setOrganizationName("letos.org");
    QCoreApplication::setApplicationVersion(LETOS->getVersionString());

    initUiScaling();

    SingleApplication a(argc, argv, true, SingleApplication::ExcludeAppPath|SingleApplication::ExcludeAppVersion|SingleApplication::User);

    if (!shouldAllowMultipleSessions() && a.isSecondary()) {
#ifdef Q_OS_WIN
        AllowSetForegroundWindow(DWORD( a.primaryPid()));
#endif
        QString dbToOpen = uiHandleCmdLineArgs();
        a.sendMessage(serializeToBytes(dbToOpen));
        return 0;
    }

    qInstallMessageHandler(uiMessageHandler);

    qRegisterMetaType<QList<QColor>>("QList<QColor>");
    qRegisterMetaType<QVector<QColor>>("QVector<QColor>");

    Config::setAskUserForConfigDirFunc([]() -> QString
    {
       return QFileDialog::getExistingDirectory(nullptr, QObject::tr("Select configuration directory"), QString(), QFileDialog::ShowDirsOnly);
    });

    QString dbToOpen = uiHandleCmdLineArgs();
    DbTreeItem::initMeta();
    SqlQueryModelColumn::initMeta();
    SqlQueryModel::staticInit();
    ColorPickerPopup::staticInit();

    LETOS->setInitialTranslationFiles({"qtbase", "core", "gui", "letos"});
    LETOS->init(a.arguments(), true);
    scanForCustomFonts();
    IconManager::getInstance()->init();
    DbTree::staticInit();
    DataView::staticInit();
    EditorWindow::staticInit();
    TableWindow::staticInit();
    VirtualTableWindow::staticInit();
    ViewWindow::staticInit();
    MultiEditorDateTime::staticInit();
    MultiEditorTime::staticInit();
    MultiEditorDate::staticInit();
    MultiEditorBool::staticInit();
    TriggerDialog::staticInit();
    SqlEditor::staticInit();

    MainWindow* mainWin = MAINWINDOW;

    QObject::connect(&a, &SingleApplication::receivedMessage, mainWin, &MainWindow::messageFromSecondaryInstance);

    if (!doNotLoadPlugins)
        LETOS->initPlugins();

    if (listPlugins)
    {
        for (const PluginManager::PluginDetails& details : PLUGINS->getAllPluginDetails())
            qOut << details.name << " " << details.versionString << "\n";

        return 0;
    }

    if (!LanguageDialog::didAskForDefaultLanguage() && !LETOS->getConfig()->isInMemory())
    {
        LanguageDialog::askedForDefaultLanguage();
        QMap<QString, QString> langs = getAvailableLanguages();

        LanguageDialog dialog;
        dialog.setLanguages(langs);
        dialog.setSelectedLang(getConfigLanguageDefault());
        if (dialog.exec() == QDialog::Accepted)
            setDefaultLanguage(dialog.getSelectedLang());

        QProcess::startDetached(qApp->arguments().at(0), qApp->arguments().mid(1));
        return 0;
    }

    // Initialize cell renderers, including those from plugins
    SqlQueryView::staticInit();

    // Shortcuts titles needs to be retranslated, because their titles were set initially in global scope,
    // while translation files were not loaded yet. Now they are.
    ExtActionContainer::refreshShortcutTranslations();

    MainWindow::getInstance()->restoreSession();
    MainWindow::getInstance()->show();

    if (!dbToOpen.isNull())
        MainWindow::getInstance()->openDb(dbToOpen);

#ifdef HAS_UPDATEMANAGER
    UPDATES->checkForUpdates();
#endif

    return a.exec();
}
