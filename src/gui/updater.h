#ifndef UPDATER_H
#define UPDATER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QByteArray>

class QNetworkAccessManager;
class QWidget;

#ifndef EASY68K_VERSION_STRING
#define EASY68K_VERSION_STRING "5.17.0"
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

enum class UpdaterState : uint8_t {
    Idle = 0,
    CheckingRelease,
    UpdateAvailable,
    FetchingChecksums,
    DownloadingBinary,
    VerifyingIntegrity,
    ApplyingUpdate,
    Failed,
    Count
};

class Updater : public QObject
{
    Q_OBJECT

public:
    static constexpr bool isValidUpdaterTransition(UpdaterState from, UpdaterState to) noexcept
    {
        if (from == to) return true;

        switch (from) {
        case UpdaterState::Idle:
            return (to == UpdaterState::CheckingRelease ||
                    to == UpdaterState::DownloadingBinary ||
                    to == UpdaterState::Failed);

        case UpdaterState::CheckingRelease:
            return (to == UpdaterState::UpdateAvailable ||
                    to == UpdaterState::Idle ||
                    to == UpdaterState::Failed);

        case UpdaterState::UpdateAvailable:
            return (to == UpdaterState::FetchingChecksums ||
                    to == UpdaterState::DownloadingBinary ||
                    to == UpdaterState::CheckingRelease ||
                    to == UpdaterState::Idle ||
                    to == UpdaterState::Failed);

        case UpdaterState::FetchingChecksums:
            return (to == UpdaterState::DownloadingBinary ||
                    to == UpdaterState::Idle ||
                    to == UpdaterState::Failed);

        case UpdaterState::DownloadingBinary:
            return (to == UpdaterState::VerifyingIntegrity ||
                    to == UpdaterState::Idle ||
                    to == UpdaterState::Failed);

        case UpdaterState::VerifyingIntegrity:
            return (to == UpdaterState::ApplyingUpdate ||
                    to == UpdaterState::Idle ||
                    to == UpdaterState::Failed);

        case UpdaterState::ApplyingUpdate:
            return (to == UpdaterState::Idle ||
                    to == UpdaterState::Failed);

        case UpdaterState::Failed:
            return (to == UpdaterState::Idle ||
                    to == UpdaterState::CheckingRelease ||
                    to == UpdaterState::DownloadingBinary);

        default:
            return false;
        }
    }

    explicit Updater(QWidget *parentWidget = nullptr);
    ~Updater() override;

    UpdaterState state() const noexcept { return m_state; }
    bool transitionTo(UpdaterState newState);

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
    void stateChanged(UpdaterState newState);

private:
    UpdaterState m_state = UpdaterState::Idle;
    QWidget *m_parentWidget = nullptr;
    QNetworkAccessManager *netManager = nullptr;
};

#endif // UPDATER_H
