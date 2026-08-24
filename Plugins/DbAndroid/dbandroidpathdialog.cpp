#include "dbandroidpathdialog.h"
#include "ui_dbandroidpathdialog.h"
#include "common/ipvalidator.h"
#include "dbandroid.h"
#include "common/widgetcover.h"
#include "adbmanager.h"
#include "uiutils.h"
#include "iconmanager.h"
#include "dbandroidconnection.h"
#include "dbandroidconnectionfactory.h"
#include "common/lazytrigger.h"
#include "common/userinputfilter.h"
#include "common/utils.h"
#include <QDebug>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>

DbAndroidPathDialog::DbAndroidPathDialog(const DbAndroid* plugin, QWidget *parent) :
    QDialog(parent),
    plugin(plugin),
    ui(new Ui::DbAndroidPathDialog)
{
    init();
}

DbAndroidPathDialog::~DbAndroidPathDialog()
{
    delete ui;
}

void DbAndroidPathDialog::setUrl(const QString& url)
{
    dbUrl = DbAndroidUrl(url);
    loadUrl();
}

void DbAndroidPathDialog::setUrl(const DbAndroidUrl& url)
{
    dbUrl = url;
    loadUrl();
}

const DbAndroidUrl& DbAndroidPathDialog::getUrl() const
{
    return dbUrl;
}

void DbAndroidPathDialog::init()
{
    ui->setupUi(this);

    dbListCover = new WidgetCover(ui->databaseCombo);
    appListCover = new WidgetCover(ui->appCombo);
    new UserInputFilter(ui->appFilterEdit, this, SLOT(applyAppFilter(QString)));

    ui->createDatabaseButton->setIcon(ICONS.PLUS);
    ui->deleteDatabaseButton->setIcon(ICONS.DELETE);


    dbListUpdateTrigger = new LazyTrigger(500, this, SLOT(refreshDbList()));
    appListUpdateTrigger = new LazyTrigger(500, this, SLOT(refreshAppList()));

    connect(ui->deviceCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(scheduleAppListUpdate()));
    connect(ui->databaseCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateState()));
    connect(ui->portSpin, SIGNAL(valueChanged(int)), this, SLOT(scheduleDbListUpdate()));
    connect(ui->createDatabaseButton, SIGNAL(clicked()), this, SLOT(createNewDatabase()));
    connect(ui->deleteDatabaseButton, SIGNAL(clicked()), this, SLOT(deleteSelectedDatabase()));
    connect(ui->passwordGroup, SIGNAL(toggled(bool)), this, SLOT(updateState()));
    connect(ui->passwordGroup, SIGNAL(toggled(bool)), this, SLOT(scheduleDbListUpdate()));
    connect(ui->passwordEdit, SIGNAL(textChanged(QString)), this, SLOT(scheduleDbListUpdate()));

    connect(this, SIGNAL(asyncDbListUpdatingFinished(bool)), this, SLOT(handleFinishedAsyncDbListUpdate(bool)));
    connect(this, SIGNAL(asyncAppListUpdatingFinished()), this, SLOT(handleFinishedAsyncAppListUpdate()));
    connect(this, SIGNAL(callForDbListUpdate(QStringList)), this, SLOT(handleUpdateDbList(QStringList)));
    connect(this, SIGNAL(callForAppListUpdate(QStringList)), this, SLOT(handleUpdateAppList(QStringList)));
    connect(this, SIGNAL(callForValidations()), this, SLOT(updateValidations()));
    connect(this, SIGNAL(callForDbCreationUpdate(bool)), this, SLOT(handleDbCreationUpdate(bool)));

    if (!plugin->isAdbValid())
    {
        ui->ipRadio->setChecked(true);
        ui->usbRadio->setEnabled(false);
        ui->shellRadio->setEnabled(false);
    }
    else
    {
        refreshDevices();
        connect(plugin->getAdbManager(), SIGNAL(deviceDetailsChanged(QList<Device>)), this, SLOT(updateDeviceList()));
    }

    connect(ui->ipRadio, SIGNAL(toggled(bool)), this, SLOT(modeChanged(bool)));
    connect(ui->usbRadio, SIGNAL(toggled(bool)), this, SLOT(modeChanged(bool)));
    connect(ui->shellRadio, SIGNAL(toggled(bool)), this, SLOT(modeChanged(bool)));
    connect(ui->ipEdit, SIGNAL(textChanged(QString)), this, SLOT(scheduleDbListUpdate()));
    setDbListUpdatesEnabled(true);

    handleDbCreationUpdate(false);
    updateState();
    adjustSize();
    scheduleDbListUpdate();
}

void DbAndroidPathDialog::updateUrl()
{
    DbAndroidMode mode = getSelectedMode();
    dbUrl.setEnforcedMode(mode);
    switch (mode)
    {
        case DbAndroidMode::NETWORK:
            dbUrl.setHost(ui->ipEdit->text());
            dbUrl.setPort(ui->portSpin->value());
            break;
        case DbAndroidMode::USB:
            dbUrl.setDevice(ui->deviceCombo->currentData().toString());
            dbUrl.setPort(ui->portSpin->value());
            break;
        case DbAndroidMode::SHELL:
            dbUrl.setDevice(ui->deviceCombo->currentData().toString());
            dbUrl.setApplication(ui->appCombo->currentText());
            break;
        case DbAndroidMode::null:
            qCritical() << "Unknown mode in DbAndroidPathDialog::updateUrl()";
            return;
    }

    dbUrl.setDbName(ui->databaseCombo->currentText());
    if (ui->passwordGroup->isChecked())
        dbUrl.setPassword(ui->passwordEdit->text());
    else
        dbUrl.setPassword(QString());
}

void DbAndroidPathDialog::loadUrl()
{
    if (!dbUrl.isValid())
        return;

    switch (dbUrl.getMode())
    {
        case DbAndroidMode::NETWORK:
            ui->ipRadio->setChecked(true);
            ui->ipEdit->setText(dbUrl.getHost());
            break;
        case DbAndroidMode::SHELL:
            ui->shellRadio->setChecked(true);
            ui->deviceCombo->setCurrentIndex(ui->deviceCombo->findData(dbUrl.getDevice()));
            setDbListUpdatesEnabled(false);
            if (ui->appCombo->findText(dbUrl.getApplication()) == -1)
                ui->appCombo->addItem(dbUrl.getApplication());

            ui->appCombo->setCurrentText(dbUrl.getApplication());
            setDbListUpdatesEnabled(true);
            break;
        case DbAndroidMode::USB:
            ui->usbRadio->setChecked(true);
            ui->deviceCombo->setCurrentIndex(ui->deviceCombo->findData(dbUrl.getDevice()));
            break;
        case DbAndroidMode::null:
            qCritical() << "Cannot load URL of mode 'null' in DbAndroidPathDialog::loadUrl().";
            return;
    }

    ui->portSpin->setValue(dbUrl.getPort());
    if (ui->databaseCombo->findText(dbUrl.getDbName()) == -1)
        ui->databaseCombo->addItem(dbUrl.getDbName());

    ui->databaseCombo->setCurrentText(dbUrl.getDbName());

    if (!dbUrl.getPassword().isNull())
    {
        ui->passwordGroup->setChecked(true);
        ui->passwordEdit->setText(dbUrl.getPassword());
    }
}

void DbAndroidPathDialog::scheduleDbListUpdate()
{
    if (suspendDbListUpdates)
        return;

    dbListUpdateTrigger->schedule();
    if (!dbListCover->isVisible())
        dbListCover->show();

    handleDbCreationUpdate(false);
    updateValidations();
}

void DbAndroidPathDialog::scheduleAppListUpdate()
{
    if (getSelectedMode() != DbAndroidMode::SHELL)
        return;

    if (suspendAppListUpdates)
        return;

    appListUpdateTrigger->schedule();
    if (!appListCover->isVisible())
        appListCover->show();

    updateValidations();
}

void DbAndroidPathDialog::refreshDbList()
{
    if (updatingDbList)
    {
        // Already busy, schedule next update afterwards.
        scheduleDbListUpdate();
        return;
    }

    updateUrl();
    ui->databaseCombo->clear();

    if (!dbUrl.isValid(false))
    {
        dbListCover->hide();
        return;
    }

    updatingDbList = true;
    runInThread([=, this]{ asyncDbUpdate(dbUrl.toUrlString(), dbUrl.getMode()); });
}

void DbAndroidPathDialog::refreshAppList()
{
    if (updatingAppList)
    {
        // Already busy, schedule next update afterwards.
        scheduleAppListUpdate();
        return;
    }

    updateUrl();
    setDbListUpdatesEnabled(false);
    ui->appCombo->clear();
    setDbListUpdatesEnabled(true);

    if (!dbUrl.isValid(false))
    {
        appListCover->hide();
        return;
    }

    updatingAppList = true;
    runInThread([=, this]{ asyncAppUpdate(dbUrl.toUrlString(), dbUrl.getMode()); });
}

void DbAndroidPathDialog::asyncDbUpdate(const QString& connectionUrl, DbAndroidMode enforcedMode)
{
    DbAndroidUrl url(connectionUrl);
    url.setEnforcedMode(enforcedMode);

    QScopedPointer<DbAndroidConnection> connection(plugin->getConnectionFactory()->create(url));
    if (!connection->connectToAndroid(url))
    {
        qDebug() << "Could not open db connection" << connectionUrl;
        emit asyncDbListUpdatingFinished(connection->isAppOkay());
        emit callForValidations();
        return;
    }

    QStringList dbList = connection->getDbList();
    bool appOk = connection->isAppOkay();

    connection->disconnectFromAndroid();

    emit callForDbCreationUpdate(appOk);
    emit callForDbListUpdate(dbList);
    emit asyncDbListUpdatingFinished(appOk);
    emit callForValidations();
}

void DbAndroidPathDialog::asyncAppUpdate(const QString& connectionUrl, DbAndroidMode enforcedMode)
{
    DbAndroidUrl url(connectionUrl);
    url.setEnforcedMode(enforcedMode);

    QScopedPointer<DbAndroidConnection> connection(plugin->getConnectionFactory()->create(url));
    QStringList appList = connection->getAppList();
    emit callForAppListUpdate(appList);
    emit asyncAppListUpdatingFinished();
    emit callForValidations();
}

void DbAndroidPathDialog::refreshDevices()
{
    static_qstring(displayNameTpl, "%1 (%2)");
    ui->deviceCombo->clear();

    QString displayName;
    QList<AdbManager::Device> deviceDetails = plugin->getAdbManager()->getDeviceDetails();
    for (const AdbManager::Device& details : deviceDetails)
    {
        if (details.fullName.isEmpty())
            displayName = details.id;
        else
            displayName = displayNameTpl.arg(details.fullName, details.id);

        ui->deviceCombo->addItem(displayName, details.id);
    }
}

DbAndroidMode DbAndroidPathDialog::getSelectedMode() const
{
    if (ui->ipRadio->isChecked())
        return DbAndroidMode::NETWORK;

    if (ui->usbRadio->isChecked())
        return DbAndroidMode::USB;

    return DbAndroidMode::SHELL;
}

void DbAndroidPathDialog::setDbListUpdatesEnabled(bool enabled)
{
    suspendDbListUpdates = !enabled;
    if (enabled)
    {
        connect(ui->deviceCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(scheduleDbListUpdate()));
        connect(ui->appCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(scheduleDbListUpdate()));
    }
    else
    {
        disconnect(ui->deviceCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(scheduleDbListUpdate()));
        disconnect(ui->appCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(scheduleDbListUpdate()));
    }
}

void DbAndroidPathDialog::updateDeviceList()
{
    suspendDbListUpdates = true;

    bool dbListNeedsUpdate = false;
    QString oldValue = ui->deviceCombo->currentData().toString();

    refreshDevices();
    int idx = ui->deviceCombo->findData(oldValue);
    if (idx > -1)
        ui->deviceCombo->setCurrentIndex(idx);
    else
        dbListNeedsUpdate = true;

    suspendDbListUpdates = false;

    updateValidations();

    if (dbListNeedsUpdate)
        scheduleDbListUpdate();
}

void DbAndroidPathDialog::updateValidations()
{
    // bool isUpdating = dbListCover->isVisible();
    bool ipOk = true;
    bool deviceOk = true;
    if (ui->ipRadio->isChecked())
    {
        ipOk = IpValidator::check(ui->ipEdit->text());
        setValidState(ui->ipEdit, ipOk, tr("Enter valid IP address."));
    }
    else
    {
        deviceOk = !ui->deviceCombo->currentData().toString().isEmpty();
        setValidState(ui->deviceCombo, deviceOk, tr("Pick Android device."));
    }

    bool dbOk = !ui->databaseCombo->currentText().isEmpty();
    setValidState(ui->databaseCombo, dbOk, tr("Pick Android database."));

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(ipOk && deviceOk && dbOk && !updatingDbList);
}

void DbAndroidPathDialog::handleUpdateDbList(const QStringList& dbList)
{
    ui->databaseCombo->addItems(dbList);
    if (dbList.contains(dbUrl.getDbName()))
        ui->databaseCombo->setCurrentText(dbUrl.getDbName());
}

void DbAndroidPathDialog::handleUpdateAppList(const QStringList& apps)
{
    fullAppList = apps;
    QStringList filtered = apps.filter(ui->appFilterEdit->text(), Qt::CaseInsensitive);
    ui->appCombo->addItems(filtered);
    if (filtered.contains(dbUrl.getApplication()))
        ui->appCombo->setCurrentText(dbUrl.getApplication());
}

void DbAndroidPathDialog::handleFinishedAsyncDbListUpdate(bool appOkay)
{
    if (getSelectedMode() == DbAndroidMode::SHELL)
        setValidState(ui->appCombo, appOkay, tr("Selected Android application is unknown, or not debuggable."));

    dbListCover->hide();
    updatingDbList = false;
}

void DbAndroidPathDialog::handleFinishedAsyncAppListUpdate()
{
    appListCover->hide();
    updatingAppList = false;
}

void DbAndroidPathDialog::handleDbCreationUpdate(bool canCreateDatabases)
{
    ui->createDatabaseButton->setEnabled(canCreateDatabases);
}

void DbAndroidPathDialog::createNewDatabase()
{
    DbAndroidUrl tmpUrl(dbUrl);
    tmpUrl.setDbName(QString());

    DbAndroidConnection::ExecutionResult results;
    QString name;
    bool ok = false;
    while (!ok)
    {
        name = QInputDialog::getText(this, tr("Create new database"), tr("Please provide name for the new database.\n"
                                                                         "It's the name which Android application will use to connect to the database:"));

        if (name.isNull())
            break;

        if (ui->databaseCombo->findText(name) > -1)
        {
            QMessageBox::warning(this, tr("Invalid name"), tr("Database with the same name (%1) already exists on the device.\n"
                                                              "The name must be unique."));
            continue;
        }

        tmpUrl.setDbName(name);
        QScopedPointer<DbAndroidConnection> connection(plugin->getConnectionFactory()->create(tmpUrl));
        if (!connection->connectToAndroid(tmpUrl))
        {
            QMessageBox::warning(this, tr("Invalid name"), tr("Could not create database '%1', because could not connect to the device.").arg(name));
            continue;
        }

        results = connection->executeQuery("PRAGMA encoding;");
        ok = !results.wasError && results.resultDataList.size() > 0;
        connection->disconnectFromAndroid();

        if (!ok)
            QMessageBox::warning(this, tr("Invalid name"), tr("Could not create database '%1'.\nDetails: %2").arg(name, results.errorMsg));
    }

    if (ok)
    {
        ui->databaseCombo->addItem(name);
        ui->databaseCombo->setCurrentText(name);
    }
}

void DbAndroidPathDialog::deleteSelectedDatabase()
{
    updateUrl();
    QString dbName = dbUrl.getDbName();

    QMessageBox::StandardButton res = QMessageBox::question(this, tr("Delete database"), tr("Are you sure you want to delete database '%1' from %2?")
                                                            .arg(dbName, dbUrl.getDisplayName()));

    if (res != QMessageBox::Yes)
        return;

    int idx = ui->databaseCombo->findText(dbName);
    if (idx < 0)
    {
        QStringList dbList;
        for (int i = 0, total = ui->databaseCombo->count(); i < total; ++i)
            dbList << ui->databaseCombo->itemText(i);

        qCritical() << "Tried to delete database, but it's not in the list of databases:" << dbName << "and the list is:" << dbList;
        return;
    }

    // Local db instance, will close on deletion.
    QScopedPointer<DbAndroidConnection> connection(plugin->getConnectionFactory()->create(dbUrl));
    if (!connection->connectToAndroid(dbUrl))
    {
        QMessageBox::critical(this, tr("Error deleting"), tr("Could not connect to %1 in order to delete database '%2'.").arg(dbUrl.getDisplayName(), dbName));
        return;
    }

    if (!connection->deleteDatabase(dbName))
    {
        QMessageBox::critical(this, tr("Error deleting"), tr("Could not delete database named '%1' from the device.\n"
                                                             "Android device refused deletion, or it was impossible.").arg(dbName));

        connection->disconnectFromAndroid();
        return;
    }
    connection->disconnectFromAndroid();

    ui->databaseCombo->removeItem(idx);
    if (ui->databaseCombo->count() > 0)
    {
        if (idx < ui->databaseCombo->count())
            ui->databaseCombo->setCurrentIndex(idx);
        else
            ui->databaseCombo->setCurrentIndex(ui->databaseCombo->count() - 1);
    }
}

void DbAndroidPathDialog::modeChanged(bool checked)
{
    if (!checked)
        return;

    updateState();
    adjustSize();
    scheduleAppListUpdate();

    if (getSelectedMode() != DbAndroidMode::SHELL)
        scheduleDbListUpdate();
}

void DbAndroidPathDialog::applyAppFilter(const QString& value)
{
    QString selectedApp = ui->appCombo->currentText();
    QStringList filtered = fullAppList.filter(value, Qt::CaseInsensitive);
    bool callDbListUpdate = false;

    setDbListUpdatesEnabled(false);
    ui->appCombo->clear();
    ui->appCombo->addItems(filtered);
    if (filtered.contains(selectedApp))
        ui->appCombo->setCurrentText(selectedApp);
    else
        callDbListUpdate = true;

    setDbListUpdatesEnabled(true);

    if (callDbListUpdate)
        scheduleDbListUpdate();
}

void DbAndroidPathDialog::accept()
{
    updateUrl();
    QDialog::accept();
}

void DbAndroidPathDialog::updateState()
{
    DbAndroidMode mode = getSelectedMode();

    ui->deviceGroup->setVisible(mode == DbAndroidMode::SHELL || mode == DbAndroidMode::USB);
    ui->ipGroup->setVisible(mode == DbAndroidMode::NETWORK);
    ui->portGroup->setVisible(mode == DbAndroidMode::NETWORK || mode == DbAndroidMode::USB);
    ui->appGroup->setVisible(mode == DbAndroidMode::SHELL);
    ui->passwordGroup->setVisible(mode == DbAndroidMode::NETWORK || mode == DbAndroidMode::USB);

    ui->deleteDatabaseButton->setEnabled(ui->databaseCombo->currentIndex() > -1);
    ui->passwordEdit->setEnabled(ui->passwordGroup->isChecked());
    updateValidations();
}
