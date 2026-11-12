#include "updater.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QVersionNumber>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryFile>
#include <QRegularExpression>
#include <QSettings>
#include <QProcess>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QEventLoop>

Updater::Updater(QWidget *parentWidget)
    : QObject(parentWidget)
    , m_parentWidget(parentWidget)
    , netManager(new QNetworkAccessManager(this))
{
}

Updater::~Updater()
{
}

bool Updater::transitionTo(UpdaterState newState)
{
    if (m_state == newState) return true;
    if (!isValidUpdaterTransition(m_state, newState)) {
        return false;
    }
    m_state = newState;
    emit stateChanged(m_state);
    return true;
}

QString Updater::currentVersion()
{
    return QStringLiteral(EASY68K_VERSION_STRING);
}

QString Updater::repositorySlug()
{
    return QStringLiteral("Sebo1411/EASy68KLinux");
}

bool Updater::isNewerVersion(const QString &currentVer, const QString &remoteVer)
{
    QString cur = currentVer.trimmed();
    if (cur.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        cur = cur.mid(1);
    }
    QString rem = remoteVer.trimmed();
    if (rem.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        rem = rem.mid(1);
    }

    QVersionNumber cVer = QVersionNumber::fromString(cur);
    QVersionNumber rVer = QVersionNumber::fromString(rem);

    if (cVer.isNull() || rVer.isNull()) {
        return false;
    }
    return rVer > cVer;
}

ReleaseInfo Updater::parseReleaseJson(const QByteArray &jsonBytes)
{
    ReleaseInfo info;
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        return info;
    }

    QJsonObject obj = doc.object();
    info.tagName = obj.value(QStringLiteral("tag_name")).toString();
    if (info.tagName.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        info.version = info.tagName.mid(1);
    } else {
        info.version = info.tagName;
    }

    info.htmlUrl = obj.value(QStringLiteral("html_url")).toString();
    info.body = obj.value(QStringLiteral("body")).toString();
    info.publishedAt = obj.value(QStringLiteral("published_at")).toString();

    QJsonArray assetsArr = obj.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &val : assetsArr) {
        if (!val.isObject()) {
            continue;
        }
        QJsonObject assetObj = val.toObject();
        ReleaseAsset asset;
        asset.name = assetObj.value(QStringLiteral("name")).toString();
        asset.downloadUrl = assetObj.value(QStringLiteral("browser_download_url")).toString();
        asset.size = assetObj.value(QStringLiteral("size")).toInteger(0);

        if (asset.name.compare(QStringLiteral("SHA256SUMS.txt"), Qt::CaseInsensitive) == 0) {
            info.sha256SumsUrl = asset.downloadUrl;
        }
        info.assets.append(asset);
    }

    return info;
}

ReleaseAsset Updater::selectBestAssetForSystem(const ReleaseInfo &info)
{
    if (info.assets.isEmpty()) {
        return ReleaseAsset();
    }

    QStringList preferredSuffixes;
    if (qEnvironmentVariableIsSet("APPIMAGE")) {
        preferredSuffixes = {QStringLiteral(".AppImage"), QStringLiteral(".tar.gz"), QStringLiteral(".deb"), QStringLiteral(".rpm")};
    } else if (QFile::exists(QStringLiteral("/usr/bin/dpkg"))) {
        preferredSuffixes = {QStringLiteral(".deb"), QStringLiteral(".AppImage"), QStringLiteral(".tar.gz"), QStringLiteral(".rpm")};
    } else if (QFile::exists(QStringLiteral("/usr/bin/rpm"))) {
        preferredSuffixes = {QStringLiteral(".rpm"), QStringLiteral(".AppImage"), QStringLiteral(".tar.gz"), QStringLiteral(".deb")};
    } else {
        preferredSuffixes = {QStringLiteral(".AppImage"), QStringLiteral(".tar.gz"), QStringLiteral(".deb"), QStringLiteral(".rpm")};
    }

    for (const QString &suffix : preferredSuffixes) {
        for (const ReleaseAsset &asset : info.assets) {
            if (asset.name.endsWith(suffix, Qt::CaseInsensitive)) {
                return asset;
            }
        }
    }

    for (const ReleaseAsset &asset : info.assets) {
        if (!asset.name.endsWith(QStringLiteral(".txt"), Qt::CaseInsensitive)) {
            return asset;
        }
    }

    return info.assets.first();
}

QString Updater::parseSha256ForAsset(const QString &sha256SumsContent, const QString &assetName)
{
    if (sha256SumsContent.isEmpty() || assetName.isEmpty()) {
        return QString();
    }

    const QStringList lines = sha256SumsContent.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
    static const QRegularExpression regex(QStringLiteral("^([0-9a-fA-F]{64})\\s+\\*?(.+)$"));

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        QRegularExpressionMatch match = regex.match(trimmed);
        if (match.hasMatch()) {
            QString hash = match.captured(1).toLower();
            QString filename = match.captured(2).trimmed();
            if (filename == assetName || QFileInfo(filename).fileName() == assetName) {
                return hash;
            }
        }
    }
    return QString();
}

bool Updater::verifyFileSha256(const QString &filePath, const QString &expectedHexHash)
{
    QString cleanExpected = expectedHexHash.trimmed();
    if (cleanExpected.length() != 64) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return false;
    }

    QString actualHash = QString::fromLatin1(hash.result().toHex()).toLower();
    return actualHash.compare(cleanExpected, Qt::CaseInsensitive) == 0;
}

QString Updater::aptRepoSetupCommand()
{
    return QStringLiteral("curl -fsSL https://sebo1411.github.io/EASy68KLinux/setup-apt.sh | sudo bash");
}

QString Updater::rpmRepoSetupCommand()
{
    return QStringLiteral("curl -fsSL https://sebo1411.github.io/EASy68KLinux/setup-rpm.sh | sudo bash");
}

bool Updater::autoCheckOnStartup()
{
    QSettings settings(QStringLiteral("EASy68K"), QStringLiteral("EASy68KLinux"));
    return settings.value(QStringLiteral("AutoCheckUpdatesOnStartup"), true).toBool();
}

void Updater::setAutoCheckOnStartup(bool enabled)
{
    QSettings settings(QStringLiteral("EASy68K"), QStringLiteral("EASy68KLinux"));
    settings.setValue(QStringLiteral("AutoCheckUpdatesOnStartup"), enabled);
}

void Updater::checkForUpdates(bool silentIfUpToDate)
{
    if (m_state != UpdaterState::Idle && m_state != UpdaterState::Failed && m_state != UpdaterState::UpdateAvailable) {
        return;
    }
    if (!transitionTo(UpdaterState::CheckingRelease)) {
        return;
    }

    QUrl url(QStringLiteral("https://api.github.com/repos/Sebo1411/EASy68KLinux/releases/latest"));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EASy68K-Linux-Updater"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = netManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, silentIfUpToDate]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            transitionTo(UpdaterState::Failed);
            QString err = reply->errorString();
            emit updateCheckCompleted(false, ReleaseInfo(), err);
            if (!silentIfUpToDate && QGuiApplication::platformName() != QStringLiteral("offscreen")) {
                QMessageBox::warning(m_parentWidget, tr("Update Check Failed"),
                                     tr("Unable to check for updates from GitHub:\n%1").arg(err));
            }
            return;
        }

        QByteArray data = reply->readAll();
        ReleaseInfo info = parseReleaseJson(data);
        if (!info.isValid()) {
            transitionTo(UpdaterState::Failed);
            QString err = tr("Invalid release data received from server.");
            emit updateCheckCompleted(false, ReleaseInfo(), err);
            if (!silentIfUpToDate && QGuiApplication::platformName() != QStringLiteral("offscreen")) {
                QMessageBox::warning(m_parentWidget, tr("Update Check Failed"), err);
            }
            return;
        }

        bool updateAvailable = isNewerVersion(currentVersion(), info.version);
        if (updateAvailable) {
            transitionTo(UpdaterState::UpdateAvailable);
        } else {
            transitionTo(UpdaterState::Idle);
        }
        emit updateCheckCompleted(updateAvailable, info, QString());

        if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
            if (updateAvailable) {
                showUpdateDialog(info);
            } else if (!silentIfUpToDate) {
                QMessageBox msgBox(m_parentWidget);
                msgBox.setWindowTitle(tr("EASy68K is Up to Date"));
                msgBox.setIcon(QMessageBox::Information);
                msgBox.setText(tr("<h3>You are running the latest version (%1).</h3>"
                                  "<p>No newer release was found on GitHub.</p>").arg(currentVersion()));
                QPushButton *repoBtn = msgBox.addButton(tr("Configure Repositories..."), QMessageBox::ActionRole);
                msgBox.addButton(QMessageBox::Ok);
                msgBox.exec();
                if (msgBox.clickedButton() == repoBtn) {
                    showRepoSetupDialog();
                }
            }
        }
    });
}

void Updater::showUpdateDialog(const ReleaseInfo &info)
{
    QDialog dialog(m_parentWidget);
    dialog.setWindowTitle(tr("New EASy68K Update Available"));
    dialog.resize(600, 450);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *headerLabel = new QLabel(
        tr("<h2>A new version of EASy68K is available!</h2>"
           "<p><b>Installed Version:</b> %1 &nbsp;&nbsp;&rarr;&nbsp;&nbsp; <b>New Version:</b> %2</p>")
            .arg(currentVersion(), info.version),
        &dialog);
    layout->addWidget(headerLabel);

    ReleaseAsset asset = selectBestAssetForSystem(info);
    QString assetInfoText;
    if (!asset.name.isEmpty()) {
        double mbSize = static_cast<double>(asset.size) / (1024.0 * 1024.0);
        assetInfoText = tr("<b>Recommended Package for your system:</b> %1 (%2 MB)")
                            .arg(asset.name)
                            .arg(QString::number(mbSize, 'f', 1));
    } else {
        assetInfoText = tr("<b>Recommended Package:</b> Standard GitHub Release Archive");
    }
    QLabel *assetLabel = new QLabel(assetInfoText, &dialog);
    layout->addWidget(assetLabel);

    QLabel *notesHeader = new QLabel(tr("<b>Release Notes:</b>"), &dialog);
    layout->addWidget(notesHeader);

    QPlainTextEdit *notesEdit = new QPlainTextEdit(&dialog);
    notesEdit->setReadOnly(true);
    notesEdit->setPlainText(info.body);
    layout->addWidget(notesEdit);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *downloadBtn = new QPushButton(tr("Download && Install Update"), &dialog);
    downloadBtn->setDefault(true);
    QPushButton *repoBtn = new QPushButton(tr("Configure APT/RPM Auto-Update..."), &dialog);
    QPushButton *webBtn = new QPushButton(tr("Open GitHub Release Page"), &dialog);
    QPushButton *laterBtn = new QPushButton(tr("Later"), &dialog);

    btnLayout->addWidget(downloadBtn);
    btnLayout->addWidget(repoBtn);
    btnLayout->addWidget(webBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(laterBtn);
    layout->addLayout(btnLayout);

    connect(downloadBtn, &QPushButton::clicked, &dialog, [this, &dialog, info]() {
        dialog.accept();
        downloadAndApplyUpdate(info);
    });
    connect(repoBtn, &QPushButton::clicked, &dialog, [this, &dialog]() {
        dialog.accept();
        showRepoSetupDialog();
    });
    connect(webBtn, &QPushButton::clicked, &dialog, [info]() {
        QDesktopServices::openUrl(QUrl(info.htmlUrl));
    });
    connect(laterBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.exec();
}

void Updater::showRepoSetupDialog()
{
    QDialog dialog(m_parentWidget);
    dialog.setWindowTitle(tr("EASy68K Update Channels & Repositories"));
    dialog.resize(650, 480);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *title = new QLabel(tr("<h3>EASy68K Automatic Update Channels</h3>"), &dialog);
    layout->addWidget(title);

    QLabel *channelsDesc = new QLabel(
        tr("You can keep EASy68K updated through three supported channels:<br><br>"
           "<b>1. Built-in In-App Updater:</b> Checks GitHub releases directly from <i>Help &rarr; Check for Updates...</i>.<br>"
           "<b>2. AppImage Delta Updates:</b> The standalone AppImage includes embedded <code>zsync</code> metadata for fast, byte-level delta updates via <code>AppImageUpdate</code>.<br>"
           "<b>3. Native APT / DNF Package Repositories:</b> Integrate directly with your Linux distribution package manager for seamless system upgrades."),
        &dialog);
    channelsDesc->setWordWrap(true);
    layout->addWidget(channelsDesc);

    // APT Section
    QLabel *aptLabel = new QLabel(tr("<b>Ubuntu / Debian / Kubuntu / Mint (APT Repository):</b>"), &dialog);
    layout->addWidget(aptLabel);

    QHBoxLayout *aptLayout = new QHBoxLayout();
    QLineEdit *aptEdit = new QLineEdit(aptRepoSetupCommand(), &dialog);
    aptEdit->setReadOnly(true);
    QPushButton *copyAptBtn = new QPushButton(tr("Copy Command"), &dialog);
    aptLayout->addWidget(aptEdit);
    aptLayout->addWidget(copyAptBtn);
    layout->addLayout(aptLayout);

    connect(copyAptBtn, &QPushButton::clicked, &dialog, [aptEdit, copyAptBtn]() {
        QGuiApplication::clipboard()->setText(aptEdit->text());
        copyAptBtn->setText(tr("Copied!"));
    });

    // RPM Section
    QLabel *rpmLabel = new QLabel(tr("<b>Fedora / RHEL / openSUSE (RPM/DNF Repository):</b>"), &dialog);
    layout->addWidget(rpmLabel);

    QHBoxLayout *rpmLayout = new QHBoxLayout();
    QLineEdit *rpmEdit = new QLineEdit(rpmRepoSetupCommand(), &dialog);
    rpmEdit->setReadOnly(true);
    QPushButton *copyRpmBtn = new QPushButton(tr("Copy Command"), &dialog);
    rpmLayout->addWidget(rpmEdit);
    rpmLayout->addWidget(copyRpmBtn);
    layout->addLayout(rpmLayout);

    connect(copyRpmBtn, &QPushButton::clicked, &dialog, [rpmEdit, copyRpmBtn]() {
        QGuiApplication::clipboard()->setText(rpmEdit->text());
        copyRpmBtn->setText(tr("Copied!"));
    });

    layout->addSpacing(15);
    QHBoxLayout *closeLayout = new QHBoxLayout();
    closeLayout->addStretch();
    QPushButton *closeBtn = new QPushButton(tr("Close"), &dialog);
    closeLayout->addWidget(closeBtn);
    layout->addLayout(closeLayout);

    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();
}

void Updater::downloadAndApplyUpdate(const ReleaseInfo &info)
{
    if (m_state != UpdaterState::UpdateAvailable && m_state != UpdaterState::Idle && m_state != UpdaterState::Failed) {
        return;
    }

    ReleaseAsset asset = selectBestAssetForSystem(info);
    if (asset.downloadUrl.isEmpty()) {
        transitionTo(UpdaterState::Failed);
        QMessageBox::warning(m_parentWidget, tr("Download Unavailable"),
                             tr("No binary download asset found for your platform in this release.\n"
                                "Opening the release webpage..."));
        QDesktopServices::openUrl(QUrl(info.htmlUrl));
        transitionTo(UpdaterState::Idle);
        return;
    }

    // 1. Fetch SHA256SUMS.txt if available
    QString sha256Content;
    if (!info.sha256SumsUrl.isEmpty()) {
        transitionTo(UpdaterState::FetchingChecksums);
        QNetworkRequest shaReq{QUrl(info.sha256SumsUrl)};
        shaReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EASy68K-Linux-Updater"));
        shaReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *shaReply = netManager->get(shaReq);
        QEventLoop shaLoop;
        connect(shaReply, &QNetworkReply::finished, &shaLoop, &QEventLoop::quit);
        shaLoop.exec();
        if (shaReply->error() == QNetworkReply::NoError) {
            sha256Content = QString::fromUtf8(shaReply->readAll());
        }
        shaReply->deleteLater();
    }
    QString expectedHash;
    if (!sha256Content.isEmpty()) {
        expectedHash = parseSha256ForAsset(sha256Content, asset.name);
    }

    // 2. Prepare temporary file
    QString tempTemplate = QDir::tempPath() + QStringLiteral("/easy68k_download_XXXXXX.") + QFileInfo(asset.name).suffix();
    QTemporaryFile tempFile(tempTemplate);
    tempFile.setAutoRemove(false);
    if (!tempFile.open()) {
        transitionTo(UpdaterState::Failed);
        QMessageBox::critical(m_parentWidget, tr("Download Error"),
                              tr("Failed to allocate a temporary file for download."));
        return;
    }
    QString downloadedFilePath = tempFile.fileName();
    tempFile.close();

    QFile outFile(downloadedFilePath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        transitionTo(UpdaterState::Failed);
        QMessageBox::critical(m_parentWidget, tr("Download Error"),
                              tr("Failed to write to temporary file: %1").arg(downloadedFilePath));
        return;
    }

    // 3. Download asset
    transitionTo(UpdaterState::DownloadingBinary);
    QNetworkRequest assetReq{QUrl(asset.downloadUrl)};
    assetReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EASy68K-Linux-Updater"));
    assetReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QProgressDialog progress(tr("Downloading %1...").arg(asset.name), tr("Cancel"), 0, 100, m_parentWidget);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);

    QNetworkReply *assetReply = netManager->get(assetReq);
    QEventLoop loop;

    connect(assetReply, &QNetworkReply::readyRead, [&outFile, assetReply]() {
        outFile.write(assetReply->readAll());
    });

    connect(assetReply, &QNetworkReply::downloadProgress, [&progress](qint64 bytesReceived, qint64 bytesTotal) {
        if (bytesTotal > 0) {
            progress.setMaximum(static_cast<int>(bytesTotal / 1024));
            progress.setValue(static_cast<int>(bytesReceived / 1024));
        } else {
            progress.setMaximum(0);
            progress.setValue(0);
        }
    });

    connect(&progress, &QProgressDialog::canceled, assetReply, &QNetworkReply::abort);
    connect(assetReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    progress.show();
    loop.exec();

    bool wasCanceled = progress.wasCanceled();
    QNetworkReply::NetworkError replyError = assetReply->error();
    QString errorStr = assetReply->errorString();
    assetReply->deleteLater();
    outFile.flush();
    outFile.close();

    if (wasCanceled) {
        QFile::remove(downloadedFilePath);
        transitionTo(UpdaterState::Idle);
        return;
    }

    if (replyError != QNetworkReply::NoError) {
        QFile::remove(downloadedFilePath);
        transitionTo(UpdaterState::Failed);
        QMessageBox::critical(m_parentWidget, tr("Download Failed"),
                              tr("Failed to download update package:\n%1").arg(errorStr));
        return;
    }

    // 4. Verify SHA-256
    transitionTo(UpdaterState::VerifyingIntegrity);
    if (!expectedHash.isEmpty()) {
        if (!verifyFileSha256(downloadedFilePath, expectedHash)) {
            QFile::remove(downloadedFilePath);
            transitionTo(UpdaterState::Failed);
            QMessageBox::critical(m_parentWidget, tr("Verification Failed"),
                                  tr("Downloaded file failed SHA-256 checksum verification!\n\n"
                                     "Expected: %1\n\n"
                                     "The file has been deleted for security.").arg(expectedHash));
            return;
        }
    }

    // 5. Apply or save
    transitionTo(UpdaterState::ApplyingUpdate);
    if (qEnvironmentVariableIsSet("APPIMAGE")) {
        QString appImagePath = QString::fromLocal8Bit(qgetenv("APPIMAGE"));
        QFile::Permissions perms = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                   QFile::ReadGroup | QFile::ExeGroup |
                                   QFile::ReadOther | QFile::ExeOther;
        QFile::setPermissions(downloadedFilePath, perms);

        QString backupPath = appImagePath + QStringLiteral(".old");
        QFile::remove(backupPath);
        bool success = false;
        if (QFile::rename(appImagePath, backupPath)) {
            if (QFile::rename(downloadedFilePath, appImagePath) || QFile::copy(downloadedFilePath, appImagePath)) {
                QFile::setPermissions(appImagePath, perms);
                QFile::remove(backupPath);
                QFile::remove(downloadedFilePath);
                success = true;
            } else {
                QFile::rename(backupPath, appImagePath);
            }
        }

        if (success) {
            auto res = QMessageBox::question(m_parentWidget, tr("Update Complete"),
                tr("EASy68K has been successfully updated to version %1.\n\n"
                   "Would you like to restart the application now?").arg(info.version),
                QMessageBox::Yes | QMessageBox::No);
            if (res == QMessageBox::Yes) {
                QProcess::startDetached(appImagePath, QCoreApplication::arguments());
                QCoreApplication::quit();
            }
        } else {
            QMessageBox::warning(m_parentWidget, tr("Update Failed"),
                                 tr("Failed to replace running AppImage (%1).\n"
                                    "Ensure you have write permissions to the file location.").arg(appImagePath));
            QFile::remove(downloadedFilePath);
        }
        transitionTo(success ? UpdaterState::Idle : UpdaterState::Failed);
        return;
    }

    // System package (.deb or .rpm) or generic archive
    QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloadDir.isEmpty() || !QDir(downloadDir).exists()) {
        downloadDir = QDir::tempPath();
    }
    QString destPath = downloadDir + QDir::separator() + asset.name;
    QFile::remove(destPath);
    if (!QFile::rename(downloadedFilePath, destPath)) {
        if (QFile::copy(downloadedFilePath, destPath)) {
            QFile::remove(downloadedFilePath);
        } else {
            destPath = downloadedFilePath;
        }
    }

    bool isDeb = asset.name.endsWith(QStringLiteral(".deb"), Qt::CaseInsensitive);
    bool isRpm = asset.name.endsWith(QStringLiteral(".rpm"), Qt::CaseInsensitive);

    QMessageBox box(m_parentWidget);
    box.setWindowTitle(tr("Update Ready"));
    box.setText(tr("<h3>Update package downloaded successfully!</h3>"
                   "<p>File: <b>%1</b></p>"
                   "<p>Saved to: <code>%2</code></p>"
                   "<p>SHA-256 integrity verified.</p>")
                    .arg(asset.name, destPath));

    QPushButton *installBtn = nullptr;
    if ((isDeb || isRpm) && QFile::exists(QStringLiteral("/usr/bin/pkexec"))) {
        installBtn = box.addButton(tr("Install Now (pkexec)"), QMessageBox::AcceptRole);
    }
    QPushButton *folderBtn = box.addButton(tr("Open Containing Folder"), QMessageBox::ActionRole);
    box.addButton(tr("Done"), QMessageBox::RejectRole);

    box.exec();

    if (installBtn && box.clickedButton() == installBtn) {
        QStringList args;
        if (isDeb) {
            if (QFile::exists(QStringLiteral("/usr/bin/apt-get"))) {
                args << QStringLiteral("apt-get") << QStringLiteral("install") << QStringLiteral("-y") << destPath;
            } else {
                args << QStringLiteral("dpkg") << QStringLiteral("-i") << destPath;
            }
        } else {
            if (QFile::exists(QStringLiteral("/usr/bin/dnf"))) {
                args << QStringLiteral("dnf") << QStringLiteral("install") << QStringLiteral("-y") << destPath;
            } else {
                args << QStringLiteral("rpm") << QStringLiteral("-Uvh") << destPath;
            }
        }
        QProcess::startDetached(QStringLiteral("pkexec"), args);
    } else if (box.clickedButton() == folderBtn) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(downloadDir));
    }
    transitionTo(UpdaterState::Idle);
}
