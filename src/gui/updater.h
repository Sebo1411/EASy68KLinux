#ifndef UPDATER_H
#define UPDATER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QByteArray>

class QNetworkAccessManager;
class QWidget;

#ifndef EASY68K_VERSION_STRING
#define EASY68K_VERSION_STRING "5.16.1"
#endif

struct ReleaseAsset {
    QString name;
    QString downloadUrl;
    qint64 size = 0;
};

struct ReleaseInfo {
    QString tagName;
    QString version;
    QString htmlUrl;
    QString body;
    QString publishedAt;
    QList<ReleaseAsset> assets;
    QString sha256SumsUrl;
    bool isValid() const { return !version.isEmpty(); }
};

class Updater : public QObject
{
    Q_OBJECT

public:
    explicit Updater(QWidget *parentWidget = nullptr);
    ~Updater() override;

    static QString currentVersion();
    static QString repositorySlug();
    static bool isNewerVersion(const QString &currentVer, const QString &remoteVer);
    static ReleaseInfo parseReleaseJson(const QByteArray &jsonBytes);
    static ReleaseAsset selectBestAssetForSystem(const ReleaseInfo &info);
    static QString parseSha256ForAsset(const QString &sha256SumsContent, const QString &assetName);
    static bool verifyFileSha256(const QString &filePath, const QString &expectedHexHash);
    static QString aptRepoSetupCommand();
    static QString rpmRepoSetupCommand();

    static bool autoCheckOnStartup();
    static void setAutoCheckOnStartup(bool enabled);

public slots:
    void checkForUpdates(bool silentIfUpToDate = false);
    void showUpdateDialog(const ReleaseInfo &info);
    void showRepoSetupDialog();
    void downloadAndApplyUpdate(const ReleaseInfo &info);

signals:
    void updateCheckCompleted(bool updateAvailable, const ReleaseInfo &info, const QString &errorString);

private:
    QWidget *m_parentWidget = nullptr;
    QNetworkAccessManager *netManager = nullptr;
};

#endif // UPDATER_H
