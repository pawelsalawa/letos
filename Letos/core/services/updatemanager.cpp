#include "common/utils.h"
#ifdef HAS_UPDATEMANAGER

#include "updatemanager.h"
#include "services/notifymanager.h"
#include "services/config.h"
#include <QDebug>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtConcurrent/QtConcurrentRun>

UpdateManager::UpdateManager(QObject *parent) :
    QObject(parent)
{
    connect(this, SIGNAL(updatingError(QString)), this, SLOT(handleUpdatingError(QString)));
    connect(this, &UpdateManager::updateAvailable, this, [this]() {emit finished(true);});
    connect(this, &UpdateManager::noUpdatesAvailable, this, [this]() {emit finished(true);});
    connect(this, &UpdateManager::updatingError, this, [this]() {emit finished(false);});
    netManager = new QNetworkAccessManager(this);
}

UpdateManager::~UpdateManager()
{

}

void UpdateManager::checkForUpdates(bool enforce)
{
    if (!CFG_CORE.General.CheckUpdatesOnStartup.get() && !enforce)
        return;

    qDebug() << "Available SSL backends:" << QSslSocket::availableBackends();
    qDebug() << "Active SSL backend:" << QSslSocket::activeBackend();

    static_qstring(url, "https://letos.org/rest/updates");
    QNetworkRequest request(url);
    QNetworkReply* response = netManager->get(request);
    connect(response, &QNetworkReply::finished, [this, response, enforce]()
    {
        response->deleteLater();
        handleUpdatesResponse(response, enforce);
    });
}

bool UpdateManager::isPlatformEligibleForUpdate() const
{
    return getDistributionType() != DistributionType::OS_MANAGED;
}

void UpdateManager::handleUpdatesResponse(QNetworkReply* response, bool enforced)
{
    if (response->error() != QNetworkReply::NoError)
    {
        emit updatingError(response->errorString());
        return;
    }

    QJsonParseError parsingError;
    QJsonDocument json = QJsonDocument::fromJson(response->readAll(), &parsingError);

    if (parsingError.error != QJsonParseError::NoError)
    {
        emit updatingError(parsingError.errorString());
        return;
    }

    QString version = json["version"].toString();
    QStringList versionParts = version.split(".");
    QString alignedVersion = versionParts[0] + versionParts[1].rightJustified(2, '0') + versionParts[2].rightJustified(2, '0');
    int versionNumber = alignedVersion.toInt();

    if (LETOS->getVersion() >= versionNumber)
    {
        emit noUpdatesAvailable(enforced);
        return;
    }


#if defined(Q_OS_WIN)
    QString cpuArch = QSysInfo::buildCpuArchitecture();
    QString dist = QFileInfo::exists(QCoreApplication::applicationDirPath() + "/unins000.exe") ? "-inst" : "-port";
    QString url = cpuArch.startsWith("arm") ? json["winarm"+dist].toString() : json["win64"+dist].toString();
#elif defined(Q_OS_LINUX)
    QString cpuArch = QSysInfo::buildCpuArchitecture();
    QString dist = qEnvironmentVariableIsSet("APPIMAGE") ? "-img" : "-port";
    QString url = cpuArch.startsWith("arm") ? json["linarm"+dist].toString() : json["lin64"+dist].toString();
#elif defined(Q_OS_OSX)
    QString url = json["mac"].toString();
#else
    QString url = LETOS->getHomePage();
#endif

    emit updateAvailable(version, url);
}

void UpdateManager::handleUpdatingError(const QString& errorMessage)
{
    NOTIFY_MANAGER->warn(tr("Could not check for updates (%1).").arg(errorMessage));
}

#endif // HAS_UPDATEMANAGER
