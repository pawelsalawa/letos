#include "dbdialog.h"
#include "common/immediatetooltip.h"
#include "services/notifymanager.h"
#include "ui_dbdialog.h"
#include "services/pluginmanager.h"
#include "plugins/dbplugin.h"
#include "uiutils.h"
#include "common/utils.h"
#include "uiconfig.h"
#include "services/dbmanager.h"
#include "common/global.h"
#include "iconmanager.h"
#include "sqleditor.h"
#include "db/sqlquery.h"
#include "common/passwordtogglehelper.h"
#include <QDateTimeEdit>
#include <QSpinBox>
#include <QDebug>
#include <QPushButton>
#include <QFileDialog>
#include <QComboBox>
#include <QTimer>
#include <QMimeData>
#include <QDir>

DbDialog::DbDialog(Mode mode, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DbDialog),
    mode(mode)
{
    init();
}

DbDialog::~DbDialog()
{
    delete ui;
}

void DbDialog::setDb(Db* db)
{
    this->db = db;
}

void DbDialog::setPermanent(bool perm)
{
    ui->permamentCheckBox->setChecked(perm);
}


void DbDialog::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasUrls())
        e->acceptProposedAction();
}

void DbDialog::dropEvent(QDropEvent* e)
{
    if (!e->isAccepted() && e->mimeData()->hasUrls())
    {
        setPath(e->mimeData()->urls().first().toLocalFile());
        e->accept();
    }
}

QString DbDialog::getPath()
{
    QString newPath = QDir::fromNativeSeparators(ui->fileEdit->text());
    return newPath;
}

void DbDialog::setPath(const QString& path)
{
    QString newPath = QDir::toNativeSeparators(path);
    ui->fileEdit->setText(newPath);
}

QString DbDialog::getName()
{
    return ui->nameEdit->text();
}

bool DbDialog::isPermanent()
{
    return ui->permamentCheckBox->isChecked();
}

void DbDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
        case QEvent::LanguageChange:
            ui->retranslateUi(this);
            break;
        default:
            break;
    }
}

void DbDialog::showEvent(QShowEvent *e)
{
    if (db)
    {
        disableTypeAutodetection = true;
        QString pluginName = db->getConnectionOptions().value("plugin", QVariant()).toString();
        if (!pluginName.isEmpty())
        {
            for (int i = 0, total = ui->typeCombo->count(); i < total; i++)
            {
                DbPlugin* plugin = static_cast<DbPlugin*>(ui->typeCombo->itemData(i).value<void*>());
                if (plugin->getName() == pluginName)
                {
                    ui->typeCombo->setCurrentIndex(i);
                    break;
                }
            }
        }

        setPath(db->getPath());
        ui->nameEdit->setText(db->getName());
        ui->nameLabel->setText(db->getName());
        disableTypeAutodetection = false;
    }

    if (ui->typeCombo->currentIndex() == -1 || !db && ui->typeCombo->count() > 0)
    {
        int idx = ui->typeCombo->findText("SQLite 3", Qt::MatchFixedString); // we should have SQLite 3 plugin
        if (idx > -1)
            ui->typeCombo->setCurrentIndex(idx);
        else
            ui->typeCombo->setCurrentIndex(0);
    }

    existingDatabaseNames = DBLIST->getDbNames();
    if (mode == EDIT)
        existingDatabaseNames.removeOne(db->getName());

    updateOptions();
    updateState();

    if (doAutoTest)
        testConnectionClicked();

    QDialog::showEvent(e);
}

void DbDialog::init()
{
    ui->setupUi(this);
    setWindowIcon(ICONS.DATABASE);
    ui->browseOpenButton->setIcon(ICONMANAGER->getIcon("directory_open"));
    connIconTooltip = new ImmediateTooltip(ui->testConnIcon);
    ui->nameEdit->setVisible(false);
    setNameLabelText("");
    updateNameLink();

    QList<DbPlugin*> plugins = PLUGINS->getLoadedPlugins<DbPlugin>();
    sSort(plugins, [](DbPlugin* p1, DbPlugin* p2)
    {
        return p1->getName().toLower() < p2->getName().toLower();
    });
    for (DbPlugin* dbPlugin : plugins)
        ui->typeCombo->addItem(dbPlugin->getLabel(), QVariant::fromValue<void*>(static_cast<void*>(dbPlugin)));

    ui->testConnIcon->setVisible(false);
    initialBrowseTooltip = ui->browseOpenButton->toolTip();

    connect(ui->existingDatabaseRadio, SIGNAL(clicked()), this, SLOT(updateCreateMode()));
    connect(ui->createDatabaseRadio, SIGNAL(clicked()), this, SLOT(updateCreateMode()));
    connect(ui->fileEdit, SIGNAL(textChanged(QString)), this, SLOT(fileChanged(QString)));
    connect(ui->nameEdit, SIGNAL(textEdited(QString)), this, SLOT(nameModified(QString)));
    connect(ui->browseOpenButton, SIGNAL(clicked()), this, SLOT(browseClicked()));
    connect(ui->testConnButton, SIGNAL(clicked()), this, SLOT(testConnectionClicked()));
    connect(ui->typeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(dbTypeChanged(int)));
    connect(ui->nameChangeLink, SIGNAL(linkActivated(QString)), this, SLOT(toggleNameEdit()));

    layout()->setSizeConstraint(QLayout::SetFixedSize);

    if (mode == Mode::ADD && CFG_UI.DbList.NewDbNotPermanentByDefault.get())
        ui->permamentCheckBox->setChecked(false);

    validate();
}

void DbDialog::updateOptions()
{
    setUpdatesEnabled(false);

    // Remove olds
    for (QWidget*& w : optionWidgets)
    {
        ui->optionsGrid->removeWidget(w);
        delete w;
    }

    customBrowseHandler = nullptr;
    ui->pathGroup->setTitle(tr("File"));
    ui->existingDatabaseRadio->setVisible(true);
    ui->existingDatabaseRadio->setChecked(!createMode);
    ui->createDatabaseRadio->setVisible(true);
    ui->createDatabaseRadio->setChecked(createMode);
    ui->browseOpenButton->setToolTip(initialBrowseTooltip);
    updateCreateMode();

    optionWidgets.clear();
    optionKeyToWidget.clear();
    optionKeyToType.clear();
    helperToKey.clear();

    lastWidgetInTabOrder = ui->permamentCheckBox;

    // Retrieve new list
    DbPlugin* plugin = getCurrentPlugin();
    if (plugin)
    {
        QList<DbPluginOption> optList = plugin->getOptionsList();
        if (optList.size() > 0)
        {
            // Add new options
            int row = ADDITIONAL_ROWS_BEGIN_INDEX;
            for (const DbPluginOption& opt : optList)
            {
                addOption(opt, row);
                row++;
            }
        }
    }


    setUpdatesEnabled(true);
}

void DbDialog::addOption(const DbPluginOption& option, int& row)
{
    if (option.type == DbPluginOption::CUSTOM_PATH_BROWSE)
    {
        // This option does not add any editor, but has it's own label for path edit.
        row--;
        ui->pathGroup->setTitle(option.label);
        ui->existingDatabaseRadio->setChecked(true);
        ui->existingDatabaseRadio->setVisible(false);
        ui->createDatabaseRadio->setChecked(false);
        ui->createDatabaseRadio->setVisible(false);
        updateCreateMode();
        if (!option.toolTip.isEmpty())
            ui->browseOpenButton->setToolTip(option.toolTip);

        customBrowseHandler = option.customBrowseHandler;
        return;
    }

    QLabel* label = new QLabel(option.label, this);
    label->setAlignment(Qt::AlignTop|Qt::AlignRight);
    QWidget* editor = nullptr;
    QWidget* editorHelper = nullptr; // TODO, based on plugins for Url handlers

    editor = getEditor(option, editorHelper);
    Q_ASSERT(editor != nullptr);

    optionWidgets << label << editor;

    optionKeyToWidget[option.key] = editor;
    optionKeyToType[option.key] = option.type;
    ui->optionsGrid->addWidget(label, row, 0);
    ui->optionsGrid->addWidget(editor, row, 1);

    setTabOrder(lastWidgetInTabOrder, editor);
    lastWidgetInTabOrder = editor;

    if (editorHelper)
    {
        ui->optionsGrid->addWidget(editorHelper, row, 2);
        optionWidgets << editorHelper;
        helperToKey[editorHelper] = option.key;

        setTabOrder(lastWidgetInTabOrder, editorHelper);
        lastWidgetInTabOrder = editorHelper;
    }

    if (db && db->getConnectionOptions().contains(option.key))
        setValueFor(option.type, editor, db->getConnectionOptions()[option.key]);
}

QWidget *DbDialog::getEditor(const DbPluginOption& opt, QWidget*& editorHelper)
{
    QWidget* editor = nullptr;
    QLineEdit* le = nullptr;
    editorHelper = nullptr;
    switch (opt.type)
    {
        case DbPluginOption::SQL:
        {
            SqlEditor* sqlEdit = new SqlEditor(this);
            editor = sqlEdit;
            sqlEdit->setShowLineNumbers(false);
            sqlEdit->setPlainText(opt.defaultValue.toString());
            sqlEdit->setMaximumHeight(sqlEdit->fontMetrics().height() * 5);
            connect(sqlEdit, SIGNAL(textChanged()), this, SLOT(propertyChanged()));
            break;
        }
        case DbPluginOption::STRING:
        {
            editor = new QLineEdit(this);
            le = dynamic_cast<QLineEdit*>(editor);
            connect(le, SIGNAL(textChanged(QString)), this, SLOT(propertyChanged()));
            break;
        }
        case DbPluginOption::PASSWORD:
        {
            editor = new QLineEdit(this);
            le = dynamic_cast<QLineEdit*>(editor);
            new PasswordToggleHelper(le);
            connect(le, SIGNAL(textChanged(QString)), this, SLOT(propertyChanged()));
            break;
        }
        case DbPluginOption::CHOICE:
        {
            QComboBox* cb = new QComboBox(this);
            editor = cb;
            cb->setEditable(!opt.choiceReadOnly);
            if (opt.choiceDataValues.isEmpty())
            {
                cb->addItems(opt.choiceValues);
                cb->setCurrentText(opt.defaultValue.toString());
            }
            else
            {
                for (auto it = opt.choiceDataValues.begin(); it != opt.choiceDataValues.end(); ++it)
                {
                    cb->addItem(it.key(), it.value());
                    if (it.value() == opt.defaultValue)
                        cb->setCurrentText(it.key());
                }
            }
            connect(cb, SIGNAL(currentTextChanged(QString)), this, SLOT(propertyChanged()));
            break;
        }
        case DbPluginOption::INT:
        {
            QSpinBox* sb = new QSpinBox(this);
            editor = sb;
            if (!isNull(opt.minValue))
                sb->setMinimum(opt.minValue.toInt());

            if (!isNull(opt.maxValue))
                sb->setMaximum(opt.maxValue.toInt());

            if (!isNull(opt.defaultValue))
                sb->setValue(opt.defaultValue.toInt());

            connect(sb, SIGNAL(valueChanged(int)), this, SLOT(propertyChanged()));
            break;
        }
        case DbPluginOption::FILE:
        {
            editor = new QLineEdit(this);
            le = dynamic_cast<QLineEdit*>(editor);
            editorHelper = new QPushButton(tr("Browse"), this);
            connect(le, SIGNAL(textChanged(QString)), this, SLOT(propertyChanged()));
            connect(editorHelper, SIGNAL(pressed()), this, SLOT(browseForFile()));
            break;
        }
        case DbPluginOption::BOOL:
        {
            QCheckBox* cb = new QCheckBox(this);
            editor = cb;
            if (!isNull(opt.defaultValue))
                cb->setChecked(opt.defaultValue.toBool());

            connect(cb, SIGNAL(stateChanged(int)), this, SLOT(propertyChanged()));
            break;
        }
        case DbPluginOption::DOUBLE:
        {
            QDoubleSpinBox* sb = new QDoubleSpinBox(this);
            editor = sb;
            if (!isNull(opt.minValue))
                sb->setMinimum(opt.minValue.toDouble());

            if (!isNull(opt.maxValue))
                sb->setMaximum(opt.maxValue.toDouble());

            if (!isNull(opt.defaultValue))
                sb->setValue(opt.defaultValue.toDouble());

            connect(sb, SIGNAL(valueChanged(double)), this, SLOT(propertyChanged()));
            break;
        }
        case DbPluginOption::CUSTOM_PATH_BROWSE:
            return nullptr; // should not happen ever, asserted one stack level before
        default:
            // TODO plugin based handling of custom editors
            qWarning() << "Unhandled DbDialog option for creating editor.";
            break;
    }

    if (le)
    {
        le->setPlaceholderText(opt.placeholderText);
        le->setText(opt.defaultValue.toString());
    }

    if (!opt.toolTip.isNull())
        editor->setToolTip(opt.toolTip);

    return editor;
}

QVariant DbDialog::getValueFrom(DbPluginOption::Type type, QWidget *editor)
{
    QVariant value;
    switch (type)
    {
        case DbPluginOption::SQL:
            value = dynamic_cast<SqlEditor*>(editor)->toPlainText();
            break;
        case DbPluginOption::STRING:
        case DbPluginOption::PASSWORD:
        case DbPluginOption::FILE:
            value = dynamic_cast<QLineEdit*>(editor)->text();
            break;
        case DbPluginOption::INT:
            value = dynamic_cast<QSpinBox*>(editor)->value();
            break;
        case DbPluginOption::BOOL:
            value = dynamic_cast<QCheckBox*>(editor)->isChecked();
            break;
        case DbPluginOption::DOUBLE:
            value = dynamic_cast<QDoubleSpinBox*>(editor)->value();
            break;
        case DbPluginOption::CHOICE:
        {
            QComboBox* cb = dynamic_cast<QComboBox*>(editor);
            QVariant data = cb->currentData();
            if (data.isValid())
            {
                value = data;
                break;
            }
            value = cb->currentText();
            break;
        }
        case DbPluginOption::CUSTOM_PATH_BROWSE:
            break; // should not happen ever
        default:
            // TODO plugin based handling of custom editors
            qWarning() << "Unhandled DbDialog option for value.";
            break;
    }
    return value;
}

void DbDialog::setValueFor(DbPluginOption::Type type, QWidget *editor, const QVariant &value)
{
    switch (type)
    {
        case DbPluginOption::SQL:
            dynamic_cast<SqlEditor*>(editor)->setPlainText(value.toString());
            break;
        case DbPluginOption::STRING:
        case DbPluginOption::FILE:
        case DbPluginOption::PASSWORD:
            dynamic_cast<QLineEdit*>(editor)->setText(value.toString());
            break;
        case DbPluginOption::INT:
            dynamic_cast<QSpinBox*>(editor)->setValue(value.toInt());
            break;
        case DbPluginOption::BOOL:
            dynamic_cast<QCheckBox*>(editor)->setChecked(value.toBool());
            break;
        case DbPluginOption::DOUBLE:
            dynamic_cast<QDoubleSpinBox*>(editor)->setValue(value.toDouble());
            break;
        case DbPluginOption::CHOICE:
        {
            QComboBox* cb = dynamic_cast<QComboBox*>(editor);
            if (value.isValid())
            {
                int idx = cb->findData(value);
                if (idx > -1)
                {
                    cb->setCurrentIndex(idx);
                    break;
                }
            }
            cb->setCurrentText(value.toString());
            break;
        }
        case DbPluginOption::CUSTOM_PATH_BROWSE:
            break; // should not happen ever
        default:
            qWarning() << "Unhandled DbDialog option to set value.";
            // TODO plugin based handling of custom editors
            break;
    }
}

void DbDialog::updateType()
{
    if (disableTypeAutodetection)
        return;

    DbPlugin* validPlugin = LETOS->getDbManager()->getPluginForDbFile(getPath());
    if (!validPlugin || validPlugin->getLabel() == ui->typeCombo->currentText())
        return;

    ui->typeCombo->setCurrentText(validPlugin->getLabel());
}

QHash<QString, QVariant> DbDialog::collectOptions()
{
    QHash<QString, QVariant> options;
    if (ui->typeCombo->currentIndex() < 0)
        return options;

    for (const QString& key : optionKeyToWidget.keys())
        options[key] = getValueFrom(optionKeyToType[key], optionKeyToWidget[key]);

    DbPlugin* plugin = getCurrentPlugin();
    if (plugin)
        options[DB_PLUGIN] = plugin->getName();

    return options;
}

bool DbDialog::testDatabase(QString& errorMsg)
{
    DbPlugin* plugin = getCurrentPlugin();
    if (!plugin)
    {
        errorMsg = tr("Database type not selected.");
        return false;
    }

    QString path = getPath();
    if (path.isEmpty())
    {
        errorMsg = tr("Database path not specified.");
        return false;
    }

    QUrl url(path);
    if (url.scheme().isEmpty())
        url.setScheme("file");

    QHash<QString, QVariant> options = collectOptions();
    Db* testDb = plugin->getInstance("", path, options, &errorMsg);

    bool res = false;
    if (testDb)
    {
        if (testDb->openForProbing())
        {
            res = !testDb->exec("SELECT sqlite_version();")->getSingleCell().toString().isEmpty();
            errorMsg = testDb->getErrorText();
            testDb->closeQuiet();
        }
        delete testDb;
    }

    return res;
}

bool DbDialog::validate()
{
    // Name
    bool nameState = true;
    if (ui->nameEdit->text().isEmpty() && nameManuallyEdited)
    {
        nameState = false;
        setValidState(ui->nameEdit, false, tr("Enter an unique database name."));
    }

    Db* registeredDb = nullptr;
    if (nameState)
    {
        registeredDb = DBLIST->getByName(ui->nameEdit->text(), Qt::CaseInsensitive);
        if (registeredDb && (mode == Mode::ADD || registeredDb != db))
        {
            nameState = false;
            setValidState(ui->nameEdit, false, tr("This name is already in use. Please enter unique name."));
        }
    }

    if (nameState)
    {
        if (nameManuallyEdited)
            setValidStateInfo(ui->nameEdit, tr("<p>Automatic name generation was disabled, because the name was edited manually. To restore automatic generation please erase contents of the name field.</p>"));
        else
            setValidState(ui->nameEdit, true);
    }

    // File
    bool fileState = true;
    if (ui->fileEdit->text().isEmpty())
    {
        setValidState(ui->fileEdit, false, tr("Enter a database file path."));
        fileState = false;
    }
    else
    {
        QUrl url = QUrl(ui->fileEdit->text());
        if (!url.scheme().isEmpty() && !url.isLocalFile())
        {
            // Plugin-driven URL. Nothing to validate more about it here.
        }
        else if (QFileInfo(getPath()).isRelative())
        {
            setValidStateWarning(ui->fileEdit,
                tr("You're using a relative file path, which will be resolved to \"%1\" according to the application's working directory. It's always better to use absolute file path to avoid unexpected database location.")
                                 .arg(QFileInfo(getPath()).absoluteFilePath()));
        }
    }

    if (fileState)
    {
        registeredDb = DBLIST->getByPath(getPath());
        if (registeredDb && (mode == Mode::ADD || registeredDb != db))
        {
            setValidState(ui->fileEdit, false, tr("This database is already on the list under name: %1").arg(registeredDb->getName()));
            fileState = false;
        }
    }

    if (fileState)
        setValidState(ui->fileEdit, true);

    // Type
    bool typeState = true;
    if (ui->typeCombo->count() == 0)
    {
        // No need to set validation message here. SQLite3 plugin is built in,
        // so if this happens, something is really, really wrong.
        qCritical() << "No db plugins loaded in db dialog!";
        typeState = false;
    }

    if (typeState)
    {
        if (ui->typeCombo->currentIndex() < 0)
        {
            setValidState(ui->typeCombo, false, tr("Select a database type."));
            typeState = false;
        }
    }

    if (typeState)
        setValidState(ui->typeCombo, true);

    return nameState && fileState && typeState;
}

void DbDialog::updateState()
{
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(validate());
}

void DbDialog::updateNameLink()
{
    static_qstring(tpl, R"(
        <html><head/><body>
            <p><a href="change"><span style="text-decoration: underline; color:#27bf73;">%1</span></a></p>
        </body></html>
    )");
    ui->nameChangeLink->setText(tpl.arg(
                                    ui->nameEdit->isVisible() ?
                                    tr("Automatic", "database name") :
                                    tr("Change", "database name")
                                ));
}

void DbDialog::setNameLabelText(const QString& value)
{
    QFont fnt = ui->nameLabel->font();
    if (value.isEmpty())
    {
        ui->nameLabel->setText(tr("(empty)"));
        ui->nameLabel->setEnabled(false);
        fnt.setItalic(true);
    }
    else
    {
        ui->nameLabel->setText(value);
        ui->nameLabel->setEnabled(true);
        fnt.setItalic(false);
    }
    ui->nameLabel->setFont(fnt);
}

DbPlugin* DbDialog::getCurrentPlugin() const
{
    if (ui->typeCombo->currentIndex() == -1)
        return nullptr;

    return static_cast<DbPlugin*>(ui->typeCombo->currentData().value<void*>());
}

void DbDialog::setDoAutoTest(bool value)
{
    doAutoTest = value;
}

void DbDialog::setCreateMode(bool createDbMode)
{
    if (createDbMode)
        ui->createDatabaseRadio->setChecked(true);
    else
        ui->existingDatabaseRadio->setChecked(true);

    updateCreateMode();
}

void DbDialog::propertyChanged()
{
    ui->testConnIcon->setVisible(false);
}

void DbDialog::typeChanged(int index)
{
    Q_UNUSED(index);
    updateOptions();
    updateState();
}

void DbDialog::valueForNameGenerationChanged()
{
    if (nameManuallyEdited)
    {
        updateState();
        return;
    }

    QString generatedName;
    DbPlugin* plugin = getCurrentPlugin();
    if (plugin)
        generatedName = DBLIST->generateUniqueDbName(plugin, getPath());
    else
        generatedName = DBLIST->generateUniqueDbName(getPath());

    ui->nameEdit->setText(generatedName);
    setNameLabelText(generatedName);
    updateState();
}

void DbDialog::browseForFile()
{
    QString dir = getFileDialogInitPath();
    QString path = QFileDialog::getOpenFileName(0, QString(), dir);
    if (path.isEmpty())
        return;

    QString key = helperToKey[dynamic_cast<QWidget*>(sender())];
    setValueFor(optionKeyToType[key], optionKeyToWidget[key], path);

    setFileDialogInitPathByFile(path);
}

void DbDialog::fileChanged(const QString &arg1)
{
    Q_UNUSED(arg1);
    valueForNameGenerationChanged();
    updateType();
    propertyChanged();

    if (!customBrowseHandler)
    {
        QString path = getPath();
        if (!path.isEmpty())
        {
            QFileInfo fileInfo(path);
            bool isFile = fileInfo.exists() && fileInfo.isFile();
            ui->existingDatabaseRadio->setChecked(isFile);
            ui->createDatabaseRadio->setChecked(!isFile);
            updateCreateMode();
        }
    }
}

void DbDialog::browseClicked()
{
    if (customBrowseHandler)
    {
        QString newUrl = customBrowseHandler(getPath());
        if (!newUrl.isNull())
        {
            setPath(newUrl);
            updateState();
        }
        return;
    }

    QFileInfo fileInfo(getPath());
    QString dir;
    if (ui->fileEdit->text().isEmpty())
        dir = getFileDialogInitPath();
    else if (fileInfo.exists() && fileInfo.isFile())
        dir = fileInfo.absolutePath();
    else if (fileInfo.dir().exists())
        dir = fileInfo.dir().absolutePath();
    else
        dir = getFileDialogInitPath();

    QString path = getDbPath(createMode, dir);
    if (path.isNull())
        return;

    setFileDialogInitPathByFile(path);

    ui->fileEdit->setText(path);
    updateState();
}

void DbDialog::testConnectionClicked()
{
    QString errorMsg;
    bool ok = testDatabase(errorMsg);
    ui->testConnIcon->setPixmap(ok ? ICONS.TEST_CONN_OK : ICONS.TEST_CONN_ERROR);
    connIconTooltip->setToolTip(ok ? QString() : errorMsg);
    ui->testConnIcon->setVisible(true);
    if (!ok)
    {
        QString path = getPath();
        if (!path.isEmpty())
            notifyWarn(QString("%1: %2").arg(getPath(), errorMsg));
        else
            notifyWarn(errorMsg);
    }
}

void DbDialog::dbTypeChanged(int index)
{
    typeChanged(index);
    propertyChanged();
}

void DbDialog::nameModified(const QString &value)
{
    nameManuallyEdited = true;
    setNameLabelText(value);
    updateState();
}

void DbDialog::updateCreateMode()
{
    createMode = ui->createDatabaseRadio->isChecked();
    ui->browseOpenButton->setToolTip(
        createMode ? tr("Choose a location for the new database file")
                   : tr("Browse for existing database file on local computer")
    );
}

void DbDialog::toggleNameEdit()
{
    if (ui->nameEdit->isVisible())
    {
        nameManuallyEdited = false;
        valueForNameGenerationChanged();
        ui->nameEdit->setVisible(false);
        ui->nameLabel->setVisible(true);
    }
    else
    {
        ui->nameEdit->setVisible(true);
        ui->nameLabel->setVisible(false);
        ui->nameEdit->setFocus();
    }
    updateNameLink();
}

void DbDialog::accept()
{
    QString name = getName();
    QString path = getPath();
    QHash<QString, QVariant> options = collectOptions();
    bool perm = isPermanent();
    bool result;
    if (mode == ADD)
        result = DBLIST->addDb(name, path, options, perm);
    else
        result = DBLIST->updateDb(db, name, path, options, perm);

    if (result)
        QDialog::accept();
}
